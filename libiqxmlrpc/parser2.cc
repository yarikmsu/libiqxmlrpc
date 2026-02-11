//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <libxml/xmlreader.h>
#include <libxml/xmlIO.h>
#include "parser2.h"
#include "except.h"

namespace iqxmlrpc {

namespace {

// Construct std::string from allocated xmlChar*, then free the xml memory.
// Use for libxml2 functions that return allocated strings (e.g., xmlGetNodePath).
inline std::string
to_string(xmlChar* s)
{
  if (s) {
    std::string retval(reinterpret_cast<const char*>(s));
    xmlFree(s);
    return retval;
  }

  return std::string();
}

} // nameless namespace

struct LibxmlInitializer {
  LibxmlInitializer()
  {
    // http://www.xmlsoft.org/threads.html
    xmlInitParser();
  }
};

LibxmlInitializer libxml_init;

//
// BuilderBase
//

BuilderBase::BuilderBase(Parser& p, bool t):
  parser_(p),
  depth_(0),
  expect_text_(t),
  want_exit_(false)
{
}

void
BuilderBase::build(bool flat)
{
  depth_ += flat ? 1 : 0;
  parser_.parse(*this);
}

void
BuilderBase::visit_element(const std::string& tag)
{
  depth_++;

  // SECURITY: Check element count limit (prevents "wide" XML DoS attacks)
  // Element count is tracked at Parser level to count across all sub-builders.
  int count = parser_.increment_element_count();
  if (count > MAX_ELEMENT_COUNT) {
    throw Parse_element_count_error(count, MAX_ELEMENT_COUNT);
  }

  // SECURITY: Check depth limit (prevents deeply nested XML attacks)
  int xml_depth = parser_.xml_depth();
  // SECURITY: xmlTextReaderDepth() returns -1 on error.
  // Check both error condition and depth limit.
  if (xml_depth < 0) {
    throw Parse_error("Failed to get XML depth (parser error)");
  }
  if (xml_depth > MAX_PARSE_DEPTH) {
    throw Parse_depth_error(xml_depth, MAX_PARSE_DEPTH);
  }

  do_visit_element(tag);
}

void
BuilderBase::visit_element_end(const std::string& tag)
{
  depth_--;
  do_visit_element_end(tag);

  if (!depth_)
    want_exit();
}

void
BuilderBase::visit_text(const std::string& text)
{
  do_visit_text(text);
}

void
BuilderBase::visit_text(std::string&& text)
{
  do_visit_text(std::move(text));
}

void
BuilderBase::do_visit_element_end(const std::string&)
{
}

void
BuilderBase::do_visit_text(const std::string&)
{
  if (expect_text_) {
    // proper handler was not implemented
    throw XML_RPC_violation(parser_.context());
  }
}

void
BuilderBase::do_visit_text(std::string&& text)
{
  // Default: forward to const-ref version
  do_visit_text(static_cast<const std::string&>(text));
}

//
// Parser
//

class Parser::Impl {
public:
  explicit Impl(const std::string& str):
    buf(str),
    reader(nullptr),
    curr(),
    pushed_back(false),
    element_count(0)
  {
    // SECURITY: Prevent integer truncation when casting size_t to int.
    // xmlReaderForMemory takes int for buffer size; a >2GB payload would
    // silently truncate, causing libxml2 to parse only a fragment.
    if (str.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
      throw Parse_error("XML payload exceeds maximum supported size");
    }
    const char* buf2 = str.data();
    int sz = static_cast<int>(str.size());
#if (LIBXML_VERSION < 20703)
#define XML_PARSE_HUGE 0
#endif
    reader = xmlReaderForMemory(buf2, sz, nullptr, nullptr, XML_PARSE_NONET | XML_PARSE_HUGE);
    if (!reader) {
      throw Parse_error("Failed to create XML reader");
    }
    if (xmlTextReaderSetParserProp(reader, XML_PARSER_SUBST_ENTITIES, 0) < 0) {
      xmlFreeTextReader(reader);
      throw Parse_error("Failed to disable XML entity substitution (XXE protection)");
    }
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  ~Impl()
  {
    xmlFreeTextReader(reader);
  }

  struct ParseStep {
    bool done;
    bool element_begin;
    bool element_end;
    bool is_empty;
    bool is_text;

    ParseStep():
      done(false),
      element_begin(false),
      element_end(false),
      is_empty(false),
      is_text(false)
    {
    }

    ParseStep(int type, xmlTextReaderPtr reader):
      done(false),
      element_begin(type == XML_READER_TYPE_ELEMENT),
      element_end(type == XML_READER_TYPE_END_ELEMENT),
      is_empty(element_begin && xmlTextReaderIsEmptyElement(reader)),
      is_text(type == XML_READER_TYPE_TEXT)
    {
    }
  };

  ParseStep
  read()
  {
    if (pushed_back) {
      pushed_back = false;
      return curr;
    }

    if (curr.is_empty) {
      curr.element_begin = false;
      curr.element_end = true;
      curr.is_empty = false;
      return curr;
    }

    int code = xmlTextReaderRead(reader);
    curr.done = true;

    if (code < 0) {
      const xmlError* err = xmlGetLastError();
      throw Parse_error(err ? err->message : "unknown parsing error");
    }

    if (code > 0) {
      curr = ParseStep(node_type(), reader);
    }

    return curr;
  }

  int
  node_type()
  {
    return xmlTextReaderNodeType(reader);
  }

  int
  depth()
  {
    return xmlTextReaderDepth(reader);
  }

  std::string
  tag_name()
  {
    // ConstName returns a dictionary-interned pointer, valid for the reader's lifetime.
    const xmlChar* name = xmlTextReaderConstName(reader);
    if (!name) {
      throw Parse_error("xmlTextReaderConstName returned NULL on element node");
    }
    std::string rv(reinterpret_cast<const char*>(name));

    // Strip namespace prefix (e.g., "ns:element" -> "element")
    // Only strip if there's content after the colon (guards against "element:")
    size_t pos = rv.find(':');
    if (pos != std::string::npos && pos + 1 < rv.size())
    {
      rv.erase(0, pos+1);
    }

    return rv;
  }

  std::string
  read_data()
  {
    if (!curr.is_text && !curr.element_end)
    {
      read();
      if (!curr.is_text && !curr.element_end) {
        std::string err = "text is expected at " + get_context();
        throw XML_RPC_violation(err);
      }
    }
    // ConstValue returns an internal buffer pointer, valid until next xmlTextReaderRead().
    if (curr.is_text) {
      const xmlChar* val = xmlTextReaderConstValue(reader);
      if (!val) {
        throw Parse_error("xmlTextReaderConstValue returned NULL on text node");
      }
      return std::string(reinterpret_cast<const char*>(val));
    }
    // element_end: no text value expected, return empty string
    return std::string();
  }

  std::string
  get_context() const
  {
    xmlNodePtr n = xmlTextReaderCurrentNode(reader);
    return to_string(xmlGetNodePath(n));
  }

  const std::string buf;
  xmlTextReaderPtr reader;
  ParseStep curr;
  bool pushed_back;
  int element_count;
};

Parser::Parser(const std::string& buf):
  impl_(new Parser::Impl(buf))
{
}

void
Parser::parse(BuilderBase& builder)
{
  for (Impl::ParseStep p = impl_->read(); !p.done; p = impl_->read()) {
    if (p.element_begin) {
      builder.visit_element(impl_->tag_name());

    } else if (p.element_end) {
      if (!builder.depth()) {
        impl_->pushed_back = true;
        break;
      }

      builder.visit_element_end(impl_->tag_name());

    } else if (p.is_text && builder.expects_text()) {
      // PERFORMANCE: get_data() returns by value (temporary rvalue) -
      // the move overload of visit_text is selected automatically.
      builder.visit_text(get_data());
    }

    if (builder.wants_exit())
      break;

  } // for
}

std::string
Parser::get_data()
{
  return impl_->read_data();
}

std::string
Parser::context() const
{
  return impl_->get_context();
}

int
Parser::xml_depth() const
{
  return impl_->depth();
}

int
Parser::element_count() const
{
  return impl_->element_count;
}

int
Parser::increment_element_count()
{
  return ++impl_->element_count;
}

//
// StateMachine
//

StateMachine::StateMachine(const Parser& p, int start_state):
  parser_(p),
  curr_(start_state)
{
}

void
StateMachine::set_transitions(const StateTransition* t)
{
  trans_ = t;
}

int
StateMachine::change(const std::string& tag)
{
  for (size_t i = 0; trans_[i].tag != nullptr; ++i) {
    if (trans_[i].tag == tag && trans_[i].prev_state == curr_) {
      curr_ = trans_[i].new_state;
      return curr_;
    }
  }

  std::string err = "unexpected tag <" + std::string(tag) + "> at " + parser_.context();
  throw XML_RPC_violation(err);
}

void
StateMachine::set_state(int new_state)
{
  curr_ = new_state;
}

} // namespace iqxmlrpc
// vim:sw=2:ts=2:et:

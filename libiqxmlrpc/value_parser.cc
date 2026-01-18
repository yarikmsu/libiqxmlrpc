//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include <stdexcept>
#include "except.h"
#include "num_conv.h"
#include "value_parser.h"

namespace iqxmlrpc {

ValueBuilderBase::ValueBuilderBase(Parser& parser, bool expect_text):
  BuilderBase(parser, expect_text),
  retval()
{
}

namespace {

class StructBuilder: public ValueBuilderBase {
public:
  StructBuilder(const StructBuilder&) = delete;
  StructBuilder& operator=(const StructBuilder&) = delete;

  explicit StructBuilder(Parser& parser):
    ValueBuilderBase(parser),
    state_(parser, NONE),
    name_(),
    value_(nullptr),
    proxy_(nullptr)
  {
    static const StateMachine::StateTransition trans[] = {
      { NONE, MEMBER, "member" },
      { MEMBER, NAME_READ, "name" },
      { NAME_READ, VALUE_READ, "value" },
      { 0, 0, nullptr }
    };
    state_.set_transitions(trans);
    retval.reset(proxy_ = new Struct());
  }

private:
  enum State {
    NONE,
    MEMBER,
    NAME_READ,
    VALUE_READ,
  };

  void
  do_visit_element(const std::string& tagname) override
  {
    switch (state_.change(tagname)) {
    case NAME_READ:
      name_ = parser_.get_data();
      break;

    case VALUE_READ:
      value_ = sub_build<Value_type*, ValueBuilder>();
      value_ = value_ ? value_ : new String("");
      break;

    case MEMBER:
      break;

    default:
      throw XML_RPC_violation(parser_.context());
    }
  }

  void
  do_visit_element_end(const std::string& tagname) override
  {
    if (tagname == "member") {
      if (state_.get_state() != VALUE_READ) {
        throw XML_RPC_violation(parser_.context());
      }

      proxy_->insert(name_, std::make_unique<Value>(value_));
      state_.set_state(NONE);
    }
  }

  StateMachine state_;
  std::string name_;
  Value_type* value_;
  Struct* proxy_;
};

class ArrayBuilder: public ValueBuilderBase {
public:
  ArrayBuilder(const ArrayBuilder&) = delete;
  ArrayBuilder& operator=(const ArrayBuilder&) = delete;

  explicit ArrayBuilder(Parser& parser):
    ValueBuilderBase(parser),
    state_(parser, NONE),
    proxy_(nullptr)
  {
    static const StateMachine::StateTransition trans[] = {
      { NONE, DATA, "data" },
      { DATA, VALUES, "value" },
      { VALUES, VALUES, "value" },
      { 0, 0, nullptr }
    };
    state_.set_transitions(trans);
    retval.reset(proxy_ = new Array());
  }

private:
  enum State {
    NONE,
    DATA,
    VALUES
  };

  void
  do_visit_element(const std::string& tagname) override
  {
    if (state_.change(tagname) == VALUES) {
      Value_type* tmp = sub_build<Value_type*, ValueBuilder>();
      tmp = tmp ? tmp : new String("");
      proxy_->push_back(std::make_unique<Value>(tmp));
    }
  }

  StateMachine state_;
  Array* proxy_;
};

} // anonymous namespace

enum ValueBuilderState {
  VALUE,
  STRING,
  INT,
  INT64,
  BOOL,
  DOUBLE,
  BINARY,
  TIME,
  STRUCT,
  ARRAY,
  NIL
};

ValueBuilder::ValueBuilder(Parser& parser):
  ValueBuilderBase(parser, true),
  state_(parser, VALUE)
{
  static const StateMachine::StateTransition trans[] = {
    { VALUE,  STRING, "string" },
    { VALUE,  INT,    "int" },
    { VALUE,  INT,    "i4" },
    { VALUE,  INT64,  "i8" },
    { VALUE,  BOOL,   "boolean" },
    { VALUE,  DOUBLE, "double" },
    { VALUE,  BINARY, "base64" },
    { VALUE,  TIME,   "dateTime.iso8601" },
    { VALUE,  STRUCT, "struct" },
    { VALUE,  ARRAY,  "array" },
    { VALUE,  NIL,    "nil" },
    { 0, 0, nullptr }
  };
  state_.set_transitions(trans);
}

void
ValueBuilder::do_visit_element(const std::string& tagname)
{
  switch (state_.change(tagname)) {
  case STRUCT:
    retval.reset(sub_build<Value_type*, StructBuilder>(true));
    break;

  case ARRAY:
    retval.reset(sub_build<Value_type*, ArrayBuilder>(true));
    break;

  case NIL:
    // NOLINTNEXTLINE(modernize-make-unique) performance-critical parsing path
    retval.reset(new Nil());
    break;

  default:
    // wait for text within <i4>...</i4>, etc...
    break;
  }

  if (retval)
    want_exit();
}

void
ValueBuilder::do_visit_element_end(const std::string&)
{
  if (retval)
    return;

  std::unique_ptr<Int> default_int(Value::get_default_int());
  std::unique_ptr<Int64> default_int64(Value::get_default_int64());

  switch (state_.get_state()) {
  case VALUE:
  case STRING:
    // NOLINTNEXTLINE(modernize-make-unique) performance-critical parsing path
    retval.reset(new String(""));
    break;

  case INT:
    if (default_int) {
      retval.reset(default_int.release());
      break;
    }
    throw XML_RPC_violation(parser_.context());

  case INT64:
    if (default_int64) {
      retval.reset(default_int64.release());
      break;
    }
    throw XML_RPC_violation(parser_.context());

  case BINARY:
    retval.reset(Binary_data::from_data(""));
    break;

  default:
    throw XML_RPC_violation(parser_.context());
  }
}

void
ValueBuilder::do_visit_text(const std::string& text)
{
  switch (state_.get_state()) {
  case VALUE:
    want_exit();
    [[fallthrough]];
  case STRING:
    // NOLINTNEXTLINE(modernize-make-unique) performance-critical parsing path
    retval.reset(new String(text));
    break;

  case INT:
    // NOLINTNEXTLINE(modernize-make-unique) performance-critical parsing path
    retval.reset(new Int(num_conv::from_string<int>(text)));
    break;

  case INT64:
    // NOLINTNEXTLINE(modernize-make-unique) performance-critical parsing path
    retval.reset(new Int64(num_conv::from_string<int64_t>(text)));
    break;

  case BOOL:
    // NOLINTNEXTLINE(modernize-make-unique) performance-critical parsing path
    retval.reset(new Bool(num_conv::from_string<int>(text) != 0));
    break;

  case DOUBLE:
    // NOLINTNEXTLINE(modernize-make-unique) performance-critical parsing path
    retval.reset(new Double(num_conv::string_to_double(text)));
    break;

  case BINARY:
    retval.reset(Binary_data::from_base64(text));
    break;

  case TIME:
    // NOLINTNEXTLINE(modernize-make-unique) performance-critical parsing path
    retval.reset(new Date_time(text));
    break;

  default:
    throw XML_RPC_violation(parser_.context());
  }
}

} // namespace iqxmlrpc

// vim:sw=2:ts=2:et:

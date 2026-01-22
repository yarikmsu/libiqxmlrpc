//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include <stdexcept>
#include "except.h"
#include "xml_builder.h"

namespace iqxmlrpc {

namespace {

template <class T>
void
throwBuildError(T res, T err_res)
{
  if (res == err_res) {
    const xmlError* err = xmlGetLastError();
    throw XmlBuild_error(err ? err->message : "unknown error");
  }
}

} // anonymous namespace

//
// XmlBuilder::Node
//

XmlBuilder::Node::Node(XmlBuilder& w, const char* name):
  ctx(w)
{
  auto xname = reinterpret_cast<const xmlChar*>(name);
  throwBuildError(xmlTextWriterStartElement(ctx.writer, xname), -1);
}

XmlBuilder::Node::~Node()
{
  xmlTextWriterEndElement(ctx.writer);
}

void
XmlBuilder::Node::set_textdata(const std::string& data)
{
  ctx.add_textdata(data);
}

//
// XmlBuilder
//

XmlBuilder::XmlBuilder():
  buf(xmlBufferCreate()),
  writer(nullptr)
{
  throwBuildError(writer = xmlNewTextWriterMemory(buf, 0), static_cast<xmlTextWriter*>(nullptr));
  throwBuildError(xmlTextWriterStartDocument(writer, nullptr, "UTF-8", nullptr), -1);
}

XmlBuilder::~XmlBuilder()
{
  xmlFreeTextWriter(writer);
  xmlBufferFree(buf);
}

void
XmlBuilder::add_textdata(const std::string& data)
{
  auto xdata = reinterpret_cast<const xmlChar*>(data.c_str());
  throwBuildError(xmlTextWriterWriteString(writer, xdata), -1);
}

void
XmlBuilder::stop()
{
  throwBuildError(xmlTextWriterEndDocument(writer), -1);
}

std::string
XmlBuilder::content() const
{
  xmlTextWriterFlush(writer);
  auto cdata = reinterpret_cast<const char*>(xmlBufferContent(buf));
  return std::string(cdata, buf->use);
}

std::string_view
XmlBuilder::content_view() const
{
  xmlTextWriterFlush(writer);
  auto cdata = reinterpret_cast<const char*>(xmlBufferContent(buf));
  return std::string_view(cdata, buf->use);
}

} // namespace iqxmlrpc
// vim:ts=2:sw=2:et

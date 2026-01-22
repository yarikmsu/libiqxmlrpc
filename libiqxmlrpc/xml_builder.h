//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_XML_BUILDER_H
#define IQXMLRPC_XML_BUILDER_H

#include "api_export.h"

#include <string>
#include <string_view>
#include <libxml/xmlwriter.h>

namespace iqxmlrpc {

class XmlBuilder {
public:
  XmlBuilder(const XmlBuilder&) = delete;
  XmlBuilder& operator=(const XmlBuilder&) = delete;

  class Node {
  public:
    Node(XmlBuilder&, const char* name);
    ~Node();

    void
    set_textdata(const std::string&);

  private:
    XmlBuilder& ctx;
  };

  XmlBuilder();
  ~XmlBuilder();

  void
  add_textdata(const std::string&);

  void
  stop();

  std::string
  content() const;

  //! Return content as string_view (avoids copy for callers that don't need ownership).
  //! @warning The returned view is only valid while the XmlBuilder exists and no
  //!          modifications are made. Caller must not use the view after XmlBuilder
  //!          is destroyed or modified.
  std::string_view
  content_view() const;

private:
  xmlBufferPtr buf;
  xmlTextWriterPtr writer;
};

} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include <boost/foreach.hpp>

#include "response.h"
#include "response_parser.h"

#include "except.h"
#include "value.h"
#include "value_type_xml.h"
#include "xml_builder.h"

namespace iqxmlrpc {

Response
parse_response( const std::string& response_string )
{
  Parser parser(response_string);
  ResponseBuilder builder(parser);
  builder.build();
  return builder.get();
}

std::string
dump_response( const Response& response )
{
  XmlBuilder writer;
  XmlBuilder::Node root(writer, "methodResponse");
  Value_type_to_xml value_xml_visitor(writer, true);

  if (!response.is_fault()) {
    XmlBuilder::Node params(writer, "params");
    XmlBuilder::Node param(writer, "param");
    response.value().apply_visitor(value_xml_visitor);
  } else {
    XmlBuilder::Node fault_node(writer, "fault");
    Struct fault;
    fault.insert( "faultCode", response.fault_code() );
    fault.insert( "faultString", response.fault_string() );
    Value(fault).apply_visitor(value_xml_visitor);
  }

  writer.stop();
  return writer.content();
}

//
// Response
//

Response::Response( Value* v ):
  value_(v),
  fault_code_(0)
{
}

Response::Response( int fcode, const std::string& fstring ):
  fault_code_(fcode),
  fault_string_(fstring)
{
}

const Value& Response::value() const
{
  if( is_fault() )
    throw iqxmlrpc::Exception( fault_string_, fault_code_ );

  return *value_;
}

} // namespace iqxmlrpc

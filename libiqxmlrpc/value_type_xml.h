//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _iqxmlrpc_value_type_xml_h_
#define _iqxmlrpc_value_type_xml_h_

#include <string>
#include "value_type_visitor.h"

namespace iqxmlrpc {

class XmlBuilder;

//! Value_type visitor that converts values into XML-RPC representation.
class Value_type_to_xml: public Value_type_visitor {
public:
  explicit Value_type_to_xml(XmlBuilder& builder, bool server_mode = false):
    builder_(builder),
    server_mode_(server_mode) {}

private:
  void do_visit_value(const Value_type&) override;
  void do_visit_nil() override;
  void do_visit_int(int) override;
  void do_visit_int64(int64_t) override;
  void do_visit_double(double) override;
  void do_visit_bool(bool) override;
  void do_visit_string(const std::string&) override;
  void do_visit_struct(const Struct&) override;
  void do_visit_array(const Array&) override;
  void do_visit_base64(const Binary_data&) override;
  void do_visit_datetime(const Date_time&) override;

  void add_textnode(const char* name, const std::string& data);

  XmlBuilder& builder_;
  bool server_mode_;
};

} // namespace iqxmlrpc

#endif // _iqxmlrpc_value_type_xml_h_

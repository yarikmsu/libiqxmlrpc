//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include "value_type_visitor.h"

#include "value.h"

#include <iostream>

namespace iqxmlrpc {

Print_value_visitor::Print_value_visitor(std::ostream& out):
  out_(out)
{
}

void Print_value_visitor::do_visit_value(const Value_type& v)
{
  v.apply_visitor(*this);
}

void Print_value_visitor::do_visit_nil()
{
  out_ << "NIL";
}

void Print_value_visitor::do_visit_int(int val)
{
  out_ << val;
}

void Print_value_visitor::do_visit_int64(int64_t val)
{
  out_ << val;
}

void Print_value_visitor::do_visit_double(double val)
{
  out_ << val;
}

void Print_value_visitor::do_visit_bool(bool val)
{
  out_ << val;
}

void Print_value_visitor::do_visit_string(const std::string& val)
{
  out_ << "'" << val << "'";
}

void Print_value_visitor::do_visit_struct(const Struct& s)
{
  out_ << "{";

  for (const auto& [key, value] : s)
  {
    out_ << " '" << key << "' => ";
    value->apply_visitor(*this);
    out_ << ",";
  }

  out_ << " }";
}

void Print_value_visitor::do_visit_array(const Array& a)
{
  out_ << "[";

  for (const auto& elem : a)
  {
    out_ << " ";
    elem.apply_visitor(*this);
    out_ << ",";
  }

  out_ << " ]";
}

void Print_value_visitor::do_visit_base64(const Binary_data&)
{
  out_ << "RAWDATA";
}

void Print_value_visitor::do_visit_datetime(const Date_time& d)
{
  do_visit_string(d.to_string());
}

} // namespace iqxmlrpc

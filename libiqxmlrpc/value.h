//  Libiqnet + Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2004 Anton Dedov
//  
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//  
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//  
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
//  
//  $Id: value.h,v 1.12 2004-05-11 10:11:46 adedov Exp $

#ifndef _iqxmlrpc_value_h_
#define _iqxmlrpc_value_h_

#include <string>
#include <vector>
#include <map>
#include <typeinfo>
#include "value_type.h"
#include "except.h"


namespace iqxmlrpc 
{
  class Value;
};


//! Proxy class to access XML-RPC values by users.
/*! For more documentation please look into \ref value_usage .
    \exception Bad_cast 
*/
class iqxmlrpc::Value {
public:
  //! Bad_cast is being thrown on illegal 
  //! type conversion or Value::get_X() call.
  class Bad_cast: public Exception {
  public:
    Bad_cast():
      Exception( "iqxmlrpc::Value: incorrect type was requested." ) {}
  };
  
private:
  Value_type* value;
  
public:
  Value( Value_type* );
  Value( const Value& );
  Value( int );
  Value( bool );
  Value( double );
  Value( std::string );
  Value( const char* );
  Value( const Binary_data& );
  Value( const Date_time& );
  Value( const struct tm* );
  Value( const Array& );
  Value( const Struct& );

  virtual ~Value();

  const Value& operator =( const Value& );

  //! \name Type identification
  //! \{
  bool is_int()    const { return can_cast<Int>(); }
  bool is_bool()   const { return can_cast<Bool>(); }
  bool is_double() const { return can_cast<Double>(); }
  bool is_string() const { return can_cast<String>(); }
  bool is_binary() const { return can_cast<Binary_data>(); }
  bool is_datetime() const { return can_cast<Date_time>(); }
  bool is_array()  const { return can_cast<Array>(); }
  bool is_struct() const { return can_cast<Struct>(); }

  std::string type_debug() const { return typeid(*value).name(); }
  //! \}

  //! \name Access scalar value
  //! \{
  int         get_int()    const { return cast<Int>()->value(); }
  bool        get_bool()   const { return cast<Bool>()->value(); }
  double      get_double() const { return cast<Double>()->value(); }
  std::string get_string() const { return cast<String>()->value(); }
  Binary_data get_binary() const { return Binary_data(*cast<Binary_data>()); }
  Date_time   get_datetime() const { return Date_time(*cast<Date_time>()); }

  operator int()         const { return get_int(); }
  operator bool()        const { return get_bool(); }
  operator double()      const { return get_double(); }
  operator std::string() const { return get_string(); }
  operator Binary_data() const { return get_binary(); }
  operator struct tm()   const { return get_datetime().get_tm(); }
  //! \}
  
  //! \name Array functions
  //! \{
  //! Access inner Array value
  Array& the_array() { return *cast<Array>(); }
  
  unsigned size() const { return cast<Array>()->size(); }
  const Value& operator []( int ) const;
  Value&       operator []( int );
  
  void push_back( Value* v )       { cast<Array>()->push_back(v); }
  void push_back( const Value& v ) { cast<Array>()->push_back(v); }
  //! \}
  
  //! \name Struct functions
  //! \{
  //! Access inner Struct value.
  Struct& the_struct() { return *cast<Struct>(); }

  bool has_field( const std::string& f ) const
  {
    return cast<Struct>()->has_field(f);
  }
  
  const Value& operator []( const char* ) const;
  Value&       operator []( const char* );
  const Value& operator []( const std::string& ) const;
  Value&       operator []( const std::string& );
  
  void insert( const std::string& n, Value* v )       
  { 
    cast<Struct>()->insert(n,v); 
  }
  
  void insert( const std::string& n, const Value& v ) 
  { 
    cast<Struct>()->insert(n,v); 
  }
  //! \}
  
  //! \name XML building
  //! \{
  /*!
    \param node  Parent node to attach to.
    \param debug Not enclose actual value in <value> tag.
  */
  void to_xml( xmlpp::Node* node, bool debug = false ) const;
  //! \}
  
private:
  template <class T> T* cast() const
  {
    T* t = dynamic_cast<T*>( value );
    if( !t )
      throw Bad_cast();
    return t;
  }
  
  template <class T> bool can_cast() const 
  {
    return dynamic_cast<T*>( value );
  }
};


#endif

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2014 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include <atomic>
#include <stdexcept>
#include <utility>

#include "value.h"
#include "value_type_visitor.h"
#include "value_type_xml.h"


namespace iqxmlrpc {

Value::Bad_cast::Bad_cast():
  Exception( "iqxmlrpc::Value: incorrect type was requested." ) {}


namespace ValueOptions {
  // Atomic flag+value pairs replace std::optional<T> for data-race freedom (CWE-362).
  // Each load/store is individually atomic (indivisible, default seq_cst ordering),
  // but the flag and value are NOT logically atomic as a pair: a reader may
  // transiently observe the flag from one writer and the value from another.
  // Caller contract: these options must be set once at startup before any server
  // threads are spawned. The library does not enforce this — it is the caller's
  // responsibility. Given that contract, readers always get a value that was
  // explicitly stored — never uninitialized or torn data.
  // Do not weaken to relaxed ordering without reviewing all call sites.
  std::atomic<bool> has_default_int{false};
  std::atomic<int>  default_int_val{0};

  std::atomic<bool>    has_default_int64{false};
  std::atomic<int64_t> default_int64_val{0};

  std::atomic<bool> omit_string_tag_in_responses{false};
}

void Value::set_default_int(int dint)
{
  // Store value before flag so readers never see flag=true with stale value
  ValueOptions::default_int_val.store(dint);
  ValueOptions::has_default_int.store(true);
}

Int* Value::get_default_int()
{
  if (ValueOptions::has_default_int.load())
    return new Int(ValueOptions::default_int_val.load());
  return nullptr;
}

void Value::drop_default_int()
{
  ValueOptions::has_default_int.store(false);
}

void Value::set_default_int64(int64_t dint)
{
  // Store value before flag so readers never see flag=true with stale value
  ValueOptions::default_int64_val.store(dint);
  ValueOptions::has_default_int64.store(true);
}

Int64* Value::get_default_int64()
{
  if (ValueOptions::has_default_int64.load())
    return new Int64(ValueOptions::default_int64_val.load());
  return nullptr;
}

void Value::drop_default_int64()
{
  ValueOptions::has_default_int64.store(false);
}

void Value::omit_string_tag_in_responses(bool v)
{
  ValueOptions::omit_string_tag_in_responses.store(v);
}

bool Value::omit_string_tag_in_responses()
{
  return ValueOptions::omit_string_tag_in_responses.load();
}

Value::Value( Value_type* v ):
  value(v)
{
}

Value::Value( const Value& v ):
  value( v.value->clone() )
{
}

Value::Value( Value&& v ) noexcept :
  value( v.value )
{
  v.value = nullptr;
}

Value::Value( const Nil& n ):
  value( n.clone() )
{
}

Value::Value( int i ):
  value( new Int(i) )
{
}

Value::Value( int64_t i ):
  value( new Int64(i) )
{
}

Value::Value( bool b ):
  value( new Bool(b) )
{
}

Value::Value( double d ):
  value( new Double(d) )
{
}

Value::Value( const std::string& s ):
  value( new String(s) )
{
}

Value::Value( const char* s ):
  value( new String(s) )
{
}

Value::Value( const Array& arr ):
  value( arr.clone() )
{
}

Value::Value( const Struct& st ):
  value( st.clone() )
{
}

Value::Value( const Binary_data& bin ):
  value( bin.clone() )
{
}

Value::Value( const Date_time& dt ):
  value( dt.clone() )
{
}

Value::Value( const struct tm* dt ):
  value( new Date_time(dt) )
{
}

Value::~Value()
{
  delete value;
}

template <class T>
T* Value::cast() const
{
  // Null check guards against use-after-move
  if (!value)
    throw Bad_cast();
  // Use type tag for O(1) type checking instead of slow dynamic_cast
  if (value->type_tag() != TypeTag<T>::value)
    throw Bad_cast();
  return static_cast<T*>(value);
}

const Value& Value::operator =( const Value& v )
{
  // Copy-and-swap idiom: provides strong exception safety and
  // automatically handles self-assignment correctly
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization) - used in swap below
  Value tmp(v);
  std::swap(value, tmp.value);
  return *this;
}

Value& Value::operator =( Value&& v ) noexcept
{
  if (this != &v) {
    delete value;
    value = v.value;
    v.value = nullptr;
  }
  return *this;
}

bool Value::is_nil() const
{
  return value && value->type_tag() == ValueTypeTag::Nil;
}

bool Value::is_int() const
{
  return value && value->type_tag() == ValueTypeTag::Int;
}

bool Value::is_int64() const
{
  return value && value->type_tag() == ValueTypeTag::Int64;
}

bool Value::is_bool() const
{
  return value && value->type_tag() == ValueTypeTag::Bool;
}

bool Value::is_double() const
{
  return value && value->type_tag() == ValueTypeTag::Double;
}

bool Value::is_string() const
{
  return value && value->type_tag() == ValueTypeTag::String;
}

bool Value::is_binary() const
{
  return value && value->type_tag() == ValueTypeTag::Binary;
}

bool Value::is_datetime() const
{
  return value && value->type_tag() == ValueTypeTag::DateTime;
}

bool Value::is_array() const
{
  return value && value->type_tag() == ValueTypeTag::Array;
}

bool Value::is_struct() const
{
  return value && value->type_tag() == ValueTypeTag::Struct;
}
const std::string& Value::type_name() const
{
  return value->type_name();
}

int Value::get_int() const
{
  return cast<Int>()->value();
}

int64_t Value::get_int64() const
{
  return cast<Int64>()->value();
}

bool Value::get_bool() const
{
  return cast<Bool>()->value();
}

double Value::get_double() const
{
  return cast<Double>()->value();
}

std::string Value::get_string() const
{
  return cast<String>()->value();
}

Binary_data Value::get_binary() const
{
  return Binary_data(*cast<Binary_data>());
}

Date_time Value::get_datetime() const
{
  return Date_time(*cast<Date_time>());
}

Value::operator int() const
{
  return get_int();
}

Value::operator int64_t() const
{
  return get_int64();
}

Value::operator bool() const
{
  return get_bool();
}

Value::operator double() const
{
  return get_double();
}

Value::operator std::string() const
{
  return get_string();
}

Value::operator Binary_data() const
{
  return get_binary();
}

Value::operator struct tm() const
{
  return get_datetime().get_tm();
}

Array& Value::the_array()
{
  return *cast<Array>();
}

const Array& Value::the_array() const
{
  return *cast<Array>();
}

size_t Value::size() const
{
  return cast<Array>()->size();
}

void Value::push_back( const Value& v )
{
  cast<Array>()->push_back(v);
}

const Value& Value::operator []( int i ) const
{
  const Array *a = cast<Array>();
  return (*a)[i];
}

Value& Value::operator []( int i )
{
  return (*cast<Array>())[i];
}

Array::const_iterator Value::arr_begin() const
{
  return cast<Array>()->begin();
}

Array::const_iterator Value::arr_end() const
{
  return cast<Array>()->end();
}

Struct& Value::the_struct()
{
  return *cast<Struct>();
}

const Struct& Value::the_struct() const
{
  return *cast<Struct>();
}

bool Value::has_field( const std::string& f ) const
{
  return cast<Struct>()->has_field(f);
}

const Value& Value::operator []( const std::string& s ) const
{
  return (*cast<Struct>())[s];
}

Value& Value::operator []( const std::string& s )
{
  return (*cast<Struct>())[s];
}

const Value& Value::operator []( const char* s ) const
{
  return (*cast<Struct>())[s];
}

Value& Value::operator []( const char* s )
{
  return (*cast<Struct>())[s];
}

void Value::insert( const std::string& n, const Value& v )
{
  cast<Struct>()->insert(n,v);
}

void Value::apply_visitor(Value_type_visitor& v) const
{
  v.visit_value(*value);
}

//
// Free functions
//

void value_to_xml(XmlBuilder& builder, const Value& v)
{
  Value_type_to_xml vis(builder);
  v.apply_visitor(vis);
}

void print_value(const Value& v, std::ostream& s)
{
  Print_value_visitor vis(s);
  v.apply_visitor(vis);
}

} // namespace iqxmlrpc

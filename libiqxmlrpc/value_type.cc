//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include "value_type.h"

#include "safe_math.h"
#include "util.h"
#include "value.h"
#include "value_type_visitor.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <ctime>
#include <string.h>

namespace iqxmlrpc {
namespace type_names {
  const std::string nil_type_name     = "nil";
  const std::string int_type_name     = "i4";
  const std::string int64_type_name   = "i8";
  const std::string bool_type_name    = "boolean";
  const std::string double_type_name  = "double";
  const std::string string_type_name  = "string";
  const std::string array_type_name   = "array";
  const std::string struct_type_name  = "struct";
  const std::string base64_type_name  = "base64";
  const std::string date_type_name    = "dateTime.iso8601";
} // namespace type_names


Value_type* Nil::clone() const
{
  return new Nil();
}


const std::string& Nil::type_name() const
{
  return type_names::nil_type_name;
}


void Nil::apply_visitor(Value_type_visitor& v) const
{
  v.visit_nil();
}


// --------------------- Scalar's specialization ------------------------------
template<>
const std::string& Int::type_name() const
{
  return type_names::int_type_name;
}

template<>
void Int::apply_visitor(Value_type_visitor& v) const
{
  v.visit_int(value_);
}

template<>
const std::string& Int64::type_name() const
{
  return type_names::int64_type_name;
}

template<>
void Int64::apply_visitor(Value_type_visitor& v) const
{
  v.visit_int64(value_);
}

template<>
const std::string& Bool::type_name() const
{
  return type_names::bool_type_name;
}

template<>
void Bool::apply_visitor(Value_type_visitor& v) const
{
  v.visit_bool(value_);
}

template<>
const std::string& Double::type_name() const
{
  return type_names::double_type_name;
}

template<>
void Double::apply_visitor(Value_type_visitor& v) const
{
  v.visit_double(value_);
}

template<>
const std::string& String::type_name() const
{
  return type_names::string_type_name;
}

template<>
void String::apply_visitor(Value_type_visitor& v) const
{
  v.visit_string(value_);
}


// --------------------------------------------------------------------------
#ifndef DOXYGEN_SHOULD_SKIP_THIS
class Array::Array_inserter {
  Array::Val_vector* vv;

public:
  explicit Array_inserter( Array::Val_vector* v ): vv(v) {}

  void operator ()( const Value& v )
  {
    vv->push_back( new Value(v) );
  }
};
#endif


Array::Array( const Array& other ):
  Value_type(ValueTypeTag::Array),
  values()
{
  std::for_each( other.begin(), other.end(), Array_inserter(&values) );
}


Array::~Array()
{
  clear();
}


Array& Array::operator =( const Array& other )
{
  if( this == &other )
    return *this;

  Array tmp(other);
  tmp.swap(*this);
  return *this;
}


void Array::swap( Array& other) noexcept
{
  values.swap(other.values);
}


Array* Array::clone() const
{
  return new Array(*this);
}


const std::string& Array::type_name() const
{
  return type_names::array_type_name;
}


void Array::apply_visitor(Value_type_visitor& v) const
{
  v.visit_array(*this);
}


void Array::clear()
{
  util::delete_ptrs( values.begin(), values.end() );
  values.clear();
  values.shrink_to_fit();
}


// This member placed here because of mutual dependence of
// value_type.h and value.h
void Array::push_back( Value_ptr v )
{
  values.push_back(v.release());
}


// This member placed here because of mutual dependence of
// value_type.h and value.h
void Array::push_back( const Value& v )
{
  values.push_back(new Value(v));
}

void Array::push_back( Value&& v )
{
  values.push_back(new Value(std::move(v)));
}


// --------------------------------------------------------------------------
Struct::Struct( const Struct& other ):
  Value_type(ValueTypeTag::Struct),
  values()
{
  for (const auto& vp : other.values) {
    values.emplace(vp.first, std::make_unique<Value>(*vp.second));
  }
}


Struct& Struct::operator =( const Struct& other )
{
  if( this == &other )
    return *this;

  Struct tmp(other);
  tmp.swap(*this);
  return *this;
}


Struct::~Struct()
{
  // unique_ptr elements are automatically destroyed by map destructor
}


void Struct::swap( Struct& other ) noexcept
{
  values.swap(other.values);
}


Struct* Struct::clone() const
{
  return new Struct(*this);
}


const std::string& Struct::type_name() const
{
  return type_names::struct_type_name;
}


void Struct::apply_visitor(Value_type_visitor& v) const
{
  v.visit_struct(*this);
}


size_t Struct::size() const
{
  return values.size();
}


bool Struct::has_field( const std::string& f ) const
{
  return values.find(f) != values.end();
}


const Value& Struct::operator []( const std::string& f ) const
{
  const_iterator i = values.find(f);

  if( i == values.end() )
    throw No_field( f );

  return (*i->second);
}


Value& Struct::operator []( const std::string& f )
{
  const_iterator i = values.find(f);

  if( i == values.end() )
    throw No_field( f );

  return (*i->second);
}


void Struct::clear()
{
  // unique_ptr handles cleanup automatically
  values.clear();
}

void Struct::insert( const std::string& f, Value_ptr val )
{
  values[f] = std::move(val);
}


void Struct::insert( const std::string& f, const Value& val )
{
  values[f] = std::make_unique<Value>(val);
}

void Struct::insert( const std::string& f, Value&& val )
{
  values[f] = std::make_unique<Value>(std::move(val));
}


void Struct::erase( const std::string& key )
{
  // unique_ptr handles cleanup automatically when erased from map
  values.erase(key);
}


// ----------------------------------------------------------------------------
const char Binary_data::base64_alpha[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' };

// Decode lookup table: -1 = invalid, -2 = padding ('='), -3 = whitespace
// Values 0-63 are valid base64 character indices
const signed char Binary_data::base64_decode[256] = {
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-3,-3,-1,-1,-3,-1,-1,  // 0-15 (9,10,13 = whitespace)
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 16-31
  -3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  // 32-47 (32=space, 43='+', 47='/')
  52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,  // 48-63 ('0'-'9', 61='=')
  -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  // 64-79 ('A'-'O')
  15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  // 80-95 ('P'-'Z')
  -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  // 96-111 ('a'-'o')
  41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  // 112-127 ('p'-'z')
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 128-143
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 144-159
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 160-175
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 176-191
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 192-207
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 208-223
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 224-239
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   // 240-255
};


Binary_data* Binary_data::from_base64( const std::string& s )
{
  return new Binary_data( s, false );
}


Binary_data* Binary_data::from_data( const std::string& s )
{
  return new Binary_data( s, true );
}


Binary_data* Binary_data::from_data( const char* s, size_t size )
{
  return new Binary_data( std::string(s, size), true );
}


Binary_data::Binary_data( const std::string& s, bool raw ):
  Value_type(ValueTypeTag::Binary),
  data(),
  base64()
{
  if( raw )
    data = s;
  else
  {
    base64 = s;
    decode();
  }
}


const std::string& Binary_data::get_data() const
{
  return data;
}


const std::string& Binary_data::get_base64() const
{
  if( base64.empty() && !data.empty() )
    encode();

  return base64;
}


inline void Binary_data::add_base64_char( int idx ) const
{
  base64 += base64_alpha[idx];
}


void Binary_data::encode() const
{
  const char* d = data.data();
  size_t dsz = data.length();

  // Pre-allocate: base64 is 4/3 of original size, plus padding
  // Use safe arithmetic to prevent integer overflow
  if (!safe_math::would_overflow_mul(dsz, size_t(4))) {
    size_t encoded_size = (dsz * 4) / 3;
    if (!safe_math::would_overflow_add(encoded_size, size_t(4))) {
      base64.reserve(encoded_size + 4);
    }
  }

  for( size_t i = 0; i < dsz; i += 3 )
  {
    // Cast to unsigned char to avoid undefined behavior when char is signed
    // Left-shifting negative values (chars >= 128 on signed char platforms) is UB
    unsigned c = static_cast<unsigned>(static_cast<unsigned char>(d[i])) << 16;
    add_base64_char( (c >> 18) & 0x3f );

    if( i+1 < dsz )
    {
      c |= static_cast<unsigned>(static_cast<unsigned char>(d[i+1])) << 8;
      add_base64_char( (c >> 12) & 0x3f );
    }
    else
    {
      add_base64_char( (c >> 12) & 0x3f );
      base64 += "==";
      return;
    }

    if( i+2 < dsz )
    {
      c |= static_cast<unsigned>(static_cast<unsigned char>(d[i+2]));
      add_base64_char( (c >> 6) & 0x3f );
      add_base64_char( c & 0x3f );
    }
    else
    {
      add_base64_char( (c >> 6) & 0x3f );
      base64 += "=";
      return;
    }
  }
}


// Optimized decode using lookup table - no exceptions, direct buffer write
void Binary_data::decode()
{
  auto src = reinterpret_cast<const unsigned char*>(base64.data());
  const size_t src_len = base64.length();

  // Reserve space (decoded is at most 3/4 of base64 size)
  // Use safe arithmetic to prevent integer overflow
  if (!safe_math::would_overflow_mul(src_len, size_t(3))) {
    size_t decoded_size = (src_len * 3) / 4;
    if (!safe_math::would_overflow_add(decoded_size, size_t(1))) {
      data.reserve(decoded_size + 1);
    }
  }

  // Collect 4 valid base64 characters at a time
  unsigned char vals[4] = {0, 0, 0, 0};  // Zero-init for defensive coding
  size_t val_idx = 0;
  bool done = false;

  for (size_t i = 0; i < src_len && !done; ++i) {
    const signed char v = base64_decode[src[i]];

    if (v >= 0) {
      // Valid base64 character (0-63)
      vals[val_idx++] = static_cast<unsigned char>(v);

      if (val_idx == 4) {
        // Decode 4 chars -> 3 bytes
        data.push_back(static_cast<char>((vals[0] << 2) | (vals[1] >> 4)));
        data.push_back(static_cast<char>((vals[1] << 4) | (vals[2] >> 2)));
        data.push_back(static_cast<char>((vals[2] << 6) | vals[3]));
        val_idx = 0;
      }
    } else if (v == -3) {
      // Whitespace - skip
      continue;
    } else if (v == -2) {
      // Padding '=' - handle end of data
      if (val_idx == 2) {
        data.push_back(static_cast<char>((vals[0] << 2) | (vals[1] >> 4)));
      } else if (val_idx == 3) {
        data.push_back(static_cast<char>((vals[0] << 2) | (vals[1] >> 4)));
        data.push_back(static_cast<char>((vals[1] << 4) | (vals[2] >> 2)));
      } else {
        throw Malformed_base64();
      }
      done = true;
      val_idx = 0;
    } else {
      // Invalid character
      throw Malformed_base64();
    }
  }

  // Incomplete group without padding is malformed (strict mode)
  if (val_idx != 0) {
    throw Malformed_base64();
  }
}


Value_type* Binary_data::clone() const
{
  return new Binary_data(*this);
}


const std::string& Binary_data::type_name() const
{
  return type_names::base64_type_name;
}


void Binary_data::apply_visitor(Value_type_visitor& v) const
{
  v.visit_base64(*this);
}


// ----------------------------------------------------------------------------
Date_time::Date_time( const struct tm* t ):
  Value_type(ValueTypeTag::DateTime),
  tm_(*t),
  cache()
{
}


Date_time::Date_time( bool use_lt ):
  Value_type(ValueTypeTag::DateTime),
  tm_(),
  cache()
{
  auto now = std::chrono::system_clock::now();
  std::time_t time = std::chrono::system_clock::to_time_t(now);
#ifdef _WIN32
  if (use_lt) {
    localtime_s(&tm_, &time);
  } else {
    gmtime_s(&tm_, &time);
  }
#else
  if (use_lt) {
    localtime_r(&time, &tm_);
  } else {
    gmtime_r(&time, &tm_);
  }
#endif
}


Date_time::Date_time( const std::string& s ):
  Value_type(ValueTypeTag::DateTime),
  tm_(),
  cache()
{
  if( s.length() != 17 || s[8] != 'T' )
    throw Malformed_iso8601();

  // Validate format: YYYYMMDDThh:mm:ss
  // Position 8 is 'T' (already checked above), positions 11 and 14 are ':'
  const char* p = s.c_str();
  for (size_t i = 0; i < 17; ++i) {
    if (i == 8) {
      continue;  // Already validated T above
    } else if (i == 11 || i == 14) {
      if (p[i] != ':') throw Malformed_iso8601();
    } else {
      if (p[i] < '0' || p[i] > '9') throw Malformed_iso8601();
    }
  }

  // Parse fields directly using std::from_chars (no allocations)
  auto parse_field = [&p](int start, int len) -> int {
    int val = 0;
    auto [ptr, ec] = std::from_chars(p + start, p + start + len, val);
    if (ec != std::errc{} || ptr != p + start + len) {
      throw Malformed_iso8601();
    }
    return val;
  };

  tm_.tm_year = parse_field(0, 4) - 1900;   // YYYY
  tm_.tm_mon  = parse_field(4, 2) - 1;      // MM
  tm_.tm_mday = parse_field(6, 2);          // DD
  tm_.tm_hour = parse_field(9, 2);          // hh
  tm_.tm_min  = parse_field(12, 2);         // mm
  tm_.tm_sec  = parse_field(15, 2);         // ss

  if( (tm_.tm_year < 0) || (tm_.tm_mon < 0 || tm_.tm_mon > 11) ||
      (tm_.tm_mday < 1 || tm_.tm_mday > 31) ||
      (tm_.tm_hour < 0 || tm_.tm_hour > 23) ||
      (tm_.tm_min < 0 || tm_.tm_min > 59) ||
      (tm_.tm_sec < 0 || tm_.tm_sec > 61)
    )
    throw Malformed_iso8601();
}


Value_type* Date_time::clone() const
{
  return new Date_time( *this );
}


const std::string& Date_time::type_name() const
{
  return type_names::date_type_name;
}


void Date_time::apply_visitor(Value_type_visitor& v) const
{
  v.visit_datetime(*this);
}


const std::string& Date_time::to_string() const
{
  if( cache.empty() )
  {
    char s[18];
    strftime( s, 18, "%Y%m%dT%H:%M:%S", &tm_ );
    cache = std::string( s, 17 );
  }

  return cache;
}

} // namespace iqxmlrpc

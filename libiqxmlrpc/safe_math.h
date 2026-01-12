//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _libiqxmlrpc_safe_math_h_
#define _libiqxmlrpc_safe_math_h_

#include "api_export.h"

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace iqxmlrpc {

//! Exception thrown when an integer overflow is detected.
class LIBIQXMLRPC_API Integer_overflow: public std::overflow_error {
public:
  Integer_overflow():
    std::overflow_error("Integer overflow detected") {}

  explicit Integer_overflow(const char* msg):
    std::overflow_error(msg) {}
};

namespace safe_math {

//! Safely add two size_t values, throwing Integer_overflow on overflow.
inline size_t add(size_t a, size_t b)
{
  if (b > std::numeric_limits<size_t>::max() - a) {
    throw Integer_overflow("Addition overflow");
  }
  return a + b;
}

//! Safely multiply two size_t values, throwing Integer_overflow on overflow.
inline size_t mul(size_t a, size_t b)
{
  if (a != 0 && b > std::numeric_limits<size_t>::max() / a) {
    throw Integer_overflow("Multiplication overflow");
  }
  return a * b;
}

//! Safely add and assign, throwing Integer_overflow on overflow.
//! Returns the new value.
inline size_t add_assign(size_t& target, size_t value)
{
  target = add(target, value);
  return target;
}

//! Check if multiplication would overflow without throwing.
inline bool would_overflow_mul(size_t a, size_t b)
{
  return a != 0 && b > std::numeric_limits<size_t>::max() / a;
}

//! Check if addition would overflow without throwing.
inline bool would_overflow_add(size_t a, size_t b)
{
  return b > std::numeric_limits<size_t>::max() - a;
}

} // namespace safe_math
} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

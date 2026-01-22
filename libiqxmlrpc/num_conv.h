//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#ifndef LIBIQXMLRPC_NUM_CONV_H
#define LIBIQXMLRPC_NUM_CONV_H

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>

namespace iqxmlrpc {
namespace num_conv {

// Exception for conversion errors
class conversion_error : public std::runtime_error {
public:
  explicit conversion_error(const char* msg) : std::runtime_error(msg) {}
};

// Integer to string conversion
// Use std::to_string which is well-optimized in modern standard libraries
template<typename T>
inline std::string to_string(T value) {
  static_assert(std::is_integral_v<T>, "to_string requires integral type");
  return std::to_string(value);
}

// Double to string - use snprintf for full precision (17 digits for IEEE 754)
// std::to_string uses fixed 6-digit precision which loses information
inline std::string double_to_string(double value) {
  char buf[32];
  int len = std::snprintf(buf, sizeof(buf), "%.17g", value);
  if (len < 0 || len >= static_cast<int>(sizeof(buf))) {
    throw conversion_error("snprintf failed for double");
  }
  return std::string(buf, static_cast<size_t>(len));
}

// String to integer using std::from_chars
template<typename T>
inline T from_string(const std::string& str) {
  static_assert(std::is_integral_v<T>, "from_string requires integral type");
  T value{};
  const char* first = str.data();
  const char* last = first + str.size();
  auto [ptr, ec] = std::from_chars(first, last, value);
  if (ec != std::errc() || ptr != last) {
    throw conversion_error("from_chars failed");
  }
  return value;
}

// String to double
inline double string_to_double(const std::string& str) {
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
  double value{};
  const char* first = str.data();
  const char* last = first + str.size();
  auto [ptr, ec] = std::from_chars(first, last, value);
  if (ec != std::errc() || ptr != last) {
    throw conversion_error("from_chars failed for double");
  }
  return value;
#else
  // Fallback: strtod is still faster than lexical_cast
  char* end = nullptr;
  errno = 0;
  double value = std::strtod(str.c_str(), &end);
  if (end != str.c_str() + str.size() || errno == ERANGE) {
    throw conversion_error("strtod failed");
  }
  return value;
#endif
}

} // namespace num_conv
} // namespace iqxmlrpc

#endif // LIBIQXMLRPC_NUM_CONV_H

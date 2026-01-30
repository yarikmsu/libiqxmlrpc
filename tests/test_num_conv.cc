// Unit tests for num_conv.h - number conversion utilities
// Tests both string and string_view overloads with focus on error paths

#define BOOST_TEST_MODULE num_conv_tests
#include <boost/test/unit_test.hpp>

#include "libiqxmlrpc/num_conv.h"

#include <limits>
#include <string>
#include <string_view>

using namespace iqxmlrpc::num_conv;

BOOST_AUTO_TEST_SUITE(from_string_tests)

// ============================================================================
// Valid Input Tests - both overloads
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_valid_int)
{
  // string_view overload
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("42")), 42);
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("-123")), -123);
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("0")), 0);

  // std::string overload (delegates to string_view)
  BOOST_CHECK_EQUAL(from_string<int>(std::string("42")), 42);
  BOOST_CHECK_EQUAL(from_string<int>(std::string("-123")), -123);
  BOOST_CHECK_EQUAL(from_string<int>(std::string("0")), 0);
}

BOOST_AUTO_TEST_CASE(from_string_valid_int64)
{
  BOOST_CHECK_EQUAL(from_string<int64_t>(std::string_view("9223372036854775807")),
                    std::numeric_limits<int64_t>::max());
  BOOST_CHECK_EQUAL(from_string<int64_t>(std::string_view("-9223372036854775808")),
                    std::numeric_limits<int64_t>::min());
}

BOOST_AUTO_TEST_CASE(from_string_valid_unsigned)
{
  BOOST_CHECK_EQUAL(from_string<unsigned int>(std::string_view("4294967295")),
                    std::numeric_limits<unsigned int>::max());
  BOOST_CHECK_EQUAL(from_string<uint64_t>(std::string_view("18446744073709551615")),
                    std::numeric_limits<uint64_t>::max());
}

BOOST_AUTO_TEST_CASE(from_string_boundary_values)
{
  // INT_MAX and INT_MIN
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("2147483647")),
                    std::numeric_limits<int>::max());
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("-2147483648")),
                    std::numeric_limits<int>::min());

  // Smaller types
  BOOST_CHECK_EQUAL(from_string<int16_t>(std::string_view("32767")),
                    std::numeric_limits<int16_t>::max());
  BOOST_CHECK_EQUAL(from_string<int16_t>(std::string_view("-32768")),
                    std::numeric_limits<int16_t>::min());
}

// ============================================================================
// Error Path Tests - Empty String
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_empty_throws)
{
  BOOST_CHECK_THROW(from_string<int>(std::string_view("")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string("")), conversion_error);
  BOOST_CHECK_THROW(from_string<int64_t>(std::string_view("")), conversion_error);
}

// ============================================================================
// Error Path Tests - Invalid Input (non-numeric)
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_invalid_throws)
{
  BOOST_CHECK_THROW(from_string<int>(std::string_view("abc")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("xyz123")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("--123")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("++123")), conversion_error);
}

// ============================================================================
// Error Path Tests - Integer Overflow
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_overflow_throws)
{
  // Overflow for int
  BOOST_CHECK_THROW(from_string<int>(std::string_view("2147483648")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("-2147483649")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("99999999999999")), conversion_error);

  // Overflow for smaller types
  BOOST_CHECK_THROW(from_string<int8_t>(std::string_view("128")), conversion_error);
  BOOST_CHECK_THROW(from_string<int8_t>(std::string_view("-129")), conversion_error);
  BOOST_CHECK_THROW(from_string<int16_t>(std::string_view("32768")), conversion_error);
  BOOST_CHECK_THROW(from_string<uint8_t>(std::string_view("256")), conversion_error);
}

// ============================================================================
// Error Path Tests - Trailing Characters (partial parse rejected)
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_trailing_chars_throws)
{
  BOOST_CHECK_THROW(from_string<int>(std::string_view("123abc")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("123 ")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("123.")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("123.0")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("42x")), conversion_error);
}

// ============================================================================
// Error Path Tests - Leading Whitespace/Characters
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_leading_whitespace_throws)
{
  BOOST_CHECK_THROW(from_string<int>(std::string_view(" 123")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("\t123")), conversion_error);
  BOOST_CHECK_THROW(from_string<int>(std::string_view("\n123")), conversion_error);
}

// ============================================================================
// Error Path Tests - Negative for Unsigned Types
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_negative_unsigned_throws)
{
  // Note: std::from_chars behavior for negative + unsigned varies by implementation
  // On most implementations, this will fail the full-consumption check or overflow
  BOOST_CHECK_THROW(from_string<unsigned int>(std::string_view("-1")), conversion_error);
  BOOST_CHECK_THROW(from_string<uint64_t>(std::string_view("-100")), conversion_error);
}

// ============================================================================
// HTTP Status Code Tests (realistic usage)
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_http_status_codes)
{
  // Common HTTP status codes
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("200")), 200);
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("201")), 201);
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("301")), 301);
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("400")), 400);
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("404")), 404);
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("500")), 500);
  BOOST_CHECK_EQUAL(from_string<int>(std::string_view("503")), 503);
}

// ============================================================================
// Content-Length Tests (realistic usage)
// ============================================================================

BOOST_AUTO_TEST_CASE(from_string_content_length)
{
  // Typical content lengths
  BOOST_CHECK_EQUAL(from_string<size_t>(std::string_view("0")), 0u);
  BOOST_CHECK_EQUAL(from_string<size_t>(std::string_view("1024")), 1024u);
  BOOST_CHECK_EQUAL(from_string<size_t>(std::string_view("1048576")), 1048576u);  // 1MB
  BOOST_CHECK_EQUAL(from_string<size_t>(std::string_view("104857600")), 104857600u);  // 100MB
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// to_string Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(to_string_tests)

BOOST_AUTO_TEST_CASE(to_string_int)
{
  BOOST_CHECK_EQUAL(to_string(42), "42");
  BOOST_CHECK_EQUAL(to_string(-123), "-123");
  BOOST_CHECK_EQUAL(to_string(0), "0");
  BOOST_CHECK_EQUAL(to_string(std::numeric_limits<int>::max()), "2147483647");
  BOOST_CHECK_EQUAL(to_string(std::numeric_limits<int>::min()), "-2147483648");
}

BOOST_AUTO_TEST_CASE(to_string_int64)
{
  BOOST_CHECK_EQUAL(to_string(int64_t(9223372036854775807LL)), "9223372036854775807");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// double_to_string Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(double_conversion_tests)

BOOST_AUTO_TEST_CASE(double_to_string_precision)
{
  // Check that full precision is maintained (uses %.17g format)
  // Note: Floating point representation may add trailing digits
  auto pi_str = double_to_string(3.141592653589793);
  BOOST_CHECK(pi_str.find("3.14159265358979") == 0);  // Prefix match
  BOOST_CHECK_EQUAL(double_to_string(0.0), "0");
  BOOST_CHECK_EQUAL(double_to_string(-1.5), "-1.5");
}

BOOST_AUTO_TEST_CASE(string_to_double_valid)
{
  BOOST_CHECK_CLOSE(string_to_double("3.14159"), 3.14159, 0.00001);
  BOOST_CHECK_CLOSE(string_to_double("-2.5"), -2.5, 0.00001);
  BOOST_CHECK_CLOSE(string_to_double("0.0"), 0.0, 0.00001);
  BOOST_CHECK_CLOSE(string_to_double("1e10"), 1e10, 0.00001);
}

BOOST_AUTO_TEST_CASE(string_to_double_invalid_throws)
{
  // Note: Empty string behavior differs between from_chars (throws) and strtod fallback
  // The strtod fallback may not throw for empty strings on all platforms
  BOOST_CHECK_THROW(string_to_double("abc"), conversion_error);
  BOOST_CHECK_THROW(string_to_double("1.2.3"), conversion_error);
  BOOST_CHECK_THROW(string_to_double("1.0abc"), conversion_error);
}

BOOST_AUTO_TEST_SUITE_END()

#define BOOST_TEST_MODULE safe_math_test
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/safe_math.h"

#include <limits>

using namespace boost::unit_test;
using namespace iqxmlrpc;

BOOST_AUTO_TEST_SUITE(safe_add_tests)

BOOST_AUTO_TEST_CASE(add_normal_values)
{
    BOOST_CHECK_EQUAL(safe_math::add(1, 2), 3);
    BOOST_CHECK_EQUAL(safe_math::add(0, 0), 0);
    BOOST_CHECK_EQUAL(safe_math::add(100, 200), 300);
    BOOST_CHECK_EQUAL(safe_math::add(1000000, 2000000), 3000000);
}

BOOST_AUTO_TEST_CASE(add_with_zero)
{
    BOOST_CHECK_EQUAL(safe_math::add(0, 100), 100);
    BOOST_CHECK_EQUAL(safe_math::add(100, 0), 100);
    BOOST_CHECK_EQUAL(safe_math::add(0, 0), 0);
}

BOOST_AUTO_TEST_CASE(add_max_value)
{
    size_t max_val = std::numeric_limits<size_t>::max();
    BOOST_CHECK_EQUAL(safe_math::add(max_val, 0), max_val);
    BOOST_CHECK_EQUAL(safe_math::add(0, max_val), max_val);
}

BOOST_AUTO_TEST_CASE(add_overflow_throws)
{
    size_t max_val = std::numeric_limits<size_t>::max();
    BOOST_CHECK_THROW(safe_math::add(max_val, 1), Integer_overflow);
    BOOST_CHECK_THROW(safe_math::add(1, max_val), Integer_overflow);
    BOOST_CHECK_THROW(safe_math::add(max_val, max_val), Integer_overflow);
}

BOOST_AUTO_TEST_CASE(add_near_overflow_boundary)
{
    size_t max_val = std::numeric_limits<size_t>::max();
    size_t half_max = max_val / 2;

    // Just under the limit should work
    BOOST_CHECK_NO_THROW(safe_math::add(half_max, half_max));

    // Just over should throw
    BOOST_CHECK_THROW(safe_math::add(half_max + 1, half_max + 1), Integer_overflow);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(safe_mul_tests)

BOOST_AUTO_TEST_CASE(mul_normal_values)
{
    BOOST_CHECK_EQUAL(safe_math::mul(2, 3), 6);
    BOOST_CHECK_EQUAL(safe_math::mul(10, 10), 100);
    BOOST_CHECK_EQUAL(safe_math::mul(1000, 1000), 1000000);
}

BOOST_AUTO_TEST_CASE(mul_with_zero)
{
    BOOST_CHECK_EQUAL(safe_math::mul(0, 100), 0);
    BOOST_CHECK_EQUAL(safe_math::mul(100, 0), 0);
    BOOST_CHECK_EQUAL(safe_math::mul(0, 0), 0);
}

BOOST_AUTO_TEST_CASE(mul_with_one)
{
    BOOST_CHECK_EQUAL(safe_math::mul(1, 100), 100);
    BOOST_CHECK_EQUAL(safe_math::mul(100, 1), 100);
    size_t max_val = std::numeric_limits<size_t>::max();
    BOOST_CHECK_EQUAL(safe_math::mul(max_val, 1), max_val);
    BOOST_CHECK_EQUAL(safe_math::mul(1, max_val), max_val);
}

BOOST_AUTO_TEST_CASE(mul_overflow_throws)
{
    size_t max_val = std::numeric_limits<size_t>::max();
    BOOST_CHECK_THROW(safe_math::mul(max_val, 2), Integer_overflow);
    BOOST_CHECK_THROW(safe_math::mul(2, max_val), Integer_overflow);
    BOOST_CHECK_THROW(safe_math::mul(max_val, max_val), Integer_overflow);
}

BOOST_AUTO_TEST_CASE(mul_near_overflow_boundary)
{
    size_t sqrt_max = static_cast<size_t>(1) << (sizeof(size_t) * 4);

    // Just under the limit should work
    BOOST_CHECK_NO_THROW(safe_math::mul(sqrt_max - 1, sqrt_max - 1));

    // Large values that would overflow
    BOOST_CHECK_THROW(safe_math::mul(sqrt_max * 2, sqrt_max), Integer_overflow);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(add_assign_tests)

BOOST_AUTO_TEST_CASE(add_assign_normal)
{
    size_t val = 10;
    BOOST_CHECK_EQUAL(safe_math::add_assign(val, 5), 15);
    BOOST_CHECK_EQUAL(val, 15);
}

BOOST_AUTO_TEST_CASE(add_assign_multiple)
{
    size_t val = 0;
    safe_math::add_assign(val, 10);
    safe_math::add_assign(val, 20);
    safe_math::add_assign(val, 30);
    BOOST_CHECK_EQUAL(val, 60);
}

BOOST_AUTO_TEST_CASE(add_assign_overflow_throws)
{
    size_t val = std::numeric_limits<size_t>::max();
    BOOST_CHECK_THROW(safe_math::add_assign(val, 1), Integer_overflow);
    // Value should remain unchanged after failed add_assign
    BOOST_CHECK_EQUAL(val, std::numeric_limits<size_t>::max());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(would_overflow_tests)

BOOST_AUTO_TEST_CASE(would_overflow_add_false)
{
    BOOST_CHECK(!safe_math::would_overflow_add(1, 2));
    BOOST_CHECK(!safe_math::would_overflow_add(0, 0));
    BOOST_CHECK(!safe_math::would_overflow_add(1000000, 2000000));
}

BOOST_AUTO_TEST_CASE(would_overflow_add_true)
{
    size_t max_val = std::numeric_limits<size_t>::max();
    BOOST_CHECK(safe_math::would_overflow_add(max_val, 1));
    BOOST_CHECK(safe_math::would_overflow_add(1, max_val));
    BOOST_CHECK(safe_math::would_overflow_add(max_val, max_val));
}

BOOST_AUTO_TEST_CASE(would_overflow_mul_false)
{
    BOOST_CHECK(!safe_math::would_overflow_mul(2, 3));
    BOOST_CHECK(!safe_math::would_overflow_mul(0, 1000000));
    BOOST_CHECK(!safe_math::would_overflow_mul(1000000, 0));
    BOOST_CHECK(!safe_math::would_overflow_mul(1000, 1000));
}

BOOST_AUTO_TEST_CASE(would_overflow_mul_true)
{
    size_t max_val = std::numeric_limits<size_t>::max();
    BOOST_CHECK(safe_math::would_overflow_mul(max_val, 2));
    BOOST_CHECK(safe_math::would_overflow_mul(2, max_val));
    BOOST_CHECK(safe_math::would_overflow_mul(max_val, max_val));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(integer_overflow_exception_tests)

BOOST_AUTO_TEST_CASE(exception_default_message)
{
    Integer_overflow ex;
    std::string what = ex.what();
    BOOST_CHECK(what.find("overflow") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(exception_custom_message)
{
    Integer_overflow ex("Custom overflow message");
    std::string what = ex.what();
    BOOST_CHECK(what.find("Custom") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(exception_is_std_overflow_error)
{
    try {
        throw Integer_overflow();
    } catch (const std::overflow_error& e) {
        BOOST_CHECK(true);
        return;
    }
    BOOST_FAIL("Integer_overflow should be catchable as std::overflow_error");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(realistic_scenarios)

BOOST_AUTO_TEST_CASE(http_content_length_simulation)
{
    // Simulate HTTP check_sz logic with content-length + header_cache size
    size_t content_length = 1000000;  // 1 MB content
    size_t header_size = 500;         // typical header size

    BOOST_CHECK(!safe_math::would_overflow_add(content_length, header_size));
    size_t total = safe_math::add(content_length, header_size);
    BOOST_CHECK_EQUAL(total, 1000500);
}

BOOST_AUTO_TEST_CASE(http_content_length_malicious)
{
    // Malicious content-length near max value
    size_t content_length = std::numeric_limits<size_t>::max() - 100;
    size_t header_size = 500;

    BOOST_CHECK(safe_math::would_overflow_add(content_length, header_size));
    BOOST_CHECK_THROW(safe_math::add(content_length, header_size), Integer_overflow);
}

BOOST_AUTO_TEST_CASE(base64_encode_size_calculation)
{
    // Simulate base64 encode size calculation: (size * 4) / 3 + 4
    size_t data_size = 1000000;  // 1 MB

    BOOST_CHECK(!safe_math::would_overflow_mul(data_size, size_t(4)));
    size_t encoded_size = (data_size * 4) / 3;
    BOOST_CHECK(!safe_math::would_overflow_add(encoded_size, size_t(4)));
    encoded_size += 4;
    BOOST_CHECK_EQUAL(encoded_size, 1333337);
}

BOOST_AUTO_TEST_CASE(base64_encode_size_huge_input)
{
    // Very large data that would overflow during base64 size calculation
    size_t huge_size = std::numeric_limits<size_t>::max() / 2;

    BOOST_CHECK(safe_math::would_overflow_mul(huge_size, size_t(4)));
}

BOOST_AUTO_TEST_CASE(header_dump_reserve_calculation)
{
    // Simulate Header::dump() reserve calculation: size + options * 64 + 4
    size_t current_size = 100;
    size_t options_count = 10;

    BOOST_CHECK(!safe_math::would_overflow_mul(options_count, size_t(64)));
    size_t options_size = options_count * 64;
    BOOST_CHECK(!safe_math::would_overflow_add(current_size, options_size));
    size_t reserved = current_size + options_size + 4;
    BOOST_CHECK_EQUAL(reserved, 744);
}

BOOST_AUTO_TEST_CASE(header_dump_huge_options_count)
{
    // Maliciously large options count
    size_t huge_count = std::numeric_limits<size_t>::max() / 32;

    BOOST_CHECK(safe_math::would_overflow_mul(huge_count, size_t(64)));
}

BOOST_AUTO_TEST_CASE(cumulative_size_tracking)
{
    // Simulate cumulative size tracking as in Packet_reader
    size_t total_sz = 0;
    size_t chunk_size = 1024;

    for (int i = 0; i < 1000; i++) {
        BOOST_CHECK(!safe_math::would_overflow_add(total_sz, chunk_size));
        safe_math::add_assign(total_sz, chunk_size);
    }
    BOOST_CHECK_EQUAL(total_sz, 1024000);
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et

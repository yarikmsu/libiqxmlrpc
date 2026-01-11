/**
 * Wire Protocol Compatibility Tests for libiqxmlrpc
 *
 * These tests verify that libiqxmlrpc can communicate correctly with
 * third-party XML-RPC implementations (Python, Java, etc.).
 *
 * The tests connect to an external XML-RPC server and verify:
 * - All value types serialize/deserialize correctly
 * - Edge cases are handled properly
 * - Error/fault responses are parsed correctly
 *
 * Usage:
 *   ./test_wire_compatibility --server HOST --port PORT
 *
 * Default: localhost:8000 (Python test server)
 */

#define BOOST_TEST_MODULE wire_compatibility_test

#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_client.h"
#include "libiqxmlrpc/value.h"
#include "libiqxmlrpc/except.h"

#include <cmath>
#include <limits>
#include <memory>
#include <string>

using namespace iqxmlrpc;

namespace {

// Get server host from environment or use default
std::string get_server_host() {
    const char* env = std::getenv("XMLRPC_TEST_HOST");
    return env ? env : "localhost";
}

// Get server port from environment or use default
int get_server_port() {
    const char* env = std::getenv("XMLRPC_TEST_PORT");
    return env ? std::atoi(env) : 8000;
}

// Get URI from environment or use default (Python uses "/")
std::string get_server_uri() {
    const char* env = std::getenv("XMLRPC_TEST_URI");
    return env ? env : "/";
}

// Helper to create a client
std::unique_ptr<Client<Http_client_connection>> make_client() {
    iqnet::Inet_addr addr(get_server_host(), get_server_port());
    return std::make_unique<Client<Http_client_connection>>(addr, get_server_uri());
}

} // namespace

// =============================================================================
// Test Fixture
// =============================================================================

struct WireCompatibilityFixture {
    std::unique_ptr<Client<Http_client_connection>> client;

    WireCompatibilityFixture()
        : client(make_client())
    {
        // Parse command line for server/port if provided
        // For now, use defaults - can be extended with global fixture
    }

    ~WireCompatibilityFixture() = default;

    Response call(const std::string& method) {
        return client->execute(method, Param_list());
    }

    Response call(const std::string& method, const Value& v1) {
        Param_list params;
        params.push_back(v1);
        return client->execute(method, params);
    }

    Response call(const std::string& method, const Value& v1, const Value& v2) {
        Param_list params;
        params.push_back(v1);
        params.push_back(v2);
        return client->execute(method, params);
    }
};

// =============================================================================
// P0: Basic Connectivity
// =============================================================================

BOOST_AUTO_TEST_SUITE(p0_connectivity)

BOOST_FIXTURE_TEST_CASE(server_responds, WireCompatibilityFixture)
{
    // Basic connectivity test - call system.listMethods
    Response r = call("system.listMethods");
    BOOST_CHECK(!r.is_fault());

    // Should return an array of method names
    const Array& methods = r.value().the_array();
    BOOST_CHECK(methods.size() > 0);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// P1: Value Type Round-Trip Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(p1_value_types)

BOOST_FIXTURE_TEST_CASE(echo_integer, WireCompatibilityFixture)
{
    Response r = call("echo_int", Value(42));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), 42);
}

BOOST_FIXTURE_TEST_CASE(echo_negative_integer, WireCompatibilityFixture)
{
    Response r = call("echo_int", Value(-12345));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), -12345);
}

BOOST_FIXTURE_TEST_CASE(echo_zero, WireCompatibilityFixture)
{
    Response r = call("echo_int", Value(0));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), 0);
}

BOOST_FIXTURE_TEST_CASE(echo_int_max, WireCompatibilityFixture)
{
    int max_val = std::numeric_limits<int>::max();
    Response r = call("echo_int", Value(max_val));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), max_val);
}

BOOST_FIXTURE_TEST_CASE(echo_int_min, WireCompatibilityFixture)
{
    int min_val = std::numeric_limits<int>::min();
    Response r = call("echo_int", Value(min_val));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), min_val);
}

BOOST_FIXTURE_TEST_CASE(echo_double, WireCompatibilityFixture)
{
    Response r = call("echo_double", Value(3.14159265358979));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_CLOSE(r.value().get_double(), 3.14159265358979, 0.0001);
}

BOOST_FIXTURE_TEST_CASE(echo_negative_double, WireCompatibilityFixture)
{
    Response r = call("echo_double", Value(-2.71828));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_CLOSE(r.value().get_double(), -2.71828, 0.0001);
}

BOOST_FIXTURE_TEST_CASE(echo_double_zero, WireCompatibilityFixture)
{
    Response r = call("echo_double", Value(0.0));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_double(), 0.0);
}

BOOST_FIXTURE_TEST_CASE(echo_bool_true, WireCompatibilityFixture)
{
    Response r = call("echo_bool", Value(true));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_bool(), true);
}

BOOST_FIXTURE_TEST_CASE(echo_bool_false, WireCompatibilityFixture)
{
    Response r = call("echo_bool", Value(false));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_bool(), false);
}

BOOST_FIXTURE_TEST_CASE(echo_string, WireCompatibilityFixture)
{
    Response r = call("echo_string", Value("Hello, World!"));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_string(), "Hello, World!");
}

BOOST_FIXTURE_TEST_CASE(echo_empty_string, WireCompatibilityFixture)
{
    Response r = call("echo_string", Value(""));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK(r.value().get_string().empty());
}

BOOST_FIXTURE_TEST_CASE(echo_special_xml_chars, WireCompatibilityFixture)
{
    std::string special = "<test>&\"'</test>";
    Response r = call("echo_string", Value(special));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_string(), special);
}

BOOST_FIXTURE_TEST_CASE(echo_unicode_string, WireCompatibilityFixture)
{
    // UTF-8 encoded Japanese text
    std::string unicode = "日本語テスト";
    Response r = call("echo_string", Value(unicode));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_string(), unicode);
}

BOOST_FIXTURE_TEST_CASE(echo_struct, WireCompatibilityFixture)
{
    Struct s;
    s.insert("name", Value("test"));
    s.insert("count", Value(42));
    s.insert("enabled", Value(true));

    Response r = call("echo_struct", Value(s));
    BOOST_CHECK(!r.is_fault());

    const Struct& result = r.value().the_struct();
    BOOST_CHECK_EQUAL(result["name"].get_string(), "test");
    BOOST_CHECK_EQUAL(result["count"].get_int(), 42);
    BOOST_CHECK_EQUAL(result["enabled"].get_bool(), true);
}

BOOST_FIXTURE_TEST_CASE(echo_empty_struct, WireCompatibilityFixture)
{
    Struct s;
    Response r = call("echo_struct", Value(s));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().the_struct().size(), 0);
}

BOOST_FIXTURE_TEST_CASE(echo_array, WireCompatibilityFixture)
{
    Array arr;
    arr.push_back(Value(1));
    arr.push_back(Value(2));
    arr.push_back(Value(3));

    Response r = call("echo_array", Value(arr));
    BOOST_CHECK(!r.is_fault());

    const Array& result = r.value().the_array();
    BOOST_CHECK_EQUAL(result.size(), 3);
    BOOST_CHECK_EQUAL(result[0].get_int(), 1);
    BOOST_CHECK_EQUAL(result[1].get_int(), 2);
    BOOST_CHECK_EQUAL(result[2].get_int(), 3);
}

BOOST_FIXTURE_TEST_CASE(echo_empty_array, WireCompatibilityFixture)
{
    Array arr;
    Response r = call("echo_array", Value(arr));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().the_array().size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// P2: Arithmetic Operations (Wire Format Correctness)
// =============================================================================

BOOST_AUTO_TEST_SUITE(p2_operations)

BOOST_FIXTURE_TEST_CASE(add_integers, WireCompatibilityFixture)
{
    Response r = call("add", Value(2), Value(3));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), 5);
}

BOOST_FIXTURE_TEST_CASE(add_large_integers, WireCompatibilityFixture)
{
    Response r = call("add", Value(1000000), Value(2000000));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), 3000000);
}

BOOST_FIXTURE_TEST_CASE(subtract_integers, WireCompatibilityFixture)
{
    Response r = call("subtract", Value(10), Value(3));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), 7);
}

BOOST_FIXTURE_TEST_CASE(multiply_integers, WireCompatibilityFixture)
{
    Response r = call("multiply", Value(6), Value(7));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), 42);
}

BOOST_FIXTURE_TEST_CASE(divide_doubles, WireCompatibilityFixture)
{
    Response r = call("divide", Value(10.0), Value(4.0));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_CLOSE(r.value().get_double(), 2.5, 0.0001);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// P3: Complex Structures
// =============================================================================

BOOST_AUTO_TEST_SUITE(p3_complex)

BOOST_FIXTURE_TEST_CASE(get_all_types, WireCompatibilityFixture)
{
    Response r = call("get_all_types");
    BOOST_CHECK(!r.is_fault());

    const Struct& result = r.value().the_struct();

    // Verify all types are present and correct
    BOOST_CHECK_EQUAL(result["int"].get_int(), 42);
    BOOST_CHECK_EQUAL(result["negative_int"].get_int(), -123);
    BOOST_CHECK_EQUAL(result["zero"].get_int(), 0);
    BOOST_CHECK_EQUAL(result["bool_true"].get_bool(), true);
    BOOST_CHECK_EQUAL(result["bool_false"].get_bool(), false);
    BOOST_CHECK_CLOSE(result["double"].get_double(), 3.14159265358979, 0.0001);
    BOOST_CHECK_CLOSE(result["negative_double"].get_double(), -2.71828, 0.0001);
    BOOST_CHECK_EQUAL(result["string"].get_string(), "Hello, World!");
    BOOST_CHECK(result["empty_string"].get_string().empty());
    BOOST_CHECK_EQUAL(result["unicode_string"].get_string(), "日本語テスト");
    BOOST_CHECK_EQUAL(result["special_chars"].get_string(), "<test>&\"'</test>");

    // Check nested struct
    const Struct& nested = result["nested_struct"].the_struct();
    BOOST_CHECK(nested.has_field("level1"));
}

BOOST_FIXTURE_TEST_CASE(nested_struct, WireCompatibilityFixture)
{
    Response r = call("get_nested_struct");
    BOOST_CHECK(!r.is_fault());

    // Navigate through nested structure
    const Struct& level0 = r.value().the_struct();
    BOOST_CHECK(level0.has_field("level"));
    BOOST_CHECK(level0.has_field("child"));
}

BOOST_FIXTURE_TEST_CASE(large_response, WireCompatibilityFixture)
{
    Response r = call("get_large_response");
    BOOST_CHECK(!r.is_fault());

    const Array& arr = r.value().the_array();
    BOOST_CHECK_EQUAL(arr.size(), 1000);
    BOOST_CHECK_EQUAL(arr[0].get_int(), 0);
    BOOST_CHECK_EQUAL(arr[999].get_int(), 999);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// P4: Fault Handling
// =============================================================================

BOOST_AUTO_TEST_SUITE(p4_faults)

BOOST_FIXTURE_TEST_CASE(fault_response, WireCompatibilityFixture)
{
    Response r = call("raise_fault", Value(42), Value("Test error message"));
    BOOST_CHECK(r.is_fault());

    BOOST_CHECK_EQUAL(r.fault_code(), 42);
    BOOST_CHECK_EQUAL(r.fault_string(), "Test error message");
}

BOOST_FIXTURE_TEST_CASE(divide_by_zero_fault, WireCompatibilityFixture)
{
    Response r = call("divide", Value(10.0), Value(0.0));
    BOOST_CHECK(r.is_fault());

    BOOST_CHECK_EQUAL(r.fault_code(), 1);
    // Message should contain "zero"
    std::string msg = r.fault_string();
    BOOST_CHECK(msg.find("zero") != std::string::npos ||
                msg.find("Zero") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

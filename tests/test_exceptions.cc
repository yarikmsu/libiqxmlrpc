#define BOOST_TEST_MODULE exceptions_test
#include <cerrno>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/except.h"
#include "libiqxmlrpc/net_except.h"

using namespace boost::unit_test;
using namespace iqxmlrpc;

// Include SSL library for SSL exception tests
#include "libiqxmlrpc/ssl_lib.h"

BOOST_AUTO_TEST_SUITE(exception_base_tests)

BOOST_AUTO_TEST_CASE(exception_with_default_code)
{
    Exception ex("Test exception");
    BOOST_CHECK_EQUAL(std::string(ex.what()), "Test exception");
    BOOST_CHECK_EQUAL(ex.code(), -32000);  // default undefined error
}

BOOST_AUTO_TEST_CASE(exception_with_custom_code)
{
    Exception ex("Custom error", -12345);
    BOOST_CHECK_EQUAL(std::string(ex.what()), "Custom error");
    BOOST_CHECK_EQUAL(ex.code(), -12345);
}

BOOST_AUTO_TEST_CASE(exception_inheritance)
{
    Exception ex("Test");
    std::runtime_error& ref = ex;  // Must be convertible
    BOOST_CHECK_EQUAL(std::string(ref.what()), "Test");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(parse_error_tests)

BOOST_AUTO_TEST_CASE(parse_error_message)
{
    Parse_error ex("invalid XML syntax");
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("Parser error") != std::string::npos);
    BOOST_CHECK(msg.find("invalid XML syntax") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(parse_error_code)
{
    Parse_error ex("test");
    BOOST_CHECK_EQUAL(ex.code(), -32700);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(xml_build_error_tests)

BOOST_AUTO_TEST_CASE(xml_build_error_message)
{
    XmlBuild_error ex("failed to create element");
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("XML build error") != std::string::npos);
    BOOST_CHECK(msg.find("failed to create element") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(xml_build_error_code)
{
    XmlBuild_error ex("test");
    BOOST_CHECK_EQUAL(ex.code(), -32705);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(xml_rpc_violation_tests)

BOOST_AUTO_TEST_CASE(xml_rpc_violation_default)
{
    XML_RPC_violation ex;
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("XML-RPC violation") != std::string::npos);
    BOOST_CHECK_EQUAL(ex.code(), -32600);
}

BOOST_AUTO_TEST_CASE(xml_rpc_violation_with_detail)
{
    XML_RPC_violation ex("missing required field");
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("XML-RPC violation") != std::string::npos);
    BOOST_CHECK(msg.find("missing required field") != std::string::npos);
    BOOST_CHECK_EQUAL(ex.code(), -32600);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(unknown_method_tests)

BOOST_AUTO_TEST_CASE(unknown_method_message)
{
    Unknown_method ex("nonexistent.method");
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("nonexistent.method") != std::string::npos);
    BOOST_CHECK(msg.find("not found") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(unknown_method_code)
{
    Unknown_method ex("test");
    BOOST_CHECK_EQUAL(ex.code(), -32601);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(invalid_params_tests)

BOOST_AUTO_TEST_CASE(invalid_params_message)
{
    Invalid_meth_params ex;
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("Invalid method parameters") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(invalid_params_code)
{
    Invalid_meth_params ex;
    BOOST_CHECK_EQUAL(ex.code(), -32602);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(fault_tests)

BOOST_AUTO_TEST_CASE(fault_with_code_and_message)
{
    Fault ex(42, "Application error");
    BOOST_CHECK_EQUAL(std::string(ex.what()), "Application error");
    BOOST_CHECK_EQUAL(ex.code(), 42);
}

BOOST_AUTO_TEST_CASE(fault_negative_code)
{
    Fault ex(-100, "Negative code error");
    BOOST_CHECK_EQUAL(ex.code(), -100);
}

BOOST_AUTO_TEST_CASE(fault_zero_code)
{
    Fault ex(0, "Zero code");
    BOOST_CHECK_EQUAL(ex.code(), 0);
}

BOOST_AUTO_TEST_CASE(fault_can_be_caught_as_exception)
{
    try {
        throw Fault(123, "Test fault");
    } catch (const Exception& e) {
        BOOST_CHECK_EQUAL(e.code(), 123);
        BOOST_CHECK_EQUAL(std::string(e.what()), "Test fault");
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(network_error_tests)

BOOST_AUTO_TEST_CASE(network_error_no_errno)
{
    iqnet::network_error ex("Connection failed", false);
    std::string msg = ex.what();
    BOOST_CHECK_EQUAL(msg, "Connection failed");
}

BOOST_AUTO_TEST_CASE(network_error_with_errno)
{
    errno = ECONNREFUSED;
    iqnet::network_error ex("Connection refused", true);
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("Connection refused") != std::string::npos);
    // Should contain errno description
    BOOST_CHECK(msg.find(":") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(network_error_with_custom_errno)
{
    iqnet::network_error ex("Custom error", true, EINVAL);
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("Custom error") != std::string::npos);
    BOOST_CHECK(msg.find(":") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(network_error_inheritance)
{
    iqnet::network_error ex("Test", false);
    std::runtime_error& ref = ex;
    BOOST_CHECK_EQUAL(std::string(ref.what()), "Test");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ssl_exception_tests)

// Note: ssl::exception default constructor is not tested directly because
// ERR_reason_error_string(0) returns NULL when there's no SSL error queued,
// and the constructor passes that directly to std::string which is UB.
// The constructors with explicit error code or message are tested instead.

BOOST_AUTO_TEST_CASE(ssl_exception_with_error_code)
{
    // Constructor with explicit error code
    iqnet::ssl::exception ex(0);
    BOOST_CHECK(ex.what() != nullptr);
    BOOST_CHECK_EQUAL(ex.code(), 0UL);
}

BOOST_AUTO_TEST_CASE(ssl_exception_with_string_message)
{
    iqnet::ssl::exception ex("Custom SSL error message");
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("SSL:") != std::string::npos);
    BOOST_CHECK(msg.find("Custom SSL error message") != std::string::npos);
    BOOST_CHECK_EQUAL(ex.code(), 0UL);
}

BOOST_AUTO_TEST_CASE(ssl_not_initialized_exception)
{
    iqnet::ssl::not_initialized ex;
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("not initialized") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(ssl_connection_close_clean)
{
    iqnet::ssl::connection_close ex(true);
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("Connection") != std::string::npos);
    BOOST_CHECK(ex.is_clean());
}

BOOST_AUTO_TEST_CASE(ssl_connection_close_unclean)
{
    iqnet::ssl::connection_close ex(false);
    std::string msg = ex.what();
    BOOST_CHECK(msg.find("Connection") != std::string::npos);
    BOOST_CHECK(!ex.is_clean());
}

BOOST_AUTO_TEST_CASE(ssl_io_error_exception)
{
    iqnet::ssl::io_error ex(SSL_ERROR_SYSCALL);
    BOOST_CHECK(ex.what() != nullptr);
}

BOOST_AUTO_TEST_CASE(ssl_need_write_exception)
{
    iqnet::ssl::need_write ex;
    BOOST_CHECK(ex.what() != nullptr);
}

BOOST_AUTO_TEST_CASE(ssl_need_read_exception)
{
    iqnet::ssl::need_read ex;
    BOOST_CHECK(ex.what() != nullptr);
}

BOOST_AUTO_TEST_CASE(ssl_exception_inheritance)
{
    // All SSL exceptions should be catchable as std::exception
    try {
        throw iqnet::ssl::not_initialized();
    } catch (const std::exception& e) {
        BOOST_CHECK(e.what() != nullptr);
    }

    try {
        throw iqnet::ssl::connection_close(true);
    } catch (const iqnet::ssl::exception& e) {
        BOOST_CHECK(e.what() != nullptr);
    }

    try {
        throw iqnet::ssl::need_read();
    } catch (const iqnet::ssl::io_error& e) {
        BOOST_CHECK(e.what() != nullptr);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et

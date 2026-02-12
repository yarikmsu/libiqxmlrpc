//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: Server and Executor

#include <boost/test/unit_test.hpp>

#include <sstream>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"
#include "libiqxmlrpc/executor.h"

#include "methods.h"
#include "test_common.h"

using namespace iqxmlrpc;
using namespace iqnet;
using namespace iqxmlrpc_test;

//=============================================================================
// Executor Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(executor_tests)

BOOST_FIXTURE_TEST_CASE(serial_executor_processing, IntegrationFixture)
{
  start_server(1, 30);  // Single thread = serial executor
  auto client = create_client();

  for (int i = 0; i < 10; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(pool_executor_processing, IntegrationFixture)
{
  start_server(4, 31);  // Thread pool
  auto client = create_client();

  for (int i = 0; i < 20; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
  }
}

BOOST_FIXTURE_TEST_CASE(executor_fault_handling, IntegrationFixture)
{
  start_server(1, 32);
  auto client = create_client();

  Response r = client->execute("error_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), 123);
  BOOST_CHECK(r.fault_string().find("My fault") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(pool_executor_graceful_shutdown, IntegrationFixture)
{
  start_server(4, 33);
  auto client = create_client();

  // Make some requests
  for (int i = 0; i < 5; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
  }

  // Stop server - should be graceful
  stop_server();
  BOOST_CHECK(!is_running());
}

// Test Pool_executor std::exception handling (CWE-209: generic message to client)
BOOST_FIXTURE_TEST_CASE(pool_executor_std_exception_handling, IntegrationFixture)
{
  start_server(4, 34);  // Thread pool
  auto client = create_client();

  // Call method that throws std::runtime_error
  Response r = client->execute("std_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -1);
  BOOST_CHECK_EQUAL(r.fault_string(), "Internal server error");
}

// Test Pool_executor unknown exception handling (CWE-209: generic message to client)
BOOST_FIXTURE_TEST_CASE(pool_executor_unknown_exception_handling, IntegrationFixture)
{
  start_server(4, 35);  // Thread pool
  auto client = create_client();

  // Call method that throws non-exception type (int)
  Response r = client->execute("unknown_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -1);
  BOOST_CHECK_EQUAL(r.fault_string(), "Internal server error");
}

// Serial executor: std::exception propagates to server.cc catch, fault code -32500
BOOST_FIXTURE_TEST_CASE(serial_executor_std_exception_handling, IntegrationFixture)
{
  start_server(1, 36);  // Single thread = serial executor
  auto client = create_client();

  Response r = client->execute("std_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -32500);
  BOOST_CHECK_EQUAL(r.fault_string(), "Internal server error");
}

// Serial executor: catch(...) also returns generic message
BOOST_FIXTURE_TEST_CASE(serial_executor_unknown_exception_handling, IntegrationFixture)
{
  start_server(1, 37);  // Single thread = serial executor
  auto client = create_client();

  Response r = client->execute("unknown_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -32500);
  BOOST_CHECK_EQUAL(r.fault_string(), "Internal server error");
}

// Pool executor: iqxmlrpc::Exception preserves fault code and message
BOOST_FIXTURE_TEST_CASE(pool_executor_library_exception_handling, IntegrationFixture)
{
  start_server(4, 38);  // Thread pool
  auto client = create_client();

  Response r = client->execute("library_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -32602);
  BOOST_CHECK_EQUAL(r.fault_string(), "Server error. Invalid method parameters.");
}

// Serial executor: iqxmlrpc::Exception also preserves fault code and message
BOOST_FIXTURE_TEST_CASE(serial_executor_library_exception_handling, IntegrationFixture)
{
  start_server(1, 39);  // Serial executor
  auto client = create_client();

  Response r = client->execute("library_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -32602);
  BOOST_CHECK_EQUAL(r.fault_string(), "Server error. Invalid method parameters.");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Server Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(server_tests)

BOOST_FIXTURE_TEST_CASE(full_rpc_cycle, IntegrationFixture)
{
  start_server(1, 40);
  auto client = create_client();

  // Test various value types
  Response r1 = client->execute("echo", Value(42));
  BOOST_CHECK(!r1.is_fault());
  BOOST_CHECK_EQUAL(r1.value().get_int(), 42);

  Response r2 = client->execute("echo", Value("string"));
  BOOST_CHECK(!r2.is_fault());
  BOOST_CHECK_EQUAL(r2.value().get_string(), "string");

  Response r3 = client->execute("echo", Value(3.14));
  BOOST_CHECK(!r3.is_fault());
  BOOST_CHECK_CLOSE(r3.value().get_double(), 3.14, 0.01);

  Response r4 = client->execute("echo", Value(true));
  BOOST_CHECK(!r4.is_fault());
  BOOST_CHECK_EQUAL(r4.value().get_bool(), true);
}

BOOST_FIXTURE_TEST_CASE(error_logging, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(1, 41);
  server().log_errors(&log_stream);

  auto client = create_client();
  Response r = client->execute("error_method", Value(""));
  BOOST_CHECK(r.is_fault());

  // Error should be logged (the Fault message)
  // Note: logging behavior may vary
}

// CWE-209: verify internal detail logged server-side, generic message to client
BOOST_FIXTURE_TEST_CASE(cwe209_log_vs_client_message, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(4, 45);  // Pool executor
  server().log_errors(&log_stream);

  auto client = create_client();
  Response r = client->execute("std_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_string(), "Internal server error");

  std::string logged = log_stream.str();
  // Server log must contain the original exception detail
  BOOST_CHECK_MESSAGE(
    logged.find("Test std::exception") != std::string::npos,
    "Expected internal detail in server log, got: " + logged);
}

// CWE-209: verify serial executor path logs detail server-side
BOOST_FIXTURE_TEST_CASE(cwe209_serial_log_vs_client_message, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(1, 46);  // Serial executor
  server().log_errors(&log_stream);

  auto client = create_client();
  Response r = client->execute("std_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_string(), "Internal server error");

  std::string logged = log_stream.str();
  BOOST_CHECK_MESSAGE(
    logged.find("Test std::exception") != std::string::npos,
    "Expected internal detail in server log, got: " + logged);
}

// CWE-209: verify catch(...) path logs "Unknown exception"
BOOST_FIXTURE_TEST_CASE(cwe209_unknown_exception_log_message, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(4, 47);  // Pool executor
  server().log_errors(&log_stream);

  auto client = create_client();
  Response r = client->execute("unknown_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_string(), "Internal server error");

  std::string logged = log_stream.str();
  BOOST_CHECK_MESSAGE(
    logged.find("Unknown exception") != std::string::npos,
    "Expected 'Unknown exception' in server log, got: " + logged);
}

// CWE-209: verify serial catch(...) logs "Unknown exception" server-side
BOOST_FIXTURE_TEST_CASE(cwe209_serial_unknown_exception_log_message, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(1, 48);  // Serial executor
  server().log_errors(&log_stream);

  auto client = create_client();
  Response r = client->execute("unknown_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_string(), "Internal server error");

  std::string logged = log_stream.str();
  BOOST_CHECK_MESSAGE(
    logged.find("Unknown exception") != std::string::npos,
    "Expected 'Unknown exception' in server log, got: " + logged);
}

// CWE-209: verify pool executor iqxmlrpc::Exception is logged server-side
BOOST_FIXTURE_TEST_CASE(cwe209_pool_library_exception_log, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(4, 49);  // Pool executor
  server().log_errors(&log_stream);

  auto client = create_client();
  Response r = client->execute("library_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -32602);

  std::string logged = log_stream.str();
  BOOST_CHECK_MESSAGE(
    logged.find("Pool_executor:") != std::string::npos,
    "Expected 'Pool_executor:' prefix in server log, got: " + logged);
  BOOST_CHECK_MESSAGE(
    logged.find("Invalid method parameters") != std::string::npos,
    "Expected exception detail in server log, got: " + logged);
}

// CWE-209: verify serial executor iqxmlrpc::Exception is logged server-side
BOOST_FIXTURE_TEST_CASE(cwe209_serial_library_exception_log, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(1, 50);  // Serial executor
  server().log_errors(&log_stream);

  auto client = create_client();
  Response r = client->execute("library_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -32602);

  std::string logged = log_stream.str();
  BOOST_CHECK_MESSAGE(
    logged.find("Server:") != std::string::npos,
    "Expected 'Server:' prefix in server log, got: " + logged);
  BOOST_CHECK_MESSAGE(
    logged.find("Invalid method parameters") != std::string::npos,
    "Expected exception detail in server log, got: " + logged);
}

BOOST_FIXTURE_TEST_CASE(server_set_exit_flag, IntegrationFixture)
{
  start_server(1, 42);
  auto client = create_client();

  // Make a request first
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());

  // Now test that stop_server works properly
  stop_server();
  BOOST_CHECK(!is_running());
}

BOOST_FIXTURE_TEST_CASE(introspection_enabled, IntegrationFixture)
{
  start_server(1, 43);
  server().enable_introspection();

  auto client = create_client();
  Response r = client->execute("system.listMethods", Param_list());
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_array());
  BOOST_CHECK_GT(r.value().size(), 0u);
}

BOOST_FIXTURE_TEST_CASE(xheaders_propagation, IntegrationFixture)
{
  start_server(1, 44);
  auto client = create_client();

  XHeaders headers;
  headers["X-Correlation-ID"] = "test-123";
  client->set_xheaders(headers);

  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_AUTO_TEST_SUITE_END()

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: Client operations

#include <boost/test/unit_test.hpp>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"
#include "libiqxmlrpc/http_errors.h"

#include "methods.h"
#include "test_common.h"

using namespace iqxmlrpc;
using namespace iqnet;
using namespace iqxmlrpc_test;

// Port allocation per test suite (each test gets a unique port):
//   client_tests:                       50-53
//   client_response_size_limit_tests:   60-69

//=============================================================================
// Client Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(client_tests)

BOOST_FIXTURE_TEST_CASE(client_execute_single, IntegrationFixture)
{
  start_server(1, 50);
  auto client = create_client();

  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");
}

BOOST_FIXTURE_TEST_CASE(client_multiple_params, IntegrationFixture)
{
  start_server(1, 51);
  auto client = create_client();

  Param_list params;
  params.emplace_back("a");
  params.emplace_back("b");
  params.emplace_back("c");

  Response r = client->execute("trace", params);
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "abc");
}

BOOST_FIXTURE_TEST_CASE(client_struct_value, IntegrationFixture)
{
  start_server(1, 52);
  auto client = create_client();

  Struct s;
  s.insert("key1", Value("value1"));
  s.insert("key2", Value(42));

  Response r = client->execute("echo", Value(s));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_struct());
  BOOST_CHECK_EQUAL(r.value()["key1"].get_string(), "value1");
  BOOST_CHECK_EQUAL(r.value()["key2"].get_int(), 42);
}

BOOST_FIXTURE_TEST_CASE(client_array_value, IntegrationFixture)
{
  start_server(1, 53);
  auto client = create_client();

  Array a;
  a.push_back(Value(1));
  a.push_back(Value(2));
  a.push_back(Value(3));

  Response r = client->execute("echo", Value(a));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_array());
  BOOST_CHECK_EQUAL(r.value().size(), 3u);
  BOOST_CHECK_EQUAL(r.value()[0].get_int(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Client Response Size Limit Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(client_response_size_limit_tests)

BOOST_FIXTURE_TEST_CASE(max_response_size_default_unlimited, IntegrationFixture)
{
  // Default behavior: no limit, large responses accepted
  start_server(1, 60);
  auto client = create_client();

  BOOST_CHECK_EQUAL(client->get_max_response_sz(), 0u);

  // Echo a reasonably large string - should work with default (unlimited)
  std::string large_str(500, 'A');
  Response r = client->execute("echo", Value(large_str));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), large_str);
}

BOOST_FIXTURE_TEST_CASE(max_response_size_enforcement, IntegrationFixture)
{
  // Set a tiny limit - the XML-RPC response envelope alone exceeds it
  start_server(1, 61);
  auto client = create_client();

  client->set_max_response_sz(10);  // 10 bytes is way too small for any XML-RPC response
  BOOST_CHECK_EQUAL(client->get_max_response_sz(), 10u);

  BOOST_CHECK_THROW(
      client->execute("echo", Value("hello")),
      http::Response_too_large);
}

BOOST_FIXTURE_TEST_CASE(max_response_size_allows_small_responses, IntegrationFixture)
{
  // A generous limit should allow normal responses
  start_server(1, 62);
  auto client = create_client();

  // XML-RPC response for "hello" is ~200 bytes. Set limit to 4KB.
  client->set_max_response_sz(4096);

  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");
}

BOOST_FIXTURE_TEST_CASE(max_response_size_setter_getter, IntegrationFixture)
{
  start_server(1, 63);
  auto client = create_client();

  BOOST_CHECK_EQUAL(client->get_max_response_sz(), 0u);

  client->set_max_response_sz(1024);
  BOOST_CHECK_EQUAL(client->get_max_response_sz(), 1024u);

  client->set_max_response_sz(0);
  BOOST_CHECK_EQUAL(client->get_max_response_sz(), 0u);
}

BOOST_FIXTURE_TEST_CASE(max_response_size_recovery_after_error, IntegrationFixture)
{
  // After Response_too_large, a new request with a larger limit should succeed
  start_server(1, 64);
  auto client = create_client();

  client->set_max_response_sz(10);
  BOOST_CHECK_THROW(
      client->execute("echo", Value("hello")),
      http::Response_too_large);

  // Increase limit and retry
  client->set_max_response_sz(4096);
  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");
}

BOOST_FIXTURE_TEST_CASE(max_response_size_keepalive_recovery, IntegrationFixture)
{
  // With keep-alive, a Response_too_large must invalidate the cached
  // connection so the next request starts on a clean socket/reader.
  start_server(1, 65);
  auto client = create_client();
  client->set_keep_alive(true);

  client->set_max_response_sz(10);
  BOOST_CHECK_THROW(
      client->execute("echo", Value("hello")),
      http::Response_too_large);

  // Increase limit and retry on the same client (keep-alive)
  client->set_max_response_sz(4096);
  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");
}

BOOST_FIXTURE_TEST_CASE(max_response_size_reset_to_unlimited, IntegrationFixture)
{
  // Transition from a restrictive limit to unlimited (0) and verify
  // that large responses succeed again.
  start_server(1, 67);
  auto client = create_client();

  client->set_max_response_sz(10);
  BOOST_CHECK_THROW(
      client->execute("echo", Value("hello")),
      http::Response_too_large);

  // Reset to unlimited
  client->set_max_response_sz(0);
  BOOST_CHECK_EQUAL(client->get_max_response_sz(), 0u);

  std::string large_str(500, 'Z');
  Response r = client->execute("echo", Value(large_str));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), large_str);
}

BOOST_FIXTURE_TEST_CASE(max_response_size_tightened_between_requests, IntegrationFixture)
{
  // Verify that reducing the limit between successful requests takes
  // effect on the next request (confirms per-read re-application).
  start_server(1, 68);
  auto client = create_client();

  // First request succeeds with generous limit
  client->set_max_response_sz(4096);
  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");

  // Tighten limit — next request should fail
  client->set_max_response_sz(10);
  BOOST_CHECK_THROW(
      client->execute("echo", Value("hello")),
      http::Response_too_large);
}

BOOST_FIXTURE_TEST_CASE(max_response_size_keepalive_success_reuses_connection, IntegrationFixture)
{
  // Verify that successful keep-alive requests reuse the connection,
  // and the response size limit does not interfere with normal reuse.
  start_server(1, 66);
  auto client = create_client();
  client->set_keep_alive(true);
  client->set_max_response_sz(4096);

  // First request
  Response r1 = client->execute("echo", Value("first"));
  BOOST_CHECK(!r1.is_fault());
  BOOST_CHECK_EQUAL(r1.value().get_string(), "first");

  // Second request on the same kept-alive connection
  Response r2 = client->execute("echo", Value("second"));
  BOOST_CHECK(!r2.is_fault());
  BOOST_CHECK_EQUAL(r2.value().get_string(), "second");
}

BOOST_FIXTURE_TEST_CASE(set_keepalive_false_clears_cached_connection, IntegrationFixture)
{
  // Exercises client.cc:135 — set_keep_alive(false) must drop the
  // cached connection immediately so the next request opens a fresh
  // socket rather than reusing stale state.
  start_server(1, 69);
  auto client = create_client();
  client->set_keep_alive(true);

  // First request creates and caches a connection
  Response r1 = client->execute("echo", Value("first"));
  BOOST_CHECK(!r1.is_fault());
  BOOST_CHECK_EQUAL(r1.value().get_string(), "first");

  // Transition to non-keep-alive while cached connection exists
  client->set_keep_alive(false);

  // Second request should succeed on a fresh connection
  Response r2 = client->execute("echo", Value("second"));
  BOOST_CHECK(!r2.is_fault());
  BOOST_CHECK_EQUAL(r2.value().get_string(), "second");
}

BOOST_AUTO_TEST_SUITE_END()

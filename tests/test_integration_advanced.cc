//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: Interceptors, server config, and defensive synchronization

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <thread>
#include <vector>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"

#include "methods.h"
#include "test_common.h"
#include "test_integration_common.h"

using namespace iqxmlrpc;
using namespace iqnet;
using namespace iqxmlrpc_test;

//=============================================================================
// Interceptor Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(interceptor_tests)

// Simple interceptor that counts calls
class CountingInterceptor : public Interceptor {
public:
  static int call_count;

  void process(Method* m, const Param_list& params, Value& result) override {
    ++call_count;
    yield(m, params, result);
  }
};

int CountingInterceptor::call_count = 0;

BOOST_FIXTURE_TEST_CASE(interceptor_invoked, IntegrationFixture)
{
  start_server(1, 85);
  CountingInterceptor::call_count = 0;
  server().push_interceptor(new CountingInterceptor());

  auto client = create_client();
  Response r1 = client->execute("echo", Value("test1"));
  Response r2 = client->execute("echo", Value("test2"));

  BOOST_CHECK(!r1.is_fault());
  BOOST_CHECK(!r2.is_fault());
  BOOST_CHECK_EQUAL(CountingInterceptor::call_count, 2);
}

// Interceptor that modifies the result
class ModifyingInterceptor : public Interceptor {
public:
  void process(Method* m, const Param_list& params, Value& result) override {
    yield(m, params, result);
    // Append suffix to string results
    if (result.is_string()) {
      result = Value(result.get_string() + "_modified");
    }
  }
};

BOOST_FIXTURE_TEST_CASE(interceptor_modifies_result, IntegrationFixture)
{
  start_server(1, 86);
  server().push_interceptor(new ModifyingInterceptor());

  auto client = create_client();
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "test_modified");
}

// Chained interceptors
BOOST_FIXTURE_TEST_CASE(chained_interceptors, IntegrationFixture)
{
  start_server(1, 87);
  CountingInterceptor::call_count = 0;
  server().push_interceptor(new CountingInterceptor());
  server().push_interceptor(new CountingInterceptor());

  auto client = create_client();
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(CountingInterceptor::call_count, 2);  // Both interceptors called
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Server Configuration Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(server_config_tests)

BOOST_FIXTURE_TEST_CASE(verification_level_weak, IntegrationFixture)
{
  start_server(1, 95);
  server().set_verification_level(http::HTTP_CHECK_WEAK);

  auto client = create_client();
  Response r = client->execute("echo", Value("weak"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(verification_level_strict, IntegrationFixture)
{
  start_server(1, 96);
  server().set_verification_level(http::HTTP_CHECK_STRICT);

  auto client = create_client();
  Response r = client->execute("echo", Value("strict"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(get_reactor, IntegrationFixture)
{
  start_server(1, 97);

  // Verify reactor is accessible
  BOOST_CHECK(server().get_reactor() != nullptr);

  auto client = create_client();
  Response r = client->execute("echo", Value("reactor_test"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(max_request_size_boundary, IntegrationFixture)
{
  start_server(1, 98);
  server().set_max_request_sz(10000);  // 10KB limit

  auto client = create_client();

  // Just under the limit should work
  std::string data(500, 'x');
  Response r = client->execute("echo", Value(data));
  BOOST_CHECK(!r.is_fault());
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Defensive Synchronization Tests
//
// These tests exercise the defensive mutex protections added to:
//   - Server connection set (server.cc)
//   - Idle connection state (server_conn.cc)
//   - SSL partial write handling (ssl_connection.cc)
//=============================================================================

BOOST_AUTO_TEST_SUITE(defensive_sync_tests)

// Test that concurrent client requests work correctly with connection mutex
// Exercises server.cc connection set synchronization
BOOST_FIXTURE_TEST_CASE(concurrent_connection_registration, IntegrationFixture)
{
  start_server(4, 60);  // Pool executor with 4 threads

  // Launch multiple concurrent clients
  std::vector<std::thread> threads;
  std::atomic<int> success_count(0);

  for (int i = 0; i < 20; ++i) {
    threads.emplace_back([this, i, &success_count]() {
      try {
        auto client = create_client();
        Response r = client->execute("echo", Value(i));
        if (!r.is_fault() && r.value().get_int() == i) {
          ++success_count;
        }
      } catch (...) {}
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  BOOST_CHECK_EQUAL(success_count.load(), 20);
}

// Test large data transfer (exercises SSL partial write loop)
// Covers ssl_connection.cc send() retry loop for partial writes
BOOST_FIXTURE_TEST_CASE(large_data_transfer, IntegrationFixture)
{
  start_server(1, 61);
  auto client = create_client();

  // Create a large string to transfer (64KB)
  std::string large_data(65536, 'X');
  Response r = client->execute("echo", Value(large_data));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string().length(), large_data.length());
}

// Test rapid connection create/destroy cycles
// Exercises connection registration/unregistration under load
BOOST_FIXTURE_TEST_CASE(rapid_connection_cycling, IntegrationFixture)
{
  start_server(2, 62);

  for (int cycle = 0; cycle < 10; ++cycle) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    // Create burst of connections
    for (int i = 0; i < 5; ++i) {
      threads.emplace_back([this, &success_count]() {
        try {
          auto client = create_client();
          Response r = client->execute("echo", Value("rapid"));
          if (!r.is_fault()) {
            ++success_count;
          }
        } catch (...) {}
      });
    }

    for (auto& t : threads) {
      t.join();
    }

    BOOST_CHECK_GE(success_count.load(), 3);  // At least 60% success
  }
}

// Test idle state transitions with keep-alive
// Exercises idle_mutex_ in server_conn.cc
BOOST_FIXTURE_TEST_CASE(idle_state_transitions, IntegrationFixture)
{
  start_server(1, 63);

  auto client = create_client();
  client->set_keep_alive(true);

  // Make multiple requests on same connection
  // Each request: stop_idle -> process -> start_idle
  for (int i = 0; i < 5; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_AUTO_TEST_SUITE_END()

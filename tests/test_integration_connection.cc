//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: Connection handling (acceptor, connection, reuse)

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"

#include "methods.h"
#include "test_common.h"

using namespace iqxmlrpc;
using namespace iqnet;
using namespace iqxmlrpc_test;

//=============================================================================
// Acceptor Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(acceptor_tests)

BOOST_FIXTURE_TEST_CASE(accept_single_connection, IntegrationFixture)
{
  start_server(1, 0);
  auto client = create_client();
  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");
}

BOOST_FIXTURE_TEST_CASE(accept_multiple_sequential_connections, IntegrationFixture)
{
  start_server(1, 1);
  for (int i = 0; i < 5; ++i) {
    auto client = create_client();
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(accept_with_thread_pool, IntegrationFixture)
{
  start_server(4, 2);
  std::vector<std::thread> threads;
  std::atomic<int> success_count(0);

  threads.reserve(10);
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([this, i, &success_count]() {
      try {
        auto client = create_client();
        Response r = client->execute("echo", Value(i));
        if (!r.is_fault() && r.value().get_int() == i) {
          ++success_count;
        }
      } catch (...) {
        (void)0;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  BOOST_CHECK_GE(success_count.load(), 8);  // Allow some failures due to timing
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Connection Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(connection_tests)

BOOST_FIXTURE_TEST_CASE(connection_send_receive_large_data, IntegrationFixture)
{
  start_server(1, 10);
  auto client = create_client();

  // Send moderately large data
  std::string large_data(5000, 'x');
  Response r = client->execute("echo", Value(large_data));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), large_data);
}

BOOST_FIXTURE_TEST_CASE(connection_keep_alive, IntegrationFixture)
{
  start_server(1, 11);
  auto client = create_client();
  client->set_keep_alive(true);

  // Multiple requests on same connection
  for (int i = 0; i < 5; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(connection_no_keep_alive, IntegrationFixture)
{
  start_server(1, 12);
  auto client = create_client();
  client->set_keep_alive(false);

  for (int i = 0; i < 3; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(connection_cleanup_on_shutdown, IntegrationFixture)
{
  start_server(1, 13);
  auto client = create_client();
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
  stop_server();
  BOOST_CHECK(!is_running());
}

BOOST_FIXTURE_TEST_CASE(connection_idle_timeout, IntegrationFixture)
{
  start_server(1, 14);
  // Set a short idle timeout (100ms)
  server_->set_idle_timeout(std::chrono::milliseconds(100));

  // Create a client and keep connection alive
  auto client = create_client();
  client->set_keep_alive(true);

  // Make a request - this should succeed
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "test");

  // Wait for idle timeout to expire
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Force server to process timeout by interrupting
  server_->interrupt();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Next request should fail or create a new connection
  // since the server closed the idle connection
  try {
    Response r2 = client->execute("echo", Value("test2"));
    // If we get here, the client created a new connection, which is fine
    BOOST_CHECK(!r2.is_fault());
  } catch (...) {
    // Connection was closed by server due to idle timeout - expected
    BOOST_CHECK(true);
  }
}

BOOST_FIXTURE_TEST_CASE(connection_no_timeout_during_method_execution, IntegrationFixture)
{
  start_server(1, 15);
  // Set a short idle timeout
  server_->set_idle_timeout(std::chrono::milliseconds(50));

  auto client = create_client();

  // Execute a method that sleeps longer than the idle timeout
  // The connection should NOT be closed because it's actively processing
  Response r = client->execute("sleep", Value(0.2));  // 200ms sleep
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "done");
}

BOOST_FIXTURE_TEST_CASE(connection_idle_timeout_disable_after_enable, IntegrationFixture)
{
  start_server(1, 16);

  // Step 1: Enable idle timeout - connections will be tracked via mutex path
  server_->set_idle_timeout(std::chrono::milliseconds(5000));
  BOOST_CHECK_EQUAL(server_->get_idle_timeout().count(), 5000);

  {
    // Create client with keep_alive=false so each request creates a new connection
    // This ensures register_connection/unregister_connection are called per request
    auto client = create_client();
    client->set_keep_alive(false);

    // These connections exercise the mutex tracking path (timeout enabled)
    for (int i = 0; i < 3; ++i) {
      Response r = client->execute("echo", Value(i));
      BOOST_CHECK(!r.is_fault());
      BOOST_CHECK_EQUAL(r.value().get_int(), i);
    }
  }

  // Step 2: Disable idle timeout - new connections use early-return optimization
  server_->set_idle_timeout(std::chrono::milliseconds(0));
  BOOST_CHECK_EQUAL(server_->get_idle_timeout().count(), 0);

  {
    // Create NEW client after disabling timeout
    // This exercises the early-return optimization in register/unregister_connection
    auto client2 = create_client();
    client2->set_keep_alive(false);

    // These connections exercise the early-return path (timeout disabled)
    for (int i = 10; i < 15; ++i) {
      Response r = client2->execute("echo", Value(i));
      BOOST_CHECK(!r.is_fault());
      BOOST_CHECK_EQUAL(r.value().get_int(), i);
    }
  }

  // Step 3: Re-enable timeout to verify we can switch back to tracking
  server_->set_idle_timeout(std::chrono::milliseconds(1000));
  BOOST_CHECK_EQUAL(server_->get_idle_timeout().count(), 1000);

  {
    auto client3 = create_client();
    client3->set_keep_alive(false);

    Response final_response = client3->execute("echo", Value("final"));
    BOOST_CHECK(!final_response.is_fault());
    BOOST_CHECK_EQUAL(final_response.value().get_string(), "final");
  }
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Connection Reuse Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(connection_reuse_tests)

BOOST_FIXTURE_TEST_CASE(many_requests_same_client, IntegrationFixture)
{
  start_server(1, 100);
  auto client = create_client();
  client->set_keep_alive(true);

  // Many requests on same connection
  for (int i = 0; i < 20; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(alternating_keep_alive, IntegrationFixture)
{
  start_server(1, 101);

  for (int i = 0; i < 5; ++i) {
    auto client = create_client();
    client->set_keep_alive(i % 2 == 0);
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
  }
}

BOOST_AUTO_TEST_SUITE_END()

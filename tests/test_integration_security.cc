//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: Authentication and Firewall

#include <sstream>

#include <boost/test/unit_test.hpp>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"
#include "libiqxmlrpc/auth_plugin.h"
#include "libiqxmlrpc/firewall.h"

#include "methods.h"
#include "test_common.h"
#include "test_integration_common.h"

using namespace iqxmlrpc;
using namespace iqnet;
using namespace iqxmlrpc_test;

//=============================================================================
// Auth Plugin Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(auth_plugin_tests)

BOOST_FIXTURE_TEST_CASE(auth_anonymous_allowed, IntegrationFixture)
{
  TestAuthPlugin auth(true);  // Allow anonymous
  start_server(1, 20);
  server().set_auth_plugin(auth);

  auto client = create_client();
  // No credentials - should work with anonymous allowed
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(auth_success_with_credentials, IntegrationFixture)
{
  TestAuthPlugin auth(false);  // Require auth
  start_server(1, 21);
  server().set_auth_plugin(auth);

  auto client = create_client();
  client->set_authinfo("testuser", "testpass");
  Response r = client->execute("echo", Value("authenticated"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "authenticated");
}

BOOST_FIXTURE_TEST_CASE(auth_failure_wrong_credentials, IntegrationFixture)
{
  TestAuthPlugin auth(false);  // Require auth
  start_server(1, 22);
  server().set_auth_plugin(auth);

  auto client = create_client();
  client->set_authinfo("wronguser", "wrongpass");

  BOOST_CHECK_THROW(client->execute("echo", Value("test")), http::Error_response);
}

BOOST_FIXTURE_TEST_CASE(auth_anonymous_denied, IntegrationFixture)
{
  TestAuthPlugin auth(false);  // Don't allow anonymous
  start_server(1, 23);
  server().set_auth_plugin(auth);

  auto client = create_client();
  // No credentials - should fail
  BOOST_CHECK_THROW(client->execute("echo", Value("test")), http::Error_response);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Firewall Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(firewall_tests)

BOOST_FIXTURE_TEST_CASE(firewall_allows_connection, IntegrationFixture)
{
  start_server(1, 80);
  server().set_firewall(new AllowAllFirewall());

  auto client = create_client();
  Response r = client->execute("echo", Value("allowed"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "allowed");
}

BOOST_FIXTURE_TEST_CASE(firewall_blocks_connection_no_leak, IntegrationFixture)
{
  // Test that firewall rejection properly closes the socket (no FD leak).
  // This test verifies the fix for Coverity CID 641369.
  start_server(1, 81);
  server().set_firewall(new BlockAllFirewall());

  // Make multiple connection attempts that will be rejected.
  // If sockets leak, the server would eventually run out of FDs.
  for (int i = 0; i < 10; ++i) {
    auto client = create_client();
    BOOST_CHECK_THROW(client->execute("echo", Value("blocked")), std::exception);
  }

  // Remove firewall and verify server still works (not exhausted of FDs)
  server().set_firewall(nullptr);
  auto client = create_client();
  Response r = client->execute("echo", Value("after_firewall"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "after_firewall");
}

BOOST_FIXTURE_TEST_CASE(firewall_replace_while_running, IntegrationFixture)
{
  // Verify that replacing the firewall via set_firewall() while the server is
  // running does not cause a use-after-free. This exercises the atomic_load
  // local-copy pattern in acceptor.cc (CWE-416 fix for Finding #9).
  start_server(1, 83);
  server().set_firewall(new AllowAllFirewall());

  // Connections should succeed with AllowAll
  {
    auto client = create_client();
    Response r = client->execute("echo", Value("before_replace"));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_string(), "before_replace");
  }

  // Switch to blocking firewall
  server().set_firewall(new BlockAllFirewall());

  // Connections should now be blocked
  {
    auto client = create_client();
    BOOST_CHECK_THROW(client->execute("echo", Value("blocked")), std::exception);
  }

  // Switch back to allow-all
  server().set_firewall(new AllowAllFirewall());

  {
    auto client = create_client();
    Response r = client->execute("echo", Value("after_restore"));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_string(), "after_restore");
  }
}

BOOST_FIXTURE_TEST_CASE(firewall_rate_limiter_releases_on_disconnect, IntegrationFixture)
{
  // Verify that RateLimitingFirewall::release() is called via
  // Server::unregister_connection() when connections close.
  // With max_connections_per_ip=2, making 3+ sequential requests proves
  // release() decrements the counter — otherwise the 3rd would be rejected.
  start_server(1, 84);
  server().set_firewall(new RateLimitingFirewall(2, 100));

  for (int i = 0; i < 5; ++i) {
    auto client = create_client();
    Response r = client->execute("echo", Value("request_" + std::to_string(i)));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_string(), "request_" + std::to_string(i));
  }
}

BOOST_FIXTURE_TEST_CASE(firewall_release_exception_is_caught, IntegrationFixture)
{
  // Verify that exceptions thrown by fw->release() in
  // Server::unregister_connection() are caught and logged,
  // not propagated to crash the server.
  std::ostringstream log_stream;
  start_server(1, 85);
  server().log_errors(&log_stream);
  server().set_firewall(new ThrowingReleaseFirewall());

  // grant() returns true, so the request succeeds.
  // When the connection closes, release() throws — this must be caught.
  {
    auto client = create_client();
    Response r = client->execute("echo", Value("test"));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_string(), "test");
  }

  // Give server time to process connection close
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Server should still be alive after the exception
  {
    auto client = create_client();
    Response r = client->execute("echo", Value("still alive"));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_string(), "still alive");
  }

  // Error should have been logged
  std::string log = log_stream.str();
  BOOST_CHECK_MESSAGE(log.find("release failed") != std::string::npos,
                      "Expected 'release failed' in log, got: " + log);
}

BOOST_FIXTURE_TEST_CASE(firewall_blocks_with_message_no_leak, IntegrationFixture)
{
  // Test that firewall rejection with custom message properly closes socket.
  // Covers the send_shutdown() path in Acceptor::accept().
  start_server(1, 82);
  server().set_firewall(new CustomMessageFirewall());

  // Make multiple connection attempts that will be rejected with message.
  for (int i = 0; i < 10; ++i) {
    auto client = create_client();
    BOOST_CHECK_THROW(client->execute("echo", Value("blocked")), std::exception);
  }

  // Verify server still works after many rejections
  server().set_firewall(nullptr);
  auto client = create_client();
  Response r = client->execute("echo", Value("recovered"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "recovered");
}

BOOST_AUTO_TEST_SUITE_END()

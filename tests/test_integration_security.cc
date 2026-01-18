//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: Authentication and Firewall

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
  AllowAllFirewall fw;
  server().set_firewall(&fw);

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
  // Heap-allocate: set_firewall() takes ownership and deletes old firewall
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

BOOST_FIXTURE_TEST_CASE(firewall_blocks_with_message_no_leak, IntegrationFixture)
{
  // Test that firewall rejection with custom message properly closes socket.
  // Covers the send_shutdown() path in Acceptor::accept().
  start_server(1, 82);
  // Heap-allocate: set_firewall() takes ownership and deletes old firewall
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

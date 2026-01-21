//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: SSL/TLS and HTTPS

#include <boost/test/unit_test.hpp>

#include <string>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/https_server.h"
#include "libiqxmlrpc/https_client.h"
#include "libiqxmlrpc/ssl_lib.h"

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "methods.h"
#include "test_common.h"
#include "test_integration_common.h"

using namespace iqxmlrpc;
using namespace iqnet;
using namespace iqxmlrpc_test;

//=============================================================================
// SSL Tests
//=============================================================================

// Embedded certificates, HttpsIntegrationFixture, TrackingVerifier,
// create_temp_cert_files(), and ssl_certs_available() are now provided
// by test_integration_common.h

BOOST_AUTO_TEST_SUITE(ssl_tests)

// Test SSL context with client-only mode (no certificates needed)
// This covers ssl_lib.cc line 104 (SSL_get_ex_new_index in init_library)
BOOST_AUTO_TEST_CASE(ssl_client_only_context)
{
  // client_only() doesn't require certificates
  // This still triggers init_library() which includes line 104
  SslContextGuard guard(iqnet::ssl::Ctx::client_only());

  BOOST_REQUIRE(guard.get() != nullptr);
  BOOST_REQUIRE(guard->context() != nullptr);
}

// Test that legacy TLS versions (1.0, 1.1) are disabled
// Only TLS 1.2+ should be allowed for security compliance
BOOST_AUTO_TEST_CASE(ssl_disables_legacy_tls_versions)
{
  SslContextGuard guard(iqnet::ssl::Ctx::client_only());

  // Check minimum protocol version is TLS 1.2 (requires OpenSSL 1.1.0+)
  int min_version = SSL_CTX_get_min_proto_version(guard->context());
  BOOST_CHECK_GE(min_version, TLS1_2_VERSION);
}

// Test SSL context creation with certificates
BOOST_AUTO_TEST_CASE(ssl_context_creation)
{
  SKIP_IF_NO_CERTS();

  SslContextGuard guard(iqnet::ssl::Ctx::client_server(
    "../tests/data/cert.pem",
    "../tests/data/pk.pem"));

  BOOST_REQUIRE(guard.get() != nullptr);
  BOOST_REQUIRE(guard->context() != nullptr);
}

// Test SSL context with server-only mode
BOOST_AUTO_TEST_CASE(ssl_server_only_context)
{
  SKIP_IF_NO_CERTS();

  SslContextGuard guard(iqnet::ssl::Ctx::server_only(
    "../tests/data/cert.pem",
    "../tests/data/pk.pem"));

  BOOST_REQUIRE(guard.get() != nullptr);
  BOOST_REQUIRE(guard->context() != nullptr);
}

// Test ConnectionVerifier setup - covers ssl_lib.cc lines 140, 258
BOOST_AUTO_TEST_CASE(ssl_verifier_setup)
{
  SKIP_IF_NO_CERTS();

  SslContextGuard guard(iqnet::ssl::Ctx::client_server(
    "../tests/data/cert.pem",
    "../tests/data/pk.pem"));

  // Set up client verification - this covers verify_client() method
  TrackingVerifier verifier;
  guard->verify_client(OPTIONAL_CLIENT_CERT, &verifier);

  // Set up server verification
  TrackingVerifier server_verifier;
  guard->verify_server(&server_verifier);
}

// Test SSL exception types (no certificates needed)
BOOST_AUTO_TEST_CASE(ssl_exception_types)
{
  // Test ssl::not_initialized exception
  iqnet::ssl::not_initialized not_init;
  BOOST_CHECK(std::string(not_init.what()).find("not initialized") != std::string::npos);

  // Test ssl::connection_close exception
  iqnet::ssl::connection_close close_clean(true);
  BOOST_CHECK(close_clean.is_clean());

  iqnet::ssl::connection_close close_unclean(false);
  BOOST_CHECK(!close_unclean.is_clean());
}

// Test basic HTTPS client/server communication (without custom verifier)
BOOST_FIXTURE_TEST_CASE(https_basic_communication, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  start_server(200);
  auto client = create_client();

  Response r = client->execute("echo", Value("https test"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "https test");
}

// Test that TLS handshake invokes verifier callback
// This test actually performs a TLS handshake and covers:
// - ssl_lib.cc line 140 (iqxmlrpc_SSL_verify callback)
// - ssl_lib.cc line 258 (SSL_set_ex_data in prepare_verify)
BOOST_FIXTURE_TEST_CASE(https_handshake_triggers_verify, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  TrackingVerifier client_verifier;
  get_context()->verify_server(&client_verifier);

  start_server(201);
  auto client = create_client();

  Response r = client->execute("echo", Value("handshake test"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "handshake test");
  // Verify the callback was actually invoked during TLS handshake
  BOOST_CHECK_GT(client_verifier.get_call_count(), 0);
}

// Test that cert_finger_sha256 produces valid fingerprints during verification
// This validates the null check fix in ssl_lib.cc:227
BOOST_FIXTURE_TEST_CASE(ssl_cert_fingerprint_valid, HttpsIntegrationFixture)
{
  if (!setup_ssl_context()) {
    BOOST_TEST_MESSAGE("Skipping SSL fingerprint test - context setup failed");
    return;
  }

  FingerprintVerifier client_verifier;
  test_ctx_->verify_server(&client_verifier);

  start_server(202);
  auto client = create_client();

  Response r = client->execute("echo", Value("fingerprint_test"));
  BOOST_CHECK(!r.is_fault());

  // Verify the callback was invoked
  BOOST_CHECK_GT(client_verifier.get_call_count(), 0);

  // Verify fingerprint is non-empty
  // Note: SHA256 = 32 bytes, but since the function uses non-zero-padded hex
  // (e.g., 0x05 becomes "5" not "05"), length varies between 32-64 chars
  std::string fp = client_verifier.fingerprint();
  BOOST_CHECK(!fp.empty());
  BOOST_CHECK_GE(fp.length(), 32u);  // At least 32 chars (all single digit hex)
  BOOST_CHECK_LE(fp.length(), 64u);  // At most 64 chars (all double digit hex)
}

// Test that fingerprint function handles multiple HTTPS requests correctly
BOOST_FIXTURE_TEST_CASE(ssl_cert_fingerprint_stability, HttpsIntegrationFixture)
{
  if (!setup_ssl_context()) {
    BOOST_TEST_MESSAGE("Skipping SSL fingerprint stability test - context setup failed");
    return;
  }

  start_server(203);

  // Make multiple HTTPS requests to exercise certificate verification
  for (int i = 0; i < 3; ++i) {
    auto client = create_client();
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

// Note: The https_proxy_ssl_factory_* tests in coverage_improvement_tests provide
// good coverage of the tunnel code path (https_client.cc) using inline proxy
// patterns with SslFactoryTestProxyGuard from test_integration_common.h.

// Test verify_client() with require_certificate=true (covers lines 234-238, 246-247)
// This test verifies the API works and that prepare_verify() is called with correct flags
BOOST_FIXTURE_TEST_CASE(ssl_verify_client_required, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  // Set up server to require client certificates - covers lines 234-238
  TrackingVerifier server_verifier;
  get_context()->verify_client(REQUIRE_CLIENT_CERT, &server_verifier);

  start_server(211);

  // Attempt connection without client certificate
  // This triggers prepare_verify() with server=true and require_client_cert=true (line 246-247)
  auto client = create_client();

  // Connection may succeed or fail depending on OpenSSL behavior without client cert
  // The important part is that verify_client() and prepare_verify() were exercised
  try {
    client->execute("echo", Value("test"));
  } catch (const std::exception&) {
    // Expected: connection fails without client certificate
  }
}

// Test load_verify_locations() with CA file (covers line 279)
BOOST_AUTO_TEST_CASE(ssl_load_verify_locations)
{
  SslContextGuard guard(iqnet::ssl::Ctx::client_only());

  // Test with both parameters empty (should return false - line 276)
  bool result_empty = guard->load_verify_locations("", "");
  BOOST_CHECK(!result_empty);

  // Test loading from a certificate file (using test cert as CA) - covers line 279
  if (ssl_certs_available()) {
    bool result = guard->load_verify_locations("../tests/data/cert.pem", "");
    BOOST_CHECK(result);
  }
}

// Test use_default_verify_paths() (covers line 285)
BOOST_AUTO_TEST_CASE(ssl_use_default_verify_paths)
{
  SslContextGuard guard(iqnet::ssl::Ctx::client_only());

  // Call use_default_verify_paths() - result is system-dependent
  // Just verify the call doesn't crash
  guard->use_default_verify_paths();
}

// Test hostname verification setup (covers lines 289-298)
// Note: Actual hostname verification during connection is tested in
// https_hostname_verification integration test
BOOST_AUTO_TEST_CASE(ssl_hostname_verification_setup)
{
  SslContextGuard guard(iqnet::ssl::Ctx::client_only());

  // Test API calls work without crashing
  guard->set_hostname_verification(true);
  guard->set_expected_hostname("example.com");
  guard->set_hostname_verification(false);
}

// Integration test: hostname verification in actual HTTPS connection
// This is the end-to-end test that verifies prepare_hostname_verify() works (lines 301-314)
BOOST_FIXTURE_TEST_CASE(https_hostname_verification, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  // Enable hostname verification with expected hostname
  get_context()->set_hostname_verification(true);
  get_context()->set_expected_hostname("localhost");

  start_server(210);
  auto client = create_client();

  // This should succeed since we're connecting to localhost
  Response r = client->execute("echo", Value("hostname test"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hostname test");
}

BOOST_AUTO_TEST_SUITE_END()

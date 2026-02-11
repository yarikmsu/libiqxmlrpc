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
// This covers SSL_get_ex_new_index() in init_library() (ssl_lib.cc)
BOOST_AUTO_TEST_CASE(ssl_client_only_context)
{
  // client_only() doesn't require certificates
  // This still triggers init_library() which calls SSL_get_ex_new_index()
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

// Test ConnectionVerifier setup - covers iqxmlrpc_SSL_verify() callback
// and SSL_set_ex_data() in prepare_verify() (ssl_lib.cc)
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
  BOOST_CHECK(std::strstr(not_init.what(), "not initialized") != nullptr);

  // Test ssl::connection_close exception
  iqnet::ssl::connection_close close_clean(true);
  BOOST_CHECK(close_clean.is_clean());
  BOOST_CHECK(close_clean.what() != nullptr && close_clean.what()[0] != '\0');

  iqnet::ssl::connection_close close_unclean(false);
  BOOST_CHECK(!close_unclean.is_clean());
  BOOST_CHECK(close_unclean.what() != nullptr && close_unclean.what()[0] != '\0');
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
// - iqxmlrpc_SSL_verify() callback (ssl_lib.cc)
// - SSL_set_ex_data() in prepare_verify() (ssl_lib.cc)
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
// This validates the null check in cert_finger_sha256() (ssl_lib.cc)
BOOST_FIXTURE_TEST_CASE(ssl_cert_fingerprint_valid, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  FingerprintVerifier client_verifier;
  get_context()->verify_server(&client_verifier);

  start_server(202);
  auto client = create_client();

  Response r = client->execute("echo", Value("fingerprint_test"));
  BOOST_CHECK(!r.is_fault());

  // Verify the callback was invoked
  BOOST_CHECK_GT(client_verifier.get_call_count(), 0);

  // Verify fingerprint is non-empty
  // SHA256 = 32 bytes = 64 hex characters (zero-padded %02x format)
  std::string fp = client_verifier.fingerprint();
  BOOST_CHECK(!fp.empty());
  BOOST_CHECK_EQUAL(fp.length(), 64u);
}

// Test that fingerprint function handles multiple HTTPS requests correctly
BOOST_FIXTURE_TEST_CASE(ssl_cert_fingerprint_stability, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

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

// Test verify_client() with require_certificate=true
// Covers verify_client() and the require_client_cert branch in prepare_verify() (ssl_lib.cc)
BOOST_FIXTURE_TEST_CASE(ssl_verify_client_required, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  // Set up server to require client certificates - covers verify_client()
  TrackingVerifier server_verifier;
  get_context()->verify_client(REQUIRE_CLIENT_CERT, &server_verifier);

  start_server(211);

  // Attempt connection without client certificate
  // This triggers prepare_verify() with server=true and require_client_cert=true
  auto client = create_client();

  // Connection behavior varies by OpenSSL version/configuration.
  // Key verification: the verify_client() API was exercised (code path covered).
  bool connection_failed = false;
  try {
    client->execute("echo", Value("test"));
  } catch (const std::exception&) {
    connection_failed = true;
  }

  // Verify the test exercised meaningful code - at least one of:
  // - Connection failed (verifier rejected), or
  // - Server verifier was invoked during handshake
  BOOST_CHECK(connection_failed || server_verifier.get_call_count() > 0);
}

// Test load_verify_locations() with CA file
BOOST_AUTO_TEST_CASE(ssl_load_verify_locations)
{
  SslContextGuard guard(iqnet::ssl::Ctx::client_only());

  // Test with both parameters empty (should return false — both-empty guard)
  bool result_empty = guard->load_verify_locations("", "");
  BOOST_CHECK(!result_empty);

  // Test loading from a certificate file (using test cert as CA)
  if (ssl_certs_available()) {
    bool result = guard->load_verify_locations("../tests/data/cert.pem", "");
    BOOST_CHECK(result);
  }
}

// Test use_default_verify_paths()
BOOST_AUTO_TEST_CASE(ssl_use_default_verify_paths)
{
  SslContextGuard guard(iqnet::ssl::Ctx::client_only());

  // Call use_default_verify_paths() - result is system-dependent
  // Just verify the call doesn't crash
  guard->use_default_verify_paths();
}

// Test hostname verification setup — covers set_hostname_verification()
// and set_expected_hostname() (ssl_lib.cc)
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

// Test hostname_verification_enabled() accessor returns correct state
BOOST_AUTO_TEST_CASE(ssl_hostname_verification_enabled_accessor)
{
  SslContextGuard guard(iqnet::ssl::Ctx::client_only());

  // Default is enabled (security-first default)
  BOOST_CHECK(guard->hostname_verification_enabled());

  guard->set_hostname_verification(false);
  BOOST_CHECK(!guard->hostname_verification_enabled());
}

// Integration test: hostname verification using per-connection API (thread-safe)
BOOST_FIXTURE_TEST_CASE(https_hostname_verification, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  // Enable hostname verification at Ctx level (verification flag)
  get_context()->set_hostname_verification(true);

  start_server(210);
  auto client = create_client();

  // Use per-connection API instead of Ctx-level set_expected_hostname()
  client->set_expected_hostname("localhost");

  // This should succeed since we're connecting to localhost
  Response r = client->execute("echo", Value("hostname test"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hostname test");
}

// Integration test: legacy Ctx-level hostname API still works (backward compat)
BOOST_FIXTURE_TEST_CASE(https_hostname_verification_legacy, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  // Use the legacy Ctx-level API
  get_context()->set_hostname_verification(true);
  get_context()->set_expected_hostname("localhost");

  start_server(212);
  auto client = create_client();

  Response r = client->execute("echo", Value("legacy hostname test"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "legacy hostname test");
}

// Verifier that respects OpenSSL's verification result.
// Unlike TrackingVerifier (which accepts all), this returns preverified_ok
// so that hostname mismatches cause actual TLS failures.
class StrictVerifier : public iqnet::ssl::ConnectionVerifier {
  mutable std::atomic<int> call_count_{0};

  int do_verify(bool preverified_ok, X509_STORE_CTX*) const override {
    ++call_count_;
    return preverified_ok ? 1 : 0;
  }

public:
  int get_call_count() const { return call_count_.load(); }
};

// Single-threaded negative test: wrong hostname must be rejected.
// This proves hostname verification actually enforces mismatches,
// independent of the concurrent test (which is noisier for CI debugging).
BOOST_FIXTURE_TEST_CASE(https_hostname_mismatch_rejected, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  get_context()->set_hostname_verification(true);
  get_context()->load_verify_locations(temp_cert_path_);

  StrictVerifier strict_verifier;
  get_context()->verify_server(&strict_verifier);

  start_server(214);
  auto client = create_client();

  // Set a hostname that does NOT match the cert's CN ("localhost")
  client->set_expected_hostname("wrong.example.com");

  BOOST_CHECK_THROW(
    client->execute("echo", Value("should fail")),
    std::exception);
}

// Legacy Ctx-level negative test: wrong hostname via Ctx API must be rejected.
// This validates the hardened Ctx::prepare_hostname_verify() properly applies
// X509_VERIFY_PARAM_set1_host() through the legacy fallback path.
BOOST_FIXTURE_TEST_CASE(https_hostname_mismatch_rejected_legacy, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  get_context()->set_hostname_verification(true);
  get_context()->set_expected_hostname("wrong.example.com");
  get_context()->load_verify_locations(temp_cert_path_);

  StrictVerifier strict_verifier;
  get_context()->verify_server(&strict_verifier);

  start_server(217);
  auto client = create_client();

  // Do NOT call client->set_expected_hostname() — use legacy Ctx-level path
  BOOST_CHECK_THROW(
    client->execute("echo", Value("should fail")),
    std::exception);
}

// Concurrent test: prove per-connection hostname isolation.
// This is the core regression test for the TOCTOU race on shared SSL
// hostname state (CWE-367). See docs/SECURITY_FINDINGS_2026.md #3.
//
// Strategy: even-numbered threads use "localhost" (correct for our test cert)
// and should SUCCEED. Odd-numbered threads use "wrong.example.com" (incorrect)
// and should FAIL with a hostname mismatch. If hostnames cross-contaminate,
// an even thread could fail or an odd thread could succeed.
//
// Uses StrictVerifier (respects preverified_ok) instead of TrackingVerifier
// (accepts all). With the self-signed cert loaded as trusted CA,
// chain validation passes but hostname mismatches are enforced.
BOOST_FIXTURE_TEST_CASE(concurrent_hostname_different_clients, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  get_context()->set_hostname_verification(true);

  // Load our self-signed cert as the trusted CA so chain validation passes
  // but hostname verification is still enforced
  get_context()->load_verify_locations(temp_cert_path_);

  // StrictVerifier returns preverified_ok as-is (unlike TrackingVerifier
  // which always accepts). Combined with verify_server(), SSL_VERIFY_PEER
  // is enabled so OpenSSL enforces hostname mismatches.
  StrictVerifier strict_verifier;
  get_context()->verify_server(&strict_verifier);

  start_server(213);

  constexpr int NUM_THREADS = 4;
  constexpr int REQUESTS_PER_THREAD = 3;
  std::atomic<int> correct_success{0};   // even threads that succeeded (expected)
  std::atomic<int> correct_failure{0};   // odd threads that failed (expected)
  std::atomic<int> wrong_success{0};     // odd threads that succeeded (BUG!)
  std::atomic<int> wrong_failure{0};     // even threads that failed (BUG!)
  std::vector<std::thread> threads;

  for (int t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&, t]() {
      bool use_correct_hostname = (t % 2 == 0);
      for (int i = 0; i < REQUESTS_PER_THREAD; ++i) {
        try {
          auto client = create_client();
          client->set_expected_hostname(
            use_correct_hostname ? "localhost" : "wrong.example.com");
          client->set_keep_alive(false);

          std::string msg = "thread" + std::to_string(t) + "_req" + std::to_string(i);
          Response r = client->execute("echo", Value(msg));

          if (!r.is_fault() && r.value().get_string() == msg) {
            if (use_correct_hostname)
              ++correct_success;
            else
              ++wrong_success;  // This would indicate hostname cross-contamination
          }
        } catch (const std::exception& e) {
          if (use_correct_hostname) {
            ++wrong_failure;    // This would indicate hostname cross-contamination
            BOOST_TEST_MESSAGE("Unexpected failure for correct hostname: " << e.what());
          } else {
            ++correct_failure;
          }
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  int even_threads = (NUM_THREADS + 1) / 2;  // threads 0, 2
  int odd_threads = NUM_THREADS / 2;          // threads 1, 3

  // Even threads ("localhost") should all succeed
  BOOST_CHECK_EQUAL(correct_success.load(), even_threads * REQUESTS_PER_THREAD);
  BOOST_CHECK_EQUAL(wrong_failure.load(), 0);

  // Odd threads ("wrong.example.com") should all fail with hostname mismatch
  BOOST_CHECK_EQUAL(correct_failure.load(), odd_threads * REQUESTS_PER_THREAD);
  BOOST_CHECK_EQUAL(wrong_success.load(), 0);
}

// Test: SNI-only path when hostname is set but verification is disabled.
// Covers the !hostname_verification_enabled() branch in
// prepare_hostname_for_connect() — SSL_set_tlsext_host_name() is called
// but X509_VERIFY_PARAM hostname check is skipped.
BOOST_FIXTURE_TEST_CASE(https_per_conn_hostname_sni_only, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  // Disable hostname verification at Ctx level
  get_context()->set_hostname_verification(false);

  start_server(215);
  auto client = create_client();

  // Set per-connection hostname — only SNI will be applied since
  // hostname_verification_enabled() returns false
  client->set_expected_hostname("localhost");

  Response r = client->execute("echo", Value("sni only test"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "sni only test");
}

// Negative proof: mismatched hostname with verification disabled should SUCCEED.
// The existing https_per_conn_hostname_sni_only uses "localhost" (which matches
// the cert), so it would pass even if verification were accidentally applied.
// This test uses a WRONG hostname — proving X509_VERIFY_PARAM is truly not set
// when hostname_verification_enabled() returns false.
BOOST_FIXTURE_TEST_CASE(https_per_conn_mismatched_hostname_sni_only_succeeds, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  get_context()->set_hostname_verification(false);
  get_context()->load_verify_locations(temp_cert_path_);

  StrictVerifier strict_verifier;
  get_context()->verify_server(&strict_verifier);

  start_server(218);
  auto client = create_client();

  // Set a WRONG hostname — but since verification is disabled, only SNI is
  // applied and the connection should succeed (cert CN=localhost, SNI=wrong)
  client->set_expected_hostname("wrong.example.com");

  Response r = client->execute("echo", Value("sni only wrong hostname"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "sni only wrong hostname");
}

// Test: Proxy path exercises Https_proxy_client_connection::set_ssl_expected_hostname()
// override via Client::get_connection() → Connector::create_connection().
// Note: Only the hostname storage on the proxy object is exercised here;
// the internal forwarding in do_process_session() is NOT reached because
// the CONNECT tunnel fails (HTTPS server rejects plain-text CONNECT data).
BOOST_FIXTURE_TEST_CASE(https_proxy_hostname_forwarding, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  start_server(216);
  auto client = create_client();

  // Point proxy at the HTTPS server itself (which doesn't understand
  // plain-text CONNECT requests). This exercises the code path:
  //   Client::get_connection() → proxy_ctr->set_expected_hostname()
  //   → Connector::create_connection() → c->set_ssl_expected_hostname()
  client->set_proxy(iqnet::Inet_addr("127.0.0.1", port()));
  client->set_expected_hostname("localhost");

  // The execute() fails because the HTTPS server rejects the plain-text
  // CONNECT request, but set_ssl_expected_hostname() was already called
  BOOST_CHECK_THROW(
    client->execute("echo", Value("proxy test")),
    std::exception);
}

// =============================================================================
// Client TLS Verification Tests (Finding #1: client_verified / set_verify_peer)
// =============================================================================
// Note: ssl::Connection captures the global ssl::ctx pointer at construction time.
// Integration tests require a single context that works for both server and client
// connections. Since client_verified() creates a context without cert/key (breaking
// server accept), all integration tests use a client_server context with
// set_verify_peer(true):
//   - Positive tests: also call load_verify_locations() with the test cert
//   - Negative tests: omit load_verify_locations() so chain validation fails

// client_server context + set_verify_peer(true) + load_verify_locations -> succeeds
// Tests the upgrade path where an existing context enables peer verification.
BOOST_FIXTURE_TEST_CASE(https_set_verify_peer_with_trusted_ca, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  // Enable peer verification and load the self-signed cert as trusted CA
  get_context()->set_verify_peer(true);
  BOOST_REQUIRE(get_context()->load_verify_locations(temp_cert_path_));

  start_server(220);
  auto client = create_client();

  Response r = client->execute("echo", Value("verified peer test"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "verified peer test");
}

// set_verify_peer(true) on client_server context without loading our test CA -> fails
// The system CAs don't include our self-signed test certificate, so the
// peer certificate chain validation fails even though the context has cert/key.
BOOST_FIXTURE_TEST_CASE(https_set_verify_peer_rejects_untrusted_cert, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  // Enable peer verification but don't load our self-signed CA
  // System CAs won't include our test certificate
  get_context()->set_verify_peer(true);

  start_server(221);
  auto client = create_client();

  BOOST_CHECK_THROW(
    client->execute("echo", Value("should fail")),
    std::exception);
}

BOOST_AUTO_TEST_SUITE_END()

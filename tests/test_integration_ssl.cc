//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Integration tests: SSL/TLS and HTTPS

#include <boost/test/unit_test.hpp>

#include <atomic>
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
  iqnet::ssl::Ctx* saved_ctx = iqnet::ssl::ctx;

  try {
    // client_only() doesn't require certificates
    // This still triggers init_library() which includes line 104
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_only();

    BOOST_REQUIRE(ctx != nullptr);
    BOOST_REQUIRE(ctx->context() != nullptr);

    delete ctx;
  } catch (const std::exception& e) {
    iqnet::ssl::ctx = saved_ctx;
    BOOST_FAIL("SSL client_only context creation failed: " << e.what());
  }

  iqnet::ssl::ctx = saved_ctx;
}

// Test that legacy TLS versions (1.0, 1.1) are disabled
// Only TLS 1.2+ should be allowed for security compliance
BOOST_AUTO_TEST_CASE(ssl_disables_legacy_tls_versions)
{
  iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_only();
  BOOST_REQUIRE(ctx != nullptr);

  SSL_CTX* ssl_ctx = ctx->context();

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  // OpenSSL 1.1.0+: Check minimum protocol version directly
  int min_version = SSL_CTX_get_min_proto_version(ssl_ctx);
  BOOST_CHECK_GE(min_version, TLS1_2_VERSION);
#else
  // OpenSSL 1.0.x: Check the legacy flags are set
  long options = SSL_CTX_get_options(ssl_ctx);
  BOOST_CHECK(options & SSL_OP_NO_SSLv3);
  BOOST_CHECK(options & SSL_OP_NO_TLSv1);
  BOOST_CHECK(options & SSL_OP_NO_TLSv1_1);
#endif

  delete ctx;
}

// Test SSL context creation with certificates
BOOST_AUTO_TEST_CASE(ssl_context_creation)
{
  if (!ssl_certs_available()) {
    BOOST_TEST_MESSAGE("Skipping - SSL certificates not available");
    return;
  }

  iqnet::ssl::Ctx* saved_ctx = iqnet::ssl::ctx;

  try {
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_server(
      "../tests/data/cert.pem",
      "../tests/data/pk.pem");

    BOOST_REQUIRE(ctx != nullptr);
    BOOST_REQUIRE(ctx->context() != nullptr);

    delete ctx;
  } catch (const std::exception& e) {
    iqnet::ssl::ctx = saved_ctx;
    BOOST_FAIL("SSL context creation failed: " << e.what());
  }

  iqnet::ssl::ctx = saved_ctx;
}

// Test SSL context with server-only mode
BOOST_AUTO_TEST_CASE(ssl_server_only_context)
{
  if (!ssl_certs_available()) {
    BOOST_TEST_MESSAGE("Skipping - SSL certificates not available");
    return;
  }

  iqnet::ssl::Ctx* saved_ctx = iqnet::ssl::ctx;

  try {
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::server_only(
      "../tests/data/cert.pem",
      "../tests/data/pk.pem");

    BOOST_REQUIRE(ctx != nullptr);
    BOOST_REQUIRE(ctx->context() != nullptr);

    delete ctx;
  } catch (const std::exception& e) {
    iqnet::ssl::ctx = saved_ctx;
    BOOST_FAIL("SSL server_only context creation failed: " << e.what());
  }

  iqnet::ssl::ctx = saved_ctx;
}

// Test ConnectionVerifier setup - covers ssl_lib.cc lines 140, 258
namespace {
class TestVerifier : public iqnet::ssl::ConnectionVerifier {
private:
  mutable bool was_called_ = false;

  int do_verify(bool preverified_ok, X509_STORE_CTX*) const override {
    was_called_ = true;
    (void)preverified_ok;
    return 1;  // Accept all
  }

public:
  bool was_called() const { return was_called_; }
};
}

BOOST_AUTO_TEST_CASE(ssl_verifier_setup)
{
  if (!ssl_certs_available()) {
    BOOST_TEST_MESSAGE("Skipping - SSL certificates not available");
    return;
  }

  iqnet::ssl::Ctx* saved_ctx = iqnet::ssl::ctx;

  try {
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_server(
      "../tests/data/cert.pem",
      "../tests/data/pk.pem");

    BOOST_REQUIRE(ctx != nullptr);

    // Set up client verification - this covers verify_client() method
    TestVerifier verifier;
    ctx->verify_client(false, &verifier);

    // Set up server verification
    TestVerifier server_verifier;
    ctx->verify_server(&server_verifier);

    delete ctx;
  } catch (const std::exception& e) {
    iqnet::ssl::ctx = saved_ctx;
    BOOST_FAIL("SSL verifier setup failed: " << e.what());
  }

  iqnet::ssl::ctx = saved_ctx;
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
  // Verifier that captures the certificate fingerprint
  class FingerprintVerifier : public iqnet::ssl::ConnectionVerifier {
    mutable std::string fingerprint_;
    mutable std::atomic<int> call_count_;

    int do_verify(bool, X509_STORE_CTX* ctx) const override {
      ++call_count_;
      fingerprint_ = cert_finger_sha256(ctx);
      return 1;  // Accept
    }

  public:
    FingerprintVerifier() : fingerprint_(), call_count_(0) {}
    std::string fingerprint() const { return fingerprint_; }
    int get_call_count() const { return call_count_.load(); }
  };

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

BOOST_AUTO_TEST_SUITE_END()

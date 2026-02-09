//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2026 Yaroslav Gorbunov
//
//  Unit tests for per-connection SSL hostname application logic.
//  Verifies that prepare_hostname_for_connect() correctly applies SNI
//  and X509 hostname verification to the OpenSSL SSL* object without
//  requiring a full TLS handshake.

#define BOOST_TEST_MODULE ssl_hostname_test
#include <boost/test/unit_test.hpp>

#include "libiqxmlrpc/ssl_connection.h"
#include "libiqxmlrpc/ssl_lib.h"

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#ifndef _WIN32
#include <csignal>
#endif

#include "test_integration_common.h"

// Suppress SIGPIPE for tests that call ssl_connect()/ssl_accept() on
// unconnected sockets. On Linux, OpenSSL uses write() internally which
// raises SIGPIPE when the socket has no peer. macOS sets SO_NOSIGPIPE
// automatically so it's unaffected.
#ifndef _WIN32
struct SuppressSigpipe {
  SuppressSigpipe() { signal(SIGPIPE, SIG_IGN); }
};
BOOST_TEST_GLOBAL_FIXTURE(SuppressSigpipe);
#endif

using namespace iqnet;
using namespace iqxmlrpc_test;

// Test subclass exposing protected members for unit-level verification.
// In production, prepare_for_ssl_connect() and the ssl handle are protected;
// this subclass makes them accessible without modifying the production class.
class TestableConnection : public iqnet::ssl::Connection {
public:
  using Connection::Connection;

  // Expose the one-time setup that applies hostname to the SSL object
  using Connection::prepare_for_ssl_connect;

  // Expose the blocking-path methods for coverage of ssl_accept()/ssl_connect()
  using Connection::ssl_accept;
  using Connection::ssl_connect;

  // Access to the underlying SSL handle for state verification
  SSL* ssl_handle() { return ssl; }
};

// RAII guard that creates a Socket and ensures it is closed on scope exit.
// Socket() creates a valid TCP FD — no connect() needed for SSL object creation.
struct SocketGuard {
  Socket sock;
  SocketGuard() : sock() {}
  ~SocketGuard() { sock.close(); }

  SocketGuard(const SocketGuard&) = delete;
  SocketGuard& operator=(const SocketGuard&) = delete;
};

BOOST_AUTO_TEST_SUITE(ssl_hostname_unit_tests)

// Test 1: Per-connection hostname applies SNI to the SSL object.
// Core new code path — prepare_hostname_for_connect() with non-empty
// expected_hostname_ sets SSL_set_tlsext_host_name().
BOOST_AUTO_TEST_CASE(per_connection_hostname_applies_sni)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  SocketGuard sg;
  TestableConnection conn(sg.sock);

  conn.set_expected_hostname("test.example.com");
  conn.prepare_for_ssl_connect();

  const char* sni = SSL_get_servername(conn.ssl_handle(), TLSEXT_NAMETYPE_host_name);
  BOOST_REQUIRE(sni != nullptr);
  BOOST_CHECK_EQUAL(std::string(sni), "test.example.com");
}

// Test 2: Per-connection hostname applies X509_VERIFY_PARAM flags
// when hostname verification is enabled (default).
BOOST_AUTO_TEST_CASE(per_connection_hostname_applies_verify_param)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  // Verification is enabled by default (hostname_verification = true)
  SocketGuard sg;
  TestableConnection conn(sg.sock);

  conn.set_expected_hostname("verify.example.com");
  conn.prepare_for_ssl_connect();

  X509_VERIFY_PARAM* param = SSL_get0_param(conn.ssl_handle());
  BOOST_REQUIRE(param != nullptr);

  unsigned int flags = X509_VERIFY_PARAM_get_hostflags(param);
  BOOST_CHECK(flags & X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
}

// Test 3: SNI-only path when hostname verification is disabled.
// SSL_set_tlsext_host_name() is called but X509_VERIFY_PARAM is not set.
BOOST_AUTO_TEST_CASE(per_connection_hostname_sni_only_when_verification_disabled)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  ctx_guard->set_hostname_verification(false);

  SocketGuard sg;
  TestableConnection conn(sg.sock);

  conn.set_expected_hostname("sni-only.example.com");
  conn.prepare_for_ssl_connect();

  // SNI should still be set
  const char* sni = SSL_get_servername(conn.ssl_handle(), TLSEXT_NAMETYPE_host_name);
  BOOST_REQUIRE(sni != nullptr);
  BOOST_CHECK_EQUAL(std::string(sni), "sni-only.example.com");

  // Verify param flags should NOT include hostname check flags
  X509_VERIFY_PARAM* param = SSL_get0_param(conn.ssl_handle());
  BOOST_REQUIRE(param != nullptr);
  unsigned int flags = X509_VERIFY_PARAM_get_hostflags(param);
  BOOST_CHECK_EQUAL(flags, 0u);
}

// Test 4: Empty per-connection hostname falls back to Ctx-level hostname.
// Backward compatibility — legacy Ctx API still works as fallback.
BOOST_AUTO_TEST_CASE(empty_per_connection_hostname_falls_back_to_ctx)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  ctx_guard->set_expected_hostname("ctx-level.example.com");

  SocketGuard sg;
  TestableConnection conn(sg.sock);

  // Do NOT set per-connection hostname — should fall back to Ctx
  conn.prepare_for_ssl_connect();

  const char* sni = SSL_get_servername(conn.ssl_handle(), TLSEXT_NAMETYPE_host_name);
  BOOST_REQUIRE(sni != nullptr);
  BOOST_CHECK_EQUAL(std::string(sni), "ctx-level.example.com");
}

// Test 5: No hostname set anywhere — no SNI, no crash.
// Proves no crash or unexpected behavior when hostname is omitted entirely.
BOOST_AUTO_TEST_CASE(empty_hostname_everywhere_no_sni)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  // No hostname set on Ctx or connection

  SocketGuard sg;
  TestableConnection conn(sg.sock);

  conn.prepare_for_ssl_connect();

  const char* sni = SSL_get_servername(conn.ssl_handle(), TLSEXT_NAMETYPE_host_name);
  BOOST_CHECK(sni == nullptr);
}

// Test 6: hostname_verification_enabled() roundtrip through enable/disable/enable.
// Verifies the accessor added by this PR returns correct state at each step.
BOOST_AUTO_TEST_CASE(ctx_hostname_verification_enabled_roundtrip)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());

  // Default: enabled (security-first)
  BOOST_CHECK(ctx_guard->hostname_verification_enabled());

  // Disable
  ctx_guard->set_hostname_verification(false);
  BOOST_CHECK(!ctx_guard->hostname_verification_enabled());

  // Re-enable
  ctx_guard->set_hostname_verification(true);
  BOOST_CHECK(ctx_guard->hostname_verification_enabled());
}

// Test 7: Blocking ssl_connect() calls prepare_for_ssl_connect() before handshake.
// Covers the blocking code path (ssl_connection.cc:97) used by non-reactor clients.
// The reactor path goes through reg_connect() instead; this exercises the alternative.
BOOST_AUTO_TEST_CASE(blocking_ssl_connect_calls_prepare)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  SocketGuard sg;
  TestableConnection conn(sg.sock);

  conn.set_expected_hostname("blocking.example.com");

  // ssl_connect() calls prepare_for_ssl_connect() then SSL_connect().
  // SSL_connect() fails (no peer connected), but prepare runs first.
  BOOST_CHECK_THROW(conn.ssl_connect(), std::exception);

  // Verify hostname was applied — proves prepare_for_ssl_connect() ran
  const char* sni = SSL_get_servername(conn.ssl_handle(), TLSEXT_NAMETYPE_host_name);
  BOOST_REQUIRE(sni != nullptr);
  BOOST_CHECK_EQUAL(std::string(sni), "blocking.example.com");
}

// Test 8: Blocking ssl_accept() calls prepare_for_ssl_accept() before handshake.
// Covers the blocking code path (ssl_connection.cc:64) used by non-reactor servers.
BOOST_AUTO_TEST_CASE(blocking_ssl_accept_calls_prepare)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  SocketGuard sg;
  TestableConnection conn(sg.sock);

  // ssl_accept() calls prepare_for_ssl_accept() then SSL_accept().
  // SSL_accept() fails (no peer connected), but prepare runs first.
  BOOST_CHECK_THROW(conn.ssl_accept(), std::exception);
}

BOOST_AUTO_TEST_SUITE_END()

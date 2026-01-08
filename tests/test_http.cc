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
    // Empty first line (after split) results in Method_not_allowed since it's not "POST"
    std::string raw_header = "\r\ncontent-length: 0";
    BOOST_CHECK_THROW(Request_header(HTTP_CHECK_WEAK, raw_header), Method_not_allowed);
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

// vim:ts=2:sw=2:et

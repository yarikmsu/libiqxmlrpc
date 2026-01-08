#define BOOST_TEST_MODULE request_response_test
#include <memory>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/request.h"
#include "libiqxmlrpc/response.h"

using namespace boost::unit_test;
using namespace iqxmlrpc;

BOOST_AUTO_TEST_SUITE(request_tests)

BOOST_AUTO_TEST_CASE(request_construction)
{
    Param_list params;
    params.push_back(Value(42));
    params.push_back(Value("hello"));

    Request req("test.method", params);
    BOOST_CHECK_EQUAL(req.get_name(), "test.method");
    BOOST_CHECK_EQUAL(req.get_params().size(), 2u);
}

BOOST_AUTO_TEST_CASE(request_empty_params)
{
    Param_list params;
    Request req("noargs.method", params);
    BOOST_CHECK_EQUAL(req.get_name(), "noargs.method");
    BOOST_CHECK(req.get_params().empty());
}

BOOST_AUTO_TEST_CASE(request_dump_simple)
{
    Param_list params;
    params.push_back(Value(42));
    Request req("test.method", params);

    std::string xml = dump_request(req);
    BOOST_CHECK(xml.find("methodCall") != std::string::npos);
    BOOST_CHECK(xml.find("methodName") != std::string::npos);
    BOOST_CHECK(xml.find("test.method") != std::string::npos);
    BOOST_CHECK(xml.find("<i4>42</i4>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(request_dump_multiple_params)
{
    Param_list params;
    params.push_back(Value(1));
    params.push_back(Value(2));
    params.push_back(Value(3));
    Request req("sum", params);

    std::string xml = dump_request(req);
    BOOST_CHECK(xml.find("<i4>1</i4>") != std::string::npos);
    BOOST_CHECK(xml.find("<i4>2</i4>") != std::string::npos);
    BOOST_CHECK(xml.find("<i4>3</i4>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(request_parse_simple)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<methodName>test.method</methodName>"
        "<params>"
        "<param><value><i4>42</i4></value></param>"
        "</params>"
        "</methodCall>";

    std::unique_ptr<Request> req(parse_request(xml));
    BOOST_REQUIRE(req != nullptr);
    BOOST_CHECK_EQUAL(req->get_name(), "test.method");
    BOOST_CHECK_EQUAL(req->get_params().size(), 1u);
    BOOST_CHECK_EQUAL(req->get_params()[0].get_int(), 42);
}

BOOST_AUTO_TEST_CASE(request_parse_multiple_params)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<methodName>add</methodName>"
        "<params>"
        "<param><value><i4>5</i4></value></param>"
        "<param><value><i4>3</i4></value></param>"
        "</params>"
        "</methodCall>";

    std::unique_ptr<Request> req(parse_request(xml));
    BOOST_REQUIRE(req != nullptr);
    BOOST_CHECK_EQUAL(req->get_name(), "add");
    BOOST_CHECK_EQUAL(req->get_params().size(), 2u);
    BOOST_CHECK_EQUAL(req->get_params()[0].get_int(), 5);
    BOOST_CHECK_EQUAL(req->get_params()[1].get_int(), 3);
}

BOOST_AUTO_TEST_CASE(request_parse_string_param)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<methodName>greet</methodName>"
        "<params>"
        "<param><value><string>Hello World</string></value></param>"
        "</params>"
        "</methodCall>";

    std::unique_ptr<Request> req(parse_request(xml));
    BOOST_REQUIRE(req != nullptr);
    BOOST_CHECK_EQUAL(req->get_params()[0].get_string(), "Hello World");
}

BOOST_AUTO_TEST_CASE(request_parse_no_params)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<methodName>noargs</methodName>"
        "<params></params>"
        "</methodCall>";

    std::unique_ptr<Request> req(parse_request(xml));
    BOOST_REQUIRE(req != nullptr);
    BOOST_CHECK_EQUAL(req->get_name(), "noargs");
    BOOST_CHECK(req->get_params().empty());
}

BOOST_AUTO_TEST_CASE(request_roundtrip)
{
    Param_list params;
    params.push_back(Value(123));
    params.push_back(Value("test"));
    params.push_back(Value(true));
    Request orig("roundtrip.test", params);

    std::string xml = dump_request(orig);
    std::unique_ptr<Request> parsed(parse_request(xml));

    BOOST_CHECK_EQUAL(parsed->get_name(), orig.get_name());
    BOOST_CHECK_EQUAL(parsed->get_params().size(), orig.get_params().size());
    BOOST_CHECK_EQUAL(parsed->get_params()[0].get_int(), 123);
    BOOST_CHECK_EQUAL(parsed->get_params()[1].get_string(), "test");
    BOOST_CHECK_EQUAL(parsed->get_params()[2].get_bool(), true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(response_tests)

BOOST_AUTO_TEST_CASE(response_value_construction)
{
    Response resp(new Value(42));
    BOOST_CHECK(!resp.is_fault());
    BOOST_CHECK_EQUAL(resp.value().get_int(), 42);
}

BOOST_AUTO_TEST_CASE(response_fault_construction)
{
    Response resp(100, "Test error");
    BOOST_CHECK(resp.is_fault());
    BOOST_CHECK_EQUAL(resp.fault_code(), 100);
    BOOST_CHECK_EQUAL(resp.fault_string(), "Test error");
}

BOOST_AUTO_TEST_CASE(response_fault_value_throws)
{
    Response resp(100, "Test error");
    BOOST_CHECK_THROW(resp.value(), Exception);
}

BOOST_AUTO_TEST_CASE(response_dump_value)
{
    Response resp(new Value("result string"));
    std::string xml = dump_response(resp);

    BOOST_CHECK(xml.find("methodResponse") != std::string::npos);
    BOOST_CHECK(xml.find("params") != std::string::npos);
    BOOST_CHECK(xml.find("result string") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(response_dump_fault)
{
    Response resp(404, "Not found");
    std::string xml = dump_response(resp);

    BOOST_CHECK(xml.find("methodResponse") != std::string::npos);
    BOOST_CHECK(xml.find("fault") != std::string::npos);
    BOOST_CHECK(xml.find("faultCode") != std::string::npos);
    BOOST_CHECK(xml.find("faultString") != std::string::npos);
    BOOST_CHECK(xml.find("404") != std::string::npos);
    BOOST_CHECK(xml.find("Not found") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(response_parse_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><string>Success</string></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(!resp.is_fault());
    BOOST_CHECK_EQUAL(resp.value().get_string(), "Success");
}

BOOST_AUTO_TEST_CASE(response_parse_int_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><int>999</int></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(!resp.is_fault());
    BOOST_CHECK_EQUAL(resp.value().get_int(), 999);
}

BOOST_AUTO_TEST_CASE(response_parse_fault)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<fault>"
        "<value>"
        "<struct>"
        "<member><name>faultCode</name><value><int>500</int></value></member>"
        "<member><name>faultString</name><value><string>Server error</string></value></member>"
        "</struct>"
        "</value>"
        "</fault>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.is_fault());
    BOOST_CHECK_EQUAL(resp.fault_code(), 500);
    BOOST_CHECK_EQUAL(resp.fault_string(), "Server error");
}

BOOST_AUTO_TEST_CASE(response_roundtrip_value)
{
    Response orig(new Value(12345));
    std::string xml = dump_response(orig);
    Response parsed = parse_response(xml);

    BOOST_CHECK(!parsed.is_fault());
    BOOST_CHECK_EQUAL(parsed.value().get_int(), 12345);
}

BOOST_AUTO_TEST_CASE(response_roundtrip_fault)
{
    Response orig(999, "Roundtrip error");
    std::string xml = dump_response(orig);
    Response parsed = parse_response(xml);

    BOOST_CHECK(parsed.is_fault());
    BOOST_CHECK_EQUAL(parsed.fault_code(), 999);
    BOOST_CHECK_EQUAL(parsed.fault_string(), "Roundtrip error");
}

BOOST_AUTO_TEST_CASE(response_complex_value)
{
    Array arr;
    arr.push_back(Value(1));
    arr.push_back(Value(2));
    arr.push_back(Value(3));

    Response resp(new Value(arr));
    std::string xml = dump_response(resp);
    Response parsed = parse_response(xml);

    BOOST_CHECK(!parsed.is_fault());
    BOOST_CHECK(parsed.value().is_array());
    BOOST_CHECK_EQUAL(parsed.value().size(), 3u);
}

BOOST_AUTO_TEST_CASE(response_struct_value)
{
    Struct s;
    s.insert("name", Value("test"));
    s.insert("value", Value(42));

    Response resp(new Value(s));
    std::string xml = dump_response(resp);
    Response parsed = parse_response(xml);

    BOOST_CHECK(!parsed.is_fault());
    BOOST_CHECK(parsed.value().is_struct());
    BOOST_CHECK_EQUAL(parsed.value()["name"].get_string(), "test");
    BOOST_CHECK_EQUAL(parsed.value()["value"].get_int(), 42);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(value_type_tests)

BOOST_AUTO_TEST_CASE(parse_double_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><double>3.14159</double></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_double());
    BOOST_CHECK_CLOSE(resp.value().get_double(), 3.14159, 0.0001);
}

BOOST_AUTO_TEST_CASE(parse_bool_value_true)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><boolean>1</boolean></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_bool());
    BOOST_CHECK_EQUAL(resp.value().get_bool(), true);
}

BOOST_AUTO_TEST_CASE(parse_bool_value_false)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><boolean>0</boolean></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_bool());
    BOOST_CHECK_EQUAL(resp.value().get_bool(), false);
}

BOOST_AUTO_TEST_CASE(parse_int64_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><i8>9223372036854775807</i8></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_int64());
    BOOST_CHECK_EQUAL(resp.value().get_int64(), 9223372036854775807LL);
}

BOOST_AUTO_TEST_CASE(parse_nested_array)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value>"
        "<array><data>"
        "<value><array><data>"
        "<value><i4>1</i4></value>"
        "<value><i4>2</i4></value>"
        "</data></array></value>"
        "<value><array><data>"
        "<value><i4>3</i4></value>"
        "<value><i4>4</i4></value>"
        "</data></array></value>"
        "</data></array>"
        "</value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_array());
    BOOST_CHECK_EQUAL(resp.value().size(), 2u);
    BOOST_CHECK(resp.value()[0].is_array());
    BOOST_CHECK_EQUAL(resp.value()[0][0].get_int(), 1);
}

BOOST_AUTO_TEST_CASE(parse_nested_struct)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value>"
        "<struct>"
        "<member><name>outer</name><value>"
        "<struct>"
        "<member><name>inner</name><value><string>nested</string></value></member>"
        "</struct>"
        "</value></member>"
        "</struct>"
        "</value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_struct());
    BOOST_CHECK(resp.value()["outer"].is_struct());
    BOOST_CHECK_EQUAL(resp.value()["outer"]["inner"].get_string(), "nested");
}

BOOST_AUTO_TEST_CASE(parse_nil_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><nil/></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_nil());
}

BOOST_AUTO_TEST_CASE(dump_nil_value)
{
    Response resp(new Value(Nil()));
    std::string xml = dump_response(resp);
    BOOST_CHECK(xml.find("<nil/>") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et

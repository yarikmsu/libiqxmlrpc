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

BOOST_AUTO_TEST_CASE(response_nullptr_construction_is_fault)
{
    // Response with nullptr is treated as fault (is_fault() returns true)
    Response resp(nullptr);

    BOOST_CHECK(resp.is_fault());
    BOOST_CHECK_EQUAL(resp.fault_code(), 0);
    // Attempting to get value should throw
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

BOOST_AUTO_TEST_SUITE(namespace_prefix_tests)

BOOST_AUTO_TEST_CASE(parse_response_with_namespace_prefix)
{
    // Test that namespace prefixes are correctly stripped from tag names
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<ns:methodResponse xmlns:ns=\"http://example.com/ns/\">"
        "<ns:params>"
        "<ns:param><ns:value><ns:int>42</ns:int></ns:value></ns:param>"
        "</ns:params>"
        "</ns:methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(!resp.is_fault());
    BOOST_CHECK(resp.value().is_int());
    BOOST_CHECK_EQUAL(resp.value().get_int(), 42);
}

BOOST_AUTO_TEST_CASE(parse_response_with_mixed_namespace)
{
    // Mix of prefixed and non-prefixed elements
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse xmlns:ns=\"http://example.com/ns/\">"
        "<params>"
        "<param><value><ns:string>hello</ns:string></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(!resp.is_fault());
    BOOST_CHECK(resp.value().is_string());
    BOOST_CHECK_EQUAL(resp.value().get_string(), "hello");
}

BOOST_AUTO_TEST_CASE(parse_request_with_namespace_prefix)
{
    // Test request parsing with namespace prefix
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<ns:methodCall xmlns:ns=\"http://example.com/ns/\">"
        "<ns:methodName>test.method</ns:methodName>"
        "<ns:params>"
        "<ns:param><ns:value><ns:i4>99</ns:i4></ns:value></ns:param>"
        "</ns:params>"
        "</ns:methodCall>";

    std::unique_ptr<Request> req(parse_request(xml));
    BOOST_REQUIRE(req != nullptr);
    BOOST_CHECK_EQUAL(req->get_name(), "test.method");
    BOOST_CHECK_EQUAL(req->get_params().size(), 1u);
    BOOST_CHECK_EQUAL(req->get_params()[0].get_int(), 99);
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

BOOST_AUTO_TEST_SUITE(xml_parsing_edge_cases)

BOOST_AUTO_TEST_CASE(parse_empty_string_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><string></string></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_string());
    BOOST_CHECK(resp.value().get_string().empty());
}

BOOST_AUTO_TEST_CASE(parse_value_without_type_tag)
{
    // Value without type tag should be treated as string
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value>implicit string</value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_string());
    BOOST_CHECK_EQUAL(resp.value().get_string(), "implicit string");
}

BOOST_AUTO_TEST_CASE(parse_base64_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><base64>SGVsbG8gV29ybGQ=</base64></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_binary());
}

BOOST_AUTO_TEST_CASE(parse_datetime_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><dateTime.iso8601>20260101T12:00:00</dateTime.iso8601></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_datetime());
}

BOOST_AUTO_TEST_CASE(parse_request_without_params_element)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<methodName>no.params</methodName>"
        "</methodCall>";

    std::unique_ptr<Request> req(parse_request(xml));
    BOOST_REQUIRE(req != nullptr);
    BOOST_CHECK_EQUAL(req->get_name(), "no.params");
    BOOST_CHECK(req->get_params().empty());
}

BOOST_AUTO_TEST_CASE(parse_empty_array)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><array><data></data></array></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_array());
    BOOST_CHECK_EQUAL(resp.value().size(), 0u);
}

BOOST_AUTO_TEST_CASE(parse_struct_with_member)
{
    // Test struct parsing using dump/parse roundtrip
    Struct s;
    s.insert("key", Value("val"));
    Response orig(new Value(s));

    std::string xml = dump_response(orig);
    Response parsed = parse_response(xml);

    BOOST_CHECK(!parsed.is_fault());
    BOOST_CHECK(parsed.value().is_struct());
    BOOST_CHECK_EQUAL(parsed.value()["key"].get_string(), "val");
}

BOOST_AUTO_TEST_CASE(parse_negative_int)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><i4>-12345</i4></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_int());
    BOOST_CHECK_EQUAL(resp.value().get_int(), -12345);
}

BOOST_AUTO_TEST_CASE(parse_negative_double)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><double>-123.456</double></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_double());
    BOOST_CHECK_CLOSE(resp.value().get_double(), -123.456, 0.001);
}

BOOST_AUTO_TEST_CASE(parse_special_chars_in_string)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><string>&lt;test&gt; &amp; \"quoted\"</string></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_string());
    BOOST_CHECK_EQUAL(resp.value().get_string(), "<test> & \"quoted\"");
}

BOOST_AUTO_TEST_CASE(dump_special_chars_in_string)
{
    Param_list params;
    params.push_back(Value("<test> & \"quoted\""));
    Request req("test", params);
    std::string xml = dump_request(req);

    // Should be XML-escaped
    BOOST_CHECK(xml.find("&lt;") != std::string::npos);
    BOOST_CHECK(xml.find("&gt;") != std::string::npos);
    BOOST_CHECK(xml.find("&amp;") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(parse_mixed_array)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse>"
        "<params>"
        "<param><value><array><data>"
        "<value><i4>1</i4></value>"
        "<value><string>two</string></value>"
        "<value><boolean>1</boolean></value>"
        "<value><double>4.0</double></value>"
        "</data></array></value></param>"
        "</params>"
        "</methodResponse>";

    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_array());
    BOOST_CHECK_EQUAL(resp.value().size(), 4u);
    BOOST_CHECK(resp.value()[0].is_int());
    BOOST_CHECK(resp.value()[1].is_string());
    BOOST_CHECK(resp.value()[2].is_bool());
    BOOST_CHECK(resp.value()[3].is_double());
}

BOOST_AUTO_TEST_CASE(fault_with_negative_code)
{
    Response resp(-32600, "Invalid Request");
    std::string xml = dump_response(resp);
    Response parsed = parse_response(xml);

    BOOST_CHECK(parsed.is_fault());
    BOOST_CHECK_EQUAL(parsed.fault_code(), -32600);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(server_mode_serialization)

BOOST_AUTO_TEST_CASE(response_string_without_omit_tag)
{
    // Default: omit_string_tag_in_responses is false
    bool original = Value::omit_string_tag_in_responses();
    Value::omit_string_tag_in_responses(false);

    Response resp(new Value("test string"));
    std::string xml = dump_response(resp);

    // Should have <string> tag
    BOOST_CHECK(xml.find("<string>") != std::string::npos);
    BOOST_CHECK(xml.find("</string>") != std::string::npos);

    Value::omit_string_tag_in_responses(original);
}

BOOST_AUTO_TEST_CASE(response_string_with_omit_tag)
{
    // When omit_string_tag_in_responses is true, server responses omit <string> tag
    bool original = Value::omit_string_tag_in_responses();
    Value::omit_string_tag_in_responses(true);

    Response resp(new Value("test string"));
    std::string xml = dump_response(resp);

    // Should NOT have <string> tag (just raw text in <value>)
    BOOST_CHECK(xml.find("<string>") == std::string::npos);
    BOOST_CHECK(xml.find("</string>") == std::string::npos);
    // But should still have the string content within <value>
    BOOST_CHECK(xml.find("test string") != std::string::npos);

    Value::omit_string_tag_in_responses(original);
}

BOOST_AUTO_TEST_CASE(response_string_with_omit_tag_roundtrip)
{
    // Verify that strings without <string> tag can be parsed
    bool original = Value::omit_string_tag_in_responses();
    Value::omit_string_tag_in_responses(true);

    Response resp(new Value("roundtrip test"));
    std::string xml = dump_response(resp);
    Response parsed = parse_response(xml);

    BOOST_CHECK(!parsed.is_fault());
    BOOST_CHECK(parsed.value().is_string());
    BOOST_CHECK_EQUAL(parsed.value().get_string(), "roundtrip test");

    Value::omit_string_tag_in_responses(original);
}

BOOST_AUTO_TEST_CASE(response_struct_with_string_omit_tag)
{
    // Struct members should also respect omit_string_tag in server mode
    bool original = Value::omit_string_tag_in_responses();
    Value::omit_string_tag_in_responses(true);

    Struct s;
    s.insert("key", Value("value"));
    Response resp(new Value(s));
    std::string xml = dump_response(resp);

    // String values in struct should not have <string> tag
    BOOST_CHECK(xml.find("<string>") == std::string::npos);
    BOOST_CHECK(xml.find("value") != std::string::npos);

    Value::omit_string_tag_in_responses(original);
}

BOOST_AUTO_TEST_CASE(response_array_with_string_omit_tag)
{
    // Array elements should also respect omit_string_tag in server mode
    bool original = Value::omit_string_tag_in_responses();
    Value::omit_string_tag_in_responses(true);

    Array arr;
    arr.push_back(Value("first"));
    arr.push_back(Value("second"));
    Response resp(new Value(arr));
    std::string xml = dump_response(resp);

    // String values in array should not have <string> tag
    BOOST_CHECK(xml.find("<string>") == std::string::npos);
    BOOST_CHECK(xml.find("first") != std::string::npos);
    BOOST_CHECK(xml.find("second") != std::string::npos);

    Value::omit_string_tag_in_responses(original);
}

BOOST_AUTO_TEST_CASE(response_fault_with_string_omit_tag)
{
    // Fault strings should also respect omit_string_tag
    bool original = Value::omit_string_tag_in_responses();
    Value::omit_string_tag_in_responses(true);

    Response resp(500, "Server error message");
    std::string xml = dump_response(resp);

    // Fault string should not have <string> tag
    BOOST_CHECK(xml.find("<string>") == std::string::npos);
    BOOST_CHECK(xml.find("Server error message") != std::string::npos);

    Value::omit_string_tag_in_responses(original);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(parser_state_machine_tests)

BOOST_AUTO_TEST_CASE(parse_empty_int_with_default)
{
    Value::set_default_int(42);
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value><i4></i4></value></param></params></methodResponse>";
    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_int());
    BOOST_CHECK_EQUAL(resp.value().get_int(), 42);
    Value::drop_default_int();
}

BOOST_AUTO_TEST_CASE(parse_empty_int64_with_default)
{
    Value::set_default_int64(9876543210LL);
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value><i8></i8></value></param></params></methodResponse>";
    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_int64());
    BOOST_CHECK_EQUAL(resp.value().get_int64(), 9876543210LL);
    Value::drop_default_int64();
}

BOOST_AUTO_TEST_CASE(parse_empty_int_without_default_throws)
{
    Value::drop_default_int();
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value><i4></i4></value></param></params></methodResponse>";
    BOOST_CHECK_THROW(parse_response(xml), XML_RPC_violation);
}

BOOST_AUTO_TEST_CASE(parse_empty_int64_without_default_throws)
{
    Value::drop_default_int64();
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value><i8></i8></value></param></params></methodResponse>";
    BOOST_CHECK_THROW(parse_response(xml), XML_RPC_violation);
}

BOOST_AUTO_TEST_CASE(parse_empty_binary_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value><base64></base64></value></param></params></methodResponse>";
    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_binary());
    BOOST_CHECK(resp.value().get_binary().get_data().empty());
}

BOOST_AUTO_TEST_CASE(parse_empty_value_tag)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value></value></param></params></methodResponse>";
    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_string());
    BOOST_CHECK(resp.value().get_string().empty());
}

BOOST_AUTO_TEST_CASE(parse_struct_with_empty_value_member)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value>"
        "<struct><member><name>key</name><value></value></member></struct>"
        "</value></param></params></methodResponse>";
    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_struct());
    BOOST_CHECK(resp.value()["key"].is_string());
    BOOST_CHECK(resp.value()["key"].get_string().empty());
}

BOOST_AUTO_TEST_CASE(parse_array_with_empty_value)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value>"
        "<array><data><value></value><value><i4>42</i4></value></data></array>"
        "</value></param></params></methodResponse>";
    Response resp = parse_response(xml);
    BOOST_CHECK(resp.value().is_array());
    BOOST_CHECK_EQUAL(resp.value().size(), 2u);
    BOOST_CHECK(resp.value()[0].is_string());
    BOOST_CHECK_EQUAL(resp.value()[1].get_int(), 42);
}

BOOST_AUTO_TEST_CASE(parse_deeply_nested_struct)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value>"
        "<struct><member><name>l1</name><value>"
        "<struct><member><name>l2</name><value>"
        "<struct><member><name>l3</name><value><string>deep</string></value></member></struct>"
        "</value></member></struct>"
        "</value></member></struct>"
        "</value></param></params></methodResponse>";
    Response resp = parse_response(xml);
    BOOST_CHECK_EQUAL(resp.value()["l1"]["l2"]["l3"].get_string(), "deep");
}

BOOST_AUTO_TEST_CASE(parse_struct_multiple_members)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value>"
        "<struct>"
        "<member><name>a</name><value><i4>1</i4></value></member>"
        "<member><name>b</name><value><i4>2</i4></value></member>"
        "<member><name>c</name><value><i4>3</i4></value></member>"
        "</struct>"
        "</value></param></params></methodResponse>";
    Response resp = parse_response(xml);
    BOOST_CHECK_EQUAL(resp.value()["a"].get_int(), 1);
    BOOST_CHECK_EQUAL(resp.value()["b"].get_int(), 2);
    BOOST_CHECK_EQUAL(resp.value()["c"].get_int(), 3);
}

BOOST_AUTO_TEST_CASE(dump_request_with_all_types)
{
    Param_list params;
    params.push_back(Value(42));
    params.push_back(Value(static_cast<int64_t>(123456789012LL)));
    params.push_back(Value(3.14));
    params.push_back(Value(true));
    params.push_back(Value("test"));
    params.push_back(Value(Nil()));
    Request req("test.allTypes", params);
    std::string xml = dump_request(req);
    BOOST_CHECK(xml.find("<i4>42</i4>") != std::string::npos);
    BOOST_CHECK(xml.find("<i8>123456789012</i8>") != std::string::npos);
    BOOST_CHECK(xml.find("<boolean>1</boolean>") != std::string::npos);
    BOOST_CHECK(xml.find("<nil/>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(dump_request_with_nested_struct)
{
    Struct inner;
    inner.insert("nk", Value("nv"));
    Struct outer;
    outer.insert("inner", Value(inner));
    Param_list params;
    params.push_back(Value(outer));
    Request req("test.nested", params);
    std::string xml = dump_request(req);
    BOOST_CHECK(xml.find("<name>inner</name>") != std::string::npos);
    BOOST_CHECK(xml.find("<name>nk</name>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(dump_request_with_binary)
{
    std::unique_ptr<Binary_data> bin(Binary_data::from_data("bin"));
    Param_list params;
    params.push_back(Value(*bin));
    Request req("test.bin", params);
    std::string xml = dump_request(req);
    BOOST_CHECK(xml.find("<base64>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(dump_request_with_datetime)
{
    Date_time dt(std::string("20260108T12:30:45"));
    Param_list params;
    params.push_back(Value(dt));
    Request req("test.dt", params);
    std::string xml = dump_request(req);
    BOOST_CHECK(xml.find("<dateTime.iso8601>20260108T12:30:45</dateTime.iso8601>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(parse_empty_boolean_throws)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value><boolean></boolean></value></param></params></methodResponse>";
    BOOST_CHECK_THROW(parse_response(xml), XML_RPC_violation);
}

BOOST_AUTO_TEST_CASE(parse_empty_double_throws)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value><double></double></value></param></params></methodResponse>";
    BOOST_CHECK_THROW(parse_response(xml), XML_RPC_violation);
}

BOOST_AUTO_TEST_CASE(parse_empty_datetime_throws)
{
    // Empty datetime causes XML_RPC_violation at parsing level
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodResponse><params><param><value><dateTime.iso8601></dateTime.iso8601></value></param></params></methodResponse>";
    BOOST_CHECK_THROW(parse_response(xml), XML_RPC_violation);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(error_handling_tests)

BOOST_AUTO_TEST_CASE(parse_malformed_xml_throws)
{
    std::string xml = "not valid xml at all";
    BOOST_CHECK_THROW(parse_response(xml), Parse_error);
}

BOOST_AUTO_TEST_CASE(parse_missing_method_name_throws)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<params></params>"
        "</methodCall>";

    BOOST_CHECK_THROW(parse_request(xml), XML_RPC_violation);
}

BOOST_AUTO_TEST_CASE(parse_empty_method_name_accepted)
{
    // Empty method name is accepted by the parser (validation happens at dispatch)
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<methodName></methodName>"
        "</methodCall>";

    std::unique_ptr<Request> req(parse_request(xml));
    BOOST_REQUIRE(req != nullptr);
    BOOST_CHECK(req->get_name().empty());
}

BOOST_AUTO_TEST_CASE(parse_invalid_root_element_throws)
{
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<invalid>content</invalid>";

    BOOST_CHECK_THROW(parse_request(xml), XML_RPC_violation);
    BOOST_CHECK_THROW(parse_response(xml), XML_RPC_violation);
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et

# HTTP Wire Protocol Compatibility Testing Plan

## Executive Summary

This plan ensures HTTP wire protocol compatibility between the production version (pre-PR#1) and the current version. **ABI compatibility is not required** - client code will be recompiled.

### Risk Assessment

| Change | Wire Impact | Risk |
|--------|-------------|------|
| `std::to_chars` for numbers | None - same output | **Low** |
| `strftime` for HTTP Date | None - same RFC 1123 format | **Low** |
| Base64 lookup table | None - decoding only | **Low** |
| TCP_NODELAY enabled | Network behavior only | **Low** |
| Struct `unordered_map` | Member order varies | **Low** (spec allows any order) |

**Conclusion**: All wire protocol changes are low-risk. No breaking changes identified.

---

## P0: Production Server Integration (HIGHEST PRIORITY)

### Test Against Actual Production Server

This is the most critical test. All other tests are secondary.

```bash
# Test current client against production server
./client_test --server production.example.com:8080 --method system.listMethods
./client_test --server production.example.com:8080 --method test.echo --param "hello"
./client_test --server production.example.com:8080 --method test.add --param 1 --param 2
```

### Checklist
- [ ] Connect to production server
- [ ] Call `system.listMethods` successfully
- [ ] Echo string values
- [ ] Echo integer values
- [ ] Echo struct values
- [ ] Echo array values
- [ ] Handle fault responses correctly
- [ ] Verify timeout behavior matches production

---

## P1: Third-Party XML-RPC Server Compatibility (HIGH PRIORITY)

### Python xmlrpc.server Test

Python's XML-RPC is widely used. Testing against it validates spec compliance.

**Test Server** (`tests/interop/python_xmlrpc_server.py`):
```python
#!/usr/bin/env python3
"""
Test libiqxmlrpc client against Python XML-RPC server.

Usage:
    1. Start server: python3 python_xmlrpc_server.py
    2. Run tests: ./client_test --server localhost:8000
"""
from xmlrpc.server import SimpleXMLRPCServer

def echo(s):
    return s

def echo_int(i):
    return i

def echo_double(d):
    return d

def echo_bool(b):
    return b

def echo_struct(s):
    return s

def echo_array(a):
    return a

def add(a, b):
    return a + b

def get_all_types():
    return {
        "int": 42,
        "bool": True,
        "double": 3.14159,
        "string": "hello",
        "array": [1, 2, 3],
        "struct": {"nested": "value"}
    }

if __name__ == "__main__":
    server = SimpleXMLRPCServer(("localhost", 8000), allow_none=True)
    server.register_function(echo)
    server.register_function(echo_int)
    server.register_function(echo_double)
    server.register_function(echo_bool)
    server.register_function(echo_struct)
    server.register_function(echo_array)
    server.register_function(add)
    server.register_function(get_all_types)
    print("Python XML-RPC server running on localhost:8000")
    server.serve_forever()
```

**Run Tests**:
```bash
# Start Python server
python3 tests/interop/python_xmlrpc_server.py &

# Test all value types
./client_test --server localhost:8000 --method echo --param "test"
./client_test --server localhost:8000 --method echo_int --param 42
./client_test --server localhost:8000 --method echo_double --param 3.14159
./client_test --server localhost:8000 --method add --param 1 --param 2
./client_test --server localhost:8000 --method get_all_types
```

### Struct Member Order Verification

The [XML-RPC specification](https://github.com/scripting/xml-rpc/blob/master/spec.md) explicitly states:

> *"The struct element does not preserve the order of the keys. The two structs are equivalent."*

All major implementations use unordered data structures:
- **Python** `xmlrpc.client`: Uses `dict`
- **Java** Apache XML-RPC: Uses `HashMap`
- **Java** Redstone XML-RPC: Uses `HashMap`

**This is a non-issue** for any spec-compliant implementation.

---

## P2: XML-RPC Value Type Round-Trip (MEDIUM PRIORITY)

### Double Precision Formatting

`std::to_chars` may produce different string representations than `boost::lexical_cast`, but parsed values must be equivalent.

```cpp
BOOST_AUTO_TEST_CASE(double_format_compatibility) {
    std::vector<double> test_values = {
        0.0, -0.0, 1.0, -1.0,
        3.14159265358979,
        1e10, 1e-10,
        std::numeric_limits<double>::min(),
        std::numeric_limits<double>::max()
    };

    for (double d : test_values) {
        Value v(d);
        std::string xml = v.dump_xml();

        // Parse back and compare VALUE, not string
        auto parsed = parse_value(xml);
        BOOST_CHECK_CLOSE(parsed.get_double(), d, 1e-10);
    }
}
```

### Edge Case Values

```cpp
BOOST_AUTO_TEST_CASE(edge_case_values) {
    // Empty string
    Value v1("");
    auto xml1 = v1.dump_xml();
    BOOST_CHECK(parse_value(xml1).get_string().empty());

    // XML special characters (must be escaped)
    Value v2("<test>&\"'</test>");
    auto xml2 = v2.dump_xml();
    BOOST_CHECK_EQUAL(parse_value(xml2).get_string(), "<test>&\"'</test>");

    // Unicode/UTF-8
    Value v3("日本語テスト");
    auto xml3 = v3.dump_xml();
    BOOST_CHECK_EQUAL(parse_value(xml3).get_string(), "日本語テスト");

    // Empty array
    Array arr;
    Value v4(arr);
    BOOST_CHECK_EQUAL(parse_value(v4.dump_xml()).size(), 0);

    // Empty struct
    Struct s;
    Value v5(s);
    BOOST_CHECK_EQUAL(parse_value(v5.dump_xml()).size(), 0);

    // Binary with null bytes
    std::string binary_data("\x00\x01\x02\x03", 4);
    Binary bin(binary_data);
    Value v6(bin);
    BOOST_CHECK_EQUAL(parse_value(v6.dump_xml()).get_binary().get_data(), binary_data);

    // INT_MIN, INT_MAX
    Value v7(std::numeric_limits<int>::min());
    Value v8(std::numeric_limits<int>::max());
    BOOST_CHECK_EQUAL(parse_value(v7.dump_xml()).get_int(), std::numeric_limits<int>::min());
    BOOST_CHECK_EQUAL(parse_value(v8.dump_xml()).get_int(), std::numeric_limits<int>::max());
}
```

### All Value Types Round-Trip

```cpp
BOOST_AUTO_TEST_CASE(value_type_round_trip) {
    // int/i4
    Value v_int(42);
    BOOST_CHECK_EQUAL(parse_value(v_int.dump_xml()).get_int(), 42);

    // boolean
    Value v_bool(true);
    BOOST_CHECK_EQUAL(parse_value(v_bool.dump_xml()).get_bool(), true);

    // string
    Value v_str("hello world");
    BOOST_CHECK_EQUAL(parse_value(v_str.dump_xml()).get_string(), "hello world");

    // double
    Value v_dbl(3.14159);
    BOOST_CHECK_CLOSE(parse_value(v_dbl.dump_xml()).get_double(), 3.14159, 0.00001);

    // dateTime.iso8601
    Date_time dt("20260110T15:30:45");
    Value v_dt(dt);
    // Verify round-trip

    // base64
    Binary bin("Hello, World!");
    Value v_bin(bin);
    BOOST_CHECK(v_bin.dump_xml().find("SGVsbG8sIFdvcmxkIQ==") != std::string::npos);

    // struct
    Struct s;
    s["key"] = Value("value");
    Value v_struct(s);
    BOOST_CHECK_EQUAL(parse_value(v_struct.dump_xml())["key"].get_string(), "value");

    // array
    Array arr;
    arr.push_back(Value(1));
    arr.push_back(Value(2));
    Value v_arr(arr);
    BOOST_CHECK_EQUAL(parse_value(v_arr.dump_xml())[0].get_int(), 1);

    // nil
    Value v_nil;
    // Verify nil handling
}
```

---

## P3: HTTP Protocol Details (LOW PRIORITY)

### Response Parsing with Existing Test Data

```cpp
BOOST_AUTO_TEST_CASE(parse_production_responses) {
    // These files represent production-era message formats
    std::vector<std::string> test_files = {
        "tests/data/response.xml",
        "tests/data/response_fault.xml",
        "tests/data/value.xml"
    };

    for (const auto& file : test_files) {
        std::string xml = read_file(file);
        BOOST_CHECK_NO_THROW(parse_response(xml));
    }
}
```

### HTTP Header Case Insensitivity

```cpp
BOOST_AUTO_TEST_CASE(http_header_case_insensitive) {
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "content-type: text/xml\r\n"      // lowercase
        "CONTENT-LENGTH: 100\r\n"          // uppercase
        "Connection: Keep-Alive\r\n"       // mixed case
        "\r\n";

    http::Packet_reader reader;
    auto packet = reader.read_response(response + body, false);
    BOOST_CHECK_EQUAL(packet->header()->content_length(), 100);
}
```

### Chunked Transfer Encoding

```cpp
BOOST_AUTO_TEST_CASE(chunked_encoding_response) {
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "6\r\n world\r\n"
        "0\r\n\r\n";

    // Verify chunked decoding works
}
```

### HTTP Date Format (RFC 1123)

```cpp
BOOST_AUTO_TEST_CASE(http_date_format) {
    http::Response_header header(200, "OK");
    std::string dump = header.dump();

    // Verify RFC 1123 format: "Day, DD Mon YYYY HH:MM:SS GMT"
    std::regex date_pattern(
        R"(Date: [A-Z][a-z]{2}, \d{2} [A-Z][a-z]{2} \d{4} \d{2}:\d{2}:\d{2} GMT)"
    );
    BOOST_CHECK(std::regex_search(dump, date_pattern));
}
```

### Packet Capture Comparison

```bash
#!/bin/bash
# scripts/compare_wire_format.sh

# Capture request with curl trace
curl -X POST -H "Content-Type: text/xml" \
  -d @tests/data/request.xml \
  http://localhost:19876/RPC2 \
  --trace-ascii trace.txt

# Extract and verify headers
grep -E "^(POST|Host|Content-Type|Content-Length)" trace.txt
```

---

## Compatibility Checklist

### P0: Production Server
- [ ] Connect and authenticate
- [ ] Call methods successfully
- [ ] Handle faults correctly

### P1: Third-Party Interop
- [ ] Python `xmlrpc.server` compatibility
- [ ] All value types work

### P2: Value Type Round-Trip
- [ ] int/i4 round-trip
- [ ] boolean round-trip
- [ ] string round-trip (with special chars, UTF-8)
- [ ] double round-trip (precision preserved)
- [ ] dateTime.iso8601 round-trip
- [ ] base64 round-trip (binary data, null bytes)
- [ ] struct round-trip
- [ ] array round-trip
- [ ] nil round-trip
- [ ] Empty values (string, array, struct)
- [ ] Boundary values (INT_MIN, INT_MAX)

### P3: HTTP Details
- [ ] Parse existing test data files
- [ ] Header case insensitivity
- [ ] Chunked encoding (if supported)
- [ ] Keep-alive connections
- [ ] Timeout handling

---

## Known Wire Format Changes

| Component | Change | Wire Impact | Risk |
|-----------|--------|-------------|------|
| Number serialization | `std::to_chars` | None | Low |
| HTTP Date header | `strftime` | None | Low |
| Base64 encoding | Lookup table decode | None | Low |
| TCP_NODELAY | Enabled by default | Network only | Low |
| Struct storage | `unordered_map` | Order varies | Low |

**Note on Struct ordering**: Per the [XML-RPC specification](https://github.com/scripting/xml-rpc/blob/master/spec.md), struct member order is explicitly **not preserved**. All major implementations ([Python](https://github.com/python/cpython/blob/main/Lib/xmlrpc/client.py), [Java](https://xmlrpc.sourceforge.net/javadoc/redstone/xmlrpc/XmlRpcStruct.html)) use unordered data structures.

---

## Conclusion

Wire protocol changes since PR#1 are **minimal and low-risk**:

1. **Number/Date formatting** - Implementation changed, output format unchanged
2. **Base64** - Algorithm optimized, encoding unchanged
3. **TCP_NODELAY** - Network optimization, protocol unchanged
4. **Struct order** - Non-issue per XML-RPC specification

**Testing Priority**:
1. **P0**: Test against production server (HIGHEST)
2. **P1**: Test against Python `xmlrpc.server`
3. **P2**: Value type edge cases and round-trips
4. **P3**: HTTP protocol details

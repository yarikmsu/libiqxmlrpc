// XML Parsing Performance Benchmarks
// Measures parsing throughput and identifies optimization opportunities
// Run: cd build && ./tests/xml-parsing-benchmark-test

#include "perf_utils.h"

#include "libiqxmlrpc/request.h"
#include "libiqxmlrpc/response.h"
#include "libiqxmlrpc/value.h"

#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace perf;
using namespace iqxmlrpc;

// ============================================================================
// Helper: Generate XML-RPC request/response strings of varying complexity
// ============================================================================

std::string generate_simple_request() {
  return R"(<?xml version="1.0"?>
<methodCall>
  <methodName>test.method</methodName>
  <params>
    <param><value><string>hello</string></value></param>
  </params>
</methodCall>)";
}

std::string generate_request_with_n_params(size_t n) {
  std::string xml = R"(<?xml version="1.0"?>
<methodCall>
  <methodName>test.multiParam</methodName>
  <params>
)";
  for (size_t i = 0; i < n; ++i) {
    xml += "    <param><value><int>" + std::to_string(i) + "</int></value></param>\n";
  }
  xml += "  </params>\n</methodCall>";
  return xml;
}

std::string generate_response_with_array(size_t n) {
  std::string xml = R"(<?xml version="1.0"?>
<methodResponse>
  <params>
    <param>
      <value>
        <array>
          <data>
)";
  for (size_t i = 0; i < n; ++i) {
    xml += "            <value><int>" + std::to_string(i) + "</int></value>\n";
  }
  xml += R"(          </data>
        </array>
      </value>
    </param>
  </params>
</methodResponse>)";
  return xml;
}

std::string generate_response_with_struct(size_t n_fields) {
  std::string xml = R"(<?xml version="1.0"?>
<methodResponse>
  <params>
    <param>
      <value>
        <struct>
)";
  for (size_t i = 0; i < n_fields; ++i) {
    xml += "          <member>\n";
    xml += "            <name>field" + std::to_string(i) + "</name>\n";
    xml += "            <value><string>value" + std::to_string(i) + "</string></value>\n";
    xml += "          </member>\n";
  }
  xml += R"(        </struct>
      </value>
    </param>
  </params>
</methodResponse>)";
  return xml;
}

std::string generate_nested_struct(size_t depth) {
  std::string xml = R"(<?xml version="1.0"?>
<methodResponse>
  <params>
    <param>
      <value>
)";
  for (size_t i = 0; i < depth; ++i) {
    xml += std::string(8 + i * 2, ' ') + "<struct>\n";
    xml += std::string(10 + i * 2, ' ') + "<member>\n";
    xml += std::string(12 + i * 2, ' ') + "<name>level" + std::to_string(i) + "</name>\n";
    xml += std::string(12 + i * 2, ' ') + "<value>\n";
  }
  xml += std::string(12 + depth * 2, ' ') + "<string>deepest</string>\n";
  for (size_t i = depth; i > 0; --i) {
    xml += std::string(12 + (i - 1) * 2, ' ') + "</value>\n";
    xml += std::string(10 + (i - 1) * 2, ' ') + "</member>\n";
    xml += std::string(8 + (i - 1) * 2, ' ') + "</struct>\n";
  }
  xml += R"(      </value>
    </param>
  </params>
</methodResponse>)";
  return xml;
}

std::string generate_mixed_types_response() {
  return R"(<?xml version="1.0"?>
<methodResponse>
  <params>
    <param>
      <value>
        <struct>
          <member><name>int_val</name><value><int>42</int></value></member>
          <member><name>str_val</name><value><string>hello world</string></value></member>
          <member><name>bool_val</name><value><boolean>1</boolean></value></member>
          <member><name>double_val</name><value><double>3.14159</double></value></member>
          <member><name>base64_val</name><value><base64>SGVsbG8gV29ybGQh</base64></value></member>
          <member><name>date_val</name><value><dateTime.iso8601>20240115T12:30:45</dateTime.iso8601></value></member>
          <member><name>array_val</name><value><array><data>
            <value><int>1</int></value>
            <value><int>2</int></value>
            <value><int>3</int></value>
          </data></array></value></member>
        </struct>
      </value>
    </param>
  </params>
</methodResponse>)";
}

// ============================================================================
// Section 1: End-to-End Request Parsing
// ============================================================================

void benchmark_request_parsing() {
  section("Request Parsing Throughput");

  const std::string simple_req = generate_simple_request();
  const std::string multi_param_10 = generate_request_with_n_params(10);
  const std::string multi_param_100 = generate_request_with_n_params(100);

  // Simple request (1 param)
  PERF_BENCHMARK("parse_request_simple", 10000, {
    std::unique_ptr<Request> req(parse_request(simple_req));
    do_not_optimize(req.get());
  });

  // Request with 10 params
  PERF_BENCHMARK("parse_request_10_params", 5000, {
    std::unique_ptr<Request> req(parse_request(multi_param_10));
    do_not_optimize(req.get());
  });

  // Request with 100 params
  PERF_BENCHMARK("parse_request_100_params", 1000, {
    std::unique_ptr<Request> req(parse_request(multi_param_100));
    do_not_optimize(req.get());
  });
}

// ============================================================================
// Section 2: End-to-End Response Parsing
// ============================================================================

void benchmark_response_parsing() {
  section("Response Parsing Throughput");

  const std::string array_10 = generate_response_with_array(10);
  const std::string array_100 = generate_response_with_array(100);
  const std::string array_1000 = generate_response_with_array(1000);
  const std::string struct_10 = generate_response_with_struct(10);
  const std::string struct_100 = generate_response_with_struct(100);
  const std::string mixed = generate_mixed_types_response();
  const std::string nested_5 = generate_nested_struct(5);
  const std::string nested_8 = generate_nested_struct(8);  // Max safe depth

  // Array responses
  PERF_BENCHMARK("parse_response_array_10", 5000, {
    Response resp = parse_response(array_10);
    do_not_optimize(&resp);
  });

  PERF_BENCHMARK("parse_response_array_100", 2000, {
    Response resp = parse_response(array_100);
    do_not_optimize(&resp);
  });

  PERF_BENCHMARK("parse_response_array_1000", 500, {
    Response resp = parse_response(array_1000);
    do_not_optimize(&resp);
  });

  // Struct responses
  PERF_BENCHMARK("parse_response_struct_10", 5000, {
    Response resp = parse_response(struct_10);
    do_not_optimize(&resp);
  });

  PERF_BENCHMARK("parse_response_struct_100", 1000, {
    Response resp = parse_response(struct_100);
    do_not_optimize(&resp);
  });

  // Mixed types
  PERF_BENCHMARK("parse_response_mixed_types", 5000, {
    Response resp = parse_response(mixed);
    do_not_optimize(&resp);
  });

  // Nested structs (tests depth handling)
  PERF_BENCHMARK("parse_response_nested_5", 5000, {
    Response resp = parse_response(nested_5);
    do_not_optimize(&resp);
  });

  PERF_BENCHMARK("parse_response_nested_8", 5000, {
    Response resp = parse_response(nested_8);
    do_not_optimize(&resp);
  });
}

// ============================================================================
// Section 3: Namespace Stripping (Micro-benchmark)
// ============================================================================

void benchmark_namespace_stripping() {
  section("Namespace Stripping Approaches");

  const size_t ITERS = 1000000;

  // Test data: typical tag names with and without namespaces
  std::vector<std::string> tags_with_ns = {
    "ns:methodCall", "ns:methodName", "ns:params", "ns:param", "ns:value",
    "ns:string", "ns:int", "ns:array", "ns:data", "ns:struct", "ns:member", "ns:name"
  };
  std::vector<std::string> tags_without_ns = {
    "methodCall", "methodName", "params", "param", "value",
    "string", "int", "array", "data", "struct", "member", "name"
  };

  // Current approach: find + erase
  PERF_BENCHMARK("ns_strip_erase_with_ns", ITERS, {
    for (size_t j = 0; j < tags_with_ns.size(); ++j) {
      std::string rv = tags_with_ns[j];
      size_t pos = rv.find(':');
      if (pos != std::string::npos && pos + 1 < rv.size()) {
        rv.erase(0, pos + 1);
      }
      do_not_optimize(rv);
    }
  });

  PERF_BENCHMARK("ns_strip_erase_without_ns", ITERS, {
    for (size_t j = 0; j < tags_without_ns.size(); ++j) {
      std::string rv = tags_without_ns[j];
      size_t pos = rv.find(':');
      if (pos != std::string::npos && pos + 1 < rv.size()) {
        rv.erase(0, pos + 1);
      }
      do_not_optimize(rv);
    }
  });

  // Proposed approach: find + substr (avoids in-place modification)
  PERF_BENCHMARK("ns_strip_substr_with_ns", ITERS, {
    for (size_t j = 0; j < tags_with_ns.size(); ++j) {
      std::string rv = tags_with_ns[j];
      size_t pos = rv.find(':');
      if (pos != std::string::npos && pos + 1 < rv.size()) {
        rv = rv.substr(pos + 1);
      }
      do_not_optimize(rv);
    }
  });

  PERF_BENCHMARK("ns_strip_substr_without_ns", ITERS, {
    for (size_t j = 0; j < tags_without_ns.size(); ++j) {
      std::string rv = tags_without_ns[j];
      size_t pos = rv.find(':');
      if (pos != std::string::npos && pos + 1 < rv.size()) {
        rv = rv.substr(pos + 1);
      }
      do_not_optimize(rv);
    }
  });

  // Alternative: find_last_of + substr (handles multiple colons)
  PERF_BENCHMARK("ns_strip_find_last_with_ns", ITERS, {
    for (size_t j = 0; j < tags_with_ns.size(); ++j) {
      std::string rv = tags_with_ns[j];
      size_t pos = rv.find_last_of(':');
      if (pos != std::string::npos && pos + 1 < rv.size()) {
        rv = rv.substr(pos + 1);
      }
      do_not_optimize(rv);
    }
  });
}

// ============================================================================
// Section 4: State Machine Lookup (Micro-benchmark)
// ============================================================================

// Simulate the current linear search state machine
struct StateTransition {
  int prev_state;
  const char* tag;
  int new_state;
};

// Request parsing transitions (simplified)
const StateTransition request_transitions[] = {
  {0, "methodCall", 1},
  {1, "methodName", 2},
  {2, "params", 3},
  {3, "param", 4},
  {4, "value", 5},
  {5, "string", 6}, {5, "int", 6}, {5, "i4", 6}, {5, "i8", 6},
  {5, "boolean", 6}, {5, "double", 6}, {5, "base64", 6},
  {5, "dateTime.iso8601", 6}, {5, "struct", 7}, {5, "array", 8},
  {7, "member", 9}, {9, "name", 10}, {10, "value", 5},
  {8, "data", 11}, {11, "value", 5},
  {0, nullptr, 0}  // Terminator
};

int linear_search_change(const StateTransition* trans, int curr_state, const std::string& tag) {
  for (size_t i = 0; trans[i].tag != nullptr; ++i) {
    if (trans[i].prev_state == curr_state && tag == trans[i].tag) {
      return trans[i].new_state;
    }
  }
  return -1;  // Not found
}

// Hash-based alternative
struct StateKey {
  int state{0};
  std::string tag{};
  bool operator==(const StateKey& other) const {
    return state == other.state && tag == other.tag;
  }
};

struct StateKeyHash {
  std::size_t operator()(const StateKey& k) const {
    return std::hash<int>()(k.state) ^ (std::hash<std::string>()(k.tag) << 1);
  }
};

std::unordered_map<StateKey, int, StateKeyHash> build_hash_transitions() {
  std::unordered_map<StateKey, int, StateKeyHash> map;
  for (size_t i = 0; request_transitions[i].tag != nullptr; ++i) {
    StateKey key{request_transitions[i].prev_state, request_transitions[i].tag};
    map[key] = request_transitions[i].new_state;
  }
  return map;
}

void benchmark_state_machine() {
  section("State Machine Lookup Approaches");

  const size_t ITERS = 100000;

  // Build hash map once
  auto hash_transitions = build_hash_transitions();

  // Typical parsing sequence
  std::vector<std::pair<int, std::string>> parse_sequence = {
    {0, "methodCall"}, {1, "methodName"}, {2, "params"},
    {3, "param"}, {4, "value"}, {5, "string"},
    {3, "param"}, {4, "value"}, {5, "int"},
    {3, "param"}, {4, "value"}, {5, "struct"},
    {7, "member"}, {9, "name"}, {10, "value"}, {5, "string"}
  };

  // Linear search (current implementation)
  PERF_BENCHMARK("state_machine_linear", ITERS, {
    for (size_t j = 0; j < parse_sequence.size(); ++j) {
      int result = linear_search_change(request_transitions, parse_sequence[j].first, parse_sequence[j].second);
      do_not_optimize(result);
    }
  });

  // Hash-based lookup (proposed)
  PERF_BENCHMARK("state_machine_hash", ITERS, {
    for (size_t j = 0; j < parse_sequence.size(); ++j) {
      StateKey key;
      key.state = parse_sequence[j].first;
      key.tag = parse_sequence[j].second;
      auto it = hash_transitions.find(key);
      int result = -1;
      if (it != hash_transitions.end()) result = it->second;
      do_not_optimize(result);
    }
  });
}

// ============================================================================
// Section 5: Serialization (dump_request/dump_response)
// ============================================================================

void benchmark_serialization() {
  section("Serialization Throughput");

  // Create test objects
  Request simple_req("test.method", {Value("hello")});

  Param_list many_params;
  for (int i = 0; i < 100; ++i) {
    many_params.push_back(Value(i));
  }
  Request multi_req("test.multiParam", many_params);

  Array arr;
  for (int i = 0; i < 100; ++i) {
    arr.push_back(Value(i));
  }

  Struct s;
  for (int i = 0; i < 50; ++i) {
    s.insert("field" + std::to_string(i), "value" + std::to_string(i));
  }

  PERF_BENCHMARK("serialize_request_simple", 10000, {
    std::string xml = dump_request(simple_req);
    do_not_optimize(xml);
  });

  PERF_BENCHMARK("serialize_request_100_params", 2000, {
    std::string xml = dump_request(multi_req);
    do_not_optimize(xml);
  });

  // For responses, we need to parse first to get a Response object
  Response arr_resp(new Value(arr));
  Response struct_resp(new Value(s));

  PERF_BENCHMARK("serialize_response_array_100", 2000, {
    std::string xml = dump_response(arr_resp);
    do_not_optimize(xml);
  });

  PERF_BENCHMARK("serialize_response_struct_50", 2000, {
    std::string xml = dump_response(struct_resp);
    do_not_optimize(xml);
  });
}

// ============================================================================
// Section 6: Parse + Serialize Round-trip
// ============================================================================

void benchmark_roundtrip() {
  section("Parse + Serialize Round-trip");

  const std::string array_100_xml = generate_response_with_array(100);
  const std::string struct_50_xml = generate_response_with_struct(50);

  PERF_BENCHMARK("roundtrip_array_100", 1000, {
    Response resp = parse_response(array_100_xml);
    std::string xml = dump_response(resp);
    do_not_optimize(xml);
  });

  PERF_BENCHMARK("roundtrip_struct_50", 1000, {
    Response resp = parse_response(struct_50_xml);
    std::string xml = dump_response(resp);
    do_not_optimize(xml);
  });
}

// ============================================================================
// Section 7: String Copy Reduction (Micro-benchmark)
// ============================================================================

// Simulate the current to_string() behavior
std::string to_string_copy(const char* s, size_t len) {
  std::string retval(s, len);
  return retval;
}

// Alternative: reuse a thread-local buffer
thread_local std::string tls_buffer;
const std::string& to_string_reuse(const char* s, size_t len) {
  tls_buffer.assign(s, len);
  return tls_buffer;
}

void benchmark_string_copy_reduction() {
  section("String Copy Reduction Approaches");

  const size_t ITERS = 1000000;

  // Typical XML text values of various sizes
  const char* short_text = "hello";
  const char* medium_text = "This is a typical XML-RPC string value";
  const char* long_text = "This is a much longer string that might appear in XML-RPC "
                          "responses containing descriptions or large text fields "
                          "that need to be parsed and copied multiple times";

  size_t short_len = strlen(short_text);
  size_t medium_len = strlen(medium_text);
  size_t long_len = strlen(long_text);

  // Current approach: copy each time
  PERF_BENCHMARK("string_copy_short", ITERS, {
    std::string s = to_string_copy(short_text, short_len);
    do_not_optimize(s);
  });

  PERF_BENCHMARK("string_copy_medium", ITERS, {
    std::string s = to_string_copy(medium_text, medium_len);
    do_not_optimize(s);
  });

  PERF_BENCHMARK("string_copy_long", ITERS, {
    std::string s = to_string_copy(long_text, long_len);
    do_not_optimize(s);
  });

  // Alternative: reuse buffer (not applicable everywhere but shows potential)
  PERF_BENCHMARK("string_reuse_short", ITERS, {
    const std::string& s = to_string_reuse(short_text, short_len);
    do_not_optimize(s);
  });

  PERF_BENCHMARK("string_reuse_medium", ITERS, {
    const std::string& s = to_string_reuse(medium_text, medium_len);
    do_not_optimize(s);
  });

  PERF_BENCHMARK("string_reuse_long", ITERS, {
    const std::string& s = to_string_reuse(long_text, long_len);
    do_not_optimize(s);
  });
}

// ============================================================================
// Section 8: Value Object Allocation (Micro-benchmark)
// ============================================================================

void benchmark_value_allocation() {
  section("Value Object Allocation Patterns");

  const size_t ITERS = 100000;

  // Current approach: allocate new Value for each parsed value
  PERF_BENCHMARK("value_alloc_int", ITERS, {
    std::unique_ptr<Value> v(new Value(42));
    do_not_optimize(v.get());
  });

  PERF_BENCHMARK("value_alloc_string", ITERS, {
    std::unique_ptr<Value> v(new Value("test string"));
    do_not_optimize(v.get());
  });

  PERF_BENCHMARK("value_alloc_array_empty", ITERS, {
    Array arr;
    std::unique_ptr<Value> v(new Value(arr));
    do_not_optimize(v.get());
  });

  PERF_BENCHMARK("value_alloc_struct_empty", ITERS, {
    Struct s;
    std::unique_ptr<Value> v(new Value(s));
    do_not_optimize(v.get());
  });

  // Measure array building without reserve
  PERF_BENCHMARK("array_build_100_no_reserve", ITERS / 10, {
    Array arr;
    for (int j = 0; j < 100; ++j) {
      arr.push_back(Value(j));
    }
    do_not_optimize(&arr);
  });

  // Measure array building with reserve (new optimization)
  PERF_BENCHMARK("array_build_100_with_reserve", ITERS / 10, {
    Array arr;
    arr.reserve(100);
    for (int j = 0; j < 100; ++j) {
      arr.push_back(Value(j));
    }
    do_not_optimize(&arr);
  });

  // Compare with raw vector for reference
  PERF_BENCHMARK("vector_build_100_no_reserve", ITERS / 10, {
    std::vector<int> vec;
    for (int j = 0; j < 100; ++j) {
      vec.push_back(j);
    }
    do_not_optimize(&vec);
  });

  PERF_BENCHMARK("vector_build_100_with_reserve", ITERS / 10, {
    std::vector<int> vec;
    vec.reserve(100);
    for (int j = 0; j < 100; ++j) {
      vec.push_back(j);
    }
    do_not_optimize(&vec);
  });

  // Measure struct building
  PERF_BENCHMARK("struct_build_50_fields", ITERS / 10, {
    Struct s;
    for (int j = 0; j < 50; ++j) {
      s.insert("field" + std::to_string(j), j);
    }
    do_not_optimize(&s);
  });

  // Pre-build keys to measure insert without string construction
  std::vector<std::string> keys;
  for (int j = 0; j < 50; ++j) {
    keys.push_back("field" + std::to_string(j));
  }

  PERF_BENCHMARK("struct_build_50_prebuilt_keys", ITERS / 10, {
    Struct s;
    for (int j = 0; j < 50; ++j) {
      s.insert(keys[static_cast<size_t>(j)], j);
    }
    do_not_optimize(&s);
  });
}

// ============================================================================
// Section 9: Array/Struct Access Patterns
// ============================================================================

void benchmark_container_access() {
  section("Array/Struct Access Patterns");

  const size_t ITERS = 100000;

  // Build test containers
  Array arr;
  for (int j = 0; j < 100; ++j) {
    arr.push_back(Value(j));
  }

  Struct s;
  std::vector<std::string> keys;
  for (int j = 0; j < 100; ++j) {
    std::string key = "field" + std::to_string(j);
    keys.push_back(key);
    s.insert(key, j);
  }

  // Array access by index
  PERF_BENCHMARK("array_access_by_index", ITERS, {
    for (size_t j = 0; j < 100; ++j) {
      const Value& v = arr[j];
      do_not_optimize(&v);
    }
  });

  // Array iteration
  PERF_BENCHMARK("array_iterate", ITERS, {
    for (Array::const_iterator it = arr.begin(); it != arr.end(); ++it) {
      do_not_optimize(&(*it));
    }
  });

  // Struct access by key
  PERF_BENCHMARK("struct_access_by_key", ITERS, {
    for (size_t j = 0; j < 100; ++j) {
      const Value& v = s[keys[j]];
      do_not_optimize(&v);
    }
  });

  // Struct has_field check
  PERF_BENCHMARK("struct_has_field", ITERS, {
    for (size_t j = 0; j < 100; ++j) {
      bool found = s.has_field(keys[j]);
      do_not_optimize(found);
    }
  });

  // Struct iteration
  PERF_BENCHMARK("struct_iterate", ITERS, {
    for (Struct::const_iterator it = s.begin(); it != s.end(); ++it) {
      do_not_optimize(&(it->second));
    }
  });
}

// ============================================================================
// Section 10: Value Type Checking
// ============================================================================

void benchmark_type_checking() {
  section("Value Type Checking Patterns");

  const size_t ITERS = 1000000;

  Value int_val(42);
  Value str_val("test");
  Array arr;
  arr.push_back(Value(1));
  Value arr_val(arr);
  Struct s;
  s.insert("key", "value");
  Value struct_val(s);

  // Type checks using type_tag (current optimized approach)
  PERF_BENCHMARK("type_check_is_int", ITERS, {
    bool is_int = int_val.is_int();
    do_not_optimize(is_int);
  });

  PERF_BENCHMARK("type_check_is_string", ITERS, {
    bool is_str = str_val.is_string();
    do_not_optimize(is_str);
  });

  PERF_BENCHMARK("type_check_is_array", ITERS, {
    bool is_arr = arr_val.is_array();
    do_not_optimize(is_arr);
  });

  PERF_BENCHMARK("type_check_is_struct", ITERS, {
    bool is_struct = struct_val.is_struct();
    do_not_optimize(is_struct);
  });

  // Multiple type checks (common pattern)
  PERF_BENCHMARK("type_check_chain", ITERS, {
    bool result = int_val.is_nil() || int_val.is_int() || int_val.is_int64();
    do_not_optimize(result);
  });
}

// ============================================================================
// Main
// ============================================================================

int main() {
  std::cout << "============================================================\n";
  std::cout << "XML Parsing Performance Benchmarks\n";
  std::cout << "============================================================\n\n";

  benchmark_request_parsing();
  benchmark_response_parsing();
  benchmark_namespace_stripping();
  benchmark_state_machine();
  benchmark_serialization();
  benchmark_roundtrip();
  benchmark_string_copy_reduction();
  benchmark_value_allocation();
  benchmark_container_access();
  benchmark_type_checking();

  std::cout << "\n============================================================\n";
  return 0;
}

// Performance Benchmarks for libiqxmlrpc2
// Measures baseline performance before optimizations
// Run: make perf-test

#include "perf_utils.h"

#include "libiqxmlrpc/value.h"
#include "libiqxmlrpc/value_type.h"
#include "libiqxmlrpc/response.h"
#include "libiqxmlrpc/response_parser.h"
#include "libiqxmlrpc/http.h"

#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <map>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include "test_integration_common.h"  // for create_temp_cert_files()

#include "libiqxmlrpc/num_conv.h"
#include "libiqxmlrpc/reactor.h"

using namespace iqxmlrpc;

// ============================================================================
// A. Number Conversion Benchmarks (High Priority)
// Target: boost::lexical_cast -> std::to_chars/from_chars
// ============================================================================

void benchmark_number_conversions() {
  perf::section("Number Conversion (std::to_chars/from_chars)");

  const size_t ITERS = 100000;

  // int -> string (num_conv)
  {
    int val = 123456;
    PERF_BENCHMARK("perf_int_to_string_new", ITERS, {
      std::string s = num_conv::to_string(val);
      perf::do_not_optimize(s);
    });
  }

  // string -> int (num_conv)
  PERF_BENCHMARK("perf_string_to_int_new", ITERS, {
    std::string s = "123456";
    int val = num_conv::from_string<int>(s);
    perf::do_not_optimize(val);
  });

  // int64_t -> string (num_conv)
  {
    int64_t val = 9223372036854775807LL;
    PERF_BENCHMARK("perf_int64_to_string_new", ITERS, {
      std::string s = num_conv::to_string(val);
      perf::do_not_optimize(s);
    });
  }

  // string -> int64_t (num_conv)
  PERF_BENCHMARK("perf_string_to_int64_new", ITERS, {
    std::string s = "9223372036854775807";
    int64_t val = num_conv::from_string<int64_t>(s);
    perf::do_not_optimize(val);
  });

  // double -> string (num_conv)
  {
    double val = 3.141592653589793;
    PERF_BENCHMARK("perf_double_to_string_new", ITERS, {
      std::string s = num_conv::double_to_string(val);
      perf::do_not_optimize(s);
    });
  }

  // string -> double (num_conv)
  PERF_BENCHMARK("perf_string_to_double_new", ITERS, {
    std::string s = "3.141592653589793";
    double val = num_conv::string_to_double(s);
    perf::do_not_optimize(val);
  });
}

// ============================================================================
// B. Type Checking Benchmarks (Medium Priority)
// Target: dynamic_cast -> type tag enum
// ============================================================================

void benchmark_type_checking() {
  perf::section("Type Checking (dynamic_cast)");

  const size_t ITERS = 1000000;

  // Create test values
  Value int_val(42);
  Value str_val("hello world");
  Value dbl_val(3.14159);

  // is_int() on int value
  PERF_BENCHMARK("perf_type_check_int_true", ITERS, {
    bool result = int_val.is_int();
    perf::do_not_optimize(result);
  });

  // is_int() on string value (false case)
  PERF_BENCHMARK("perf_type_check_int_false", ITERS, {
    bool result = str_val.is_int();
    perf::do_not_optimize(result);
  });

  // is_string() on string value
  PERF_BENCHMARK("perf_type_check_string_true", ITERS, {
    bool result = str_val.is_string();
    perf::do_not_optimize(result);
  });

  // Mixed type checking pattern (common in value dispatch)
  PERF_BENCHMARK("perf_type_check_mixed", ITERS / 10, {
    // Check all types on each value
    perf::do_not_optimize(int_val.is_nil());
    perf::do_not_optimize(int_val.is_int());
    perf::do_not_optimize(int_val.is_int64());
    perf::do_not_optimize(int_val.is_bool());
    perf::do_not_optimize(int_val.is_double());
    perf::do_not_optimize(int_val.is_string());
    perf::do_not_optimize(int_val.is_binary());
    perf::do_not_optimize(int_val.is_datetime());
    perf::do_not_optimize(int_val.is_array());
    perf::do_not_optimize(int_val.is_struct());
  });
}

// ============================================================================
// C. HTTP Date Formatting Benchmark (Medium Priority)
// Target: Cache locale/facet in current_date()
// ============================================================================

void benchmark_http_date() {
  perf::section("HTTP Date Formatting");

  const size_t ITERS = 10000;

  // Response_header construction calls current_date() internally
  PERF_BENCHMARK("perf_http_response_header", ITERS, {
    http::Response_header hdr(200, "OK");
    perf::do_not_optimize(hdr.code());
  });

  // Also benchmark header dump (includes date in output)
  PERF_BENCHMARK("perf_http_response_dump", ITERS, {
    http::Response_header hdr(200, "OK");
    hdr.set_content_length(100);
    std::string s = hdr.dump();
    perf::do_not_optimize(s);
  });
}

// ============================================================================
// D. DateTime Parsing Benchmark (Medium Priority)
// Target: substr() allocations -> direct parsing
// ============================================================================

void benchmark_datetime_parsing() {
  perf::section("DateTime Parsing");

  const size_t ITERS = 1000000;

  // Parse ISO8601 datetime string
  PERF_BENCHMARK("perf_datetime_parse", ITERS, {
    Date_time dt("20260109T12:30:45");
    perf::do_not_optimize(dt);
  });

  // Also benchmark serialization (to_string)
  Date_time dt("20260109T12:30:45");
  PERF_BENCHMARK("perf_datetime_to_string", ITERS, {
    const std::string& s = dt.to_string();
    perf::do_not_optimize(s);
  });
}

// ============================================================================
// E. Value Operations Benchmarks (Medium Priority)
// Target: Move semantics, smart pointer optimization
// ============================================================================

void benchmark_value_operations() {
  perf::section("Value Operations");

  const size_t ITERS_SIMPLE = 100000;
  const size_t ITERS_COMPLEX = 1000;

  // Simple value clone (int)
  Value int_val(42);
  PERF_BENCHMARK("perf_value_clone_int", ITERS_SIMPLE, {
    Value copy(int_val);
    perf::do_not_optimize(copy);
  });

  // Simple value clone (string)
  Value str_val("hello world, this is a test string");
  PERF_BENCHMARK("perf_value_clone_string", ITERS_SIMPLE, {
    Value copy(str_val);
    perf::do_not_optimize(copy);
  });

  // Array clone - 10 elements
  {
    Array arr10;
    for (int i = 0; i < 10; i++) arr10.push_back(i);
    Value arr_val(arr10);
    PERF_BENCHMARK("perf_value_clone_array_10", ITERS_SIMPLE / 10, {
      Value copy(arr_val);
      perf::do_not_optimize(copy);
    });
  }

  // Array clone - 100 elements
  {
    Array arr100;
    for (int i = 0; i < 100; i++) arr100.push_back(i);
    Value arr_val(arr100);
    PERF_BENCHMARK("perf_value_clone_array_100", ITERS_COMPLEX, {
      Value copy(arr_val);
      perf::do_not_optimize(copy);
    });
  }

  // Array clone - 1000 elements
  {
    Array arr1000;
    for (int i = 0; i < 1000; i++) arr1000.push_back(i);
    Value arr_val(arr1000);
    PERF_BENCHMARK("perf_value_clone_array_1000", ITERS_COMPLEX / 10, {
      Value copy(arr_val);
      perf::do_not_optimize(copy);
    });
  }

  // Struct clone - 5 fields
  {
    Struct s5;
    s5.insert("f1", "value1");
    s5.insert("f2", "value2");
    s5.insert("f3", 123);
    s5.insert("f4", 3.14);
    s5.insert("f5", true);
    Value struct_val(s5);
    PERF_BENCHMARK("perf_value_clone_struct_5", ITERS_SIMPLE / 10, {
      Value copy(struct_val);
      perf::do_not_optimize(copy);
    });
  }

  // Struct clone - 20 fields
  {
    Struct s20;
    for (int i = 0; i < 20; i++) {
      s20.insert("field_" + std::to_string(i), i);
    }
    Value struct_val(s20);
    PERF_BENCHMARK("perf_value_clone_struct_20", ITERS_COMPLEX, {
      Value copy(struct_val);
      perf::do_not_optimize(copy);
    });
  }

  // Array push_back performance
  PERF_BENCHMARK("perf_array_push_back", ITERS_COMPLEX, {
    Array arr;
    for (int i = 0; i < 100; i++) {
      arr.push_back(i);
    }
    perf::do_not_optimize(arr);
  });

  // Struct insert performance
  PERF_BENCHMARK("perf_struct_insert", ITERS_COMPLEX, {
    Struct s;
    for (int i = 0; i < 20; i++) {
      s.insert("field_" + std::to_string(i), i);
    }
    perf::do_not_optimize(s);
  });

  // --- Element Access Benchmarks (for smart pointer impact measurement) ---

  // Array element access - random access pattern
  {
    Array arr;
    for (int i = 0; i < 1000; i++) arr.push_back(i);

    PERF_BENCHMARK("perf_array_access", ITERS_SIMPLE, {
      int sum = 0;
      for (unsigned i = 0; i < arr.size(); i++) {
        sum += arr[i].get_int();
      }
      perf::do_not_optimize(sum);
    });
  }

  // Struct element access - key lookup
  {
    Struct s;
    for (int i = 0; i < 20; i++) {
      s.insert("field_" + std::to_string(i), i * 10);
    }

    PERF_BENCHMARK("perf_struct_access", ITERS_SIMPLE, {
      int sum = 0;
      sum += s["field_0"].get_int();
      sum += s["field_5"].get_int();
      sum += s["field_10"].get_int();
      sum += s["field_15"].get_int();
      sum += s["field_19"].get_int();
      perf::do_not_optimize(sum);
    });
  }

  // --- Iteration Benchmarks ---

  // Array iteration using const_iterator
  {
    Array arr;
    for (int i = 0; i < 1000; i++) arr.push_back(i);

    PERF_BENCHMARK("perf_array_iterate", ITERS_SIMPLE, {
      int sum = 0;
      for (auto it = arr.begin(); it != arr.end(); ++it) {
        sum += (*it).get_int();
      }
      perf::do_not_optimize(sum);
    });
  }

  // Struct iteration using const_iterator
  {
    Struct s;
    for (int i = 0; i < 20; i++) {
      s.insert("field_" + std::to_string(i), i * 10);
    }

    PERF_BENCHMARK("perf_struct_iterate", ITERS_SIMPLE, {
      int sum = 0;
      for (auto it = s.begin(); it != s.end(); ++it) {
        sum += it->second->get_int();
      }
      perf::do_not_optimize(sum);
    });
  }

  // --- Destruction Benchmarks ---

  // Array destruction (measures delete overhead)
  PERF_BENCHMARK("perf_array_destroy", ITERS_COMPLEX, {
    Array* arr = new Array();
    for (int i = 0; i < 100; i++) {
      arr->push_back(i);
    }
    delete arr;  // Triggers destructor chain
  });

  // Struct destruction
  PERF_BENCHMARK("perf_struct_destroy", ITERS_COMPLEX, {
    Struct* s = new Struct();
    for (int i = 0; i < 20; i++) {
      s->insert("field_" + std::to_string(i), i);
    }
    delete s;  // Triggers destructor chain
  });
}

// ============================================================================
// F. Base64 Encoding/Decoding Benchmarks (Low Priority)
// ============================================================================

void benchmark_base64() {
  perf::section("Base64 Encoding/Decoding");

  const size_t ITERS_SMALL = 1000000;
  const size_t ITERS_LARGE = 1000;

  // 1KB data
  std::string data_1kb(1024, 'X');
  std::unique_ptr<Binary_data> bin_1kb(Binary_data::from_data(data_1kb));

  PERF_BENCHMARK("perf_base64_encode_1kb", ITERS_SMALL, {
    const std::string& encoded = bin_1kb->get_base64();
    perf::do_not_optimize(encoded);
  });

  std::string encoded_1kb = bin_1kb->get_base64();
  PERF_BENCHMARK("perf_base64_decode_1kb", ITERS_SMALL, {
    std::unique_ptr<Binary_data> decoded(Binary_data::from_base64(encoded_1kb));
    const std::string& data = decoded->get_data();
    perf::do_not_optimize(data);
  });

  // 64KB data
  std::string data_64kb(65536, 'Y');
  std::unique_ptr<Binary_data> bin_64kb(Binary_data::from_data(data_64kb));

  PERF_BENCHMARK("perf_base64_encode_64kb", ITERS_LARGE, {
    const std::string& encoded = bin_64kb->get_base64();
    perf::do_not_optimize(encoded);
  });

  std::string encoded_64kb = bin_64kb->get_base64();
  PERF_BENCHMARK("perf_base64_decode_64kb", ITERS_LARGE, {
    std::unique_ptr<Binary_data> decoded(Binary_data::from_base64(encoded_64kb));
    const std::string& data = decoded->get_data();
    perf::do_not_optimize(data);
  });
}

// ============================================================================
// G. Full Parse/Dump Benchmarks (Integration)
// ============================================================================

// Helper to create XML response string
std::string create_response_xml(int num_values) {
  std::string xml = "<?xml version=\"1.0\"?>\n"
                    "<methodResponse><params><param><value><array><data>\n";
  for (int i = 0; i < num_values; i++) {
    xml += "<value><struct>"
           "<member><name>id</name><value><i4>" + std::to_string(i) + "</i4></value></member>"
           "<member><name>name</name><value><string>item" + std::to_string(i) + "</string></value></member>"
           "<member><name>price</name><value><double>" + std::to_string(i * 1.5) + "</double></value></member>"
           "</struct></value>\n";
  }
  xml += "</data></array></value></param></params></methodResponse>";
  return xml;
}

void benchmark_parse_dump() {
  perf::section("Full Parse/Dump (Integration)");

  const size_t ITERS = 100;

  // Small response (10 structs)
  std::string xml_small = create_response_xml(10);
  PERF_BENCHMARK("perf_parse_response_10", ITERS * 10, {
    Parser p(xml_small);
    ResponseBuilder b(p);
    b.build();
    Response r = b.get();
    perf::do_not_optimize(r);
  });

  // Large response (1000 structs)
  std::string xml_large = create_response_xml(1000);
  PERF_BENCHMARK("perf_parse_response_1000", ITERS, {
    Parser p(xml_large);
    ResponseBuilder b(p);
    b.build();
    Response r = b.get();
    perf::do_not_optimize(r);
  });

  // Dump small response
  {
    Array arr;
    for (int i = 0; i < 10; i++) {
      Struct s;
      s.insert("id", i);
      s.insert("name", "item" + std::to_string(i));
      s.insert("price", i * 1.5);
      arr.push_back(s);
    }
    Value* v = new Value(arr);
    Response resp(v);

    PERF_BENCHMARK("perf_dump_response_10", ITERS * 10, {
      std::string xml = dump_response(resp);
      perf::do_not_optimize(xml);
    });
  }

  // Dump large response
  {
    Array arr;
    for (int i = 0; i < 1000; i++) {
      Struct s;
      s.insert("id", i);
      s.insert("name", "item" + std::to_string(i));
      s.insert("price", i * 1.5);
      arr.push_back(s);
    }
    Value* v = new Value(arr);
    Response resp(v);

    PERF_BENCHMARK("perf_dump_response_1000", ITERS, {
      std::string xml = dump_response(resp);
      perf::do_not_optimize(xml);
    });
  }
}

// ============================================================================
// H. HTTP Header Parsing Benchmark (Low Priority)
// Target: Single-pass parsing instead of 5 passes
// ============================================================================

void benchmark_http_header_parsing() {
  perf::section("HTTP Header Parsing");

  const size_t ITERS = 10000;

  // Typical HTTP request header (8 options)
  // Note: No trailing CRLF - the packet reader strips the \r\n\r\n separator
  std::string request_header =
      "POST /RPC2 HTTP/1.1\r\n"
      "Host: localhost:8080\r\n"
      "Content-Type: text/xml\r\n"
      "Content-Length: 256\r\n"
      "User-Agent: libiqxmlrpc/0.13.6\r\n"
      "Connection: keep-alive\r\n"
      "Accept: */*\r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "X-Custom-Header: some-value-here";

  PERF_BENCHMARK("perf_http_request_parse", ITERS, {
    http::Request_header hdr(http::HTTP_CHECK_WEAK, request_header);
    perf::do_not_optimize(hdr);
  });

  // Typical HTTP response header (6 options)
  std::string response_header =
      "HTTP/1.1 200 OK\r\n"
      "Server: libiqxmlrpc/0.13.6\r\n"
      "Date: Fri, 10 Jan 2026 12:30:45 GMT\r\n"
      "Content-Type: text/xml\r\n"
      "Content-Length: 512\r\n"
      "Connection: close\r\n"
      "X-Request-Id: abc-123-def-456";

  PERF_BENCHMARK("perf_http_response_parse", ITERS, {
    http::Response_header hdr(http::HTTP_CHECK_WEAK, response_header);
    perf::do_not_optimize(hdr);
  });

  // Large header (15 options) - stress test
  std::string large_header =
      "POST /RPC2 HTTP/1.1\r\n"
      "Host: api.example.com:443\r\n"
      "Content-Type: text/xml; charset=utf-8\r\n"
      "Content-Length: 4096\r\n"
      "User-Agent: Mozilla/5.0 (compatible; libiqxmlrpc/0.13.6)\r\n"
      "Connection: keep-alive\r\n"
      "Accept: text/xml, application/xml\r\n"
      "Accept-Encoding: gzip, deflate, br\r\n"
      "Accept-Language: en-US,en;q=0.9\r\n"
      "Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
      "Cache-Control: no-cache\r\n"
      "X-Forwarded-For: 192.168.1.1\r\n"
      "X-Forwarded-Proto: https\r\n"
      "X-Request-Id: 550e8400-e29b-41d4-a716-446655440000\r\n"
      "X-Trace-Id: trace-abc-123-def-456-ghi-789";

  PERF_BENCHMARK("perf_http_request_parse_large", ITERS, {
    http::Request_header hdr(http::HTTP_CHECK_WEAK, large_header);
    perf::do_not_optimize(hdr);
  });
}

// ============================================================================
// I. Server Performance Benchmarks
// Target: Method dispatch, response handling, XML serialization
// ============================================================================

void benchmark_server_performance() {
  perf::section("Server Performance");

  const size_t ITERS = 100000;

  // --- Map Lookup Pattern Benchmark ---
  // Simulates dispatcher_manager.cc double lookup issue
  {
    std::map<std::string, int> method_map;
    // Register 20 methods (typical server)
    for (int i = 0; i < 20; i++) {
      method_map["method_" + std::to_string(i)] = i;
    }
    std::string lookup_key = "method_10";  // Middle of map

    // Pattern 1: Double lookup (current code)
    PERF_BENCHMARK("perf_map_double_lookup", ITERS, {
      if (method_map.find(lookup_key) != method_map.end()) {
        int val = method_map[lookup_key];  // Second lookup!
        perf::do_not_optimize(val);
      }
    });

    // Pattern 2: Single lookup with iterator (optimized)
    PERF_BENCHMARK("perf_map_single_lookup", ITERS, {
      auto it = method_map.find(lookup_key);
      if (it != method_map.end()) {
        int val = it->second;  // No second lookup
        perf::do_not_optimize(val);
      }
    });
  }

  // --- Response Partial Write Benchmark ---
  // Simulates http_server.cc:126 string erase issue
  {
    const size_t RESPONSE_SIZE = 65536;  // 64KB response
    const size_t CHUNK_SIZE = 16384;     // 16KB chunks (4 writes)

    // Pattern 1: String erase (current code)
    PERF_BENCHMARK("perf_response_erase", 1000, {
      std::string response(RESPONSE_SIZE, 'X');
      size_t remaining = RESPONSE_SIZE;
      while (remaining > 0) {
        size_t chunk = std::min(CHUNK_SIZE, remaining);
        // Simulate send - just access the data
        perf::do_not_optimize(response.c_str());
        response.erase(0, chunk);  // O(n) erase!
        remaining -= chunk;
      }
    });

    // Pattern 2: Offset tracking (optimized)
    PERF_BENCHMARK("perf_response_offset", 1000, {
      std::string response(RESPONSE_SIZE, 'X');
      size_t offset = 0;
      size_t remaining = RESPONSE_SIZE;
      while (remaining > 0) {
        size_t chunk = std::min(CHUNK_SIZE, remaining);
        // Simulate send - access data at offset
        perf::do_not_optimize(response.c_str() + offset);
        offset += chunk;  // O(1) offset update!
        remaining -= chunk;
      }
    });
  }

  // --- XML Struct Serialization Benchmark ---
  // Measures value_type_xml.cc struct visitor pattern
  {
    // Small struct (5 fields)
    Struct s5;
    s5.insert("id", 12345);
    s5.insert("name", "test_item");
    s5.insert("price", 99.99);
    s5.insert("active", true);
    s5.insert("count", 42);
    Value* v5 = new Value(s5);
    Response resp5(v5);

    PERF_BENCHMARK("perf_serialize_struct_5", 10000, {
      std::string xml = dump_response(resp5);
      perf::do_not_optimize(xml);
    });

    // Medium struct (20 fields)
    Struct s20;
    for (int i = 0; i < 20; i++) {
      s20.insert("field_" + std::to_string(i), i * 100);
    }
    Value* v20 = new Value(s20);
    Response resp20(v20);

    PERF_BENCHMARK("perf_serialize_struct_20", 10000, {
      std::string xml = dump_response(resp20);
      perf::do_not_optimize(xml);
    });

    // Nested struct (struct containing structs)
    Struct outer;
    for (int i = 0; i < 5; i++) {
      Struct inner;
      inner.insert("id", i);
      inner.insert("value", "nested_" + std::to_string(i));
      outer.insert("item_" + std::to_string(i), inner);
    }
    Value* vn = new Value(outer);
    Response respn(vn);

    PERF_BENCHMARK("perf_serialize_struct_nested", 10000, {
      std::string xml = dump_response(respn);
      perf::do_not_optimize(xml);
    });
  }
}

// ============================================================================
// J. Threading Primitives Benchmark
// Compare mutex-protected bool vs atomic<bool>
// ============================================================================

#include <atomic>
#include <mutex>

void benchmark_threading_primitives() {
  perf::section("Threading Primitives (mutex vs atomic)");

  const size_t ITERS = 1000000;

  // Mutex-protected bool read (simulates old is_being_destructed())
  {
    bool flag = false;
    std::mutex mtx;

    PERF_BENCHMARK("perf_mutex_bool_read", ITERS, {
      std::lock_guard<std::mutex> lk(mtx);
      bool val = flag;
      perf::do_not_optimize(val);
    });
  }

  // Atomic bool read (simulates new is_being_destructed())
  {
    std::atomic<bool> flag{false};

    PERF_BENCHMARK("perf_atomic_bool_read", ITERS, {
      bool val = flag.load(std::memory_order_acquire);
      perf::do_not_optimize(val);
    });
  }

  // Mutex-protected bool write (simulates old destruction_started())
  {
    bool flag = false;
    std::mutex mtx;

    PERF_BENCHMARK("perf_mutex_bool_write", ITERS, {
      std::lock_guard<std::mutex> lk(mtx);
      flag = true;
      perf::do_not_optimize(flag);
    });
  }

  // Atomic bool write (simulates new destruction_started())
  {
    std::atomic<bool> flag{false};

    PERF_BENCHMARK("perf_atomic_bool_write", ITERS, {
      flag.store(true, std::memory_order_release);
      perf::do_not_optimize(flag);
    });
  }
}

// ============================================================================
// K. SSL/TLS Context Creation Benchmark
// Measures SSL context creation overhead for P1a optimization
// ============================================================================

#include "libiqxmlrpc/ssl_lib.h"

// RAII wrapper for temp cert files - ensures cleanup even if benchmark throws
// Uses create_temp_cert_files() from test_integration_common.h
class TempCertFiles {
  std::pair<std::string, std::string> paths_;
public:
  TempCertFiles()
    : paths_(iqxmlrpc_test::create_temp_cert_files())
  {}
  ~TempCertFiles() {
    std::remove(paths_.first.c_str());
    std::remove(paths_.second.c_str());
  }
  // Non-copyable, non-movable (Rule of Five)
  TempCertFiles(const TempCertFiles&) = delete;
  TempCertFiles& operator=(const TempCertFiles&) = delete;
  TempCertFiles(TempCertFiles&&) = delete;
  TempCertFiles& operator=(TempCertFiles&&) = delete;

  const std::string& cert_path() const { return paths_.first; }
  const std::string& key_path() const { return paths_.second; }
};

void benchmark_ssl_context() {
  perf::section("SSL/TLS Context Creation");

  // Create temp cert files with RAII cleanup
  TempCertFiles certs;

  // Fewer iterations because SSL context creation is expensive
  const size_t ITERS_SSL = 100;

  // Server context creation (includes P1a session caching + P1b cipher optimization)
  // This is what Https_server uses
  PERF_BENCHMARK("perf_ssl_ctx_server", ITERS_SSL, {
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_server(certs.cert_path(), certs.key_path());
    perf::do_not_optimize(ctx);
    delete ctx;
  });

  // Client-only context creation (no server optimizations)
  // This is what Https_client uses - no cipher restrictions
  PERF_BENCHMARK("perf_ssl_ctx_client_only", ITERS_SSL, {
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_only();
    perf::do_not_optimize(ctx);
    delete ctx;
  });

  // Cleanup handled by TempCertFiles RAII destructor
}

// ============================================================================
// L. Cipher Throughput Benchmark
// Measures actual encryption/decryption speed with different ciphers
// This demonstrates the real P1b benefit: AES-NI hardware acceleration
// ============================================================================

void benchmark_cipher_throughput() {
  perf::section("Cipher Throughput (P1b real benefit)");

  // Use 64KB buffer to simulate typical TLS record sizes
  const size_t DATA_SIZE = 64 * 1024;
  const size_t ITERS_CIPHER = 1000;

  // Allocate buffers
  std::vector<unsigned char> plaintext(DATA_SIZE);
  std::vector<unsigned char> ciphertext(DATA_SIZE + EVP_MAX_BLOCK_LENGTH);
  std::vector<unsigned char> key(32);  // 256-bit key
  std::vector<unsigned char> iv(16);   // 128-bit IV
  std::vector<unsigned char> tag(16);  // GCM auth tag

  // Generate random data
  RAND_bytes(plaintext.data(), DATA_SIZE);
  RAND_bytes(key.data(), 32);
  RAND_bytes(iv.data(), 16);

  // AES-128-GCM (P1b preferred cipher - uses AES-NI)
  PERF_BENCHMARK("perf_cipher_aes128_gcm_64kb", ITERS_CIPHER, {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    int outlen = 0;
    int tmplen = 0;
    EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, key.data(), iv.data());
    EVP_EncryptUpdate(ctx, ciphertext.data(), &outlen, plaintext.data(), DATA_SIZE);
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + outlen, &tmplen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
    perf::do_not_optimize(ciphertext);
    EVP_CIPHER_CTX_free(ctx);
  });

  // AES-256-GCM (P1b preferred cipher - uses AES-NI)
  PERF_BENCHMARK("perf_cipher_aes256_gcm_64kb", ITERS_CIPHER, {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    int outlen = 0;
    int tmplen = 0;
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key.data(), iv.data());
    EVP_EncryptUpdate(ctx, ciphertext.data(), &outlen, plaintext.data(), DATA_SIZE);
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + outlen, &tmplen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
    perf::do_not_optimize(ciphertext);
    EVP_CIPHER_CTX_free(ctx);
  });

  // ChaCha20-Poly1305 (P1b fallback for non-AES-NI CPUs)
  PERF_BENCHMARK("perf_cipher_chacha20_poly1305_64kb", ITERS_CIPHER, {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    int outlen = 0;
    int tmplen = 0;
    unsigned char nonce[12];
    memcpy(nonce, iv.data(), 12);
    EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, key.data(), nonce);
    EVP_EncryptUpdate(ctx, ciphertext.data(), &outlen, plaintext.data(), DATA_SIZE);
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + outlen, &tmplen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag.data());
    perf::do_not_optimize(ciphertext);
    EVP_CIPHER_CTX_free(ctx);
  });

  // AES-256-CBC (older cipher without AEAD - for comparison)
  PERF_BENCHMARK("perf_cipher_aes256_cbc_64kb", ITERS_CIPHER, {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    int outlen = 0;
    int tmplen = 0;
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key.data(), iv.data());
    EVP_EncryptUpdate(ctx, ciphertext.data(), &outlen, plaintext.data(), DATA_SIZE);
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + outlen, &tmplen);
    perf::do_not_optimize(ciphertext);
    EVP_CIPHER_CTX_free(ctx);
  });

  // Calculate and display throughput
  std::cout << "\n  Throughput comparison (higher is better):\n";
  std::cout << "  - AES-128-GCM uses AES-NI hardware acceleration\n";
  std::cout << "  - AES-256-GCM uses AES-NI hardware acceleration\n";
  std::cout << "  - ChaCha20 is optimized for CPUs without AES-NI\n";
  std::cout << "  - AES-256-CBC is the older TLS 1.0/1.1 cipher\n";
}

// ============================================================================
// M. Exception vs Return Code Benchmark (P3)
// Measures the overhead of using exceptions for normal SSL I/O flow control
// Current code throws need_read()/need_write() exceptions for SSL_ERROR_WANT_*
// ============================================================================

// Simulate the SSL I/O result codes (for benchmark purposes)
enum class BenchSslResult { OK, WANT_READ, WANT_WRITE, ERROR, CONNECTION_CLOSE };

// Simulate checking SSL result with return code (proposed P3 implementation)
__attribute__((noinline))
BenchSslResult check_ssl_result_return_code(int ssl_error) {
  switch (ssl_error) {
    case SSL_ERROR_NONE: return BenchSslResult::OK;
    case SSL_ERROR_WANT_READ: return BenchSslResult::WANT_READ;
    case SSL_ERROR_WANT_WRITE: return BenchSslResult::WANT_WRITE;
    case SSL_ERROR_ZERO_RETURN: return BenchSslResult::CONNECTION_CLOSE;
    default: return BenchSslResult::ERROR;
  }
}

// Simulate checking SSL result with exceptions (current implementation)
__attribute__((noinline))
void check_ssl_result_exception(int ssl_error) {
  switch (ssl_error) {
    case SSL_ERROR_NONE: return;
    case SSL_ERROR_WANT_READ: throw iqnet::ssl::need_read();
    case SSL_ERROR_WANT_WRITE: throw iqnet::ssl::need_write();
    case SSL_ERROR_ZERO_RETURN: throw iqnet::ssl::connection_close(true);
    default: throw iqnet::ssl::io_error(ssl_error);
  }
}

void benchmark_exception_vs_return_code() {
  perf::section("Exception vs Return Code (P3 optimization)");

  // High iterations to reduce variance on sub-nanosecond measurements
  const size_t ITERS = 1000000;

  // Benchmark return code path for WANT_READ (most common case in non-blocking I/O)
  PERF_BENCHMARK("perf_ssl_result_return_code", ITERS, {
    BenchSslResult result = check_ssl_result_return_code(SSL_ERROR_WANT_READ);
    if (result == BenchSslResult::WANT_READ) {
      // Would register for read - just do a simple operation
      volatile int x = 1;
      perf::do_not_optimize(x);
    }
  });

  // Benchmark exception path for WANT_READ (current implementation)
  PERF_BENCHMARK("perf_ssl_result_exception", ITERS, {
    try {
      check_ssl_result_exception(SSL_ERROR_WANT_READ);
    } catch (const iqnet::ssl::need_read&) {
      // Would register for read - just do a simple operation
      volatile int x = 1;
      perf::do_not_optimize(x);
    }
  });

  // Benchmark return code path for OK (no error case)
  PERF_BENCHMARK("perf_ssl_result_ok_return", ITERS, {
    BenchSslResult result = check_ssl_result_return_code(SSL_ERROR_NONE);
    if (result == BenchSslResult::OK) {
      volatile int x = 1;
      perf::do_not_optimize(x);
    }
  });

  // Benchmark exception path for OK (no exception thrown)
  PERF_BENCHMARK("perf_ssl_result_ok_exception", ITERS, {
    try {
      check_ssl_result_exception(SSL_ERROR_NONE);
      volatile int x = 1;
      perf::do_not_optimize(x);
    } catch (...) {
      // Should not reach here
    }
  });

  // Benchmark the actual reactor registration simulation
  // This simulates the full switch_state() logic
  {
    volatile int registered_input = 0;
    volatile int registered_output = 0;

    // Return code version
    PERF_BENCHMARK("perf_ssl_switch_state_return", ITERS, {
      BenchSslResult result = check_ssl_result_return_code(SSL_ERROR_WANT_READ);
      switch (result) {
        case BenchSslResult::WANT_READ:
          registered_input++;
          break;
        case BenchSslResult::WANT_WRITE:
          registered_output++;
          break;
        default:
          break;
      }
      perf::do_not_optimize(registered_input);
    });

    // Exception version
    PERF_BENCHMARK("perf_ssl_switch_state_exception", ITERS, {
      try {
        check_ssl_result_exception(SSL_ERROR_WANT_READ);
      } catch (const iqnet::ssl::need_read&) {
        registered_input++;
      } catch (const iqnet::ssl::need_write&) {
        registered_output++;
      } catch (...) {
      }
      perf::do_not_optimize(registered_input);
    });
  }
}

// ============================================================================
// L. Reactor Handler List Copy-on-Write Benchmark
// ============================================================================

void benchmark_reactor_handler_list() {
  perf::section("Reactor Handler List Copy-on-Write");

  const size_t ITERS = 100000;

  // --- Benchmark with 100 handlers (moderate connection count) ---
  {
    // Old approach: Full std::list copy
    iqnet::Reactor_base::HandlerStateList handlers_copy;
    for (size_t i = 0; i < 100; ++i) {
      handlers_copy.push_back(iqnet::Reactor_base::HandlerState(static_cast<iqnet::Socket::Handler>(i)));
    }

    PERF_BENCHMARK("perf_handler_list_copy_100", ITERS, {
      iqnet::Reactor_base::HandlerStateList tmp(handlers_copy);
      perf::do_not_optimize(tmp);
    });
  }

  {
    // New approach: shared_ptr snapshot (COW)
    auto handlers_cow = std::make_shared<iqnet::Reactor_base::HandlerStateList>();
    for (size_t i = 0; i < 100; ++i) {
      handlers_cow->push_back(iqnet::Reactor_base::HandlerState(static_cast<iqnet::Socket::Handler>(i)));
    }

    PERF_BENCHMARK("perf_handler_list_cow_100", ITERS, {
      auto snapshot = handlers_cow;  // Just refcount increment
      perf::do_not_optimize(snapshot);
    });
  }

  // --- Benchmark with 1000 handlers (high connection count) ---
  {
    iqnet::Reactor_base::HandlerStateList handlers_copy;
    for (size_t i = 0; i < 1000; ++i) {
      handlers_copy.push_back(iqnet::Reactor_base::HandlerState(static_cast<iqnet::Socket::Handler>(i)));
    }

    PERF_BENCHMARK("perf_handler_list_copy_1000", ITERS / 10, {
      iqnet::Reactor_base::HandlerStateList tmp(handlers_copy);
      perf::do_not_optimize(tmp);
    });
  }

  {
    auto handlers_cow = std::make_shared<iqnet::Reactor_base::HandlerStateList>();
    for (size_t i = 0; i < 1000; ++i) {
      handlers_cow->push_back(iqnet::Reactor_base::HandlerState(static_cast<iqnet::Socket::Handler>(i)));
    }

    PERF_BENCHMARK("perf_handler_list_cow_1000", ITERS / 10, {
      auto snapshot = handlers_cow;
      perf::do_not_optimize(snapshot);
    });
  }
}

// ============================================================================
// N. Lock-Free Queue Benchmark (Before/After Comparison)
// Proves lock-free implementation has better tail latency under contention
// ============================================================================

#include <boost/lockfree/queue.hpp>
#include <condition_variable>
#include <deque>
#include <thread>

// Wrapper for mutex-based queue (OLD implementation)
class MutexQueue {
  std::deque<int64_t> queue_;
  mutable std::mutex lock_;

public:
  MutexQueue() : queue_(), lock_() {}

  void push(int64_t item) {
    std::lock_guard<std::mutex> lk(lock_);
    queue_.push_back(item);
  }

  bool try_pop(int64_t& item) {
    std::lock_guard<std::mutex> lk(lock_);
    if (queue_.empty()) return false;
    item = queue_.front();
    queue_.pop_front();
    return true;
  }

  void clear() {
    std::lock_guard<std::mutex> lk(lock_);
    queue_.clear();
  }
};

// Wrapper for lock-free queue (NEW implementation)
class LockFreeQueue {
  boost::lockfree::queue<int64_t, boost::lockfree::capacity<8192>> queue_;

public:
  LockFreeQueue() : queue_() {}

  void push(int64_t item) {
    while (!queue_.push(item)) {
      std::this_thread::yield();
    }
  }

  bool try_pop(int64_t& item) {
    return queue_.pop(item);
  }

  void clear() {
    int64_t dummy;
    while (queue_.pop(dummy)) {}
  }
};

void benchmark_lockfree_queue() {
  perf::section("Lock-Free Queue Comparison (Latency Percentiles)");

  using Clock = std::chrono::high_resolution_clock;

  // Configuration
  const size_t NUM_PRODUCERS = 4;
  const size_t NUM_CONSUMERS = 2;
  const size_t ITEMS_PER_PRODUCER = 25000;
  const size_t TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

  std::cout << "\nScenario: " << NUM_PRODUCERS << " producers, "
            << NUM_CONSUMERS << " consumers, "
            << ITEMS_PER_PRODUCER << " items/producer\n";
  std::cout << "Total items: " << TOTAL_ITEMS << "\n\n";

  // --- Benchmark mutex-based queue ---
  perf::LatencyStats mutex_stats;
  mutex_stats.reserve(TOTAL_ITEMS);
  {
    MutexQueue queue;
    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};
    std::atomic<bool> done{false};

    // Producers: push timestamp as payload
    auto producer = [&]() {
      for (size_t i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        auto now = Clock::now().time_since_epoch().count();
        queue.push(now);
        produced++;
      }
    };

    // Consumers: pop and measure latency
    std::mutex stats_lock;
    auto consumer = [&]() {
      while (!done || consumed < TOTAL_ITEMS) {
        int64_t enqueue_time;
        if (queue.try_pop(enqueue_time)) {
          auto now = Clock::now().time_since_epoch().count();
          auto latency = now - enqueue_time;
          {
            std::lock_guard<std::mutex> lk(stats_lock);
            mutex_stats.add(latency);
          }
          consumed++;
        } else {
          std::this_thread::yield();
        }
      }
    };

    auto start = Clock::now();

    std::vector<std::thread> producers;
    for (size_t i = 0; i < NUM_PRODUCERS; ++i) {
      producers.emplace_back(producer);
    }

    std::vector<std::thread> consumers;
    for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
      consumers.emplace_back(consumer);
    }

    for (auto& t : producers) t.join();
    done = true;
    for (auto& t : consumers) t.join();

    auto end = Clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double throughput = static_cast<double>(TOTAL_ITEMS) / (ms / 1000.0);

    std::cout << "[Mutex Queue]     Total time: " << std::fixed << std::setprecision(2)
              << ms << " ms, Throughput: " << std::setprecision(0) << throughput << " ops/s\n";
  }

  // --- Benchmark lock-free queue ---
  perf::LatencyStats lockfree_stats;
  lockfree_stats.reserve(TOTAL_ITEMS);
  {
    LockFreeQueue queue;
    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};
    std::atomic<bool> done{false};

    // Producers: push timestamp as payload
    auto producer = [&]() {
      for (size_t i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        auto now = Clock::now().time_since_epoch().count();
        queue.push(now);
        produced++;
      }
    };

    // Consumers: pop and measure latency
    std::mutex stats_lock;
    auto consumer = [&]() {
      while (!done || consumed < TOTAL_ITEMS) {
        int64_t enqueue_time;
        if (queue.try_pop(enqueue_time)) {
          auto now = Clock::now().time_since_epoch().count();
          auto latency = now - enqueue_time;
          {
            std::lock_guard<std::mutex> lk(stats_lock);
            lockfree_stats.add(latency);
          }
          consumed++;
        } else {
          std::this_thread::yield();
        }
      }
    };

    auto start = Clock::now();

    std::vector<std::thread> producers;
    for (size_t i = 0; i < NUM_PRODUCERS; ++i) {
      producers.emplace_back(producer);
    }

    std::vector<std::thread> consumers;
    for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
      consumers.emplace_back(consumer);
    }

    for (auto& t : producers) t.join();
    done = true;
    for (auto& t : consumers) t.join();

    auto end = Clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double throughput = static_cast<double>(TOTAL_ITEMS) / (ms / 1000.0);

    std::cout << "[Lock-Free Queue] Total time: " << std::fixed << std::setprecision(2)
              << ms << " ms, Throughput: " << std::setprecision(0) << throughput << " ops/s\n";
  }

  // --- Print comparison table ---
  perf::print_latency_comparison(
    "Queue Implementation Comparison (Enqueue-to-Dequeue Latency)",
    "Mutex Queue", mutex_stats,
    "Lock-Free", lockfree_stats
  );

  // Record p95 latencies as benchmark results for baseline tracking
  // (p95 is more stable than p99 in CI environments with thread scheduling noise)
  auto mutex_p95 = mutex_stats.p95();
  auto lockfree_p95 = lockfree_stats.p95();
  if (mutex_p95 > 0) {
    perf::ResultCollector::instance().add_result(
      perf::BenchmarkResult("perf_mutex_queue_p95_latency", mutex_p95 / 1e6, 1));
  }
  if (lockfree_p95 > 0) {
    perf::ResultCollector::instance().add_result(
      perf::BenchmarkResult("perf_lockfree_queue_p95_latency", lockfree_p95 / 1e6, 1));
  }
}

// ============================================================================
// Main
// ============================================================================

int main(int /*argc*/, char* /*argv*/[]) {
  std::cout << "============================================================\n";
  std::cout << "libiqxmlrpc2 Performance Benchmark\n";

  // Get current time
  std::time_t now = std::time(nullptr);
  char time_buf[64];
  std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  std::cout << "Date: " << time_buf << "\n";
  std::cout << "============================================================\n";

  perf::ResultCollector::instance().start_suite();

  // Run all benchmarks
  benchmark_number_conversions();
  benchmark_type_checking();
  benchmark_http_date();
  benchmark_http_header_parsing();
  benchmark_datetime_parsing();
  benchmark_value_operations();
  benchmark_base64();
  benchmark_parse_dump();
  benchmark_server_performance();
  benchmark_threading_primitives();
  benchmark_ssl_context();
  benchmark_cipher_throughput();
  benchmark_exception_vs_return_code();
  benchmark_reactor_handler_list();
  benchmark_lockfree_queue();

  // Save baseline
  std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", std::localtime(&now));
  std::string baseline_file = "performance_baseline.txt";
  perf::ResultCollector::instance().save_baseline(baseline_file);

  return 0;
}

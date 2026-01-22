// Performance Benchmarks for M1-M4 Optimization Opportunities
// Measures baseline performance before std::map -> unordered_map changes
// Run: cd build && ./tests/performance-m1-m4-test

#include "perf_utils.h"

#include <string>
#include <map>
#include <unordered_map>
#include <sstream>
#include <cstdio>
#include <memory>

using namespace perf;

// ============================================================================
// M1: HTTP Header Options - std::map vs std::unordered_map
// ============================================================================

void benchmark_m1_http_options_map() {
  section("M1: HTTP Header Options Lookup");

  const size_t ITERS = 100000;

  // Typical HTTP headers (15 headers)
  std::map<std::string, std::string> options_map;
  std::unordered_map<std::string, std::string> options_unordered;

  // Populate with typical headers
  std::vector<std::pair<std::string, std::string>> headers = {
    {"host", "example.com:8080"},
    {"user-agent", "libiqxmlrpc/2.0"},
    {"accept", "*/*"},
    {"content-type", "text/xml"},
    {"content-length", "1234"},
    {"connection", "keep-alive"},
    {"cache-control", "no-cache"},
    {"accept-encoding", "gzip, deflate"},
    {"accept-language", "en-US,en;q=0.9"},
    {"authorization", "Basic dXNlcjpwYXNz"},
    {"x-request-id", "abc-123-def-456"},
    {"x-forwarded-for", "192.0.2.1"},
    {"x-real-ip", "192.0.2.1"},
    {"referer", "https://example.com/page"},
    {"origin", "https://example.com"}
  };

  for (const auto& h : headers) {
    options_map[h.first] = h.second;
    options_unordered[h.first] = h.second;
  }

  // Benchmark: Lookup with std::map (current)
  PERF_BENCHMARK("m1_http_options_map_lookup", ITERS, {
    auto it = options_map.find("authorization");
    if (it != options_map.end()) {
      do_not_optimize(it->second);
    }
  });

  // Benchmark: Lookup with unordered_map (proposed)
  PERF_BENCHMARK("m1_http_options_unordered_lookup", ITERS, {
    auto it = options_unordered.find("authorization");
    if (it != options_unordered.end()) {
      do_not_optimize(it->second);
    }
  });

  // Benchmark: Multiple lookups (realistic request processing)
  PERF_BENCHMARK("m1_http_options_map_multi_lookup", ITERS / 10, {
    auto h1 = options_map.find("host");
    auto h2 = options_map.find("content-type");
    auto h3 = options_map.find("content-length");
    auto h4 = options_map.find("authorization");
    do_not_optimize(h1); do_not_optimize(h2); do_not_optimize(h3); do_not_optimize(h4);
  });

  PERF_BENCHMARK("m1_http_options_unordered_multi_lookup", ITERS / 10, {
    auto h1 = options_unordered.find("host");
    auto h2 = options_unordered.find("content-type");
    auto h3 = options_unordered.find("content-length");
    auto h4 = options_unordered.find("authorization");
    do_not_optimize(h1); do_not_optimize(h2); do_not_optimize(h3); do_not_optimize(h4);
  });

  // Benchmark: Insert (during header parsing) - use lambda to avoid macro issues
  auto insert_map = [&headers]() {
    std::map<std::string, std::string> tmp;
    for (const auto& h : headers) {
      tmp[h.first] = h.second;
    }
    return tmp.size();
  };

  auto insert_unordered = [&headers]() {
    std::unordered_map<std::string, std::string> tmp;
    for (const auto& h : headers) {
      tmp[h.first] = h.second;
    }
    return tmp.size();
  };

  PERF_BENCHMARK("m1_http_options_map_insert", ITERS / 10, {
    size_t sz = insert_map();
    do_not_optimize(sz);
  });

  PERF_BENCHMARK("m1_http_options_unordered_insert", ITERS / 10, {
    size_t sz = insert_unordered();
    do_not_optimize(sz);
  });
}

// ============================================================================
// M2: Method Dispatcher - std::map vs std::unordered_map
// ============================================================================

// Mock method factory
struct MockMethodFactory {
  std::string name;
  explicit MockMethodFactory(const std::string& n) : name(n) {}
};

void benchmark_m2_method_dispatcher() {
  section("M2: Method Dispatcher Lookup");

  const size_t ITERS = 100000;

  // Typical RPC server with 50 methods
  std::map<std::string, MockMethodFactory*> factory_map;
  std::unordered_map<std::string, MockMethodFactory*> factory_unordered;

  std::vector<std::string> method_names;
  for (int i = 0; i < 50; ++i) {
    std::string name = "method_" + std::to_string(i);
    method_names.push_back(name);
    factory_map[name] = new MockMethodFactory(name);
    factory_unordered[name] = new MockMethodFactory(name);
  }

  // Benchmark: Single lookup (hot path - every RPC request)
  PERF_BENCHMARK("m2_dispatcher_map_lookup", ITERS, {
    auto it = factory_map.find("method_25");
    if (it != factory_map.end()) {
      do_not_optimize(it->second);
    }
  });

  PERF_BENCHMARK("m2_dispatcher_unordered_lookup", ITERS, {
    auto it = factory_unordered.find("method_25");
    if (it != factory_unordered.end()) {
      do_not_optimize(it->second);
    }
  });

  // Benchmark: Varying lookups (simulates different method calls)
  PERF_BENCHMARK("m2_dispatcher_map_varied_lookup", ITERS / 10, {
    auto it0 = factory_map.find(method_names[0]);
    auto it1 = factory_map.find(method_names[5]);
    auto it2 = factory_map.find(method_names[10]);
    auto it3 = factory_map.find(method_names[15]);
    auto it4 = factory_map.find(method_names[20]);
    do_not_optimize(it0); do_not_optimize(it1); do_not_optimize(it2); do_not_optimize(it3); do_not_optimize(it4);
  });

  PERF_BENCHMARK("m2_dispatcher_unordered_varied_lookup", ITERS / 10, {
    auto it0 = factory_unordered.find(method_names[0]);
    auto it1 = factory_unordered.find(method_names[5]);
    auto it2 = factory_unordered.find(method_names[10]);
    auto it3 = factory_unordered.find(method_names[15]);
    auto it4 = factory_unordered.find(method_names[20]);
    do_not_optimize(it0); do_not_optimize(it1); do_not_optimize(it2); do_not_optimize(it3); do_not_optimize(it4);
  });

  // Benchmark: Registration (server startup) - use lambda to avoid macro issues
  auto register_map = [&factory_map]() {
    std::map<std::string, MockMethodFactory*> tmp;
    for (int i = 0; i < 50; ++i) {
      std::string name = "method_" + std::to_string(i);
      tmp[name] = factory_map.at(name);
    }
    return tmp.size();
  };

  auto register_unordered = [&factory_unordered]() {
    std::unordered_map<std::string, MockMethodFactory*> tmp;
    for (int i = 0; i < 50; ++i) {
      std::string name = "method_" + std::to_string(i);
      tmp[name] = factory_unordered.at(name);
    }
    return tmp.size();
  };

  PERF_BENCHMARK("m2_dispatcher_map_register", ITERS / 100, {
    size_t sz = register_map();
    do_not_optimize(sz);
  });

  PERF_BENCHMARK("m2_dispatcher_unordered_register", ITERS / 100, {
    size_t sz = register_unordered();
    do_not_optimize(sz);
  });

  // Cleanup
  for (auto& pair : factory_map) {
    delete pair.second;
  }
}

// ============================================================================
// M3: dynamic_cast vs static_cast
// ============================================================================

// Mock header hierarchy
class BaseHeader {
public:
  virtual ~BaseHeader() = default;
  virtual int type() const = 0;
};

class RequestHeader : public BaseHeader {
  std::string uri_;
public:
  explicit RequestHeader(const std::string& uri) : uri_(uri) {}
  int type() const override { return 1; }
  const std::string& uri() const { return uri_; }
};

class ResponseHeader : public BaseHeader {
  int code_;
public:
  explicit ResponseHeader(int code) : code_(code) {}
  int type() const override { return 2; }
  int code() const { return code_; }
};

void benchmark_m3_dynamic_cast() {
  section("M3: dynamic_cast vs static_cast");

  const size_t ITERS = 100000;

  std::unique_ptr<BaseHeader> req_base(new RequestHeader("/api/test"));
  std::unique_ptr<BaseHeader> resp_base(new ResponseHeader(200));

  // Benchmark: dynamic_cast (current in server.cc:221)
  PERF_BENCHMARK("m3_dynamic_cast_request", ITERS, {
    const RequestHeader* req = dynamic_cast<const RequestHeader*>(req_base.get());
    if (req) {
      do_not_optimize(req->uri());
    }
  });

  // Benchmark: type check + static_cast (proposed)
  PERF_BENCHMARK("m3_type_check_static_cast", ITERS, {
    if (req_base->type() == 1) {
      const RequestHeader* req = static_cast<const RequestHeader*>(req_base.get());
      do_not_optimize(req->uri());
    }
  });

  // Benchmark: dynamic_cast for error path (http.h:296)
  PERF_BENCHMARK("m3_dynamic_cast_response", ITERS, {
    const ResponseHeader* resp = dynamic_cast<const ResponseHeader*>(resp_base.get());
    if (resp) {
      do_not_optimize(resp->code());
    }
  });

  PERF_BENCHMARK("m3_type_check_response", ITERS, {
    if (resp_base->type() == 2) {
      const ResponseHeader* resp = static_cast<const ResponseHeader*>(resp_base.get());
      do_not_optimize(resp->code());
    }
  });
}

// ============================================================================
// M4: ostringstream vs snprintf for HTTP response header
// ============================================================================

void benchmark_m4_response_header_generation() {
  section("M4: Response Header Generation");

  const size_t ITERS = 100000;

  int code = 200;
  std::string phrase = "OK";

  // Benchmark: ostringstream (current in http.cc:617)
  PERF_BENCHMARK("m4_ostringstream_response_head", ITERS, {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << code << " " << phrase << "\r\n";
    std::string result = ss.str();
    do_not_optimize(result);
  });

  // Benchmark: snprintf to stack buffer (proposed)
  PERF_BENCHMARK("m4_snprintf_response_head", ITERS, {
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\n", code, phrase.c_str());
    std::string result(buf, len);
    do_not_optimize(result);
  });

  // Benchmark: string concatenation (alternative)
  PERF_BENCHMARK("m4_string_concat_response_head", ITERS, {
    std::string result = "HTTP/1.1 " + std::to_string(code) + " " + phrase + "\r\n";
    do_not_optimize(result);
  });

  // Benchmark: Reserve + append (alternative)
  PERF_BENCHMARK("m4_string_reserve_response_head", ITERS, {
    std::string result;
    result.reserve(64);
    result = "HTTP/1.1 ";
    result += std::to_string(code);
    result += " ";
    result += phrase;
    result += "\r\n";
    do_not_optimize(result);
  });

  // Benchmark: Different status codes (404, 500)
  PERF_BENCHMARK("m4_ostringstream_404", ITERS, {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << 404 << " Not Found\r\n";
    std::string result = ss.str();
    do_not_optimize(result);
  });

  PERF_BENCHMARK("m4_snprintf_404", ITERS, {
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\n", 404, "Not Found");
    std::string result(buf, len);
    do_not_optimize(result);
  });
}

// ============================================================================
// Main
// ============================================================================

int main() {
  std::cout << "============================================================\n";
  std::cout << "libiqxmlrpc - M1-M4 Performance Baseline Benchmarks\n";
  std::cout << "Measuring current performance before optimizations\n";
  std::cout << "============================================================\n";

  ResultCollector::instance().start_suite();

  // Run all M1-M4 benchmarks
  benchmark_m1_http_options_map();
  benchmark_m2_method_dispatcher();
  benchmark_m3_dynamic_cast();
  benchmark_m4_response_header_generation();

  // Save results
  std::string baseline_file = "performance_m1_m4_baseline.txt";
  ResultCollector::instance().save_baseline(baseline_file);

  std::cout << "\n============================================================\n";
  std::cout << "Next Steps:\n";
  std::cout << "1. Implement M1-M4 optimizations\n";
  std::cout << "2. Re-run this benchmark\n";
  std::cout << "3. Compare results to measure improvement\n";
  std::cout << "============================================================\n";

  return 0;
}

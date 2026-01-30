// Performance Benchmarks for M6: HTTP Tokenization with string_view
// Measures improvement from zero-allocation tokenization
// Run: cd build && ./tests/http-tokenize-benchmark-test

#include "perf_utils.h"
#include "libiqxmlrpc/num_conv.h"

#include <cctype>
#include <charconv>
#include <cstdlib>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

using namespace perf;

// ============================================================================
// Original: split_by_whitespace (allocates via substr)
// ============================================================================

template<typename Container>
void split_by_whitespace_alloc(Container& result, const std::string& s) {
  result.clear();
  size_t start = 0;
  size_t len = s.size();

  while (start < len) {
    while (start < len && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    if (start >= len) break;

    size_t end = start;
    while (end < len && !std::isspace(static_cast<unsigned char>(s[end]))) ++end;

    result.push_back(s.substr(start, end - start));  // Allocates!
    start = end;
  }
}

// ============================================================================
// Optimized: split_by_whitespace_sv (zero-allocation)
// ============================================================================

template<typename Container>
void split_by_whitespace_sv(Container& result, std::string_view s) {
  result.clear();
  size_t start = 0;
  size_t len = s.size();

  while (start < len) {
    while (start < len && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    if (start >= len) break;

    size_t end = start;
    while (end < len && !std::isspace(static_cast<unsigned char>(s[end]))) ++end;

    result.push_back(s.substr(start, end - start));  // No allocation!
    start = end;
  }
}

// ============================================================================
// M6: HTTP Line Parsing Benchmarks
// ============================================================================

void benchmark_m6_http_tokenization() {
  section("M6: HTTP Line Tokenization");

  const size_t ITERS = 100000;

  // Test cases: realistic HTTP lines
  const std::string request_line = "POST /xmlrpc HTTP/1.1";
  const std::string response_line = "HTTP/1.1 200 OK";
  const std::string long_uri = "POST /api/v2/xmlrpc/endpoint/with/long/path?param1=value1&param2=value2 HTTP/1.1";

  // Benchmark: Request line with std::deque<std::string> (original)
  PERF_BENCHMARK("m6_request_line_deque_string", ITERS, {
    std::deque<std::string> tokens;
    split_by_whitespace_alloc(tokens, request_line);
    do_not_optimize(tokens);
  });

  // Benchmark: Request line with std::vector<std::string_view> (optimized)
  PERF_BENCHMARK("m6_request_line_vector_sv", ITERS, {
    std::vector<std::string_view> tokens;
    split_by_whitespace_sv(tokens, request_line);
    do_not_optimize(tokens);
  });

  // Benchmark: Response line with std::deque<std::string> (original)
  PERF_BENCHMARK("m6_response_line_deque_string", ITERS, {
    std::deque<std::string> tokens;
    split_by_whitespace_alloc(tokens, response_line);
    do_not_optimize(tokens);
  });

  // Benchmark: Response line with std::vector<std::string_view> (optimized)
  PERF_BENCHMARK("m6_response_line_vector_sv", ITERS, {
    std::vector<std::string_view> tokens;
    split_by_whitespace_sv(tokens, response_line);
    do_not_optimize(tokens);
  });

  // Benchmark: Long URI with std::deque<std::string> (original)
  PERF_BENCHMARK("m6_long_uri_deque_string", ITERS, {
    std::deque<std::string> tokens;
    split_by_whitespace_alloc(tokens, long_uri);
    do_not_optimize(tokens);
  });

  // Benchmark: Long URI with std::vector<std::string_view> (optimized)
  PERF_BENCHMARK("m6_long_uri_vector_sv", ITERS, {
    std::vector<std::string_view> tokens;
    split_by_whitespace_sv(tokens, long_uri);
    do_not_optimize(tokens);
  });
}

// ============================================================================
// Realistic usage: tokenize + use tokens
// ============================================================================

void benchmark_m6_realistic_usage() {
  section("M6: Realistic HTTP Parsing Usage");

  const size_t ITERS = 100000;

  const std::string request_line = "POST /xmlrpc HTTP/1.1";
  const std::string response_line = "HTTP/1.1 200 OK";

  // Request parsing: tokenize + compare method + extract URI (original)
  PERF_BENCHMARK("m6_request_parse_alloc", ITERS, {
    std::deque<std::string> tokens;
    split_by_whitespace_alloc(tokens, request_line);
    bool is_post = (tokens.size() > 0 && tokens[0] == "POST");
    std::string uri;
    if (tokens.size() > 1) uri = tokens[1];
    do_not_optimize(is_post);
    do_not_optimize(uri);
  });

  // Request parsing: tokenize + compare method + extract URI (optimized)
  PERF_BENCHMARK("m6_request_parse_sv", ITERS, {
    std::vector<std::string_view> tokens;
    split_by_whitespace_sv(tokens, request_line);
    bool is_post = (tokens.size() > 0 && tokens[0] == "POST");
    std::string uri;
    if (tokens.size() > 1) uri = std::string(tokens[1]);  // Only allocate when storing
    do_not_optimize(is_post);
    do_not_optimize(uri);
  });

  // Response parsing: tokenize + extract code + phrase (original)
  PERF_BENCHMARK("m6_response_parse_alloc", ITERS, {
    std::deque<std::string> tokens;
    split_by_whitespace_alloc(tokens, response_line);
    int code = 0;
    std::string phrase;
    if (tokens.size() >= 2) {
      code = std::atoi(tokens[1].c_str());
    }
    if (tokens.size() > 2) phrase = tokens[2];
    do_not_optimize(code);
    do_not_optimize(phrase);
  });

  // Response parsing: tokenize + extract code + phrase (optimized)
  // Uses from_string<int>(string_view) directly - zero allocation for parsing
  PERF_BENCHMARK("m6_response_parse_sv", ITERS, {
    std::vector<std::string_view> tokens;
    split_by_whitespace_sv(tokens, response_line);
    int code = 0;
    std::string phrase;
    if (tokens.size() >= 2) {
      code = iqxmlrpc::num_conv::from_string<int>(tokens[1]);
    }
    if (tokens.size() > 2) phrase = std::string(tokens[2]);
    do_not_optimize(code);
    do_not_optimize(phrase);
  });
}

// ============================================================================
// M6: Number Conversion Benchmarks (string vs string_view)
// Tests the new from_string<T>(string_view) overload
// ============================================================================

void benchmark_m6_num_conversion() {
  section("M6: Number Conversion (string vs string_view)");

  const size_t ITERS = 500000;
  const std::string code_str = "200";
  const std::string_view code_sv = "200";

  // Baseline: atoi (traditional C-style conversion)
  PERF_BENCHMARK("m6_int_from_string_atoi", ITERS, {
    int code = std::atoi(code_str.c_str());
    do_not_optimize(code);
  });

  // num_conv with std::string (existing API, delegates to string_view version)
  PERF_BENCHMARK("m6_int_from_string_numconv", ITERS, {
    int code = iqxmlrpc::num_conv::from_string<int>(code_str);
    do_not_optimize(code);
  });

  // num_conv with string_view (new API, zero allocation)
  PERF_BENCHMARK("m6_int_from_sv_numconv", ITERS, {
    int code = iqxmlrpc::num_conv::from_string<int>(code_sv);
    do_not_optimize(code);
  });

  // Direct std::from_chars (baseline for comparison)
  PERF_BENCHMARK("m6_int_from_chars_direct", ITERS, {
    int code{};
    const char* first = code_sv.data();
    const char* last = first + code_sv.size();
    std::from_chars(first, last, code);
    do_not_optimize(code);
  });
}

int main() {
  std::cout << "============================================================\n";
  std::cout << "libiqxmlrpc - M6 HTTP Tokenization Benchmarks\n";
  std::cout << "Measuring string_view vs string allocation for HTTP parsing\n";
  std::cout << "============================================================\n";

  ResultCollector::instance().start_suite();

  benchmark_m6_http_tokenization();
  benchmark_m6_realistic_usage();
  benchmark_m6_num_conversion();

  // Save results
  ResultCollector::instance().save_baseline("performance_m6_http_tokenize.txt");

  return 0;
}

// vim:ts=2:sw=2:et

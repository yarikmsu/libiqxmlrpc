// Performance Benchmarks for E1-E3 Optimization Opportunities
// Measures performance improvements after ostringstream -> stack buffer/string concat changes
// Run: cd build && ./tests/performance-e1-e3-test

#include "perf_utils.h"

#include <string>
#include <sstream>
#include <cstdio>
#include <cstring>

using namespace perf;

// ============================================================================
// E1: SSL Certificate Hex Formatting - ostringstream vs stack buffer
// ============================================================================

void benchmark_e1_hex_formatting() {
  section("E1: SSL Certificate Hex Formatting");

  const size_t ITERS = 100000;

  // Simulate SHA256 digest (32 bytes)
  unsigned char md[32];
  for (int i = 0; i < 32; i++) {
    md[i] = static_cast<unsigned char>((i * 31 + 17) % 256);
  }

  // Benchmark: ostringstream (current in ssl_lib.cc)
  PERF_BENCHMARK("e1_ostringstream_hex", ITERS, {
    std::ostringstream ss;
    for(int i = 0; i < 32; i++)
      ss << std::hex << int(md[i]);
    std::string result = ss.str();
    do_not_optimize(result);
  });

  // Benchmark: snprintf stack buffer (proposed)
  PERF_BENCHMARK("e1_snprintf_hex", ITERS, {
    char hex_buf[65];  // 32 bytes * 2 chars/byte + null terminator
    for(int i = 0; i < 32; i++) {
      snprintf(&hex_buf[i * 2], 3, "%02x", md[i]);
    }
    hex_buf[64] = '\0';
    std::string result(hex_buf, 64);
    do_not_optimize(result);
  });

  // Benchmark: Multiple certificates (typical SSL handshake verification)
  PERF_BENCHMARK("e1_ostringstream_hex_multi", ITERS / 10, {
    for (int cert = 0; cert < 3; cert++) {
      std::ostringstream ss;
      for(int i = 0; i < 32; i++)
        ss << std::hex << int(md[i]);
      std::string result = ss.str();
      do_not_optimize(result);
    }
  });

  PERF_BENCHMARK("e1_snprintf_hex_multi", ITERS / 10, {
    for (int cert = 0; cert < 3; cert++) {
      char hex_buf[65];
      for(int i = 0; i < 32; i++) {
        snprintf(&hex_buf[i * 2], 3, "%02x", md[i]);
      }
      hex_buf[64] = '\0';
      std::string result(hex_buf, 64);
      do_not_optimize(result);
    }
  });
}

// ============================================================================
// E2: Proxy URI Construction - ostringstream vs string concat with reserve
// ============================================================================

void benchmark_e2_proxy_uri() {
  section("E2: Proxy URI Construction");

  const size_t ITERS = 100000;

  // Realistic proxy URI components
  const std::string vhost = "proxy.example.com";
  const std::string uri = "/api/xmlrpc/method";
  const int port = 8080;

  // Benchmark: ostringstream (current in http_client.cc)
  PERF_BENCHMARK("e2_ostringstream_uri", ITERS, {
    std::ostringstream ss;
    ss << "http://" << vhost << ':' << port;
    if (!uri.empty() && uri[0] != '/')
      ss << '/';
    ss << uri;
    std::string result = ss.str();
    do_not_optimize(result);
  });

  // Benchmark: string concat with reserve (proposed)
  PERF_BENCHMARK("e2_string_concat_uri", ITERS, {
    std::string result;
    result.reserve(7 + vhost.size() + 1 + 5 + 1 + uri.size());
    result = "http://";
    result += vhost;
    result += ':';
    result += std::to_string(port);
    if (!uri.empty() && uri[0] != '/')
      result += '/';
    result += uri;
    do_not_optimize(result);
  });

  // Benchmark: varying ports (different sizes: 80, 8080, 65535)
  const std::vector<int> ports = {80, 8080, 65535};

  PERF_BENCHMARK("e2_ostringstream_varied_ports", ITERS / 10, {
    for (int p : ports) {
      std::ostringstream ss;
      ss << "http://" << vhost << ':' << p;
      if (!uri.empty() && uri[0] != '/')
        ss << '/';
      ss << uri;
      std::string result = ss.str();
      do_not_optimize(result);
    }
  });

  PERF_BENCHMARK("e2_string_concat_varied_ports", ITERS / 10, {
    for (int p : ports) {
      std::string result;
      result.reserve(7 + vhost.size() + 1 + 5 + 1 + uri.size());
      result = "http://";
      result += vhost;
      result += ':';
      result += std::to_string(p);
      if (!uri.empty() && uri[0] != '/')
        result += '/';
      result += uri;
      do_not_optimize(result);
    }
  });

  // Benchmark: varying URIs (with and without leading /)
  const std::vector<std::string> uris = {
    "/api/xmlrpc",
    "api/xmlrpc",
    "/api/xmlrpc/method",
    ""
  };

  PERF_BENCHMARK("e2_ostringstream_varied_uris", ITERS / 10, {
    for (const auto& u : uris) {
      std::ostringstream ss;
      ss << "http://" << vhost << ':' << port;
      if (!u.empty() && u[0] != '/')
        ss << '/';
      ss << u;
      std::string result = ss.str();
      do_not_optimize(result);
    }
  });

  PERF_BENCHMARK("e2_string_concat_varied_uris", ITERS / 10, {
    for (const auto& u : uris) {
      std::string result;
      result.reserve(7 + vhost.size() + 1 + 5 + 1 + u.size());
      result = "http://";
      result += vhost;
      result += ':';
      result += std::to_string(port);
      if (!u.empty() && u[0] != '/')
        result += '/';
      result += u;
      do_not_optimize(result);
    }
  });
}

// ============================================================================
// E3: Host:Port Formatting - ostringstream vs string concat
// ============================================================================

void benchmark_e3_host_port() {
  section("E3: Host:Port Formatting");

  const size_t ITERS = 100000;

  const std::string vhost = "example.com";
  const int port = 8080;

  // Benchmark: ostringstream (current in http.cc)
  PERF_BENCHMARK("e3_ostringstream_host_port", ITERS, {
    std::ostringstream host_opt;
    host_opt << vhost << ":" << port;
    std::string result = host_opt.str();
    do_not_optimize(result);
  });

  // Benchmark: string concat (proposed)
  PERF_BENCHMARK("e3_string_concat_host_port", ITERS, {
    std::string result = vhost + ":" + std::to_string(port);
    do_not_optimize(result);
  });

  // Benchmark: varying hostnames (short, long, FQDN)
  const std::vector<std::string> vhosts = {
    "localhost",
    "example.com",
    "very.long.fully.qualified.domain.name.example.com",
    "192.168.1.1"
  };

  PERF_BENCHMARK("e3_ostringstream_varied_hosts", ITERS / 10, {
    for (const auto& v : vhosts) {
      std::ostringstream host_opt;
      host_opt << v << ":" << port;
      std::string result = host_opt.str();
      do_not_optimize(result);
    }
  });

  PERF_BENCHMARK("e3_string_concat_varied_hosts", ITERS / 10, {
    for (const auto& v : vhosts) {
      std::string result = v + ":" + std::to_string(port);
      do_not_optimize(result);
    }
  });

  // Benchmark: varying ports (single digit to 5 digits)
  const std::vector<int> ports_list = {1, 80, 443, 8080, 65535};

  PERF_BENCHMARK("e3_ostringstream_varied_ports", ITERS / 10, {
    for (int p : ports_list) {
      std::ostringstream host_opt;
      host_opt << vhost << ":" << p;
      std::string result = host_opt.str();
      do_not_optimize(result);
    }
  });

  PERF_BENCHMARK("e3_string_concat_varied_ports", ITERS / 10, {
    for (int p : ports_list) {
      std::string result = vhost + ":" + std::to_string(p);
      do_not_optimize(result);
    }
  });
}

// ============================================================================
// Main
// ============================================================================

int main() {
  std::cout << "============================================================\n";
  std::cout << "libiqxmlrpc - E1-E3 Performance Benchmarks\n";
  std::cout << "Measuring improvements after ostringstream optimizations\n";
  std::cout << "============================================================\n";

  ResultCollector::instance().start_suite();

  // Run all E1-E3 benchmarks
  benchmark_e1_hex_formatting();
  benchmark_e2_proxy_uri();
  benchmark_e3_host_port();

  // Save results
  std::string baseline_file = "performance_e1_e3_baseline.txt";
  ResultCollector::instance().save_baseline(baseline_file);

  std::cout << "\n============================================================\n";
  std::cout << "Next Steps:\n";
  std::cout << "1. Compare results to measure improvement\n";
  std::cout << "2. Run ./scripts/local_benchmark_compare.sh for detailed analysis\n";
  std::cout << "============================================================\n";

  return 0;
}

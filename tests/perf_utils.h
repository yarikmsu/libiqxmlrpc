#ifndef IQXMLRPC_PERF_UTILS_H
#define IQXMLRPC_PERF_UTILS_H

#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>

namespace perf {

// Result from a single benchmark
struct BenchmarkResult {
  std::string name;
  double total_ms;
  size_t iterations;
  double ns_per_op;

  BenchmarkResult(const std::string& n, double ms, size_t iters)
    : name(n), total_ms(ms), iterations(iters), ns_per_op((ms * 1000000.0) / static_cast<double>(iters))
  {
  }
};

// Global collection of results
class ResultCollector {
  std::vector<BenchmarkResult> results_;
  std::chrono::high_resolution_clock::time_point suite_start_;

public:
  static ResultCollector& instance() {
    static ResultCollector inst;
    return inst;
  }

  void start_suite() {
    suite_start_ = std::chrono::high_resolution_clock::now();
  }

  void add_result(const BenchmarkResult& r) {
    results_.push_back(r);

    // Print to console
    std::cout << "[PERF] " << std::setw(30) << std::left << r.name << ": "
              << std::fixed << std::setprecision(3) << std::setw(10) << std::right << r.total_ms << " ms"
              << " (" << r.iterations << " iters, "
              << std::setprecision(2) << r.ns_per_op << " ns/op)"
              << std::endl;
  }

  void save_baseline(const std::string& filename) {
    auto suite_end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double>(suite_end - suite_start_).count();

    std::ofstream out(filename);
    if (!out) {
      std::cerr << "Warning: Could not write to " << filename << std::endl;
      return;
    }

    // Header
    std::time_t now = std::time(nullptr);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    out << "# Performance Baseline - libiqxmlrpc2\n";
    out << "# Run date: " << time_buf << "\n";
    out << "# Total benchmarks: " << results_.size() << "\n";
    out << "# Total time: " << std::fixed << std::setprecision(2) << total_time << " seconds\n";
    out << "#\n";
    out << "# Format: benchmark_name: ns_per_op\n";
    out << "#\n";

    for (const auto& r : results_) {
      out << r.name << ": " << std::fixed << std::setprecision(2) << r.ns_per_op << "\n";
    }

    std::cout << "\n--- Summary ---\n";
    std::cout << "Total benchmarks: " << results_.size() << "\n";
    std::cout << "Total time: " << std::fixed << std::setprecision(2) << total_time << " seconds\n";
    std::cout << "Results saved to: " << filename << "\n";
  }

  void clear() { results_.clear(); }

private:
  ResultCollector(): results_(), suite_start_() {}
};

// Timer class - measures elapsed time on destruction
class Timer {
  std::chrono::high_resolution_clock::time_point start_;
  std::string name_;
  size_t iterations_;
  bool stopped_ = false;

public:
  Timer(const std::string& name, size_t iterations)
    : start_(std::chrono::high_resolution_clock::now())
    , name_(name)
    , iterations_(iterations)
  {}

  ~Timer() {
    if (!stopped_) stop();
  }

  void stop() {
    if (stopped_) return;
    stopped_ = true;

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start_).count();

    ResultCollector::instance().add_result(BenchmarkResult(name_, ms, iterations_));
  }
};

// Helper macro for running benchmarks with warmup
#define PERF_BENCHMARK(name, iterations, code)                            \
  do {                                                                    \
    /* Warmup - 10% of iterations */                                      \
    for (size_t i = 0; i < (iterations) / 10; ++i) { code; }              \
    /* Actual benchmark */                                                \
    {                                                                     \
      perf::Timer timer(name, iterations);                                \
      for (size_t i = 0; i < (iterations); ++i) { code; }                 \
    }                                                                     \
  } while (0)

// Prevent compiler from optimizing away results
template<typename T>
inline void do_not_optimize(const T& value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

// Print section header
inline void section(const std::string& name) {
  std::cout << "\n--- " << name << " ---\n";
}

} // namespace perf

#endif // IQXMLRPC_PERF_UTILS_H

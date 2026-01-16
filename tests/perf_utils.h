#ifndef IQXMLRPC_PERF_UTILS_H
#define IQXMLRPC_PERF_UTILS_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

// Latency statistics collector for percentile analysis
class LatencyStats {
  std::vector<int64_t> samples_;
  bool sorted_;

public:
  LatencyStats() : samples_(), sorted_(false) {}

  void add(int64_t ns) {
    samples_.push_back(ns);
    sorted_ = false;
  }

  void reserve(size_t n) {
    samples_.reserve(n);
  }

  size_t count() const { return samples_.size(); }

  void sort() {
    if (!sorted_ && !samples_.empty()) {
      std::sort(samples_.begin(), samples_.end());
      sorted_ = true;
    }
  }

  int64_t percentile(double p) {
    if (samples_.empty()) return 0;
    sort();
    size_t idx = static_cast<size_t>(p * static_cast<double>(samples_.size() - 1));
    return samples_[idx];
  }

  int64_t p50() { return percentile(0.50); }
  int64_t p90() { return percentile(0.90); }
  int64_t p95() { return percentile(0.95); }
  int64_t p99() { return percentile(0.99); }
  int64_t min() { sort(); return samples_.empty() ? 0 : samples_.front(); }
  int64_t max() { sort(); return samples_.empty() ? 0 : samples_.back(); }

  double mean() const {
    if (samples_.empty()) return 0.0;
    double sum = 0.0;
    for (auto s : samples_) sum += static_cast<double>(s);
    return sum / static_cast<double>(samples_.size());
  }

  double stddev() const {
    if (samples_.size() < 2) return 0.0;
    double m = mean();
    double sum_sq = 0.0;
    for (auto s : samples_) {
      double diff = static_cast<double>(s) - m;
      sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / static_cast<double>(samples_.size() - 1));
  }

  void clear() {
    samples_.clear();
    sorted_ = false;
  }
};

// Print comparison between two latency stats
inline void print_latency_comparison(const std::string& scenario,
                                     const std::string& name1, LatencyStats& stats1,
                                     const std::string& name2, LatencyStats& stats2) {
  auto format_delta = [](int64_t v1, int64_t v2) -> std::string {
    if (v1 == 0) return "N/A";
    double delta = (static_cast<double>(v2) - static_cast<double>(v1)) / static_cast<double>(v1) * 100.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0);
    if (delta > 0) oss << "+";
    oss << delta << "%";
    if (delta < -10) oss << " ✓";  // Winner marker
    return oss.str();
  };

  std::cout << "\n=== " << scenario << " ===\n";
  std::cout << std::setw(20) << std::left << "Metric"
            << " | " << std::setw(14) << std::right << name1
            << " | " << std::setw(14) << name2
            << " | " << std::setw(10) << "Delta" << "\n";
  std::cout << std::string(20, '-') << "-+-" << std::string(14, '-')
            << "-+-" << std::string(14, '-') << "-+-" << std::string(10, '-') << "\n";

  auto row = [&](const std::string& metric, int64_t v1, int64_t v2) {
    std::cout << std::setw(20) << std::left << metric
              << " | " << std::setw(14) << std::right << v1
              << " | " << std::setw(14) << v2
              << " | " << std::setw(10) << format_delta(v1, v2) << "\n";
  };

  row("p50 latency (ns)", stats1.p50(), stats2.p50());
  row("p95 latency (ns)", stats1.p95(), stats2.p95());
  row("p99 latency (ns)", stats1.p99(), stats2.p99());
  row("max latency (ns)", stats1.max(), stats2.max());

  // Stddev row with floating point
  std::cout << std::setw(20) << std::left << "std deviation"
            << " | " << std::setw(14) << std::right << std::fixed << std::setprecision(1) << stats1.stddev()
            << " | " << std::setw(14) << stats2.stddev()
            << " | " << std::setw(10) << format_delta(static_cast<int64_t>(stats1.stddev()),
                                                       static_cast<int64_t>(stats2.stddev())) << "\n";
}

} // namespace perf

#endif // IQXMLRPC_PERF_UTILS_H

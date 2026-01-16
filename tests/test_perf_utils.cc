#define BOOST_TEST_MODULE perf_utils_test
#include <boost/test/unit_test.hpp>
#include "perf_utils.h"

using namespace boost::unit_test;

// ============================================================================
// LatencyStats Unit Tests
// Tests edge cases and mathematical correctness of statistics collection
// ============================================================================

BOOST_AUTO_TEST_SUITE(latency_stats_tests)

// --- Empty State Tests ---

BOOST_AUTO_TEST_CASE(empty_stats_count)
{
    perf::LatencyStats stats;
    BOOST_CHECK_EQUAL(stats.count(), 0u);
}

BOOST_AUTO_TEST_CASE(empty_stats_percentiles_return_zero)
{
    perf::LatencyStats stats;
    BOOST_CHECK_EQUAL(stats.p50(), 0);
    BOOST_CHECK_EQUAL(stats.p95(), 0);
    BOOST_CHECK_EQUAL(stats.p99(), 0);
    BOOST_CHECK_EQUAL(stats.percentile(0.5), 0);
}

BOOST_AUTO_TEST_CASE(empty_stats_min_max_return_zero)
{
    perf::LatencyStats stats;
    BOOST_CHECK_EQUAL(stats.min(), 0);
    BOOST_CHECK_EQUAL(stats.max(), 0);
}

BOOST_AUTO_TEST_CASE(empty_stats_mean_returns_zero)
{
    perf::LatencyStats stats;
    BOOST_CHECK_EQUAL(stats.mean(), 0.0);
}

BOOST_AUTO_TEST_CASE(empty_stats_stddev_returns_zero)
{
    perf::LatencyStats stats;
    BOOST_CHECK_EQUAL(stats.stddev(), 0.0);
}

// --- Single Sample Tests ---

BOOST_AUTO_TEST_CASE(single_sample_count)
{
    perf::LatencyStats stats;
    stats.add(100);
    BOOST_CHECK_EQUAL(stats.count(), 1u);
}

BOOST_AUTO_TEST_CASE(single_sample_all_percentiles_equal)
{
    perf::LatencyStats stats;
    stats.add(100);
    BOOST_CHECK_EQUAL(stats.p50(), 100);
    BOOST_CHECK_EQUAL(stats.p95(), 100);
    BOOST_CHECK_EQUAL(stats.p99(), 100);
    BOOST_CHECK_EQUAL(stats.min(), 100);
    BOOST_CHECK_EQUAL(stats.max(), 100);
}

BOOST_AUTO_TEST_CASE(single_sample_mean)
{
    perf::LatencyStats stats;
    stats.add(100);
    BOOST_CHECK_EQUAL(stats.mean(), 100.0);
}

BOOST_AUTO_TEST_CASE(single_sample_stddev_zero)
{
    // With only one sample, stddev should be 0 (n-1 = 0 in denominator)
    perf::LatencyStats stats;
    stats.add(100);
    BOOST_CHECK_EQUAL(stats.stddev(), 0.0);
}

// --- Two Sample Tests ---

BOOST_AUTO_TEST_CASE(two_samples_min_max)
{
    perf::LatencyStats stats;
    stats.add(100);
    stats.add(200);
    BOOST_CHECK_EQUAL(stats.min(), 100);
    BOOST_CHECK_EQUAL(stats.max(), 200);
}

BOOST_AUTO_TEST_CASE(two_samples_mean)
{
    perf::LatencyStats stats;
    stats.add(100);
    stats.add(200);
    BOOST_CHECK_EQUAL(stats.mean(), 150.0);
}

BOOST_AUTO_TEST_CASE(two_samples_stddev)
{
    // stddev = sqrt(((100-150)^2 + (200-150)^2) / (2-1))
    //        = sqrt((2500 + 2500) / 1) = sqrt(5000) ≈ 70.71
    perf::LatencyStats stats;
    stats.add(100);
    stats.add(200);
    BOOST_CHECK_CLOSE(stats.stddev(), 70.71, 1.0);  // 1% tolerance
}

// --- Percentile Boundary Tests ---

BOOST_AUTO_TEST_CASE(percentile_boundaries)
{
    perf::LatencyStats stats;
    stats.add(10);
    stats.add(20);
    stats.add(30);

    // p=0.0 should return first element (min)
    BOOST_CHECK_EQUAL(stats.percentile(0.0), 10);

    // p=1.0 should return last element (max)
    BOOST_CHECK_EQUAL(stats.percentile(1.0), 30);
}

BOOST_AUTO_TEST_CASE(percentile_interpolation)
{
    perf::LatencyStats stats;
    for (int i = 1; i <= 100; ++i) {
        stats.add(i);
    }

    // With 100 samples, p50 should be around 50
    BOOST_CHECK_EQUAL(stats.p50(), 50);

    // p90 should be around 90
    BOOST_CHECK_EQUAL(stats.p90(), 90);

    // p95 should be around 95
    BOOST_CHECK_EQUAL(stats.p95(), 95);

    // p99 should be around 99
    BOOST_CHECK_EQUAL(stats.p99(), 99);
}

// --- Sorting Tests ---

BOOST_AUTO_TEST_CASE(unsorted_input_sorted_correctly)
{
    perf::LatencyStats stats;
    stats.add(300);
    stats.add(100);
    stats.add(200);

    BOOST_CHECK_EQUAL(stats.min(), 100);
    BOOST_CHECK_EQUAL(stats.max(), 300);
    BOOST_CHECK_EQUAL(stats.p50(), 200);
}

BOOST_AUTO_TEST_CASE(sort_is_idempotent)
{
    perf::LatencyStats stats;
    stats.add(30);
    stats.add(10);
    stats.add(20);

    stats.sort();
    BOOST_CHECK_EQUAL(stats.min(), 10);

    stats.sort();  // Should be no-op
    BOOST_CHECK_EQUAL(stats.min(), 10);
    BOOST_CHECK_EQUAL(stats.max(), 30);
}

// --- Clear Tests ---

BOOST_AUTO_TEST_CASE(clear_resets_stats)
{
    perf::LatencyStats stats;
    stats.add(100);
    stats.add(200);
    BOOST_CHECK_EQUAL(stats.count(), 2u);

    stats.clear();
    BOOST_CHECK_EQUAL(stats.count(), 0u);
    BOOST_CHECK_EQUAL(stats.p50(), 0);
    BOOST_CHECK_EQUAL(stats.mean(), 0.0);
}

BOOST_AUTO_TEST_CASE(clear_allows_reuse)
{
    perf::LatencyStats stats;
    stats.add(100);
    stats.clear();
    stats.add(500);

    BOOST_CHECK_EQUAL(stats.count(), 1u);
    BOOST_CHECK_EQUAL(stats.p50(), 500);
}

// --- Reserve Tests ---

BOOST_AUTO_TEST_CASE(reserve_then_add)
{
    perf::LatencyStats stats;
    stats.reserve(1000);

    for (int i = 0; i < 100; ++i) {
        stats.add(i);
    }

    BOOST_CHECK_EQUAL(stats.count(), 100u);
    BOOST_CHECK_EQUAL(stats.min(), 0);
    BOOST_CHECK_EQUAL(stats.max(), 99);
}

// --- Statistical Accuracy Tests ---

BOOST_AUTO_TEST_CASE(mean_accuracy_large_sample)
{
    perf::LatencyStats stats;
    // Add values 1 to 1000: mean should be 500.5
    for (int i = 1; i <= 1000; ++i) {
        stats.add(i);
    }
    BOOST_CHECK_CLOSE(stats.mean(), 500.5, 0.01);  // 0.01% tolerance
}

BOOST_AUTO_TEST_CASE(stddev_accuracy_uniform_distribution)
{
    perf::LatencyStats stats;
    // Uniform distribution 1 to 1000
    // stddev = sqrt((n^2 - 1) / 12) for uniform 1..n
    // For n=1000: sqrt(999999/12) ≈ 288.68
    for (int i = 1; i <= 1000; ++i) {
        stats.add(i);
    }
    BOOST_CHECK_CLOSE(stats.stddev(), 288.82, 1.0);  // 1% tolerance
}

BOOST_AUTO_TEST_CASE(stddev_zero_variance)
{
    // All same values should give stddev = 0
    perf::LatencyStats stats;
    for (int i = 0; i < 100; ++i) {
        stats.add(42);
    }
    BOOST_CHECK_EQUAL(stats.mean(), 42.0);
    BOOST_CHECK_CLOSE(stats.stddev(), 0.0, 0.001);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// BenchmarkResult Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(benchmark_result_tests)

BOOST_AUTO_TEST_CASE(benchmark_result_basic)
{
    perf::BenchmarkResult r("test_bench", 1000.0, 1000);
    BOOST_CHECK_EQUAL(r.name, "test_bench");
    BOOST_CHECK_EQUAL(r.total_ms, 1000.0);
    BOOST_CHECK_EQUAL(r.iterations, 1000u);
    // ns_per_op = (1000.0 * 1000000.0) / 1000 = 1000000 ns/op
    BOOST_CHECK_EQUAL(r.ns_per_op, 1000000.0);
}

BOOST_AUTO_TEST_CASE(benchmark_result_ns_per_op_calculation)
{
    // 100ms for 10000 iterations = 10000 ns/op
    perf::BenchmarkResult r("fast_op", 100.0, 10000);
    BOOST_CHECK_CLOSE(r.ns_per_op, 10000.0, 0.01);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ResultCollector Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(result_collector_tests)

BOOST_AUTO_TEST_CASE(result_collector_singleton)
{
    auto& inst1 = perf::ResultCollector::instance();
    auto& inst2 = perf::ResultCollector::instance();
    BOOST_CHECK_EQUAL(&inst1, &inst2);
}

BOOST_AUTO_TEST_CASE(result_collector_add_and_clear)
{
    auto& collector = perf::ResultCollector::instance();
    collector.clear();
    collector.start_suite();

    // Add a result (this also prints to console)
    perf::BenchmarkResult r("test_result", 50.0, 100);
    collector.add_result(r);

    // Clear for next test
    collector.clear();
}

BOOST_AUTO_TEST_CASE(result_collector_save_baseline)
{
    auto& collector = perf::ResultCollector::instance();
    collector.clear();
    collector.start_suite();

    perf::BenchmarkResult r1("bench1", 100.0, 1000);
    perf::BenchmarkResult r2("bench2", 200.0, 2000);
    collector.add_result(r1);
    collector.add_result(r2);

    // Save to a temp file
    std::string temp_file = "/tmp/test_baseline.txt";
    collector.save_baseline(temp_file);

    // Verify file was created
    std::ifstream in(temp_file);
    BOOST_CHECK(in.good());

    // Cleanup
    collector.clear();
    std::remove(temp_file.c_str());
}

BOOST_AUTO_TEST_CASE(result_collector_save_baseline_invalid_path)
{
    auto& collector = perf::ResultCollector::instance();
    collector.clear();
    collector.start_suite();

    // Try to save to an invalid path (should print warning but not crash)
    collector.save_baseline("/nonexistent/directory/file.txt");

    collector.clear();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Timer Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(timer_tests)

BOOST_AUTO_TEST_CASE(timer_basic_usage)
{
    auto& collector = perf::ResultCollector::instance();
    collector.clear();
    collector.start_suite();

    {
        perf::Timer timer("timer_test", 100);
        // Do some trivial work
        volatile int x = 0;
        for (int i = 0; i < 100; ++i) x += i;
        (void)x;
    }
    // Timer destructor should have added a result

    collector.clear();
}

BOOST_AUTO_TEST_CASE(timer_explicit_stop)
{
    auto& collector = perf::ResultCollector::instance();
    collector.clear();
    collector.start_suite();

    {
        perf::Timer timer("explicit_stop_test", 50);
        timer.stop();  // Explicit stop
        timer.stop();  // Second stop should be no-op
    }
    // Destructor should also be safe (no double-add)

    collector.clear();
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Utility Function Tests
// ============================================================================

BOOST_AUTO_TEST_SUITE(utility_function_tests)

BOOST_AUTO_TEST_CASE(section_prints_header)
{
    // Just verify it doesn't crash
    perf::section("Test Section");
}

BOOST_AUTO_TEST_CASE(do_not_optimize_various_types)
{
    int x = 42;
    double y = 3.14;
    std::string s = "test";

    // These should compile and not crash
    perf::do_not_optimize(x);
    perf::do_not_optimize(y);
    perf::do_not_optimize(s);
}

BOOST_AUTO_TEST_CASE(print_latency_comparison_basic)
{
    perf::LatencyStats stats1;
    perf::LatencyStats stats2;

    // Add some data
    for (int i = 1; i <= 100; ++i) {
        stats1.add(i * 10);
        stats2.add(i * 8);  // stats2 is 20% faster
    }

    // Should print comparison table
    perf::print_latency_comparison("Test Scenario", "Baseline", stats1, "Improved", stats2);
}

BOOST_AUTO_TEST_CASE(print_latency_comparison_empty_stats)
{
    perf::LatencyStats stats1;
    perf::LatencyStats stats2;

    // Should handle empty stats gracefully (division by zero protection)
    perf::print_latency_comparison("Empty Stats", "A", stats1, "B", stats2);
}

BOOST_AUTO_TEST_SUITE_END()

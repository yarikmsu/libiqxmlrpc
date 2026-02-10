//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2026 Yaroslav Gorbunov
//
//  Thread safety tests for global ValueOptions atomics (std::atomic).
//  Separated into its own binary so TSan can validate these tests in CI.
//  The Boost.Lockfree tests in test_thread_safety.cc produce TSan false
//  positives and are excluded via the "lockfree_stress" label; std::atomic
//  operations are fully TSan-instrumented, so these tests run clean.

#define BOOST_TEST_MODULE value_options_thread_safety_test

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <thread>
#include <vector>

#include "test_common.h"
#include "test_utils.h"

using namespace iqxmlrpc;

BOOST_AUTO_TEST_SUITE(value_options_thread_safety)

// Verify concurrent reads/writes of omit_string_tag_in_responses are race-free
BOOST_AUTO_TEST_CASE(concurrent_omit_string_tag_no_race)
{
  iqxmlrpc::test::OmitStringTagGuard guard;

  constexpr size_t NUM_WRITERS = 4;
  constexpr size_t NUM_READERS = 4;
  constexpr size_t ITERATIONS = 50000;

  std::atomic<bool> stop{false};
  std::atomic<size_t> reads{0};

  // Writers toggle the flag
  auto writer = [&](bool initial) {
    bool val = initial;
    for (size_t i = 0; i < ITERATIONS; ++i) {
      Value::omit_string_tag_in_responses(val);
      val = !val;
    }
  };

  // Readers observe the flag
  auto reader = [&]() {
    while (!stop.load(std::memory_order_acquire)) {
      bool v = Value::omit_string_tag_in_responses();
      (void)v; // suppress unused-variable warning; load exercises TSan
      reads.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(NUM_WRITERS + NUM_READERS);

  for (size_t i = 0; i < NUM_READERS; ++i)
    threads.emplace_back(reader);
  for (size_t i = 0; i < NUM_WRITERS; ++i)
    threads.emplace_back(writer, i % 2 == 0);

  // Wait for writers to finish
  for (size_t i = NUM_READERS; i < threads.size(); ++i)
    threads[i].join();

  stop.store(true, std::memory_order_release);
  for (size_t i = 0; i < NUM_READERS; ++i)
    threads[i].join();

  BOOST_CHECK_GT(reads.load(), 0u);
  THREAD_SAFE_TEST_MESSAGE("omit_string_tag race test: " +
    std::to_string(reads.load()) + " reads completed");
}

// Verify concurrent set/drop/get of default_int are race-free
BOOST_AUTO_TEST_CASE(concurrent_default_int_no_race)
{
  iqxmlrpc::test::DefaultIntGuard guard;  // RAII: drops default on scope exit

  constexpr size_t NUM_WRITERS = 4;
  constexpr size_t NUM_READERS = 4;
  constexpr size_t ITERATIONS = 50000;
  constexpr int VAL_A = 42;
  constexpr int VAL_B = 99;

  std::atomic<bool> stop{false};
  std::atomic<size_t> reads{0};
  std::atomic<size_t> valid_reads{0};
  // BOOST_CHECK is not thread-safe; capture first bad value via atomic instead
  std::atomic<int> bad_value{0};

  // Writers alternate between set (two distinct values) and drop
  auto writer = [&](bool start_with_set) {
    bool do_set = start_with_set;
    for (size_t i = 0; i < ITERATIONS; ++i) {
      if (do_set)
        Value::set_default_int(i % 2 == 0 ? VAL_A : VAL_B);
      else
        Value::drop_default_int();
      do_set = !do_set;
    }
  };

  // Readers call get_default_int and verify the result is one of the valid values
  auto reader = [&]() {
    while (!stop.load(std::memory_order_acquire)) {
      Int* result = Value::get_default_int();
      if (result) {
        int v = result->value();
        if (v != VAL_A && v != VAL_B) {
          int expected = 0;
          bad_value.compare_exchange_strong(expected, v, std::memory_order_relaxed);
        }
        valid_reads.fetch_add(1, std::memory_order_relaxed);
        delete result;
      }
      reads.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(NUM_WRITERS + NUM_READERS);

  for (size_t i = 0; i < NUM_READERS; ++i)
    threads.emplace_back(reader);
  for (size_t i = 0; i < NUM_WRITERS; ++i)
    threads.emplace_back(writer, i % 2 == 0);

  for (size_t i = NUM_READERS; i < threads.size(); ++i)
    threads[i].join();

  stop.store(true, std::memory_order_release);
  for (size_t i = 0; i < NUM_READERS; ++i)
    threads[i].join();

  // Assert after all threads joined (BOOST_CHECK is not thread-safe)
  BOOST_CHECK_MESSAGE(bad_value.load() == 0,
    "Unexpected value observed: " + std::to_string(bad_value.load()) +
    " (expected " + std::to_string(VAL_A) + " or " + std::to_string(VAL_B) + ")");
  BOOST_CHECK_GT(reads.load(), 0u);
  BOOST_CHECK_GT(valid_reads.load(), 0u);
  THREAD_SAFE_TEST_MESSAGE("default_int race test: " +
    std::to_string(reads.load()) + " total reads, " +
    std::to_string(valid_reads.load()) + " with value");
}

// Verify concurrent set/drop/get of default_int64 are race-free
BOOST_AUTO_TEST_CASE(concurrent_default_int64_no_race)
{
  iqxmlrpc::test::DefaultInt64Guard guard;  // RAII: drops default on scope exit

  constexpr size_t NUM_WRITERS = 4;
  constexpr size_t NUM_READERS = 4;
  constexpr size_t ITERATIONS = 50000;
  constexpr int64_t VAL_A = 9876543210LL;
  constexpr int64_t VAL_B = 1234567890LL;

  std::atomic<bool> stop{false};
  std::atomic<size_t> reads{0};
  std::atomic<size_t> valid_reads{0};
  // BOOST_CHECK is not thread-safe; capture first bad value via atomic instead
  std::atomic<int64_t> bad_value{0};

  // Writers alternate between set (two distinct values) and drop
  auto writer = [&](bool start_with_set) {
    bool do_set = start_with_set;
    for (size_t i = 0; i < ITERATIONS; ++i) {
      if (do_set)
        Value::set_default_int64(i % 2 == 0 ? VAL_A : VAL_B);
      else
        Value::drop_default_int64();
      do_set = !do_set;
    }
  };

  // Readers call get_default_int64 and verify the result is one of the valid values
  auto reader = [&]() {
    while (!stop.load(std::memory_order_acquire)) {
      Int64* result = Value::get_default_int64();
      if (result) {
        int64_t v = result->value();
        if (v != VAL_A && v != VAL_B) {
          int64_t expected = 0;
          bad_value.compare_exchange_strong(expected, v, std::memory_order_relaxed);
        }
        valid_reads.fetch_add(1, std::memory_order_relaxed);
        delete result;
      }
      reads.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(NUM_WRITERS + NUM_READERS);

  for (size_t i = 0; i < NUM_READERS; ++i)
    threads.emplace_back(reader);
  for (size_t i = 0; i < NUM_WRITERS; ++i)
    threads.emplace_back(writer, i % 2 == 0);

  for (size_t i = NUM_READERS; i < threads.size(); ++i)
    threads[i].join();

  stop.store(true, std::memory_order_release);
  for (size_t i = 0; i < NUM_READERS; ++i)
    threads[i].join();

  BOOST_CHECK_MESSAGE(bad_value.load() == 0,
    "Unexpected value observed: " + std::to_string(bad_value.load()) +
    " (expected " + std::to_string(VAL_A) + " or " + std::to_string(VAL_B) + ")");
  BOOST_CHECK_GT(reads.load(), 0u);
  BOOST_CHECK_GT(valid_reads.load(), 0u);
  THREAD_SAFE_TEST_MESSAGE("default_int64 race test: " +
    std::to_string(reads.load()) + " total reads, " +
    std::to_string(valid_reads.load()) + " with value");
}

BOOST_AUTO_TEST_SUITE_END()

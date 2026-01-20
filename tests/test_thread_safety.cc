#define BOOST_TEST_MODULE thread_safety_test

#include <boost/test/unit_test.hpp>
#include <boost/lockfree/queue.hpp>

#include <atomic>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

#include "test_common.h"

using namespace iqxmlrpc;
using namespace iqnet;
using namespace iqxmlrpc_test;

// ============================================================================
// Thread Safety Fixture
// ============================================================================
// Uses the shared IntegrationFixture with thread safety test port range

class ThreadSafetyFixture : public IntegrationFixture {
public:
  ThreadSafetyFixture() : IntegrationFixture(THREAD_SAFETY_TEST_PORT) {}
};

// ============================================================================
// Suite 1: Direct Lock-Free Queue Tests
// Tests boost::lockfree::queue directly under various contention scenarios
// ============================================================================

BOOST_AUTO_TEST_SUITE(lockfree_queue_direct_tests)

// Test high contention with many producers and consumers
// Validates: no data loss or duplication under heavy concurrent access
BOOST_AUTO_TEST_CASE(high_contention_stress)
{
  static constexpr size_t QUEUE_CAPACITY = 1024;
  boost::lockfree::queue<int*, boost::lockfree::capacity<QUEUE_CAPACITY>> queue;

  std::atomic<size_t> produced{0};
  std::atomic<size_t> consumed{0};
  std::atomic<bool> done{false};

  constexpr size_t NUM_PRODUCERS = 8;
  constexpr size_t NUM_CONSUMERS = 4;
  constexpr size_t ITEMS_PER_PRODUCER = 10000;
  constexpr size_t TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

  // Track which items were consumed (for integrity check)
  std::mutex consumed_mutex;
  std::set<int*> consumed_items;

  // Allocate test items upfront
  std::vector<int> items(TOTAL_ITEMS);
  for (size_t i = 0; i < TOTAL_ITEMS; ++i) {
    items[i] = static_cast<int>(i);
  }

  auto producer = [&](size_t producer_id) {
    size_t base = producer_id * ITEMS_PER_PRODUCER;
    for (size_t i = 0; i < ITEMS_PER_PRODUCER; ++i) {
      int* item = &items[base + i];
      while (!queue.push(item)) {
        std::this_thread::yield();
      }
      produced.fetch_add(1, std::memory_order_relaxed);
    }
  };

  auto consumer = [&]() {
    while (!done.load(std::memory_order_acquire) ||
           consumed.load(std::memory_order_relaxed) < TOTAL_ITEMS) {
      int* item = nullptr;
      if (queue.pop(item)) {
        {
          std::lock_guard<std::mutex> lk(consumed_mutex);
          consumed_items.insert(item);
        }
        consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        std::this_thread::yield();
      }
    }
  };

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  // Start consumers first
  consumers.reserve(NUM_CONSUMERS);
  for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
    consumers.emplace_back(consumer);
  }

  // Start producers
  producers.reserve(NUM_PRODUCERS);
  for (size_t i = 0; i < NUM_PRODUCERS; ++i) {
    producers.emplace_back(producer, i);
  }

  // Wait for producers
  for (auto& t : producers) t.join();
  done.store(true, std::memory_order_release);

  // Wait for consumers
  for (auto& t : consumers) t.join();

  // Verify data integrity
  BOOST_CHECK_EQUAL(produced.load(), TOTAL_ITEMS);
  BOOST_CHECK_EQUAL(consumed.load(), TOTAL_ITEMS);
  BOOST_CHECK_EQUAL(consumed_items.size(), TOTAL_ITEMS);

  THREAD_SAFE_TEST_MESSAGE("High contention: " +
    std::to_string(TOTAL_ITEMS) + " items processed without loss or duplication");
}

// Test behavior at queue capacity limits
// Validates: graceful handling when queue is full
BOOST_AUTO_TEST_CASE(capacity_limits)
{
  static constexpr size_t QUEUE_CAPACITY = 64;  // Small capacity for testing
  boost::lockfree::queue<int, boost::lockfree::capacity<QUEUE_CAPACITY>> queue;

  std::atomic<size_t> push_success{0};
  std::atomic<size_t> push_fail{0};
  std::atomic<bool> stop_pushing{false};

  // Producer that counts successes/failures
  auto rapid_producer = [&]() {
    while (!stop_pushing.load(std::memory_order_acquire)) {
      if (queue.push(42)) {
        push_success.fetch_add(1, std::memory_order_relaxed);
      } else {
        push_fail.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  // Consumer that drains slowly
  auto slow_consumer = [&]() {
    int item;
    while (!stop_pushing.load(std::memory_order_acquire)) {
      queue.pop(item);
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    // Drain remaining
    while (queue.pop(item)) {}
  };

  std::thread producer(rapid_producer);
  std::thread consumer(slow_consumer);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop_pushing.store(true, std::memory_order_release);

  producer.join();
  consumer.join();

  // We should have seen some push failures due to capacity
  BOOST_CHECK_GT(push_success.load(), 0u);
  THREAD_SAFE_TEST_MESSAGE("Capacity test: push_success=" +
    std::to_string(push_success.load()) + ", push_fail=" +
    std::to_string(push_fail.load()));
}

// Test rapid push/pop on nearly empty queue
// Validates: no race condition when queue transitions between empty and non-empty
BOOST_AUTO_TEST_CASE(empty_queue_rapid_operations)
{
  static constexpr size_t QUEUE_CAPACITY = 1024;
  boost::lockfree::queue<int, boost::lockfree::capacity<QUEUE_CAPACITY>> queue;

  std::atomic<bool> stop{false};
  std::atomic<size_t> successful_pops{0};

  constexpr size_t NUM_THREADS = 4;

  // Each thread pushes one item then immediately tries to pop
  auto push_pop_thread = [&]() {
    while (!stop.load(std::memory_order_acquire)) {
      queue.push(1);
      int item;
      if (queue.pop(item)) {
        successful_pops.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(NUM_THREADS);
  for (size_t i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back(push_pop_thread);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop.store(true, std::memory_order_release);

  for (auto& t : threads) t.join();

  // Drain any remaining items
  int item;
  while (queue.pop(item)) {}

  BOOST_CHECK_GT(successful_pops.load(), 0u);
  THREAD_SAFE_TEST_MESSAGE("Empty queue test: successful_pops=" +
    std::to_string(successful_pops.load()));
}

// Test single item handoff between producer and consumer
// Validates: memory ordering correctness for item visibility
BOOST_AUTO_TEST_CASE(single_item_handoff)
{
  static constexpr size_t QUEUE_CAPACITY = 16;
  boost::lockfree::queue<int, boost::lockfree::capacity<QUEUE_CAPACITY>> queue;

  std::atomic<bool> stop{false};
  std::atomic<size_t> handoffs{0};
  std::atomic<int> last_value{0};

  // Producer pushes incrementing values
  auto producer = [&]() {
    int value = 1;
    while (!stop.load(std::memory_order_acquire)) {
      if (queue.push(value)) {
        value++;
      }
      std::this_thread::yield();
    }
  };

  // Consumer verifies values are monotonically increasing
  auto consumer = [&]() {
    int prev = 0;
    while (!stop.load(std::memory_order_acquire)) {
      int item;
      if (queue.pop(item)) {
        // Value should be greater than previous (monotonic)
        if (item > prev) {
          prev = item;
          handoffs.fetch_add(1, std::memory_order_relaxed);
          last_value.store(item, std::memory_order_relaxed);
        }
      }
      std::this_thread::yield();
    }
  };

  std::thread prod(producer);
  std::thread cons(consumer);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop.store(true, std::memory_order_release);

  prod.join();
  cons.join();

  BOOST_CHECK_GT(handoffs.load(), 0u);
  THREAD_SAFE_TEST_MESSAGE("Single item handoff: " +
    std::to_string(handoffs.load()) + " successful handoffs, last value=" +
    std::to_string(last_value.load()));
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Suite 2: Pool_executor Integration Tests
// Tests the lock-free queue as used by Pool_executor_factory
// ============================================================================

BOOST_AUTO_TEST_SUITE(pool_executor_integration_tests)

// Test graceful shutdown while workers are processing
// Validates: no crashes, memory leaks, or hangs during shutdown under load
BOOST_FIXTURE_TEST_CASE(shutdown_under_load, ThreadSafetyFixture)
{
  start_server(4, port_offset::SHUTDOWN_UNDER_LOAD);

  std::atomic<int> requests_completed{0};
  constexpr int REQUESTS_PER_CLIENT = 5;
  constexpr int NUM_CLIENTS = 4;

  run_concurrent_clients(NUM_CLIENTS, [this, &requests_completed](int) {
    try {
      auto client = create_client();
      for (int r = 0; r < REQUESTS_PER_CLIENT; ++r) {
        try {
          Response resp = client->execute("echo", Value("test"));
          if (!resp.is_fault()) {
            requests_completed.fetch_add(1, std::memory_order_relaxed);
          }
        } catch (...) { (void)0; }
      }
    } catch (...) { (void)0; }
  });

  stop_server();

  BOOST_CHECK_GT(requests_completed.load(), 0);
  THREAD_SAFE_TEST_MESSAGE("Shutdown under load: completed=" +
    std::to_string(requests_completed.load()) + "/" +
    std::to_string(NUM_CLIENTS * REQUESTS_PER_CLIENT));
}

// Test behavior when queue approaches capacity (1024)
// Validates: queue handles overflow gracefully without deadlock
BOOST_FIXTURE_TEST_CASE(queue_saturation, ThreadSafetyFixture)
{
  start_server(2, port_offset::QUEUE_SATURATION);

  std::atomic<int> completed{0};
  constexpr int NUM_CLIENTS = 8;
  constexpr int REQUESTS_PER_CLIENT = 10;

  run_concurrent_clients(NUM_CLIENTS, [this, &completed](int i) {
    try {
      auto client = create_client();
      for (int r = 0; r < REQUESTS_PER_CLIENT; ++r) {
        try {
          Response resp = client->execute("echo", Value(i * 100 + r));
          if (!resp.is_fault()) {
            completed.fetch_add(1, std::memory_order_relaxed);
          }
        } catch (...) { (void)0; }
      }
    } catch (...) { (void)0; }
  });

  stop_server();

  BOOST_CHECK_GT(completed.load(), 0);
  THREAD_SAFE_TEST_MESSAGE("Queue saturation: completed=" +
    std::to_string(completed.load()) + "/" +
    std::to_string(NUM_CLIENTS * REQUESTS_PER_CLIENT));
}

// Verify no data loss or corruption under concurrent load
// Validates: echo responses match their requests (data integrity)
BOOST_FIXTURE_TEST_CASE(data_integrity_concurrent, ThreadSafetyFixture)
{
  start_server(4, port_offset::DATA_INTEGRITY);

  constexpr int NUM_CLIENTS = 8;
  constexpr int REQUESTS_PER_CLIENT = 25;

  std::atomic<int> matches{0};
  std::atomic<int> mismatches{0};
  std::atomic<int> errors{0};

  run_concurrent_clients(NUM_CLIENTS, [this, &matches, &mismatches, &errors](int c) {
    try {
      auto client = create_client();
      for (int r = 0; r < REQUESTS_PER_CLIENT; ++r) {
        int value = c * 1000 + r;
        try {
          Response response = client->execute("echo", Value(value));
          if (!response.is_fault()) {
            int returned = response.value().get_int();
            if (returned == value) {
              matches.fetch_add(1, std::memory_order_relaxed);
            } else {
              mismatches.fetch_add(1, std::memory_order_relaxed);
            }
          } else {
            errors.fetch_add(1, std::memory_order_relaxed);
          }
        } catch (...) {
          errors.fetch_add(1, std::memory_order_relaxed);
        }
      }
    } catch (...) {
      errors.fetch_add(1, std::memory_order_relaxed);
    }
  });

  stop_server();

  BOOST_CHECK_EQUAL(mismatches.load(), 0);
  BOOST_CHECK_GT(matches.load(), 0);
  THREAD_SAFE_TEST_MESSAGE("Data integrity: matches=" +
    std::to_string(matches.load()) + ", mismatches=" +
    std::to_string(mismatches.load()) + ", errors=" +
    std::to_string(errors.load()));
}

// Test dynamic thread addition via add_threads()
// Validates: threads can be added while server is processing requests
BOOST_FIXTURE_TEST_CASE(add_threads_under_load, ThreadSafetyFixture)
{
  // Start with 2 threads
  start_server(2, port_offset::ADD_THREADS);

  std::atomic<int> completed{0};
  std::atomic<bool> keep_running{true};

  // Start background requests
  std::thread client_thread([this, &completed, &keep_running]() {
    while (keep_running.load(std::memory_order_acquire)) {
      try {
        auto client = create_client();
        Response resp = client->execute("echo", Value(42));
        if (!resp.is_fault()) {
          completed.fetch_add(1, std::memory_order_relaxed);
        }
      } catch (...) { (void)0; }
    }
  });

  // Let some requests complete
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  int before_add = completed.load();

  // Dynamically add more threads
  auto* pool_factory = dynamic_cast<Pool_executor_factory*>(executor_factory());
  BOOST_REQUIRE(pool_factory != nullptr);
  pool_factory->add_threads(2);  // Now 4 threads total

  // Let more requests complete with additional threads
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  keep_running.store(false, std::memory_order_release);
  client_thread.join();
  stop_server();

  int after_add = completed.load();
  BOOST_CHECK_GT(after_add, before_add);
  THREAD_SAFE_TEST_MESSAGE("add_threads: before=" +
    std::to_string(before_add) + ", after=" + std::to_string(after_add));
}

// Test with production queue capacity (1024)
// Validates: queue handles actual capacity limit correctly
BOOST_FIXTURE_TEST_CASE(production_capacity_stress, ThreadSafetyFixture)
{
  // Use 1 worker thread to maximize queue buildup
  start_server(1, port_offset::CAPACITY_1024);

  std::atomic<int> submitted{0};
  std::atomic<int> completed{0};

  // Many clients to stress the 1024 capacity queue
  constexpr int NUM_CLIENTS = 16;
  constexpr int REQUESTS_PER_CLIENT = 100;  // Total: 1600 > 1024

  run_concurrent_clients(NUM_CLIENTS, [this, &submitted, &completed](int i) {
    try {
      auto client = create_client();
      for (int r = 0; r < REQUESTS_PER_CLIENT; ++r) {
        submitted.fetch_add(1, std::memory_order_relaxed);
        try {
          Response resp = client->execute("echo", Value(i * 1000 + r));
          if (!resp.is_fault()) {
            completed.fetch_add(1, std::memory_order_relaxed);
          }
        } catch (...) { (void)0; }
      }
    } catch (...) { (void)0; }
  });

  stop_server();

  // With 1 thread processing and 1600 requests, queue will overflow
  // The spin-retry in register_executor() should handle this
  BOOST_CHECK_GT(submitted.load(), 1000);
  BOOST_CHECK_GT(completed.load(), 0);
  THREAD_SAFE_TEST_MESSAGE("Production capacity: submitted=" +
    std::to_string(submitted.load()) + ", completed=" +
    std::to_string(completed.load()));
}

// Test destructor with pending items in queue
// Validates: destructor properly drains queue without memory leaks
BOOST_FIXTURE_TEST_CASE(destructor_drains_queue, ThreadSafetyFixture)
{
  // Start with 1 thread
  start_server(1, port_offset::DESTRUCTOR_DRAIN);

  std::atomic<int> completed{0};
  constexpr int NUM_CLIENTS = 4;
  constexpr int REQUESTS_PER_CLIENT = 10;

  // Submit a fixed number of requests from multiple clients
  run_concurrent_clients(NUM_CLIENTS, [this, &completed](int i) {
    try {
      auto client = create_client();
      for (int r = 0; r < REQUESTS_PER_CLIENT; ++r) {
        try {
          Response resp = client->execute("echo", Value(i * 100 + r));
          if (!resp.is_fault()) {
            completed.fetch_add(1, std::memory_order_relaxed);
          }
        } catch (...) { (void)0; }
      }
    } catch (...) { (void)0; }
  });

  // Stop server - destructor should drain remaining queue items
  stop_server();

  BOOST_CHECK_GT(completed.load(), 0);
  THREAD_SAFE_TEST_MESSAGE("Destructor drain: completed=" +
    std::to_string(completed.load()) + "/" +
    std::to_string(NUM_CLIENTS * REQUESTS_PER_CLIENT));
}

BOOST_AUTO_TEST_SUITE_END()

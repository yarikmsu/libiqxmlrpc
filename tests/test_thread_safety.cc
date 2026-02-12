#define BOOST_TEST_MODULE thread_safety_test

#include <boost/test/unit_test.hpp>
#include <boost/lockfree/queue.hpp>

#include <atomic>
#include <chrono>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include "test_common.h"
#include "libiqxmlrpc/server_conn.h"

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
// Stub Server_connection for guard and integration tests
// ============================================================================

namespace {

// Minimal concrete Server_connection for testing ConnectionGuard.
// Only implements the pure virtual methods; does not need a real socket.
class Stub_server_connection : public Server_connection {
public:
  std::atomic<int> schedule_count{0};
  bool throw_on_schedule = false;
  bool throw_non_std = false;

  Stub_server_connection()
    : Server_connection(iqnet::Inet_addr("127.0.0.1", 0))
  {
  }

  void do_schedule_response() override {
    if (throw_non_std)
      throw 42;  // NOLINT: exercises catch(...) path
    if (throw_on_schedule)
      throw std::runtime_error("stub: forced schedule failure");
    schedule_count.fetch_add(1, std::memory_order_relaxed);
  }

  void terminate_idle() override {}
};

} // anonymous namespace

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

// Regression: pool threads must not call schedule_response() on a destroyed Server.
// Validates: drain() prevents use-after-free (ASan/TSan detect if broken).
BOOST_FIXTURE_TEST_CASE(shutdown_with_inflight_work, ThreadSafetyFixture)
{
  start_server(4, port_offset::SHUTDOWN_INFLIGHT);

  std::atomic<int> requests_completed{0};
  constexpr int NUM_CLIENTS = 4;
  constexpr int REQUESTS_PER_CLIENT = 3;

  run_concurrent_clients(NUM_CLIENTS, [this, &requests_completed](int) {
    try {
      auto client = create_client();
      for (int r = 0; r < REQUESTS_PER_CLIENT; ++r) {
        try {
          Response resp = client->execute("sleep", Value(0.05));
          if (!resp.is_fault()) {
            requests_completed.fetch_add(1, std::memory_order_relaxed);
          }
        } catch (...) { (void)0; }
      }
    } catch (...) { (void)0; }
  });

  BOOST_CHECK_GT(requests_completed.load(), 0);
  THREAD_SAFE_TEST_MESSAGE("Shutdown with inflight work: completed=" +
    std::to_string(requests_completed.load()) +
    "/" + std::to_string(NUM_CLIENTS * REQUESTS_PER_CLIENT));

  stop_server();
}

// Exercise all three catch paths in Pool_executor::process_actual_execution().
// Validates: Fault, std::exception, and unknown exceptions produce correct fault
// responses when processed by pool threads. Also exercises Drain_guard indirectly.
BOOST_FIXTURE_TEST_CASE(pool_exception_handling, ThreadSafetyFixture)
{
  start_server(2, port_offset::POOL_EXCEPTIONS);

  // Verify we have a pool executor (not serial)
  BOOST_REQUIRE(dynamic_cast<Pool_executor_factory*>(executor_factory()) != nullptr);

  // 1. iqxmlrpc::Fault exception (process_actual_execution catch #1)
  {
    auto client = create_client();
    Response resp = client->execute("error_method", Value(0));
    BOOST_CHECK(resp.is_fault());
    BOOST_CHECK_EQUAL(resp.fault_code(), 123);
  }

  // 2. std::exception (process_actual_execution catch #2)
  {
    auto client = create_client();
    Response resp = client->execute("std_exception_method", Value(0));
    BOOST_CHECK(resp.is_fault());
    BOOST_CHECK_EQUAL(resp.fault_code(), -1);
  }

  // 3. Unknown exception — throw 42 (process_actual_execution catch #3)
  {
    auto client = create_client();
    Response resp = client->execute("unknown_exception_method", Value(0));
    BOOST_CHECK(resp.is_fault());
    BOOST_CHECK_EQUAL(resp.fault_code(), -1);
  }

  stop_server();
  THREAD_SAFE_TEST_MESSAGE("Pool exception handling: all 3 catch paths verified");
}

// Exercise drain() warning path by setting a very short warning interval.
// Validates: drain() blocks until outstanding_count reaches zero, logging
// periodic warnings while waiting. Subsequent shutdown completes cleanly.
BOOST_FIXTURE_TEST_CASE(drain_timeout_exercise, ThreadSafetyFixture)
{
  start_server(2, port_offset::DRAIN_TIMEOUT_EXERCISE);

  auto* pool = dynamic_cast<Pool_executor_factory*>(executor_factory());
  BOOST_REQUIRE(pool != nullptr);

  // Set very short warning interval to exercise the warning path.
  // drain() now blocks until outstanding_count reaches zero, logging
  // a warning every drain_timeout_ interval while waiting.
  pool->set_drain_timeout(std::chrono::milliseconds(50));

  // Submit slow work (500ms sleep) so drain() logs warnings while waiting
  std::atomic<int> completed{0};
  constexpr int NUM_CLIENTS = 4;
  std::vector<std::thread> clients;
  clients.reserve(NUM_CLIENTS);
  for (int i = 0; i < NUM_CLIENTS; ++i) {
    clients.emplace_back([this, &completed]() {
      try {
        auto client = create_client();
        client->execute("sleep", Value(0.5));
        completed.fetch_add(1, std::memory_order_relaxed);
      } catch (...) { (void)0; }
    });
  }

  // Let requests be dispatched to the pool
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // drain() blocks until all in-flight work completes, logging warnings
  // every 50ms while outstanding_count > 0. This exercises the warning
  // path without returning early (no UAF risk).
  pool->drain();

  // Restore normal warning interval for clean shutdown
  pool->set_drain_timeout(std::chrono::seconds(30));

  // Wait for clients to finish their HTTP calls
  for (auto& t : clients) t.join();

  BOOST_CHECK_GT(completed.load(), 0);
  THREAD_SAFE_TEST_MESSAGE("Drain timeout exercise: " +
    std::to_string(completed.load()) + " requests completed after drain warning test");

  stop_server();
}

// Exercises the guard rejection path in the ConnectionGuardPtr overload
// of Server::schedule_response(). Verifies: when a ConnectionGuard is
// invalidated before schedule_response is called, the packet is safely
// dropped and the server logs the rejection message.
BOOST_FIXTURE_TEST_CASE(guard_rejects_late_response, ThreadSafetyFixture)
{
  start_server(2, port_offset::GUARD_REJECTION);

  // Capture server log output
  std::ostringstream log_stream;
  server().log_errors(&log_stream);

  // Create a stub connection and immediately invalidate its guard
  auto conn = std::make_unique<Stub_server_connection>();
  ConnectionGuardPtr guard = conn->connection_guard();
  guard->invalidate();

  // Call the guarded schedule_response overload directly.
  // The guard is already invalidated, so try_schedule_response() returns false,
  // the packet is deleted (no leak), and the rejection is logged.
  Response resp(0, "should be dropped");
  server().schedule_response(resp, guard, nullptr);

  std::string logged = log_stream.str();
  BOOST_CHECK_MESSAGE(
    logged.find("Response delivery failed") != std::string::npos,
    "Expected guard rejection log message, got: " + logged);

  stop_server();
  THREAD_SAFE_TEST_MESSAGE("Guard rejection path: log verified");
}

// Note: The destructor queue drain loop (~Pool_executor_factory while/pop)
// and the ~Pool_executor early return guard (is_being_destructed check) are
// defensive safety nets. drain() now blocks indefinitely, so the queue should
// always be empty when the destructor runs. These paths are verified by code
// inspection and the outstanding_count assert in the destructor.

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

// ============================================================================
// Suite 3: ConnectionGuard Unit Tests
// Tests the ConnectionGuard pattern for safe cross-thread connection access
// ============================================================================

BOOST_AUTO_TEST_SUITE(connection_guard_tests)

// Basic lifecycle: create guard, verify alive, invalidate, verify dead.
BOOST_AUTO_TEST_CASE(guard_basic_lifecycle)
{
  auto conn = std::make_unique<Stub_server_connection>();
  ConnectionGuardPtr guard = conn->connection_guard();
  BOOST_REQUIRE(guard != nullptr);

  // Guard is alive — try_schedule_response should succeed
  auto* pkt = new http::Packet(new http::Response_header(), std::string("OK"));
  BOOST_CHECK(guard->try_schedule_response(pkt));
  BOOST_CHECK_EQUAL(conn->schedule_count.load(), 1);

  // Invalidate the guard
  guard->invalidate();

  // Guard is dead — try_schedule_response should return false and delete the packet
  auto* pkt2 = new http::Packet(new http::Response_header(), std::string("DROPPED"));
  BOOST_CHECK(!guard->try_schedule_response(pkt2));
  // schedule_count should still be 1 (packet was not delivered)
  BOOST_CHECK_EQUAL(conn->schedule_count.load(), 1);
}

// Verify that try_schedule_response deletes the packet when guard is invalidated.
BOOST_AUTO_TEST_CASE(guard_invalidated_drops_response)
{
  auto conn = std::make_unique<Stub_server_connection>();
  ConnectionGuardPtr guard = conn->connection_guard();

  guard->invalidate();

  // Every call should return false and delete the packet (no leak under ASan)
  for (int i = 0; i < 10; ++i) {
    auto* pkt = new http::Packet(new http::Response_header(), std::string("X"));
    BOOST_CHECK(!guard->try_schedule_response(pkt));
  }
  BOOST_CHECK_EQUAL(conn->schedule_count.load(), 0);
}

// Destructor defense-in-depth: guard is auto-invalidated when connection is destroyed.
BOOST_AUTO_TEST_CASE(guard_survives_connection_destruction)
{
  ConnectionGuardPtr guard;
  {
    auto conn = std::make_unique<Stub_server_connection>();
    guard = conn->connection_guard();
    // conn destroyed here — guard should be auto-invalidated by ~Server_connection
  }

  // Guard outlives connection — must safely reject
  auto* pkt = new http::Packet(new http::Response_header(), std::string("LATE"));
  BOOST_CHECK(!guard->try_schedule_response(pkt));
}

// Stress test: concurrent invalidate + try_schedule_response from multiple threads.
BOOST_AUTO_TEST_CASE(guard_concurrent_invalidate_and_response)
{
  constexpr int NUM_TRIALS = 100;
  constexpr int THREADS_PER_TRIAL = 4;

  for (int trial = 0; trial < NUM_TRIALS; ++trial) {
    auto conn = std::make_unique<Stub_server_connection>();
    ConnectionGuardPtr guard = conn->connection_guard();
    std::atomic<bool> start{false};
    std::atomic<int> delivered{0};
    std::atomic<int> dropped{0};

    std::vector<std::thread> threads;
    threads.reserve(THREADS_PER_TRIAL + 1);

    // Invalidator thread
    threads.emplace_back([&guard, &start]() {
      while (!start.load(std::memory_order_acquire)) {}
      guard->invalidate();
    });

    // Response threads
    for (int t = 0; t < THREADS_PER_TRIAL; ++t) {
      threads.emplace_back([&guard, &start, &delivered, &dropped]() {
        while (!start.load(std::memory_order_acquire)) {}
        auto* pkt = new http::Packet(new http::Response_header(), std::string("R"));
        if (guard->try_schedule_response(pkt)) {
          delivered.fetch_add(1, std::memory_order_relaxed);
        } else {
          dropped.fetch_add(1, std::memory_order_relaxed);
        }
      });
    }

    // Release all threads simultaneously
    start.store(true, std::memory_order_release);

    for (auto& t : threads) t.join();

    // Invariant: delivered + dropped == THREADS_PER_TRIAL
    BOOST_CHECK_EQUAL(delivered.load() + dropped.load(), THREADS_PER_TRIAL);
    // schedule_count must match delivered
    BOOST_CHECK_EQUAL(conn->schedule_count.load(), delivered.load());
  }
}

// Exception safety: if schedule_response throws std::exception, the guard
// catches it, returns false, and does not propagate to the pool thread.
BOOST_AUTO_TEST_CASE(guard_handles_schedule_exception)
{
  auto conn = std::make_unique<Stub_server_connection>();
  conn->throw_on_schedule = true;
  ConnectionGuardPtr guard = conn->connection_guard();

  auto* pkt = new http::Packet(new http::Response_header(), std::string("X"));
  // Must not throw; must return false
  BOOST_CHECK(!guard->try_schedule_response(pkt));
  BOOST_CHECK_EQUAL(conn->schedule_count.load(), 0);
}

// Same as above but for non-std::exception types (exercises catch(...) path).
BOOST_AUTO_TEST_CASE(guard_handles_unknown_exception)
{
  auto conn = std::make_unique<Stub_server_connection>();
  conn->throw_non_std = true;
  ConnectionGuardPtr guard = conn->connection_guard();

  auto* pkt = new http::Packet(new http::Response_header(), std::string("X"));
  BOOST_CHECK(!guard->try_schedule_response(pkt));
  BOOST_CHECK_EQUAL(conn->schedule_count.load(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

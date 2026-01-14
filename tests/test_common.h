#ifndef IQXMLRPC_TEST_COMMON_H
#define IQXMLRPC_TEST_COMMON_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"
#include "libiqxmlrpc/executor.h"

#include "methods.h"

namespace iqxmlrpc_test {

// ============================================================================
// Thread-Safe Test Message Macro
// ============================================================================
// BOOST_TEST_MESSAGE is not thread-safe. Use this macro for multi-threaded tests.

inline std::mutex& test_message_mutex() {
  static std::mutex mtx;
  return mtx;
}

#define THREAD_SAFE_TEST_MESSAGE(msg) \
  do { \
    std::lock_guard<std::mutex> lock(iqxmlrpc_test::test_message_mutex()); \
    BOOST_TEST_MESSAGE(msg); \
  } while(0)

// ============================================================================
// Port Constants
// ============================================================================
// Each test file uses a different port range to avoid conflicts

constexpr int INTEGRATION_TEST_PORT = 19876;    // test_integration.cc
constexpr int THREAD_SAFETY_TEST_PORT = 19950;  // test_thread_safety.cc

// Port offsets for individual tests within a file
namespace port_offset {
  constexpr int SHUTDOWN_UNDER_LOAD = 0;
  constexpr int QUEUE_SATURATION = 1;
  constexpr int DATA_INTEGRITY = 2;
  constexpr int ADD_THREADS = 3;
  constexpr int CAPACITY_1024 = 4;
  constexpr int DESTRUCTOR_DRAIN = 5;
}

// ============================================================================
// Integration Test Fixture
// ============================================================================
// Shared fixture for server/client integration tests.
// Supports both serial (1 thread) and pool executor (multiple threads).

class IntegrationFixture {
protected:
  std::unique_ptr<iqxmlrpc::Http_server> server_;
  std::unique_ptr<iqxmlrpc::Executor_factory_base> exec_factory_;
  std::thread server_thread_;
  std::mutex ready_mutex_;
  std::condition_variable ready_cond_;
  bool server_ready_;
  std::atomic<bool> server_running_;
  int port_;
  int base_port_;

public:
  explicit IntegrationFixture(int base_port = INTEGRATION_TEST_PORT)
    : server_()
    , exec_factory_()
    , server_thread_()
    , ready_mutex_()
    , ready_cond_()
    , server_ready_(false)
    , server_running_(false)
    , port_(base_port)
    , base_port_(base_port) {}

  virtual ~IntegrationFixture() {
    stop_server();
  }

  void start_server(int numthreads = 1, int port_offset = 0) {
    port_ = base_port_ + port_offset;

    if (numthreads > 1) {
      exec_factory_.reset(new iqxmlrpc::Pool_executor_factory(numthreads));
    } else {
      exec_factory_.reset(new iqxmlrpc::Serial_executor_factory);
    }

    server_.reset(new iqxmlrpc::Http_server(
      iqnet::Inet_addr("127.0.0.1", port_),
      exec_factory_.get()));

    // Register test methods
    register_user_methods(*server_);

    server_running_ = true;
    server_thread_ = std::thread([this]() {
      {
        std::unique_lock<std::mutex> lk(ready_mutex_);
        server_ready_ = true;
        ready_cond_.notify_one();
      }
      server_->work();
      server_running_ = false;
    });

    // Wait for server to be ready
    std::unique_lock<std::mutex> lk(ready_mutex_);
    bool result = ready_cond_.wait_for(lk,
      std::chrono::seconds(5),
      [this]{ return server_ready_; });
    BOOST_REQUIRE_MESSAGE(result, "Server startup timeout");

    // Give the server a moment to enter the work loop
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  void stop_server() {
    if (server_ && server_running_) {
      server_->set_exit_flag();
      server_->interrupt();
      if (server_thread_.joinable()) {
        server_thread_.join();
      }
    }
    server_.reset();
    exec_factory_.reset();
    server_ready_ = false;
    server_running_ = false;
  }

  std::unique_ptr<iqxmlrpc::Client_base> create_client() {
    return std::unique_ptr<iqxmlrpc::Client_base>(
      new iqxmlrpc::Client<iqxmlrpc::Http_client_connection>(
        iqnet::Inet_addr("127.0.0.1", port_)));
  }

  iqxmlrpc::Server& server() { return *server_; }
  bool is_running() const { return server_running_; }

  // Access to executor factory for advanced tests (e.g., add_threads)
  iqxmlrpc::Executor_factory_base* executor_factory() { return exec_factory_.get(); }
};

// ============================================================================
// Concurrent Client Helper
// ============================================================================
// Runs a function across multiple client threads and waits for completion.

template<typename ClientFunc>
void run_concurrent_clients(int num_clients, ClientFunc&& fn) {
  std::vector<std::thread> threads;
  threads.reserve(num_clients);

  for (int i = 0; i < num_clients; ++i) {
    threads.emplace_back(std::forward<ClientFunc>(fn), i);
  }

  for (auto& t : threads) {
    t.join();
  }
}

// Overload that doesn't pass client index
template<typename ClientFunc>
void run_concurrent_clients_simple(int num_clients, ClientFunc&& fn) {
  std::vector<std::thread> threads;
  threads.reserve(num_clients);

  for (int i = 0; i < num_clients; ++i) {
    threads.emplace_back(fn);
  }

  for (auto& t : threads) {
    t.join();
  }
}

} // namespace iqxmlrpc_test

#endif // IQXMLRPC_TEST_COMMON_H

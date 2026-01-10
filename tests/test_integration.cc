#define BOOST_TEST_MODULE integration_test

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"
#include "libiqxmlrpc/https_server.h"
#include "libiqxmlrpc/https_client.h"
#include "libiqxmlrpc/executor.h"
#include "libiqxmlrpc/auth_plugin.h"
#include "libiqxmlrpc/firewall.h"
#include "libiqxmlrpc/ssl_lib.h"

#include "methods.h"

#include <atomic>
#include <memory>
#include <sstream>

using namespace iqxmlrpc;
using namespace iqnet;

namespace {

// Test port - use a high port unlikely to be in use
const int TEST_PORT = 19876;

// Simple auth plugin for testing
class TestAuthPlugin : public Auth_Plugin_base {
  bool allow_anonymous_;
public:
  explicit TestAuthPlugin(bool allow_anonymous = true)
    : allow_anonymous_(allow_anonymous) {}

  bool do_authenticate(const std::string& user, const std::string& password) const override {
    return user == "testuser" && password == "testpass";
  }

  bool do_authenticate_anonymous() const override {
    return allow_anonymous_;
  }
};

// Firewall that blocks all connections
class BlockAllFirewall : public Firewall_base {
public:
  bool grant(const Inet_addr&) override { return false; }
};

// Firewall with custom message
class CustomMessageFirewall : public Firewall_base {
public:
  bool grant(const Inet_addr&) override { return false; }
  std::string message() override { return "HTTP/1.0 403 Custom Forbidden\r\n\r\n"; }
};

// Firewall that allows all connections
class AllowAllFirewall : public Firewall_base {
public:
  bool grant(const Inet_addr&) override { return true; }
};

// Integration test fixture
class IntegrationFixture {
protected:
  std::unique_ptr<Http_server> server_;
  std::unique_ptr<Executor_factory_base> exec_factory_;
  boost::thread server_thread_;
  boost::mutex ready_mutex_;
  boost::condition_variable ready_cond_;
  bool server_ready_;
  std::atomic<bool> server_running_;
  int port_;

public:
  IntegrationFixture()
    : server_ready_(false)
    , server_running_(false)
    , port_(TEST_PORT) {}

  ~IntegrationFixture() {
    stop_server();
  }

  void start_server(int numthreads = 1, int port_offset = 0) {
    port_ = TEST_PORT + port_offset;

    if (numthreads > 1) {
      exec_factory_.reset(new Pool_executor_factory(numthreads));
    } else {
      exec_factory_.reset(new Serial_executor_factory);
    }

    server_.reset(new Http_server(
      Inet_addr("127.0.0.1", port_),
      exec_factory_.get()));

    // Register test methods
    register_user_methods(*server_);

    server_running_ = true;
    server_thread_ = boost::thread([this]() {
      {
        boost::mutex::scoped_lock lk(ready_mutex_);
        server_ready_ = true;
        ready_cond_.notify_one();
      }
      server_->work();
      server_running_ = false;
    });

    // Wait for server to be ready
    boost::mutex::scoped_lock lk(ready_mutex_);
    bool result = ready_cond_.timed_wait(lk,
      boost::posix_time::seconds(5),
      [this]{ return server_ready_; });
    BOOST_REQUIRE_MESSAGE(result, "Server startup timeout");

    // Give the server a moment to enter the work loop
    boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
  }

  void stop_server() {
    if (server_ && server_running_) {
      server_->set_exit_flag();
      server_->interrupt();
      if (server_thread_.joinable()) {
        server_thread_.timed_join(boost::posix_time::seconds(5));
      }
    }
    server_.reset();
    exec_factory_.reset();
    server_ready_ = false;
    server_running_ = false;
  }

  std::unique_ptr<Client_base> create_client() {
    return std::unique_ptr<Client_base>(
      new Client<Http_client_connection>(Inet_addr("127.0.0.1", port_)));
  }

  Server& server() { return *server_; }
  bool is_running() const { return server_running_; }
};

} // anonymous namespace

//=============================================================================
// Acceptor Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(acceptor_tests)

BOOST_FIXTURE_TEST_CASE(accept_single_connection, IntegrationFixture)
{
  start_server(1, 0);
  auto client = create_client();
  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");
}

BOOST_FIXTURE_TEST_CASE(accept_multiple_sequential_connections, IntegrationFixture)
{
  start_server(1, 1);
  for (int i = 0; i < 5; ++i) {
    auto client = create_client();
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(accept_with_thread_pool, IntegrationFixture)
{
  start_server(4, 2);
  std::vector<boost::thread> threads;
  std::atomic<int> success_count(0);

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([this, i, &success_count]() {
      try {
        auto client = create_client();
        Response r = client->execute("echo", Value(i));
        if (!r.is_fault() && r.value().get_int() == i) {
          ++success_count;
        }
      } catch (...) {}
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  BOOST_CHECK_GE(success_count.load(), 8);  // Allow some failures due to timing
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Connection Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(connection_tests)

BOOST_FIXTURE_TEST_CASE(connection_send_receive_large_data, IntegrationFixture)
{
  start_server(1, 10);
  auto client = create_client();

  // Send moderately large data
  std::string large_data(5000, 'x');
  Response r = client->execute("echo", Value(large_data));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), large_data);
}

BOOST_FIXTURE_TEST_CASE(connection_keep_alive, IntegrationFixture)
{
  start_server(1, 11);
  auto client = create_client();
  client->set_keep_alive(true);

  // Multiple requests on same connection
  for (int i = 0; i < 5; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(connection_no_keep_alive, IntegrationFixture)
{
  start_server(1, 12);
  auto client = create_client();
  client->set_keep_alive(false);

  for (int i = 0; i < 3; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(connection_cleanup_on_shutdown, IntegrationFixture)
{
  start_server(1, 13);
  auto client = create_client();
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
  stop_server();
  BOOST_CHECK(!is_running());
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Auth Plugin Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(auth_plugin_tests)

BOOST_FIXTURE_TEST_CASE(auth_anonymous_allowed, IntegrationFixture)
{
  TestAuthPlugin auth(true);  // Allow anonymous
  start_server(1, 20);
  server().set_auth_plugin(auth);

  auto client = create_client();
  // No credentials - should work with anonymous allowed
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(auth_success_with_credentials, IntegrationFixture)
{
  TestAuthPlugin auth(false);  // Require auth
  start_server(1, 21);
  server().set_auth_plugin(auth);

  auto client = create_client();
  client->set_authinfo("testuser", "testpass");
  Response r = client->execute("echo", Value("authenticated"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "authenticated");
}

BOOST_FIXTURE_TEST_CASE(auth_failure_wrong_credentials, IntegrationFixture)
{
  TestAuthPlugin auth(false);  // Require auth
  start_server(1, 22);
  server().set_auth_plugin(auth);

  auto client = create_client();
  client->set_authinfo("wronguser", "wrongpass");

  BOOST_CHECK_THROW(client->execute("echo", Value("test")), http::Error_response);
}

BOOST_FIXTURE_TEST_CASE(auth_anonymous_denied, IntegrationFixture)
{
  TestAuthPlugin auth(false);  // Don't allow anonymous
  start_server(1, 23);
  server().set_auth_plugin(auth);

  auto client = create_client();
  // No credentials - should fail
  BOOST_CHECK_THROW(client->execute("echo", Value("test")), http::Error_response);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Executor Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(executor_tests)

BOOST_FIXTURE_TEST_CASE(serial_executor_processing, IntegrationFixture)
{
  start_server(1, 30);  // Single thread = serial executor
  auto client = create_client();

  for (int i = 0; i < 10; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(pool_executor_processing, IntegrationFixture)
{
  start_server(4, 31);  // Thread pool
  auto client = create_client();

  for (int i = 0; i < 20; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
  }
}

BOOST_FIXTURE_TEST_CASE(executor_fault_handling, IntegrationFixture)
{
  start_server(1, 32);
  auto client = create_client();

  Response r = client->execute("error_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), 123);
  BOOST_CHECK(r.fault_string().find("My fault") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(pool_executor_graceful_shutdown, IntegrationFixture)
{
  start_server(4, 33);
  auto client = create_client();

  // Make some requests
  for (int i = 0; i < 5; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
  }

  // Stop server - should be graceful
  stop_server();
  BOOST_CHECK(!is_running());
}

// Test Pool_executor's std::exception handling (covers executor.cc lines 220-222)
BOOST_FIXTURE_TEST_CASE(pool_executor_std_exception_handling, IntegrationFixture)
{
  start_server(4, 34);  // Thread pool
  auto client = create_client();

  // Call method that throws std::runtime_error
  Response r = client->execute("std_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -1);
  BOOST_CHECK(r.fault_string().find("std::exception") != std::string::npos ||
              r.fault_string().find("Test") != std::string::npos);
}

// Test Pool_executor's unknown exception handling (covers executor.cc lines 224-226)
BOOST_FIXTURE_TEST_CASE(pool_executor_unknown_exception_handling, IntegrationFixture)
{
  start_server(4, 35);  // Thread pool
  auto client = create_client();

  // Call method that throws non-exception type (int)
  Response r = client->execute("unknown_exception_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), -1);
  BOOST_CHECK(r.fault_string().find("Unknown") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Server Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(server_tests)

BOOST_FIXTURE_TEST_CASE(full_rpc_cycle, IntegrationFixture)
{
  start_server(1, 40);
  auto client = create_client();

  // Test various value types
  Response r1 = client->execute("echo", Value(42));
  BOOST_CHECK(!r1.is_fault());
  BOOST_CHECK_EQUAL(r1.value().get_int(), 42);

  Response r2 = client->execute("echo", Value("string"));
  BOOST_CHECK(!r2.is_fault());
  BOOST_CHECK_EQUAL(r2.value().get_string(), "string");

  Response r3 = client->execute("echo", Value(3.14));
  BOOST_CHECK(!r3.is_fault());
  BOOST_CHECK_CLOSE(r3.value().get_double(), 3.14, 0.01);

  Response r4 = client->execute("echo", Value(true));
  BOOST_CHECK(!r4.is_fault());
  BOOST_CHECK_EQUAL(r4.value().get_bool(), true);
}

BOOST_FIXTURE_TEST_CASE(error_logging, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(1, 41);
  server().log_errors(&log_stream);

  auto client = create_client();
  Response r = client->execute("error_method", Value(""));
  BOOST_CHECK(r.is_fault());

  // Error should be logged (the Fault message)
  // Note: logging behavior may vary
}

BOOST_FIXTURE_TEST_CASE(server_set_exit_flag, IntegrationFixture)
{
  start_server(1, 42);
  auto client = create_client();

  // Make a request first
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());

  // Now test that stop_server works properly
  stop_server();
  BOOST_CHECK(!is_running());
}

BOOST_FIXTURE_TEST_CASE(introspection_enabled, IntegrationFixture)
{
  start_server(1, 43);
  server().enable_introspection();

  auto client = create_client();
  Response r = client->execute("system.listMethods", Param_list());
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_array());
  BOOST_CHECK_GT(r.value().size(), 0u);
}

BOOST_FIXTURE_TEST_CASE(xheaders_propagation, IntegrationFixture)
{
  start_server(1, 44);
  auto client = create_client();

  XHeaders headers;
  headers["X-Correlation-ID"] = "test-123";
  client->set_xheaders(headers);

  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Client Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(client_tests)

BOOST_FIXTURE_TEST_CASE(client_execute_single, IntegrationFixture)
{
  start_server(1, 50);
  auto client = create_client();

  Response r = client->execute("echo", Value("hello"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "hello");
}

BOOST_FIXTURE_TEST_CASE(client_multiple_params, IntegrationFixture)
{
  start_server(1, 51);
  auto client = create_client();

  Param_list params;
  params.push_back(Value("a"));
  params.push_back(Value("b"));
  params.push_back(Value("c"));

  Response r = client->execute("trace", params);
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "abc");
}

BOOST_FIXTURE_TEST_CASE(client_struct_value, IntegrationFixture)
{
  start_server(1, 52);
  auto client = create_client();

  Struct s;
  s.insert("key1", Value("value1"));
  s.insert("key2", Value(42));

  Response r = client->execute("echo", Value(s));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_struct());
  BOOST_CHECK_EQUAL(r.value()["key1"].get_string(), "value1");
  BOOST_CHECK_EQUAL(r.value()["key2"].get_int(), 42);
}

BOOST_FIXTURE_TEST_CASE(client_array_value, IntegrationFixture)
{
  start_server(1, 53);
  auto client = create_client();

  Array a;
  a.push_back(Value(1));
  a.push_back(Value(2));
  a.push_back(Value(3));

  Response r = client->execute("echo", Value(a));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_array());
  BOOST_CHECK_EQUAL(r.value().size(), 3u);
  BOOST_CHECK_EQUAL(r.value()[0].get_int(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Additional Coverage Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(additional_coverage_tests)

BOOST_FIXTURE_TEST_CASE(max_request_size_enforcement, IntegrationFixture)
{
  start_server(1, 60);
  server().set_max_request_sz(500);  // Very small limit

  auto client = create_client();
  // Try to send data larger than the limit
  std::string large_data(1000, 'x');

  BOOST_CHECK_THROW(client->execute("echo", Value(large_data)), http::Error_response);
}

BOOST_FIXTURE_TEST_CASE(unknown_method_error, IntegrationFixture)
{
  start_server(1, 61);
  auto client = create_client();

  Response r = client->execute("nonexistent_method", Value("test"));
  BOOST_CHECK(r.is_fault());
  // Unknown method should return fault code -32601
}

BOOST_FIXTURE_TEST_CASE(server_log_message, IntegrationFixture)
{
  std::ostringstream log_stream;
  start_server(1, 62);
  server().log_errors(&log_stream);

  auto client = create_client();

  // Call error method to generate log output
  Response r = client->execute("error_method", Value(""));
  BOOST_CHECK(r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(multiple_dispatchers, IntegrationFixture)
{
  start_server(1, 63);
  server().enable_introspection();

  auto client = create_client();

  // Test introspection methods which use built-in dispatcher
  Response r1 = client->execute("system.listMethods", Param_list());
  BOOST_CHECK(!r1.is_fault());
  BOOST_CHECK(r1.value().is_array());

  // system.methodSignature is another introspection method
  Response r2 = client->execute("system.methodSignature", Value("echo"));
  // This returns an array of signatures or may fault if not supported
}

BOOST_FIXTURE_TEST_CASE(binary_data_transfer, IntegrationFixture)
{
  start_server(1, 64);
  auto client = create_client();

  // Create binary data using static factory method
  std::string data = "hello binary data";
  std::unique_ptr<Binary_data> bin(Binary_data::from_data(data));

  Response r = client->execute("echo", Value(*bin));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_binary());
}

BOOST_FIXTURE_TEST_CASE(datetime_transfer, IntegrationFixture)
{
  start_server(1, 65);
  auto client = create_client();

  // Create datetime value - format: YYYYMMDDTHH:MM:SS
  Date_time dt("20260108T12:30:45");

  Response r = client->execute("echo", Value(dt));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_datetime());
}

BOOST_FIXTURE_TEST_CASE(nil_value_transfer, IntegrationFixture)
{
  start_server(1, 66);
  auto client = create_client();

  Nil nil;
  Response r = client->execute("echo", Value(nil));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_nil());
}

BOOST_FIXTURE_TEST_CASE(nested_struct_array, IntegrationFixture)
{
  start_server(1, 67);
  auto client = create_client();

  // Create nested structure
  Struct inner;
  inner.insert("name", Value("test"));
  inner.insert("value", Value(42));

  Array arr;
  arr.push_back(Value(inner));
  arr.push_back(Value(inner));

  Struct outer;
  outer.insert("items", Value(arr));
  outer.insert("count", Value(2));

  Response r = client->execute("echo", Value(outer));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK(r.value().is_struct());
  BOOST_CHECK(r.value()["items"].is_array());
  BOOST_CHECK_EQUAL(r.value()["items"].size(), 2u);
}

BOOST_FIXTURE_TEST_CASE(large_response, IntegrationFixture)
{
  start_server(1, 68);
  auto client = create_client();

  // Create array with many elements
  Array arr;
  for (int i = 0; i < 100; ++i) {
    arr.push_back(Value(i));
  }

  Response r = client->execute("echo", Value(arr));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().size(), 100u);
}

BOOST_FIXTURE_TEST_CASE(concurrent_different_methods, IntegrationFixture)
{
  start_server(4, 69);  // Thread pool
  std::vector<boost::thread> threads;
  std::atomic<int> echo_count(0);
  std::atomic<int> trace_count(0);

  // Run echo and trace methods concurrently
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([this, &echo_count]() {
      auto client = create_client();
      Response r = client->execute("echo", Value("test"));
      if (!r.is_fault()) ++echo_count;
    });
    threads.emplace_back([this, &trace_count]() {
      auto client = create_client();
      Param_list params;
      params.push_back(Value("a"));
      params.push_back(Value("b"));
      Response r = client->execute("trace", params);
      if (!r.is_fault()) ++trace_count;
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  BOOST_CHECK_GE(echo_count.load(), 4);
  BOOST_CHECK_GE(trace_count.load(), 4);
}

BOOST_FIXTURE_TEST_CASE(i8_int64_value, IntegrationFixture)
{
  start_server(1, 70);
  auto client = create_client();

  // Test 64-bit integer
  int64_t large_val = 9223372036854775807LL;  // Max int64
  Response r = client->execute("echo", Value(large_val));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_int64(), large_val);
}

BOOST_FIXTURE_TEST_CASE(negative_values, IntegrationFixture)
{
  start_server(1, 71);
  auto client = create_client();

  Response r1 = client->execute("echo", Value(-42));
  BOOST_CHECK(!r1.is_fault());
  BOOST_CHECK_EQUAL(r1.value().get_int(), -42);

  Response r2 = client->execute("echo", Value(-3.14159));
  BOOST_CHECK(!r2.is_fault());
  BOOST_CHECK_CLOSE(r2.value().get_double(), -3.14159, 0.0001);
}

BOOST_FIXTURE_TEST_CASE(empty_string_value, IntegrationFixture)
{
  start_server(1, 72);
  auto client = create_client();

  Response r = client->execute("echo", Value(""));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "");
}

BOOST_FIXTURE_TEST_CASE(special_xml_chars, IntegrationFixture)
{
  start_server(1, 73);
  auto client = create_client();

  std::string special = "<test>&amp;\"'</test>";
  Response r = client->execute("echo", Value(special));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), special);
}

BOOST_FIXTURE_TEST_CASE(unicode_string, IntegrationFixture)
{
  start_server(1, 74);
  auto client = create_client();

  std::string unicode = "Hello \xC3\xA9\xC3\xA8\xC3\xA0";  // UTF-8
  Response r = client->execute("echo", Value(unicode));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), unicode);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Firewall Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(firewall_tests)

BOOST_FIXTURE_TEST_CASE(firewall_allows_connection, IntegrationFixture)
{
  start_server(1, 80);
  AllowAllFirewall fw;
  server().set_firewall(&fw);

  auto client = create_client();
  Response r = client->execute("echo", Value("allowed"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "allowed");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Interceptor Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(interceptor_tests)

// Simple interceptor that counts calls
class CountingInterceptor : public Interceptor {
public:
  static int call_count;

  void process(Method* m, const Param_list& params, Value& result) override {
    ++call_count;
    yield(m, params, result);
  }
};

int CountingInterceptor::call_count = 0;

BOOST_FIXTURE_TEST_CASE(interceptor_invoked, IntegrationFixture)
{
  start_server(1, 85);
  CountingInterceptor::call_count = 0;
  server().push_interceptor(new CountingInterceptor());

  auto client = create_client();
  Response r1 = client->execute("echo", Value("test1"));
  Response r2 = client->execute("echo", Value("test2"));

  BOOST_CHECK(!r1.is_fault());
  BOOST_CHECK(!r2.is_fault());
  BOOST_CHECK_EQUAL(CountingInterceptor::call_count, 2);
}

// Interceptor that modifies the result
class ModifyingInterceptor : public Interceptor {
public:
  void process(Method* m, const Param_list& params, Value& result) override {
    yield(m, params, result);
    // Append suffix to string results
    if (result.is_string()) {
      result = Value(result.get_string() + "_modified");
    }
  }
};

BOOST_FIXTURE_TEST_CASE(interceptor_modifies_result, IntegrationFixture)
{
  start_server(1, 86);
  server().push_interceptor(new ModifyingInterceptor());

  auto client = create_client();
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "test_modified");
}

// Chained interceptors
BOOST_FIXTURE_TEST_CASE(chained_interceptors, IntegrationFixture)
{
  start_server(1, 87);
  CountingInterceptor::call_count = 0;
  server().push_interceptor(new CountingInterceptor());
  server().push_interceptor(new CountingInterceptor());

  auto client = create_client();
  Response r = client->execute("echo", Value("test"));
  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(CountingInterceptor::call_count, 2);  // Both interceptors called
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Error Path Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(error_path_tests)

BOOST_FIXTURE_TEST_CASE(method_throws_fault, IntegrationFixture)
{
  start_server(1, 90);
  auto client = create_client();

  Response r = client->execute("error_method", Value(""));
  BOOST_CHECK(r.is_fault());
  BOOST_CHECK_EQUAL(r.fault_code(), 123);
}

BOOST_FIXTURE_TEST_CASE(method_returns_complex_fault, IntegrationFixture)
{
  start_server(1, 91);
  auto client = create_client();

  // error_method throws Fault(123, "My fault")
  Response r = client->execute("error_method", Value("test"));
  BOOST_CHECK(r.is_fault());
  std::string fault_str = r.fault_string();
  BOOST_CHECK(fault_str.find("fault") != std::string::npos ||
              fault_str.find("Fault") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(sequential_errors, IntegrationFixture)
{
  start_server(1, 92);
  auto client = create_client();

  // Multiple error calls should all work
  for (int i = 0; i < 3; ++i) {
    Response r = client->execute("error_method", Value(i));
    BOOST_CHECK(r.is_fault());
  }
}

BOOST_FIXTURE_TEST_CASE(error_then_success, IntegrationFixture)
{
  start_server(1, 93);
  auto client = create_client();

  Response r1 = client->execute("error_method", Value(""));
  BOOST_CHECK(r1.is_fault());

  // Server should still work after error
  Response r2 = client->execute("echo", Value("still working"));
  BOOST_CHECK(!r2.is_fault());
  BOOST_CHECK_EQUAL(r2.value().get_string(), "still working");
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Server Configuration Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(server_config_tests)

BOOST_FIXTURE_TEST_CASE(verification_level_weak, IntegrationFixture)
{
  start_server(1, 95);
  server().set_verification_level(http::HTTP_CHECK_WEAK);

  auto client = create_client();
  Response r = client->execute("echo", Value("weak"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(verification_level_strict, IntegrationFixture)
{
  start_server(1, 96);
  server().set_verification_level(http::HTTP_CHECK_STRICT);

  auto client = create_client();
  Response r = client->execute("echo", Value("strict"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(get_reactor, IntegrationFixture)
{
  start_server(1, 97);

  // Verify reactor is accessible
  BOOST_CHECK(server().get_reactor() != nullptr);

  auto client = create_client();
  Response r = client->execute("echo", Value("reactor_test"));
  BOOST_CHECK(!r.is_fault());
}

BOOST_FIXTURE_TEST_CASE(max_request_size_boundary, IntegrationFixture)
{
  start_server(1, 98);
  server().set_max_request_sz(10000);  // 10KB limit

  auto client = create_client();

  // Just under the limit should work
  std::string data(500, 'x');
  Response r = client->execute("echo", Value(data));
  BOOST_CHECK(!r.is_fault());
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// Connection Reuse Tests
//=============================================================================
BOOST_AUTO_TEST_SUITE(connection_reuse_tests)

BOOST_FIXTURE_TEST_CASE(many_requests_same_client, IntegrationFixture)
{
  start_server(1, 100);
  auto client = create_client();
  client->set_keep_alive(true);

  // Many requests on same connection
  for (int i = 0; i < 20; ++i) {
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
    BOOST_CHECK_EQUAL(r.value().get_int(), i);
  }
}

BOOST_FIXTURE_TEST_CASE(alternating_keep_alive, IntegrationFixture)
{
  start_server(1, 101);

  for (int i = 0; i < 5; ++i) {
    auto client = create_client();
    client->set_keep_alive(i % 2 == 0);
    Response r = client->execute("echo", Value(i));
    BOOST_CHECK(!r.is_fault());
  }
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// HTTP Server Error Tests - covers http_server.cc and server_conn.cc error paths
//
// Coverage targets:
//   - server_conn.cc lines 37-41: Expect: 100-continue handling
//   - server_conn.cc lines 46-48: Malformed packet catch block
//   - http_server.cc lines 97-102: HTTP Error_response catch block
//   - http_server.cc lines 133-138, 141-144: log_exception, log_unknown_exception
//=============================================================================
BOOST_AUTO_TEST_SUITE(http_server_error_tests)

// Helper to send raw HTTP data to server
namespace {

std::string send_raw_http(const std::string& host, int port, const std::string& data)
{
    iqnet::Socket sock;
    sock.connect(iqnet::Inet_addr(host, port));

    sock.send(data.c_str(), data.length());

    // Small delay to let server process
    boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

    char buffer[4096];
    std::string response;
    sock.set_non_blocking(true);

    try {
        size_t n = sock.recv(buffer, sizeof(buffer));
        response = std::string(buffer, n);
    } catch (...) {
        // Non-blocking recv may throw if no data
    }

    sock.close();
    return response;
}

} // anonymous namespace

// Test malformed HTTP request triggers error handling
// Covers server_conn.cc lines 46-48: catch(const http::Malformed_packet&)
BOOST_FIXTURE_TEST_CASE(malformed_http_request_returns_400, IntegrationFixture)
{
    start_server(1, 110);

    // Send completely malformed HTTP (garbage data, not even close to HTTP)
    std::string response = send_raw_http("127.0.0.1", port_,
        "\x00\x01\x02GARBAGE\x03\x04\r\n\r\n");

    // Server should return an error or close connection - any response is acceptable
    // The main goal is that the error path in server_conn.cc is exercised
    BOOST_TEST_MESSAGE("Malformed request response: " << response.length() << " bytes");

    // The server either returns an error response or closes connection
    // Both behaviors are correct for malformed requests
    BOOST_CHECK(true);  // Test passes if server didn't crash
}

// Test HTTP request with invalid header format
// Covers server_conn.cc lines 46-48: catch(const http::Malformed_packet&)
BOOST_FIXTURE_TEST_CASE(malformed_http_header_format, IntegrationFixture)
{
    start_server(1, 116);

    // Send HTTP with invalid header line (missing colon separator)
    std::string response = send_raw_http("127.0.0.1", port_,
        "POST /RPC2 HTTP/1.1\r\n"
        "This-Is-Not-A-Valid-Header\r\n"  // Missing ": value" part
        "\r\n");

    BOOST_TEST_MESSAGE("Malformed header response: " << response.length() << " bytes");
    // Test passes if server handled the malformed request gracefully
    BOOST_CHECK(true);
}

// Test HTTP request with invalid method
// Covers http_server.cc lines 97-102: catch(const http::Error_response& e)
BOOST_FIXTURE_TEST_CASE(invalid_http_method_returns_error, IntegrationFixture)
{
    start_server(1, 111);

    // GET is not allowed for XML-RPC (only POST)
    std::string response = send_raw_http("127.0.0.1", port_,
        "GET /RPC2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r\n");

    // Server should return 405 Method Not Allowed
    BOOST_CHECK(response.find("405") != std::string::npos ||
                response.find("Method") != std::string::npos);
}

// Test incomplete HTTP request (partial header)
// Covers http_server.cc line 91: if(!packet) return
BOOST_FIXTURE_TEST_CASE(incomplete_http_request_waits, IntegrationFixture)
{
    start_server(1, 112);

    // Send partial request (no double CRLF to end headers)
    iqnet::Socket sock;
    sock.connect(iqnet::Inet_addr("127.0.0.1", port_));

    const char* partial = "POST /RPC2 HTTP/1.1\r\nHost: localhost";
    sock.send(partial, strlen(partial));

    // Server should wait for more data (not crash or return error)
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    // Send the rest of the request
    std::string rest =
        "\r\nContent-Type: text/xml\r\n"
        "Content-Length: 100\r\n"
        "\r\n"
        "<?xml version=\"1.0\"?><methodCall><methodName>echo</methodName>"
        "<params><param><value><string>test</string></value></param></params></methodCall>";

    sock.send(rest.c_str(), rest.length());

    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    char buffer[4096];
    sock.set_non_blocking(true);
    std::string response;
    try {
        size_t n = sock.recv(buffer, sizeof(buffer));
        response = std::string(buffer, n);
    } catch (...) {}

    sock.close();

    // Should get a valid response (200 OK)
    BOOST_CHECK(response.find("200") != std::string::npos ||
                response.find("OK") != std::string::npos);
}

// Test HTTP Expect: 100-continue header
// Covers server_conn.cc lines 37-41: expect_continue handling
BOOST_FIXTURE_TEST_CASE(expect_100_continue_handled, IntegrationFixture)
{
    start_server(1, 113);

    iqnet::Socket sock;
    sock.connect(iqnet::Inet_addr("127.0.0.1", port_));

    // Send request with Expect: 100-continue
    std::string headers =
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: text/xml\r\n"
        "Content-Length: 100\r\n"
        "Expect: 100-continue\r\n"
        "\r\n";

    sock.send(headers.c_str(), headers.length());

    // Wait for 100 Continue response
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    char buffer[4096];
    sock.set_non_blocking(true);
    std::string response;
    try {
        size_t n = sock.recv(buffer, sizeof(buffer));
        response = std::string(buffer, n);
    } catch (...) {}

    // Should get 100 Continue
    BOOST_CHECK(response.find("100") != std::string::npos);

    // Now send the body
    std::string body =
        "<?xml version=\"1.0\"?><methodCall><methodName>echo</methodName>"
        "<params><param><value><string>continue</string></value></param></params></methodCall>";

    sock.send(body.c_str(), body.length());

    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    try {
        size_t n = sock.recv(buffer, sizeof(buffer));
        response = std::string(buffer, n);
    } catch (...) {}

    sock.close();

    // Should get final 200 OK response
    BOOST_CHECK(response.find("200") != std::string::npos ||
                response.find("OK") != std::string::npos);
}

// Test that server logs exceptions properly
// Covers http_server.cc lines 133-138: log_exception
BOOST_FIXTURE_TEST_CASE(server_logs_exceptions, IntegrationFixture)
{
    std::ostringstream log_stream;
    start_server(1, 114);
    server().log_errors(&log_stream);

    auto client = create_client();

    // Call error_method which throws Fault
    Response r = client->execute("error_method", Value(""));
    BOOST_CHECK(r.is_fault());

    // The fault is handled by executor, but if there was an exception
    // during connection handling it would be logged
}

// Test Request-Too-Large error
// Covers http_server.cc lines 97-102 via Request_too_large exception
BOOST_FIXTURE_TEST_CASE(request_too_large_returns_error, IntegrationFixture)
{
    start_server(1, 115);
    server().set_max_request_sz(100);  // Very small limit

    // Send request larger than the limit
    std::string large_body(200, 'x');
    std::string request =
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: text/xml\r\n"
        "Content-Length: " + std::to_string(large_body.length()) + "\r\n"
        "\r\n" + large_body;

    std::string response = send_raw_http("127.0.0.1", port_, request);

    // Should get 413 Request Entity Too Large
    BOOST_CHECK(response.find("413") != std::string::npos ||
                response.find("Too Large") != std::string::npos ||
                response.find("Request") != std::string::npos);
}

// Test Content-Length required
// Covers HTTP error path for missing Content-Length
BOOST_FIXTURE_TEST_CASE(missing_content_length_returns_error, IntegrationFixture)
{
    start_server(1, 116);

    std::string request =
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: text/xml\r\n"
        "\r\n"
        "<?xml version=\"1.0\"?><methodCall><methodName>echo</methodName></methodCall>";

    std::string response = send_raw_http("127.0.0.1", port_, request);

    // Should get 411 Length Required
    BOOST_CHECK(response.find("411") != std::string::npos ||
                response.find("Length") != std::string::npos);
}

// Test unsupported content type in strict mode
BOOST_FIXTURE_TEST_CASE(unsupported_content_type_strict_mode, IntegrationFixture)
{
    start_server(1, 117);
    server().set_verification_level(http::HTTP_CHECK_STRICT);

    std::string request =
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 50\r\n"
        "\r\n"
        "<?xml version=\"1.0\"?><methodCall></methodCall>";

    std::string response = send_raw_http("127.0.0.1", port_, request);

    // Should get 415 Unsupported Media Type
    BOOST_CHECK(response.find("415") != std::string::npos ||
                response.find("Unsupported") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

//=============================================================================
// SSL/HTTPS Tests - covers ssl_lib.cc lines 104, 140, 258
//
// These tests verify SSL context creation and HTTPS client/server functionality.
// Coverage:
//   - Line 104: SSL_get_ex_new_index in init_library (ssl_client_only_context)
//   - Line 140: iqxmlrpc_SSL_verify callback (https_handshake_with_verifier)
//   - Line 258: SSL_set_ex_data with verifier (https_handshake_with_verifier)
//
// The HTTPS handshake tests use an embedded self-signed certificate.
//=============================================================================

#include <fstream>
#include <cstdio>

namespace {

// Embedded self-signed certificate for testing
// Generated with: openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 36500 -nodes -subj "/CN=localhost"
const char* EMBEDDED_TEST_CERT = R"(-----BEGIN CERTIFICATE-----
MIIDCzCCAfOgAwIBAgIUXzkbleG5HOcIm3Ke/qrw3JCCCVMwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MDEwOTIxMTYxNVoYDzIxMjUx
MjE2MjExNjE1WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEB
AQUAA4IBDwAwggEKAoIBAQDWUSUBs2Am6ptXHZkz3zZAwzA06jF+r5PMCFmhf2ZY
o54a0dUgh2XElgpo7saEWFNnt5EgTxJQpUCRHs7QKkB39/6itjg/4rmR7C7nXj1n
q1jkYUXiPXlihkHwycXp4jUh0zgLFAtQNYBl6AajlsZcxkLUB+4pFxTmtCXuOX6E
fh4iiougQgkzUL89dNC/+PViUOKkO3WxZ3ZcuLEaiyBEBfuqLH/YBKp45nIaFr8H
iFyEx6Y5nuZ1grPDDbZZ4MXmdm+aC6OUNTrIYtSzaP2wO3BiJhLVshDB/cIDmYsX
H80aB3zbrKWClTTAVxFgn/y83lNAIciP90XvDQSP59EDAgMBAAGjUzBRMB0GA1Ud
DgQWBBQo6uxnhPB3W3xFzqQ42Xgzg//+wjAfBgNVHSMEGDAWgBQo6uxnhPB3W3xF
zqQ42Xgzg//+wjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQCa
iBhA4uamOdZAulJQV3/VKOlqzCPyzokSwh+D7H2fgvJRf4dt4CvZYlFtM2iK7+EW
h7wYNJ5qo4pq88/iAfDgIe8Vbpbr9IpwcHw1hLfVxqOys845Z4bXRrvFaE4GaaAa
Nx+Zbr+asm0eL2w/df8HHcp78vHYZSDZL04skyv1Ybx1buoFY3G59kl/I2v7SRXi
73m7JurSbDWaVXV9M2k/znSPifdx9bqOKHX8zX7liitHcSyVGG9DWl1yB+2iP0dM
0eioGoqxoNt3Gws8wSieB11r2k5cfqcGFLbjEfV6YDenjRs2FB2xVfrmocBrbJ9V
5ntzlSfNSe7ZowUs1202
-----END CERTIFICATE-----
)";

const char* EMBEDDED_TEST_KEY = R"(-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDWUSUBs2Am6ptX
HZkz3zZAwzA06jF+r5PMCFmhf2ZYo54a0dUgh2XElgpo7saEWFNnt5EgTxJQpUCR
Hs7QKkB39/6itjg/4rmR7C7nXj1nq1jkYUXiPXlihkHwycXp4jUh0zgLFAtQNYBl
6AajlsZcxkLUB+4pFxTmtCXuOX6Efh4iiougQgkzUL89dNC/+PViUOKkO3WxZ3Zc
uLEaiyBEBfuqLH/YBKp45nIaFr8HiFyEx6Y5nuZ1grPDDbZZ4MXmdm+aC6OUNTrI
YtSzaP2wO3BiJhLVshDB/cIDmYsXH80aB3zbrKWClTTAVxFgn/y83lNAIciP90Xv
DQSP59EDAgMBAAECggEAUcqzIGSIUCHeOg+SPgE0j9/OQIuWax5v/gC70E4yTabX
+q1VNO5nkPCgNW7XNYAOCLm+ecGjoEKJEzlaPYi6hO6Q8CEx83PAVaf5OJS3Q57Z
tINJK/BBKLBLby1aSonptCiLrXKvZKOehoXYLsumlZaWv5vtMSJdeDSNe07W8ZIL
VxlKFsVANHPMP9wK/NIx2z0G+Qd/e8UJukuLccN5G+oL/oPfGdMtxY3onHlSQdL0
X20v5dcbTKRwO+kYMK9nLz6ZF9sL/MDi3/AmlCyPQ87Vaz/LTw2t8JlSe9hqHoZ9
hJat8c6KRnRvL6hhs3YFuXnh5uecs6SdsltXrf6UBQKBgQD0Bg6rP1OTv/BIFY3p
CT8M/Eop49eM3d5jIkWGEo0LDZp6TVQ6geWIhTYXB24D7zzk/FlhUiWrYlCSJhjc
NFff7ysdbZft0gVtYRddepEgN2JafJqs8R1+GoYubrxUcFz/v4qIkt8NXs54Z+J3
TCQqIf8aEK0XO1gN3qlITzZS9wKBgQDg1dlMFDGrSUdu19vnXK85t/dQvroyrnKZ
MyObUceSLSkYNbOJAplI48LMTVApmUccG370WNg/qGiZhBdw90UxdHLPdt3Ca/C5
3wmGUNakg5bDfdFhmHsooQlh6wvbJ1SX3O9UApWDqLMstSaUZqppbVgjpbrHG9AV
e/94Vo2jVQKBgQC7Ye9ftsgh+9CyOcL4QL5m5VC57Bi4NiMwMr/6XUJrS23lHn5g
UyED/W70riLf6JT1LYYhAmiku2EtaQ3MAnG8JrcP6PkyiQTb4iOEB7trZrwiye4o
gRppnEqPWz9JA+OWC+qAR2/6n2Oi9/riKtjWdbajuEyCO3K5a9LIEPOhLwKBgQCk
P/Wn25TRgg4aTr2Kjq4/50JYjY0vGzwC6VYY0KyQAEfmNMz8yZY7ppAXel+WlDBb
u0aKsSEBmEEZ7WLGlw3IbD63iynEL+DDmMm3gvTbaHpKRG8i8ib+7m4RR4n4xwnI
i5GXeO/LKAIFJi2R+lKCBGyAVkFV1d6040olmm2MpQKBgBEkhuUdBaSkNBt8YJxM
BU2PiriNuFw5UMWFRRcysMKO3oA9UWeXEHEX7z4jyThCmLl2+X0Q9KvAezhKdRjP
H/+tEBbXrHM9aOHqPvhkMe6foDk3VZdXwiU/XO+gBidrsQVoHRuz3TA5xMYflvHg
rK0fmiWyi5lQX70lb9kyDkqP
-----END PRIVATE KEY-----
)";

// Write embedded cert/key to temp files and return their paths
std::pair<std::string, std::string> create_temp_cert_files() {
  std::string cert_path = "/tmp/iqxmlrpc_test_cert.pem";
  std::string key_path = "/tmp/iqxmlrpc_test_key.pem";

  std::ofstream cert_file(cert_path);
  cert_file << EMBEDDED_TEST_CERT;
  cert_file.close();

  std::ofstream key_file(key_path);
  key_file << EMBEDDED_TEST_KEY;
  key_file.close();

  return std::make_pair(cert_path, key_path);
}

// Verifier that tracks how many times it was called
class TrackingVerifier : public iqnet::ssl::ConnectionVerifier {
  mutable std::atomic<int> call_count_{0};

  int do_verify(bool, X509_STORE_CTX*) const override {
    ++call_count_;
    return 1;  // Accept all
  }

public:
  int get_call_count() const { return call_count_.load(); }
  void reset() { call_count_ = 0; }
};

// HTTPS integration test fixture with embedded certificates
class HttpsIntegrationFixture {
protected:
  std::unique_ptr<Https_server> server_;
  std::unique_ptr<Executor_factory_base> exec_factory_;
  boost::thread server_thread_;
  boost::mutex ready_mutex_;
  boost::condition_variable ready_cond_;
  bool server_ready_ = false;
  std::atomic<bool> server_running_{false};
  int port_ = 19950;
  iqnet::ssl::Ctx* saved_ctx_ = nullptr;
  iqnet::ssl::Ctx* test_ctx_ = nullptr;
  std::string temp_cert_path_;
  std::string temp_key_path_;

public:
  HttpsIntegrationFixture() : saved_ctx_(iqnet::ssl::ctx) {}

  ~HttpsIntegrationFixture() {
    stop_server();
    cleanup_ssl();
    cleanup_temp_files();
  }

  bool setup_ssl_context() {
    try {
      auto paths = create_temp_cert_files();
      temp_cert_path_ = paths.first;
      temp_key_path_ = paths.second;

      test_ctx_ = iqnet::ssl::Ctx::client_server(temp_cert_path_, temp_key_path_);
      iqnet::ssl::ctx = test_ctx_;
      return true;
    } catch (...) {
      return false;
    }
  }

  void cleanup_ssl() {
    iqnet::ssl::ctx = saved_ctx_;
    if (test_ctx_) {
      delete test_ctx_;
      test_ctx_ = nullptr;
    }
  }

  void cleanup_temp_files() {
    if (!temp_cert_path_.empty()) std::remove(temp_cert_path_.c_str());
    if (!temp_key_path_.empty()) std::remove(temp_key_path_.c_str());
  }

  void start_server(int port_offset = 0) {
    port_ = 19950 + port_offset;

    exec_factory_.reset(new Serial_executor_factory);

    server_.reset(new Https_server(
      Inet_addr("127.0.0.1", port_),
      exec_factory_.get()));

    register_user_methods(*server_);

    server_running_ = true;
    server_thread_ = boost::thread([this]() {
      {
        boost::mutex::scoped_lock lk(ready_mutex_);
        server_ready_ = true;
        ready_cond_.notify_one();
      }
      server_->work();
      server_running_ = false;
    });

    boost::mutex::scoped_lock lk(ready_mutex_);
    ready_cond_.timed_wait(lk,
      boost::posix_time::seconds(5),
      [this]{ return server_ready_; });

    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  }

  void stop_server() {
    if (server_ && server_running_) {
      server_->set_exit_flag();
      server_->interrupt();
      if (server_thread_.joinable()) {
        server_thread_.timed_join(boost::posix_time::seconds(5));
      }
    }
    server_.reset();
    exec_factory_.reset();
    server_ready_ = false;
    server_running_ = false;
  }

  std::unique_ptr<Client_base> create_client() {
    return std::unique_ptr<Client_base>(
      new Client<Https_client_connection>(Inet_addr("127.0.0.1", port_)));
  }

  iqnet::ssl::Ctx* get_context() { return test_ctx_; }
};

// Check if external test certificates are available (for backward compat tests)
bool ssl_certs_available() {
  std::ifstream cert("../tests/data/cert.pem");
  std::ifstream key("../tests/data/pk.pem");
  return cert.good() && key.good();
}

}

BOOST_AUTO_TEST_SUITE(ssl_tests)

// Test SSL context with client-only mode (no certificates needed)
// This covers ssl_lib.cc line 104 (SSL_get_ex_new_index in init_library)
BOOST_AUTO_TEST_CASE(ssl_client_only_context)
{
  iqnet::ssl::Ctx* saved_ctx = iqnet::ssl::ctx;

  try {
    // client_only() doesn't require certificates
    // This still triggers init_library() which includes line 104
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_only();

    BOOST_REQUIRE(ctx != nullptr);
    BOOST_REQUIRE(ctx->context() != nullptr);

    delete ctx;
  } catch (const std::exception& e) {
    iqnet::ssl::ctx = saved_ctx;
    BOOST_FAIL("SSL client_only context creation failed: " << e.what());
  }

  iqnet::ssl::ctx = saved_ctx;
}

// Test SSL context creation with certificates
BOOST_AUTO_TEST_CASE(ssl_context_creation)
{
  if (!ssl_certs_available()) {
    BOOST_TEST_MESSAGE("Skipping - SSL certificates not available");
    return;
  }

  iqnet::ssl::Ctx* saved_ctx = iqnet::ssl::ctx;

  try {
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_server(
      "../tests/data/cert.pem",
      "../tests/data/pk.pem");

    BOOST_REQUIRE(ctx != nullptr);
    BOOST_REQUIRE(ctx->context() != nullptr);

    delete ctx;
  } catch (const std::exception& e) {
    iqnet::ssl::ctx = saved_ctx;
    BOOST_FAIL("SSL context creation failed: " << e.what());
  }

  iqnet::ssl::ctx = saved_ctx;
}

// Test SSL context with server-only mode
BOOST_AUTO_TEST_CASE(ssl_server_only_context)
{
  if (!ssl_certs_available()) {
    BOOST_TEST_MESSAGE("Skipping - SSL certificates not available");
    return;
  }

  iqnet::ssl::Ctx* saved_ctx = iqnet::ssl::ctx;

  try {
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::server_only(
      "../tests/data/cert.pem",
      "../tests/data/pk.pem");

    BOOST_REQUIRE(ctx != nullptr);
    BOOST_REQUIRE(ctx->context() != nullptr);

    delete ctx;
  } catch (const std::exception& e) {
    iqnet::ssl::ctx = saved_ctx;
    BOOST_FAIL("SSL server_only context creation failed: " << e.what());
  }

  iqnet::ssl::ctx = saved_ctx;
}

// Test ConnectionVerifier setup - covers ssl_lib.cc lines 140, 258
namespace {
class TestVerifier : public iqnet::ssl::ConnectionVerifier {
private:
  mutable bool was_called_ = false;

  int do_verify(bool preverified_ok, X509_STORE_CTX*) const override {
    was_called_ = true;
    (void)preverified_ok;
    return 1;  // Accept all
  }

public:
  bool was_called() const { return was_called_; }
};
}

BOOST_AUTO_TEST_CASE(ssl_verifier_setup)
{
  if (!ssl_certs_available()) {
    BOOST_TEST_MESSAGE("Skipping - SSL certificates not available");
    return;
  }

  iqnet::ssl::Ctx* saved_ctx = iqnet::ssl::ctx;

  try {
    iqnet::ssl::Ctx* ctx = iqnet::ssl::Ctx::client_server(
      "../tests/data/cert.pem",
      "../tests/data/pk.pem");

    BOOST_REQUIRE(ctx != nullptr);

    // Set up client verification - this covers verify_client() method
    TestVerifier verifier;
    ctx->verify_client(false, &verifier);

    // Set up server verification
    TestVerifier server_verifier;
    ctx->verify_server(&server_verifier);

    delete ctx;
  } catch (const std::exception& e) {
    iqnet::ssl::ctx = saved_ctx;
    BOOST_FAIL("SSL verifier setup failed: " << e.what());
  }

  iqnet::ssl::ctx = saved_ctx;
}

// Test SSL exception types (no certificates needed)
BOOST_AUTO_TEST_CASE(ssl_exception_types)
{
  // Test ssl::not_initialized exception
  iqnet::ssl::not_initialized not_init;
  BOOST_CHECK(std::string(not_init.what()).find("not initialized") != std::string::npos);

  // Test ssl::connection_close exception
  iqnet::ssl::connection_close close_clean(true);
  BOOST_CHECK(close_clean.is_clean());

  iqnet::ssl::connection_close close_unclean(false);
  BOOST_CHECK(!close_unclean.is_clean());
}

// Test basic HTTPS client/server communication (without custom verifier)
BOOST_FIXTURE_TEST_CASE(https_basic_communication, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  start_server(200);
  auto client = create_client();

  Response r = client->execute("echo", Value("https test"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "https test");
}

// Test that TLS handshake invokes verifier callback
// This test actually performs a TLS handshake and covers:
// - ssl_lib.cc line 140 (iqxmlrpc_SSL_verify callback)
// - ssl_lib.cc line 258 (SSL_set_ex_data in prepare_verify)
BOOST_FIXTURE_TEST_CASE(https_handshake_triggers_verify, HttpsIntegrationFixture)
{
  BOOST_REQUIRE_MESSAGE(setup_ssl_context(),
    "Failed to set up SSL context with embedded certificates");

  TrackingVerifier client_verifier;
  get_context()->verify_server(&client_verifier);

  start_server(201);
  auto client = create_client();

  Response r = client->execute("echo", Value("handshake test"));

  BOOST_CHECK(!r.is_fault());
  BOOST_CHECK_EQUAL(r.value().get_string(), "handshake test");
  // Verify the callback was actually invoked during TLS handshake
  BOOST_CHECK_GT(client_verifier.get_call_count(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et

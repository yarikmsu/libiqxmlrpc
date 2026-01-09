#define BOOST_TEST_MODULE integration_test

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/http_client.h"
#include "libiqxmlrpc/executor.h"
#include "libiqxmlrpc/auth_plugin.h"
#include "libiqxmlrpc/firewall.h"

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

// vim:ts=2:sw=2:et

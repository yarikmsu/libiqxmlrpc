/**
 * Standalone benchmark server for RPS (Requests Per Second) testing.
 *
 * Usage:
 *   ./benchmark-server --port 8080 --numthreads 4
 *
 * This minimal server registers the 'echo' method and runs until SIGINT (Ctrl+C).
 * It's designed for external benchmarking using Python's xmlrpc.client or other
 * XML-RPC clients.
 *
 * The server supports configurable thread pool size for testing scalability.
 */

#include <csignal>
#include <iostream>
#include <memory>
#include <atomic>
#include <string>
#include <thread>
#include <boost/program_options.hpp>

#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/executor.h"
#include "libiqxmlrpc/method.h"
#include "libiqxmlrpc/value.h"

namespace {

// Global shutdown flag - set by signal handler, polled by main thread
// Using sig_atomic_t for guaranteed async-signal-safe access
volatile std::sig_atomic_t g_shutdown_requested = 0;

// Signal handler for graceful shutdown
// IMPORTANT: Only async-signal-safe operations here.
// We do NOT call server->set_exit_flag() because it internally calls
// interrupt() which acquires a mutex - not async-signal-safe.
// Instead, we set a flag and let the main thread handle shutdown.
void signal_handler(int /*sig*/) {
  g_shutdown_requested = 1;
}

/**
 * Echo method - returns the first argument unchanged.
 * This tests the full RPC stack: parsing, dispatch, serialization.
 */
void echo_method(
    iqxmlrpc::Method*,
    const iqxmlrpc::Param_list& args,
    iqxmlrpc::Value& retval)
{
  if (!args.empty()) {
    retval = args[0];
  }
}

/**
 * Serverctl.stop method - allows remote shutdown for scripted benchmarks.
 *
 * SECURITY NOTE: This method has no authentication. Only use this server
 * in trusted networks. The default bind address (127.0.0.1) mitigates
 * remote attacks. If using --bind 0.0.0.0, ensure the network is trusted.
 */
class ServerctlStop : public iqxmlrpc::Method {
public:
  void execute(const iqxmlrpc::Param_list&, iqxmlrpc::Value& retval) override {
    server().set_exit_flag();
    retval = true;
  }
};

void register_benchmark_methods(iqxmlrpc::Server& server) {
  iqxmlrpc::register_method(server, "echo", echo_method);
  iqxmlrpc::register_method<ServerctlStop>(server, "serverctl.stop");
}

struct ServerConfig {
  int port = 8080;
  int numthreads = 4;
  std::string bind_addr = "127.0.0.1";  // Default to localhost for security
  size_t max_request_size = 10 * 1024 * 1024;  // 10MB default for large payload tests
  bool quiet = false;

  bool parse(int argc, char** argv) {
    namespace po = boost::program_options;

    po::options_description desc("Benchmark Server Options");
    desc.add_options()
      ("help,h", "Show this help message")
      ("port,p", po::value<int>(&port)->default_value(8080),
        "Port to listen on")
      ("numthreads,t", po::value<int>(&numthreads)->default_value(4),
        "Number of worker threads (1 = serial executor)")
      ("bind,b", po::value<std::string>(&bind_addr)->default_value("127.0.0.1"),
        "Address to bind to (use 0.0.0.0 for all interfaces)")
      ("max-request-size,m", po::value<size_t>(&max_request_size)->default_value(10 * 1024 * 1024),
        "Maximum request size in bytes")
      ("quiet,q", po::bool_switch(&quiet),
        "Suppress startup banner");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);
    } catch (const po::error& e) {
      std::cerr << "Error: " << e.what() << "\n\n" << desc << "\n";
      return false;
    }

    if (vm.count("help")) {
      std::cout << desc << "\n";
      std::cout << "\nExample:\n";
      std::cout << "  " << argv[0] << " --port 8080 --numthreads 4\n";
      return false;
    }

    if (port <= 0 || port > 65535) {
      std::cerr << "Error: Port must be between 1 and 65535\n";
      return false;
    }

    if (numthreads <= 0) {
      std::cerr << "Error: Thread count must be positive\n";
      return false;
    }

    return true;
  }
};

}  // namespace

int main(int argc, char** argv) {
  ServerConfig config;
  if (!config.parse(argc, argv)) {
    return 1;
  }

  try {
    // Create executor factory based on thread count
    std::unique_ptr<iqxmlrpc::Executor_factory_base> exec_factory;
    if (config.numthreads > 1) {
      exec_factory = std::make_unique<iqxmlrpc::Pool_executor_factory>(config.numthreads);
    } else {
      exec_factory = std::make_unique<iqxmlrpc::Serial_executor_factory>();
    }

    // Create HTTP server
    iqxmlrpc::Http_server server(
      iqnet::Inet_addr(config.bind_addr, config.port),
      exec_factory.get());

    // Configure server
    server.set_max_request_sz(config.max_request_size);
    server.log_errors(&std::cerr);

    // Register methods
    register_benchmark_methods(server);

    // Set up signal handlers BEFORE starting server
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!config.quiet) {
      std::cout << "=== libiqxmlrpc Benchmark Server ===\n";
      std::cout << "Bind:        " << config.bind_addr << "\n";
      std::cout << "Port:        " << config.port << "\n";
      std::cout << "Threads:     " << config.numthreads
                << (config.numthreads == 1 ? " (serial)" : " (pool)") << "\n";
      std::cout << "Max request: " << (config.max_request_size / 1024 / 1024) << " MB\n";
      std::cout << "Endpoint:    http://" << config.bind_addr << ":" << config.port << "/RPC\n";
      std::cout << "\nMethods:\n";
      std::cout << "  - echo(value) -> value\n";
      std::cout << "  - serverctl.stop() -> true\n";
      std::cout << "\nPress Ctrl+C to stop...\n\n";
    }

    // Run server in a separate thread so main thread can poll for signals
    // This avoids calling set_exit_flag() from signal handler (not async-signal-safe)
    std::thread server_thread([&server]() {
      server.work();
    });

    // Poll for shutdown signal (async-signal-safe flag set by signal handler)
    while (!g_shutdown_requested) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Call set_exit_flag from main thread (safe, not in signal context)
    server.set_exit_flag();
    server_thread.join();

    std::cout << "Server stopped.\n";
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

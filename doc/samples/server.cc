#include <iostream>
#include <chrono>
#include <libiqxmlrpc/libiqxmlrpc.h>
#include <libiqxmlrpc/http_server.h>

// Simple method that just returns back first input parameter
class Echo: public iqxmlrpc::Method {
public:
  void execute(const iqxmlrpc::Param_list& params, iqxmlrpc::Value& retval)
  {
    if (params.empty())
      retval = 0;
    else
      retval = params[0];
  }
};

int main()
{
  int port = 3344;

  iqxmlrpc::Serial_executor_factory ef;
  iqxmlrpc::Http_server server(port, &ef);

  iqxmlrpc::register_method<Echo>(server, "echo");

  // optional settings
  server.log_errors( &std::cerr );
  server.enable_introspection();

  // Resource limits (recommended for production, see docs/HARDENING_GUIDE.md)
  // Defaults are 0 (unlimited) for backward compatibility.
  server.set_max_request_sz(10 * 1024 * 1024);            // 10 MB
  server.set_idle_timeout(std::chrono::seconds(30));       // 30s

  // start server
  server.work();
}

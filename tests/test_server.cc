#include <csignal>
#include <memory>
#include <iostream>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_server.h"
#include "libiqxmlrpc/https_server.h"
#include "libiqxmlrpc/executor.h"
#include "libiqxmlrpc/auth_plugin.h"
#include "server_config.h"
#include "methods.h"
#include "libiqxmlrpc/xheaders.h"

#if defined(WIN32)
#include <winsock2.h>
#endif

using namespace boost::unit_test;
using namespace iqxmlrpc;

class LogInterceptor: public Interceptor {
public:
  void process(Method* m, const Param_list& p, Value& r) override
  {
    std::cout << "Log Interceptor: " << m->name() << " is executing." << m->xheaders() << "\n";
    yield(m, p, r);
  }
};

class TraceInterceptor: public Interceptor {
public:
  void process(Method* m, const Param_list& p, Value& r) override
  {
    if( m->name() == "trace" ){
      Param_list np;
      auto it = m->xheaders().find("X-Correlation-ID");
      if (it != m->xheaders().end()) {
        np.emplace_back(it->second);
      }
      it = m->xheaders().find("X-Span-ID");
      if (it != m->xheaders().end()) {
        np.emplace_back(it->second);
      }
      yield(m, np, r);
    } else {
      yield(m, p, r);
    }
  }
};

class CallCountingInterceptor: public Interceptor {
  unsigned count;

public:
  CallCountingInterceptor(): count(0) {}

  ~CallCountingInterceptor() override
  {
    std::cout << "Calls Count: " << count << '\n';
  }

  void process(Method* m, const Param_list& p, Value& r) override
  {
    std::cout << "Executing " << ++count << " call\n";
    yield(m, p, r);
  }
};

class PermissiveAuthPlugin: public iqxmlrpc::Auth_Plugin_base {
public:
  PermissiveAuthPlugin() = default;

  bool do_authenticate(const std::string& username, const std::string& /*pw*/) const override
  {
    return username != "badman";
  }

  bool do_authenticate_anonymous() const override
  {
    return true;
  }
};

class Test_server {
  std::unique_ptr<Executor_factory_base> ef_;
  std::unique_ptr<Server> impl_;
  PermissiveAuthPlugin auth_plugin_;

public:
  Test_server(const Test_server&) = delete;
  Test_server& operator=(const Test_server&) = delete;

  Test_server(const Test_server_config&);

  Server& impl() { return *impl_; }

  void work();
};

Test_server* test_server = nullptr;

Test_server::Test_server(const Test_server_config& conf):
  ef_(nullptr),
  impl_(nullptr),
  auth_plugin_()
{
  if (conf.numthreads > 1)
  {
    ef_ = std::make_unique<Pool_executor_factory>(conf.numthreads);
  }
  else
  {
    ef_ = std::make_unique<Serial_executor_factory>();
  }

  if (conf.use_ssl)
  {
    namespace ssl = iqnet::ssl;
    ssl::ctx = ssl::Ctx::server_only("data/cert.pem", "data/pk.pem");
    impl_ = std::make_unique<Https_server>(iqnet::Inet_addr(conf.port), ef_.get());
  }
  else
  {
    impl_ = std::make_unique<Http_server>(iqnet::Inet_addr(conf.port), ef_.get());
  }

  impl_->push_interceptor(new CallCountingInterceptor);
  impl_->push_interceptor(new LogInterceptor);
  impl_->push_interceptor(new TraceInterceptor);

  impl_->log_errors( &std::cerr );
  impl_->enable_introspection();
  impl_->set_max_request_sz(static_cast<size_t>(1024) * 1024);
  impl_->set_verification_level(http::HTTP_CHECK_STRICT);

  impl_->set_auth_plugin(auth_plugin_);

  register_user_methods(impl());
}

void Test_server::work()
{
  impl_->work();
}

// Ctrl-C handler
void test_server_sig_handler(int)
{
  if (test_server)
    test_server->impl().set_exit_flag();
}

int
main(int argc, const char** argv)
{
#if defined(WIN32)
  WORD wVersionRequested;
  WSADATA wsaData;
  wVersionRequested = MAKEWORD(2, 2);
  WSAStartup(wVersionRequested, &wsaData);
#endif

  try {
    Test_server_config conf(argc, const_cast<char**>(argv));
    test_server = new Test_server(conf);
    (void)::signal(SIGINT, &test_server_sig_handler);
    test_server->work();
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "E: " << e.what() << '\n';
    return 1;
  }
}

// vim:ts=2:sw=2:et

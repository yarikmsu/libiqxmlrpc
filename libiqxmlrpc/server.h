//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#ifndef IQXMLRPC_SERVER_H
#define IQXMLRPC_SERVER_H

#include <chrono>

#include "acceptor.h"
#include "builtins.h"
#include "connection.h"
#include "conn_factory.h"
#include "dispatcher_manager.h"
#include "executor.h"
#include "firewall.h"
#include "http.h"
#include "util.h"

namespace iqnet
{
  class Reactor_base;
  class Reactor_interrupter;
}

namespace iqxmlrpc {

class Auth_Plugin_base;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#pragma warning(disable: 4275)
#endif

//! XML-RPC server.
class LIBIQXMLRPC_API Server {
public:
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  Server(
    const iqnet::Inet_addr& addr,
    iqnet::Accepted_conn_factory* conn_factory,
    Executor_factory_base* executor_factory );

  virtual ~Server();

  //! Register method using abstract factory.
  void register_method(const std::string& name, Method_factory_base*);

  //! Push one more alternative Method Dispatcher
  //! Method Dispatchers will be used in order they added
  //! until requested method would't be found.
  //! Grabs ownership.
  void push_dispatcher(Method_dispatcher_base*);

  //! Push user defined interceptor into stack of interceptors.
  //! Grabs the ownership.
  void push_interceptor(Interceptor*);

  //! Allow clients to request introspection information
  //! via special built-in methods.
  void enable_introspection();

  //! Set stream to log errors. Transfer NULL to turn loggin off.
  void log_errors( std::ostream* );

  //! Set maximum size of incoming client's request in bytes.
  void set_max_request_sz( size_t );
  size_t get_max_request_sz() const;

  //! Set optional firewall object.
  //! Takes ownership of the pointer. Must be heap-allocated via new.
  //! Pass nullptr to remove the firewall.
  void set_firewall( iqnet::Firewall_base* );

  void set_verification_level(http::Verification_level);
  http::Verification_level get_verification_level() const;

  void set_auth_plugin(const Auth_Plugin_base&);

  //! Set idle timeout for keep-alive connections.
  //! Connections waiting for input longer than this will be closed.
  //! A value of 0 disables idle timeout (default).
  void set_idle_timeout(std::chrono::milliseconds timeout);
  std::chrono::milliseconds get_idle_timeout() const;

  //! Register a connection for idle timeout tracking.
  void register_connection(Server_connection* conn);

  //! Unregister a connection from idle timeout tracking.
  void unregister_connection(Server_connection* conn);
  /*! \} */

  //! \name Run/stop server
  /*! \{ */
  //! Process accepting connections and methods dispatching.
  void work();

  //! Ask server to exit from work() event handle loop.
  void set_exit_flag();

  //! Interrupt poll cycle.
  void interrupt();
  /*! \} */

  iqnet::Reactor_base* get_reactor();

  void schedule_execute( http::Packet*, Server_connection* );
  void schedule_response( const Response&, Server_connection*, Executor* );
  void schedule_response( const Response&, const ConnectionGuardPtr&, Executor* );

  void log_err_msg( const std::string& );

protected:
  iqnet::Accepted_conn_factory* get_conn_factory();

private:
  void check_idle_timeouts();

  class Impl;
  Impl *impl;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

//! Register class Method_class as handler for call "name" with specific server.
template <class Method_class>
inline void register_method(Server& server, const std::string& name)
{
  server.register_method(name, new Method_factory<Method_class>);
}

//! Register function "fn" as handler for call "name" with specific server.
inline void LIBIQXMLRPC_API
register_method(Server& server, const std::string& name, Method_function fn)
{
  server.register_method(name, new Method_factory<Method_function_adapter>(fn));
}

} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

#include "server.h"
#include "auth_plugin.h"
#include "http_errors.h"
#include "reactor.h"
#include "reactor_interrupter.h"
#include "request.h"
#include "response.h"
#include "server_conn.h"
#include "xheaders.h"

namespace iqxmlrpc {

class Server::Impl {
public:
  Executor_factory_base* exec_factory;

  iqnet::Inet_addr bind_addr;

  std::unique_ptr<iqnet::Reactor_base>          reactor;
  std::unique_ptr<iqnet::Reactor_interrupter>   interrupter;
  std::unique_ptr<iqnet::Accepted_conn_factory> conn_factory;
  std::unique_ptr<iqnet::Acceptor>              acceptor;
  std::atomic<iqnet::Firewall_base*> firewall;

  std::atomic<bool> exit_flag;
  std::ostream* log;
  size_t max_req_sz;
  http::Verification_level ver_level;

  Method_dispatcher_manager  disp_manager;
  std::unique_ptr<Interceptor> interceptors;
  const Auth_Plugin_base*    auth_plugin;

  std::atomic<int64_t> idle_timeout_ms{0};

  // Mutex for connections set - provides defensive synchronization.
  // Currently all access is from the reactor thread, but mutex protects
  // against future changes or API misuse.
  std::set<Server_connection*> connections;
  mutable std::mutex connections_mutex;

  // Mutex for acceptor - protects against race between set_firewall()
  // from main thread and work() from worker thread
  mutable std::mutex acceptor_mutex;

  Impl(
    const iqnet::Inet_addr& addr,
    iqnet::Accepted_conn_factory* cf,
    Executor_factory_base* ef):
      exec_factory(ef),
      bind_addr(addr),
      reactor(ef->create_reactor()),
      interrupter(new iqnet::Reactor_interrupter(reactor.get())),
      conn_factory(cf),
      acceptor(nullptr),
      firewall(nullptr),
      exit_flag(false),
      log(nullptr),
      max_req_sz(0),
      ver_level(http::HTTP_CHECK_WEAK),
      disp_manager(),
      interceptors(nullptr),
      auth_plugin(nullptr),
      idle_timeout_ms(0),
      connections(),
      connections_mutex(),
      acceptor_mutex()
  {
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
};

// ---------------------------------------------------------------------------
Server::Server(
  const iqnet::Inet_addr& addr,
  iqnet::Accepted_conn_factory* cf,
  Executor_factory_base* ef):
    impl(new Server::Impl(addr, cf, ef))
{
}

Server::~Server()
{
  delete impl;
}

void Server::register_method(const std::string& name, Method_factory_base* f)
{
  impl->disp_manager.register_method(name, f);
}

void Server::set_exit_flag()
{
  impl->exit_flag = true;
  interrupt();
}

void Server::interrupt()
{
  impl->interrupter->make_interrupt();
}

iqnet::Reactor_base* Server::get_reactor()
{
  return impl->reactor.get();
}

void Server::push_interceptor(Interceptor* ic)
{
  // Exception-safe interceptor chaining:
  // Release old interceptor first, then try to nest it in the new one.
  // If nest() throws, restore the old interceptor to prevent memory leak.
  Interceptor* old = impl->interceptors.release();
  try {
    ic->nest(old);
    impl->interceptors.reset(ic);
  } catch (...) {
    impl->interceptors.reset(old);  // Restore on failure
    throw;
  }
}

void Server::push_dispatcher(Method_dispatcher_base* disp)
{
  impl->disp_manager.push_back(disp);
}

void Server::enable_introspection()
{
  impl->disp_manager.enable_introspection();
}

void Server::log_errors( std::ostream* log_ )
{
  impl->log = log_;
}

void Server::set_max_request_sz( size_t sz )
{
  impl->max_req_sz = sz;
}

size_t Server::get_max_request_sz() const
{
  return impl->max_req_sz;
}

void Server::set_verification_level( http::Verification_level lev )
{
  impl->ver_level = lev;
}

http::Verification_level Server::get_verification_level() const
{
  return impl->ver_level;
}

void Server::set_auth_plugin( const Auth_Plugin_base& ap )
{
  impl->auth_plugin = &ap;
}

void Server::set_idle_timeout(std::chrono::milliseconds timeout)
{
  impl->idle_timeout_ms = timeout.count();
}

std::chrono::milliseconds Server::get_idle_timeout() const
{
  return std::chrono::milliseconds(impl->idle_timeout_ms.load());
}

void Server::register_connection(Server_connection* conn)
{
  std::lock_guard<std::mutex> lock(impl->connections_mutex);
  impl->connections.insert(conn);
}

void Server::unregister_connection(Server_connection* conn)
{
  // SECURITY: Notify firewall that connection is closing for accurate tracking
  iqnet::Firewall_base* fw = impl->firewall.load();
  if (fw) {
    fw->release(conn->get_peer_addr());
  }

  std::lock_guard<std::mutex> lock(impl->connections_mutex);
  impl->connections.erase(conn);
}

void Server::log_err_msg( const std::string& msg )
{
  if( impl->log )
    *impl->log << msg << '\n';
}

namespace {

std::optional<std::string>
authenticate(const http::Packet& pkt, const Auth_Plugin_base* ap)
{
  using namespace http;

  if (!ap)
    return std::nullopt;

  // Use type tag instead of dynamic_cast for performance (M3 optimization)
  const Header* hdr_base = pkt.header();
  if (hdr_base->header_type() != HeaderType::REQUEST)
    throw http::Malformed_packet("Expected request header");
  const auto& hdr = static_cast<const Request_header&>(*hdr_base);

  if (!hdr.has_authinfo())
  {
    if (!ap->authenticate_anonymous())
      throw Unauthorized();

    return std::nullopt;
  }

  std::string username, password;
  hdr.get_authinfo(username, password);

  if (!ap->authenticate(username, password))
    throw Unauthorized();

  return username;
}

} // anonymous namespace

void Server::schedule_execute( http::Packet* pkt, Server_connection* conn )
{
  Executor* executor = nullptr;

  try {
    std::unique_ptr<http::Packet> packet(pkt);
    std::optional<std::string> authname = authenticate(*pkt, impl->auth_plugin);
    std::unique_ptr<Request> req( parse_request(packet->content()) );

    Method::Data mdata = {
      req->get_name(),
      conn->get_peer_addr(),
      Server_feedback(this)
    };

    Method* meth = impl->disp_manager.create_method( mdata );

    if (authname)
      meth->authname(*authname);

    pkt->header()->get_xheaders(meth->xheaders());

    executor = impl->exec_factory->create( meth, this, conn );
    executor->set_interceptors(impl->interceptors.get());
    executor->execute( req->get_params() );
  }
  catch( const iqxmlrpc::http::Error_response& e )
  {
    log_err_msg( e.what() );
    std::unique_ptr<Executor> executor_to_delete(executor);
    // Packet payload is sufficient here; slicing is intentional.
    auto *err_pkt = new http::Packet(e);  // NOLINT(cppcoreguidelines-slicing)
    conn->schedule_response( err_pkt );
  }
  catch( const iqxmlrpc::Exception& e )
  {
    log_err_msg( std::string("Server: ") + e.what() );
    Response err_r( e.code(), e.what() );
    schedule_response( err_r, conn, executor );
  }
  catch( const std::exception& e )
  {
    log_err_msg( std::string("Server: ") + e.what() );
    Response err_r( -32500 /*application error*/, e.what() );
    schedule_response( err_r, conn, executor );
  }
  catch( ... )
  {
    log_err_msg( "Server: Unknown exception" );
    Response err_r( -32500  /*application error*/, "Unknown Error" );
    schedule_response( err_r, conn, executor );
  }
}

// cppcheck-suppress functionStatic
void Server::schedule_response(
  const Response& resp, Server_connection* conn, Executor* exec )
{
  std::unique_ptr<Executor> executor_to_delete(exec);
  std::string resp_str = dump_response(resp);
  auto *packet = new http::Packet(new http::Response_header(), resp_str);
  conn->schedule_response( packet );
}

void Server::set_firewall( iqnet::Firewall_base* _firewall )
{
  impl->firewall = _firewall;
  // Propagate to existing acceptor if server is already running
  // Lock to prevent race with work() creating/destroying acceptor
  std::lock_guard<std::mutex> lock(impl->acceptor_mutex);
  if (impl->acceptor)
    impl->acceptor->set_firewall(_firewall);
}

void Server::check_idle_timeouts()
{
  const auto timeout_ms = impl->idle_timeout_ms.load();
  if (timeout_ms <= 0)
    return;

  // Collect expired connections first to avoid modifying set during iteration
  std::vector<Server_connection*> expired;
  auto timeout = std::chrono::milliseconds(timeout_ms);
  {
    std::lock_guard<std::mutex> lock(impl->connections_mutex);
    std::copy_if(impl->connections.begin(), impl->connections.end(),
                 std::back_inserter(expired),
                 // cppcheck-suppress constParameterPointer
                 [timeout](Server_connection* conn) {
                   return conn->is_idle_timeout_expired(timeout);
                 });
  }

  // Terminate expired connections (outside lock)
  for (auto* conn : expired)
  {
    log_err_msg("Connection idle timeout expired for " +
                conn->get_peer_addr().get_host_name());
    conn->terminate_idle();
  }
}

void Server::work()
{
  // Lock to prevent race with set_firewall() accessing acceptor
  {
    std::lock_guard<std::mutex> lock(impl->acceptor_mutex);
    if( !impl->acceptor )
    {
      impl->acceptor = std::make_unique<iqnet::Acceptor>( impl->bind_addr, get_conn_factory(), get_reactor());
      impl->acceptor->set_firewall( impl->firewall );
    }
  }

  // Use shorter poll timeout when idle timeout is enabled
  // to ensure timely cleanup of idle connections
  const int poll_timeout_ms = (impl->idle_timeout_ms.load() > 0) ? 1000 : -1;

  for(bool have_handlers = true; have_handlers;)
  {
    if (impl->exit_flag)
      break;

    have_handlers = get_reactor()->handle_events(poll_timeout_ms);
    check_idle_timeouts();
  }

  // Lock to prevent race with set_firewall() accessing acceptor
  {
    std::lock_guard<std::mutex> lock(impl->acceptor_mutex);
    impl->acceptor.reset();
  }
  impl->exit_flag = false;
}

iqnet::Accepted_conn_factory* Server::get_conn_factory()
{
  return impl->conn_factory.get();
}

} // namespace iqxmlrpc

// vim:ts=2:sw=2:et

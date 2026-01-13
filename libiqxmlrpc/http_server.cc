//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include "http_server.h"
#include "server.h"
#include "server_conn.h"

using namespace iqnet;

namespace iqxmlrpc {

namespace {

//! Represents server-side \b HTTP non-blocking connection.
class Http_server_connection:
  public iqnet::Connection,
  public Server_connection
{
  iqnet::Reactor_base* reactor = nullptr;

public:
  explicit Http_server_connection( const iqnet::Socket& );

  Http_server_connection(const Http_server_connection&) = delete;
  Http_server_connection& operator=(const Http_server_connection&) = delete;

  void set_reactor( iqnet::Reactor_base* r ) { reactor = r; }

  void post_accept() override;
  void finish() override;

  void handle_input( bool& ) override;
  void handle_output( bool& ) override;

  void terminate_idle() override;

  bool catch_in_reactor() const override { return true; }
  void log_exception( const std::exception& ) override;
  void log_unknown_exception() override;

private:
  void do_schedule_response() override;
};

typedef Server_conn_factory<Http_server_connection> Http_conn_factory;

} // anonymous namespace

//
// Http_server
//

Http_server::Http_server(const iqnet::Inet_addr& bind_addr, Executor_factory_base* ef):
  Server(bind_addr, new Http_conn_factory, ef)
{
  static_cast<Http_conn_factory*>(get_conn_factory())->post_init(this, get_reactor());
}

//
// Http_server_connection
//

Http_server_connection::Http_server_connection( const iqnet::Socket& s ):
  Connection( s ),
  Server_connection( s.get_peer_addr() )
{
}


void Http_server_connection::post_accept()
{
  sock.set_non_blocking(true);
  server->register_connection(this);
  start_idle();
  reactor->register_handler( this, Reactor_base::INPUT );
}


void Http_server_connection::finish()
{
  server->unregister_connection(this);
  delete this;
}


void Http_server_connection::handle_input( bool& terminate )
{
  try {
    size_t n = recv( read_buf(), read_buf_sz() );

    if( !n )
    {
      terminate = true;
      return;
    }

    http::Packet* packet = read_request( std::string(read_buf(), n) );
    if( !packet )
      return;

    stop_idle();
    reactor->unregister_handler( this, Reactor_base::INPUT );
    server->schedule_execute( packet, this );
  }
  catch( const http::Error_response& e )
  {
    // Close connection after sending HTTP error response
    keep_alive = false;
    schedule_response( new http::Packet(e) );
  }
}


void Http_server_connection::handle_output( bool& terminate )
{
  // Defensive check: prevent unsigned underflow if offset exceeds length
  // This should never happen in normal operation, but protects against
  // corruption or logic errors that could cause buffer over-read in send()
  if (response_offset > response.length()) {
    server->log_err_msg("Response offset corruption detected, resetting connection");
    response.clear();
    response_offset = 0;
    terminate = true;
    return;
  }

  // Use offset tracking instead of O(n) string erase for partial sends
  size_t remaining = response.length() - response_offset;
  size_t sz = send( response.c_str() + response_offset, remaining );

  if( sz == remaining )
  {
    // Response fully sent - clear for potential reuse
    response.clear();
    response_offset = 0;

    if( keep_alive )
    {
      reactor->unregister_handler( this, Reactor_base::OUTPUT );
      start_idle();
      reactor->register_handler( this, Reactor_base::INPUT );
    }
    else
      terminate = true;

    return;
  }

  // Partial send - just advance offset (O(1) instead of O(n) erase)
  response_offset += sz;
}


void Http_server_connection::do_schedule_response()
{
  reactor->register_handler( this, iqnet::Reactor_base::OUTPUT );
}


void Http_server_connection::terminate_idle()
{
  // Atomically check if still idle to prevent TOCTOU race:
  // Connection may have received data between timeout check and termination
  if (!try_claim_for_termination()) {
    return;  // Connection is no longer idle, don't terminate
  }
  reactor->unregister_handler( this );
  finish();
}


void Http_server_connection::log_exception( const std::exception& ex )
{
  std::string s( "iqxmlrpc::Http_server_connection: " );
  s += ex.what();
  server->log_err_msg( s );
}


void Http_server_connection::log_unknown_exception()
{
  server->log_err_msg( "iqxmlrpc::Http_server_connection: unknown exception." );
}

} // namespace iqxmlrpc

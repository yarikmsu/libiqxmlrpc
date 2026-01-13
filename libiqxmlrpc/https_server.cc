//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include "https_server.h"
#include "server_conn.h"
#include "ssl_connection.h"

using namespace iqnet;

namespace iqxmlrpc {

namespace {

//! Represents server-side \b HTTPS non-blocking connection.
class Https_server_connection:
  public iqnet::ssl::Reaction_connection,
  public iqxmlrpc::Server_connection
{
public:
  explicit Https_server_connection( const iqnet::Socket& );

  void post_accept() override { Reaction_connection::post_accept(); }
  void finish() override;

  void terminate_idle() override;

  bool catch_in_reactor() const override { return true; }
  void log_exception( const std::exception& ) override;
  void log_unknown_exception() override;

protected:
  void my_reg_recv();
  void accept_succeed() override;
  void recv_succeed( bool& terminate, size_t req_len, size_t real_len ) override;
  void send_succeed( bool& terminate ) override;
  void do_schedule_response() override;
};

typedef Server_conn_factory<Https_server_connection> Https_conn_factory;

} // anonymous namespace

//
// Https_server
//

Https_server::Https_server(const iqnet::Inet_addr& bind_addr, Executor_factory_base* ef):
  Server(bind_addr, new Https_conn_factory, ef)
{
  static_cast<Https_conn_factory*>(get_conn_factory())->post_init(this, get_reactor());
}


//
// Https_server_connection
//

Https_server_connection::Https_server_connection( const iqnet::Socket& s ):
  ssl::Reaction_connection( s ),
  Server_connection( s.get_peer_addr() )
{
}


inline void Https_server_connection::my_reg_recv()
{
  reg_recv( read_buf(), read_buf_sz() );
}


void Https_server_connection::finish()
{
  server->unregister_connection(this);
  delete this;
}


void Https_server_connection::terminate_idle()
{
  // Atomically check if still idle to prevent TOCTOU race:
  // Connection may have received data between timeout check and termination
  if (!try_claim_for_termination()) {
    return;  // Connection is no longer idle, don't terminate
  }
  // Use reg_shutdown to properly close SSL connection
  reg_shutdown();
  finish();
}


void Https_server_connection::accept_succeed()
{
  server->register_connection(this);
  start_idle();
  my_reg_recv();
}


void Https_server_connection::recv_succeed( bool&, size_t, size_t real_len )
{
  try
  {
    std::string s( read_buf(), real_len );
    http::Packet* packet = read_request( s );

    if( !packet )
    {
      if (response.empty())
        my_reg_recv();
      return;
    }

    stop_idle();
    server->schedule_execute( packet, this );
  }
  catch( const http::Error_response& e )
  {
    // Close connection after sending HTTP error response
    keep_alive = false;
    schedule_response( new http::Packet(e) );
  }
}


void Https_server_connection::send_succeed( bool& terminate )
{
  response = std::string();

  if( keep_alive )
  {
    start_idle();
    my_reg_recv();
  }
  else
    terminate = reg_shutdown();
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

void Https_server_connection::do_schedule_response()
{
  reg_send( response.data(), response.length() );
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

void Https_server_connection::log_exception( const std::exception& ex )
{
  std::string s( "iqxmlrpc::Https_server_connection: " );
  s += ex.what();
  server->log_err_msg( s );
}


void Https_server_connection::log_unknown_exception()
{
  server->log_err_msg( "iqxmlrpc::Https_server_connection: unknown exception." );
}

} // namespace iqxmlrpc

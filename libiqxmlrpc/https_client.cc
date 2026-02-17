//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include "https_client.h"

#include "client_opts.h"
#include "num_conv.h"
#include "reactor_impl.h"
#include "safe_math.h"

using namespace iqnet;

namespace iqxmlrpc {

//
// Proxy_request_header
//

class LIBIQXMLRPC_API Proxy_request_header: public http::Header {
public:
  explicit Proxy_request_header( const iqnet::Inet_addr& connect_addr ):
    addr_(connect_addr) {}

private:
  std::string dump_head() const override
  {
    return
      "CONNECT "
      + addr_.get_host_name() + ":"
      + num_conv::to_string(addr_.get_port())
      + " HTTP/1.0\r\n";
  }

  const iqnet::Inet_addr& addr_;
};

//
// Https_proxy_client_connection
//

Https_proxy_client_connection::Https_proxy_client_connection(
  const iqnet::Socket& s,
  bool nb
):
  Client_connection(),
  Connection( s ),
  reactor( new Reactor<Null_lock> ),
  resp_packet(nullptr),
  non_blocking(nb),
  out_str(),
  out_str_offset(0),
  expected_hostname_()
{
  sock.set_non_blocking( nb );
}

http::Packet* Https_proxy_client_connection::do_process_session( const std::string& s )
{
  setup_tunnel();

#ifdef IQXMLRPC_TESTING
  // Use factory if provided (for testing), otherwise create real SSL connection
  if (ssl_factory_) {
    return ssl_factory_(sock, non_blocking, s);
  }
#endif

  Https_client_connection https_conn(sock, non_blocking);
  https_conn.set_options(opts());
  if (!expected_hostname_.empty())
    https_conn.set_ssl_expected_hostname(expected_hostname_);
  https_conn.post_connect();

  return https_conn.do_process_session(s);
}

void Https_proxy_client_connection::setup_tunnel()
{
  reactor->register_handler( this, Reactor_base::OUTPUT );

  Proxy_request_header h(opts().addr());
  out_str = h.dump();
  out_str_offset = 0;  // Reset offset for new request

  do {
    int to = opts().timeout() >= 0 ? opts().timeout() * 1000 : -1;
    if( !reactor->handle_events(to) )
      throw Client_timeout();
  }
  while( !resp_packet );

  const auto* res_h =
    static_cast<const http::Response_header*>(resp_packet->header());

  if( res_h->code() != 200 )
    throw http::Error_response( res_h->phrase(), res_h->code() );
}

void Https_proxy_client_connection::handle_output( bool& )
{
  // Use offset tracking instead of string.erase() for O(1) vs O(n) performance
  size_t remaining = out_str.length() - out_str_offset;
  size_t sz = send( out_str.c_str() + out_str_offset, remaining );
  safe_math::add_assign(out_str_offset, sz);

  if( out_str_offset >= out_str.length() )
  {
    out_str.clear();  // Release memory
    out_str_offset = 0;
    reactor->unregister_handler( this, Reactor_base::OUTPUT );
    reactor->register_handler( this, Reactor_base::INPUT );
  }
}

void Https_proxy_client_connection::handle_input( bool& )
{
  for( size_t sz = read_buf_sz(); (sz == read_buf_sz()) && !resp_packet ; )
  {
    sz = recv( read_buf(), read_buf_sz() );
    if( sz == 0 )
      throw iqnet::network_error( "Connection closed by peer.", false );

    resp_packet.reset( read_response(std::string(read_buf(), sz), true) );
  }

  if( resp_packet )
  {
    reactor->unregister_handler( this );
  }
}

//
// Https_client_connection
//

Https_client_connection::Https_client_connection( const iqnet::Socket& s, bool nb ):
  Client_connection(),
  iqnet::ssl::Reaction_connection( s ),
  reactor( new Reactor<Null_lock> ),
  resp_packet(nullptr),
  out_str(),
  established(false)
{
  iqnet::ssl::Reaction_connection::sock.set_non_blocking( nb );
}


inline void Https_client_connection::reg_send_request()
{
  reg_send( out_str.c_str(), out_str.length() );
}


http::Packet* Https_client_connection::do_process_session( const std::string& s )
{
  out_str = s;
  resp_packet = nullptr;

  if( established )
    reg_send_request();

  do {
    int to = opts().timeout() >= 0 ? opts().timeout() * 1000 : -1;
    if( !reactor->handle_events(to) )
      throw Client_timeout();
  }
  while( !resp_packet );

  return resp_packet;
}


void Https_client_connection::connect_succeed()
{
  established = true;
  reg_send_request();
}


void Https_client_connection::send_succeed( bool& )
{
  reg_recv( read_buf(), read_buf_sz() );
}


void Https_client_connection::recv_succeed( bool&, size_t, size_t sz )
{
  if( !sz )
    throw iqnet::network_error( "Connection closed by peer.", false );

  std::string s( read_buf(), sz );
  resp_packet = read_response( s );

  if( !resp_packet )
  {
    reg_recv( read_buf(), read_buf_sz() );
  }
}

} // namespace iqxmlrpc

// vim:ts=2:sw=2:et

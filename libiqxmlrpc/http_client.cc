//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include "http_client.h"

#include "client_opts.h"
#include "reactor_impl.h"
#include "safe_math.h"

using namespace iqxmlrpc;
using namespace iqnet;


Http_client_connection::Http_client_connection( const iqnet::Socket& s, bool nb ):
  Client_connection(),
  Connection( s ),
  reactor( new Reactor<Null_lock> ),
  out_str(),
  out_str_offset(0),
  resp_packet()
{
  sock.set_non_blocking( nb );
}


std::unique_ptr<http::Packet> Http_client_connection::do_process_session( const std::string& s )
{
  out_str = s;
  out_str_offset = 0;  // Reset offset for new request
  resp_packet.reset();
  reactor->register_handler( this, Reactor_base::OUTPUT );

  do {
    int to = opts().timeout() >= 0 ? opts().timeout() * 1000 : -1;
    if( !reactor->handle_events(to) )
      throw Client_timeout();
  }
  // cppcheck-suppress knownConditionTrueFalse
  while( !resp_packet );

  return std::move(resp_packet);
}


void Http_client_connection::handle_output( bool& )
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


void Http_client_connection::handle_input( bool& )
{
  // cppcheck-suppress knownConditionTrueFalse
  for( size_t sz = read_buf_sz(); (sz == read_buf_sz()) && !resp_packet ; )
  {
    sz = recv( read_buf(), read_buf_sz() );
    if( sz == 0 )
      throw iqnet::network_error( "Connection closed by peer.", false );

    resp_packet.reset( read_response( std::string(read_buf(), sz) ) );
  }

  // cppcheck-suppress knownConditionTrueFalse
  if( resp_packet )
    reactor->unregister_handler( this );
}

//
// Http_proxy_client_connection
//

std::string Http_proxy_client_connection::decorate_uri() const
{
  std::string result = "http://";
  // Reserve space for: vhost + ':' + port (max 5 digits) + '/' + uri
  // Reserve is called after initial assignment to avoid reallocation
  result.reserve(result.size() + opts().vhost().size() + 1 + 5 + 1 + opts().uri().size());

  result += opts().vhost();
  result += ':';
  result += std::to_string(opts().addr().get_port());

  if (!opts().uri().empty() && opts().uri()[0] != '/')
    result += '/';

  result += opts().uri();

  return result;
}

// vim:ts=2:sw=2:et

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#ifndef LIBIQXMLRPC_HTTP_CLIENT_H
#define LIBIQXMLRPC_HTTP_CLIENT_H

#include <memory>

#include "client.h"
#include "client_conn.h"
#include "connector.h"
#include "reactor.h"

namespace iqxmlrpc
{

class Http_proxy_client_connection;

//! XML-RPC \b HTTP client's connection (works in blocking mode).
class LIBIQXMLRPC_API Http_client_connection:
  public iqxmlrpc::Client_connection,
  public iqnet::Connection
{
  std::unique_ptr<iqnet::Reactor_base> reactor;
  std::string out_str;
  size_t out_str_offset;  // Offset tracking instead of string.erase() for performance
  std::unique_ptr<http::Packet> resp_packet;

public:
  typedef Http_proxy_client_connection Proxy_connection;

  Http_client_connection( const iqnet::Socket&, bool non_block );

  Http_client_connection(const Http_client_connection&) = delete;
  Http_client_connection& operator=(const Http_client_connection&) = delete;

  void handle_input( bool& ) override;
  void handle_output( bool& ) override;

protected:
  std::unique_ptr<http::Packet> do_process_session( const std::string& ) override;
};

//! XML-RPC \b HTTP PROXY client connection.
//! DO NOT USE IT IN YOUR CODE.
class LIBIQXMLRPC_API Http_proxy_client_connection:
  public Http_client_connection
{
public:
  Http_proxy_client_connection( const iqnet::Socket& s, bool non_block ):
    Http_client_connection( s, non_block ) {}

private:
  std::string decorate_uri() const override;
};

} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

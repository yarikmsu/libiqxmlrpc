//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#ifndef LIBIQXMLRPC_HTTPS_CLIENT_H
#define LIBIQXMLRPC_HTTPS_CLIENT_H

#include "client.h"
#include "client_conn.h"
#include "connection.h"
#include "reactor.h"
#include "ssl_connection.h"

#include <functional>
#include <memory>

namespace iqxmlrpc
{

//! XML-RPC \b HTTPS PROXY client connection.
//! DO NOT USE IT IN YOUR CODE.
class LIBIQXMLRPC_API Https_proxy_client_connection:
  public iqxmlrpc::Client_connection,
  public iqnet::Connection
{
public:
#ifdef IQXMLRPC_TESTING
#pragma message("WARNING: IQXMLRPC_TESTING enabled - SSL factory injection available. NOT FOR PRODUCTION!")
  //! Factory function type for creating SSL connections.
  //! @warning Testing only - bypasses SSL security! Not available in release builds.
  using SslConnectionFactory = std::function<http::Packet*(
    const iqnet::Socket&, bool non_blocking, const std::string& request)>;

  //! Set custom SSL connection factory for testing.
  //! @warning This bypasses all SSL/TLS security! Only available when
  //!          compiled with -DIQXMLRPC_TESTING. Do not use in production.
  void set_ssl_factory(SslConnectionFactory factory) { ssl_factory_ = std::move(factory); }
#endif // IQXMLRPC_TESTING

  Https_proxy_client_connection( const iqnet::Socket&, bool non_block_flag );

  Https_proxy_client_connection(const Https_proxy_client_connection&) = delete;
  Https_proxy_client_connection& operator=(const Https_proxy_client_connection&) = delete;

  void set_ssl_expected_hostname(const std::string& hostname) override {
    expected_hostname_ = hostname;
  }

  void handle_input( bool& ) override;
  void handle_output( bool& ) override;

protected:
  http::Packet* do_process_session( const std::string& ) override;

  void setup_tunnel();

  std::unique_ptr<iqnet::Reactor_base> reactor;
  std::unique_ptr<http::Packet> resp_packet;
  bool non_blocking;
  std::string out_str;
  size_t out_str_offset;  // Offset tracking instead of string.erase() for performance

private:
  std::string expected_hostname_;
#ifdef IQXMLRPC_TESTING
  SslConnectionFactory ssl_factory_{};  //!< Optional factory for testing
#endif // IQXMLRPC_TESTING
};

//! XML-RPC \b HTTPS client's connection.
class LIBIQXMLRPC_API Https_client_connection:
  public iqxmlrpc::Client_connection,
  public iqnet::ssl::Reaction_connection
{
  std::unique_ptr<iqnet::Reactor_base> reactor;
  std::unique_ptr<http::Packet> resp_packet;
  std::string out_str;
  bool established;

public:
  typedef Https_proxy_client_connection Proxy_connection;

  Https_client_connection( const iqnet::Socket&, bool non_block_flag );

  Https_client_connection(const Https_client_connection&) = delete;
  Https_client_connection& operator=(const Https_client_connection&) = delete;

  void set_ssl_expected_hostname(const std::string& hostname) override {
    iqnet::ssl::Connection::set_expected_hostname(hostname);
  }

  void post_connect() override
  {
    set_reactor( reactor.get() );
    iqnet::ssl::Reaction_connection::post_connect();
  }

  void connect_succeed() override;
  void send_succeed( bool& ) override;
  void recv_succeed( bool&, size_t req_len, size_t real_len  ) override;

protected:
  friend class Https_proxy_client_connection;
  http::Packet* do_process_session( const std::string& ) override;

private:
  void reg_send_request();
};

} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

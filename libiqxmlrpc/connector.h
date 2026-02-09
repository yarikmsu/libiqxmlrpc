//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef LIBIQNET_CONNECTOR_H
#define LIBIQNET_CONNECTOR_H

#include "client_conn.h"

#include <string>

namespace iqnet {

class LIBIQXMLRPC_API Connector_base {
  Inet_addr peer_addr;
  std::string expected_hostname_;

public:
  explicit Connector_base( const iqnet::Inet_addr& peer );
  virtual ~Connector_base();

  //! Process connection.
  iqxmlrpc::Client_connection* connect(int timeout);

  //! Set expected hostname for per-connection SSL hostname verification.
  void set_expected_hostname(const std::string& hostname) { expected_hostname_ = hostname; }

protected:
  const std::string& expected_hostname() const { return expected_hostname_; }

private:
  virtual iqxmlrpc::Client_connection*
  create_connection(const Socket&) = 0;
};

template <class Conn_type>
class Connector: public Connector_base {
public:
  explicit Connector( const iqnet::Inet_addr& peer ):
    Connector_base(peer)
  {
  }

private:
  iqxmlrpc::Client_connection*
  create_connection(const Socket& s) override
  {
    auto* c = new Conn_type( s, true );
    if (!expected_hostname().empty())
      c->set_ssl_expected_hostname(expected_hostname());
    c->post_connect();
    return c;
  }
};

} // namespace iqnet

#endif
// vim:ts=2:sw=2:et

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef LIBIQNET_ACCEPTOR_H
#define LIBIQNET_ACCEPTOR_H

#include "inet_addr.h"
#include "reactor.h"
#include "socket.h"

#include <memory>

namespace iqnet {

class Accepted_conn_factory;
class Firewall_base;

//! An implementation of pattern that separates TCP-connection
//! establishment from connection handling.
/*!
    Acceptor binds server-side socket to specified bind_addr
    and waits for incoming connections. When incoming connection
    is occured the Acceptor is using instance of Connection_factory
    to create a specific connection handler.
*/
class LIBIQXMLRPC_API Acceptor: public Event_handler {
  Socket sock;
  Accepted_conn_factory *factory;
  Reactor_base *reactor;
  std::shared_ptr<Firewall_base> firewall;

public:
  Acceptor( const iqnet::Inet_addr& bind_addr, Accepted_conn_factory*, Reactor_base* );
  ~Acceptor() override;

  Acceptor(const Acceptor&) = delete;
  Acceptor& operator=(const Acceptor&) = delete;

  void set_firewall( std::shared_ptr<Firewall_base> );

  void handle_input( bool& ) override;

protected:
  void finish() override {}
  Socket::Handler get_handler() const override { return sock.get_handler(); }

  void accept();
  void listen();
};

} // namespace iqnet

#endif

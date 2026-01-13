//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include "acceptor.h"

#include "connection.h"
#include "conn_factory.h"
#include "firewall.h"
#include "inet_addr.h"
#include "net_except.h"

using namespace iqnet;


Acceptor::Acceptor( const iqnet::Inet_addr& bind_addr, Accepted_conn_factory* factory_, Reactor_base* reactor_ ):
  sock(),
  factory(factory_),
  reactor(reactor_),
  firewall(nullptr)
{
  sock.bind( bind_addr );
  listen();
  reactor->register_handler( this, Reactor_base::INPUT );
}


Acceptor::~Acceptor()
{
  reactor->unregister_handler(this);
  sock.close();
}


void Acceptor::set_firewall( iqnet::Firewall_base* fw )
{
  // Atomically swap to new firewall and delete the old one
  Firewall_base* old = firewall.exchange(fw);
  delete old;
}


void Acceptor::handle_input( bool& )
{
  accept();
}


inline void Acceptor::listen()
{
  sock.listen( 100 );
}


void Acceptor::accept()
{
  Socket new_sock( sock.accept() );

  // Load firewall pointer atomically for thread-safe access
  Firewall_base* fw = firewall.load();
  if( fw && !fw->grant( new_sock.get_peer_addr() ) )
  {
    std::string msg = fw->message();

    if (!msg.empty())
    {
      new_sock.send_shutdown(msg.c_str(), msg.size());
    } else {
      new_sock.shutdown();
    }

    return;
  }

  factory->create_accepted( new_sock );
}

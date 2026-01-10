//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _libiqnet_ssl_connection_
#define _libiqnet_ssl_connection_

#include "connection.h"
#include "conn_factory.h"
#include "reactor.h"
#include "ssl_lib.h"

#include <openssl/ssl.h>

namespace iqnet {
namespace ssl {

//! SSL connection class.
class LIBIQXMLRPC_API Connection: public iqnet::Connection {
protected:
  ssl::Ctx* ssl_ctx;
  SSL *ssl;

public:
  explicit Connection( const Socket& sock );
  ~Connection() override;

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  void shutdown();
  size_t send( const char*, size_t ) override;
  size_t recv( char*, size_t ) override;

  //! Does ssl_accept()
  void post_accept() override;
  //! Does ssl_connect()
  void post_connect() override;

protected:
  //! Performs SSL accepting
  virtual void ssl_accept();
  //! Performs SSL connecting
  virtual void ssl_connect();

  bool shutdown_recved();
  bool shutdown_sent();
};

//! Server-side established SSL-connection based on reactive model
//! (with underlying non-blocking socket).
class LIBIQXMLRPC_API Reaction_connection: public ssl::Connection {
  Reactor_base* reactor;

  enum State { EMPTY, ACCEPTING, CONNECTING, READING, WRITING, SHUTDOWN };
  State state = EMPTY;

  char* recv_buf = nullptr;
  const char* send_buf = nullptr;
  size_t buf_len = 0;

public:
  Reaction_connection( const Socket&, Reactor_base* = 0 );

  Reaction_connection(const Reaction_connection&) = delete;
  Reaction_connection& operator=(const Reaction_connection&) = delete;

  //! A trick for supporting generic factory.
  void set_reactor( Reactor_base* r )
  {
    reactor = r;
  }

  void post_accept() override;
  void post_connect() override;
  void handle_input( bool& ) override;
  void handle_output( bool& ) override;

private:
  void switch_state( bool& terminate );
  void try_send();
  size_t try_recv();

protected:
  void ssl_accept() override;
  void ssl_connect() override;
  //! Returns true if shutdown already performed.
  bool reg_shutdown();
  void reg_accept();
  void reg_connect();
  void reg_send( const char*, size_t );
  void reg_recv( char*, size_t );

  //! Overwrite it for server connection.
  virtual void accept_succeed()  {};
  //! Overwrite it for client connection.
  virtual void connect_succeed() {};

  virtual void recv_succeed( bool& terminate, size_t req_len, size_t real_len ) = 0;
  virtual void send_succeed( bool& terminate ) = 0;
};

} // namespace ssl
} // namespace iqnet

#endif

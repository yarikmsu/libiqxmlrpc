//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef LIBIQXMLRPC_HTTPS_SERVER_H
#define LIBIQXMLRPC_HTTPS_SERVER_H

#include "inet_addr.h"
#include "server.h"
#include "ssl_lib.h"

namespace iqxmlrpc {

//! XML-RPC server that works over secured HTTP connections (HTTPS).
class LIBIQXMLRPC_API Https_server: public Server {
public:
  Https_server(const iqnet::Inet_addr& bind_addr, Executor_factory_base*);
  bool is_tls() const noexcept override;
};

} // namespace iqxmlrpc

#endif

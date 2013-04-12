//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _libiqnet_ssl_lib_h_
#define _libiqnet_ssl_lib_h_

#include "api_export.h"

#include <openssl/ssl.h>

#include <stdexcept>

namespace iqnet {
namespace ssl {

class Ctx;

//! Global SSL context.
/*! One must initialize it with appropriate ssl::Ctx
    object before using iqnet's SSL code.
    \see ssl::Ctx
*/
extern LIBIQXMLRPC_API Ctx* ctx;

//! Throws concrete SSL IO subsystem's exception.
void LIBIQXMLRPC_API throw_io_exception( SSL*, int ret );

//! SSL context class. Initializes SSL library.
/*!
  \code
  using namespace iqnet;
  ssl::ctx = ssl::Ctx::client_server( "/path/to/cert", "/path/to/key" );
  //...
  \endcode
*/
class LIBIQXMLRPC_API Ctx {
  SSL_CTX* ctx;

public:
  static Ctx* client_server( const std::string& cert_path, const std::string& key_path );
  static Ctx* server_only( const std::string& cert_path, const std::string& key_path );
  static Ctx* client_only();

  ~Ctx();

  SSL_CTX* context() { return ctx; }

private:
  Ctx( const std::string&, const std::string&, bool init_client );
  Ctx();
};

#ifdef _MSC_VER
#pragma warning(disable: 4251)
#endif

//! Exception class to wrap errors generated by openssl library.
class LIBIQXMLRPC_API exception: public std::exception {
  unsigned long ssl_err;
  std::string msg;

public:
  exception() throw();
  explicit exception( unsigned long ssl_err ) throw();
  exception( const std::string& msg ) throw();
  virtual ~exception() throw() {}

  const char*   what() const throw() { return msg.c_str(); }
  unsigned long code() const throw() { return ssl_err; }
};

class LIBIQXMLRPC_API not_initialized: public ssl::exception {
public:
  not_initialized():
    exception( "Libiqnet::ssl not initialized." ) {}
};

class LIBIQXMLRPC_API connection_close: public ssl::exception {
  bool clean;
public:
  connection_close( bool clean_ ):
    exception( "Connection has been closed." ),
    clean(clean_) {}

  bool is_clean() const { return clean; }
};

class LIBIQXMLRPC_API io_error: public ssl::exception {
public:
  io_error( int err ):
    exception( err ) {}
};

class LIBIQXMLRPC_API need_write: public ssl::io_error {
public:
  need_write():
    io_error( SSL_ERROR_WANT_WRITE ) {}
};

class LIBIQXMLRPC_API need_read: public ssl::io_error {
public:
  need_read():
    io_error( SSL_ERROR_WANT_READ ) {}
};

} // namespace ssl
} // namespace iqnet

#endif

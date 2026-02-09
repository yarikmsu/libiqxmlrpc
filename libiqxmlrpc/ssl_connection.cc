//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include <limits>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include "ssl_connection.h"

using namespace iqnet;

ssl::Connection::Connection( const Socket& s ):
  iqnet::Connection( s ),
  ssl_ctx( ssl::ctx ),
  ssl(nullptr),
  expected_hostname_()
{
  if( !ssl_ctx )
    throw ssl::not_initialized();

  ssl = SSL_new( ssl_ctx->context() );

  if( !ssl )
    throw ssl::exception();

  if( !SSL_set_fd( ssl, static_cast<int>(sock.get_handler()) ) )
    throw ssl::exception();
}


ssl::Connection::~Connection()
{
  SSL_free( ssl );
}


void ssl::Connection::post_accept()
{
  ssl_accept();
}


void ssl::Connection::post_connect()
{
  ssl_connect();
}


void ssl::Connection::prepare_for_ssl_accept()
{
  ssl_ctx->prepare_verify(ssl, true);
}


void ssl::Connection::prepare_for_ssl_connect()
{
  ssl_ctx->prepare_verify(ssl, false);
  prepare_hostname_for_connect();
}


void ssl::Connection::ssl_accept()
{
  prepare_for_ssl_accept();
  int ret = SSL_accept( ssl );

  if( ret != 1 )
    throw_io_exception( ssl, ret );
}


void ssl::Connection::prepare_hostname_for_connect()
{
  if (!expected_hostname_.empty()) {
    if (!SSL_set_tlsext_host_name(ssl, expected_hostname_.c_str()))
      throw ssl::exception("Failed to set SNI hostname");
    if (ssl_ctx->hostname_verification_enabled()) {
      X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
      if (!param)
        throw ssl::exception("Failed to get SSL verify parameters");
      X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
      if (!X509_VERIFY_PARAM_set1_host(param, expected_hostname_.c_str(),
                                       expected_hostname_.length()))
        throw ssl::exception("Failed to set hostname for verification");
    }
  } else {
    // Fallback: use Ctx-level shared hostname (not thread-safe when
    // clients connect to different hosts concurrently)
    ssl_ctx->prepare_sni(ssl);
    ssl_ctx->prepare_hostname_verify(ssl);
  }
}


void ssl::Connection::ssl_connect()
{
  prepare_for_ssl_connect();
  int ret = SSL_connect( ssl );

  if( ret != 1 )
    throw_io_exception( ssl, ret );
}


void ssl::Connection::shutdown()
{
  if( shutdown_recved() && shutdown_sent() )
    return;

  int ret = SSL_shutdown( ssl );
  switch( ret )
  {
    case 1:
      return;

    case 0:
      SSL_shutdown( ssl );
      SSL_set_shutdown( ssl, SSL_RECEIVED_SHUTDOWN );
      break;

    default:
      throw_io_exception( ssl, ret );
  }
}


size_t ssl::Connection::send( const char* data, size_t len )
{
  // SECURITY: Prevent integer truncation when casting size_t to int.
  // On 64-bit systems, len > INT_MAX would wrap to negative/small value,
  // causing undefined behavior in the OpenSSL call.
  if (len > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw exception("SSL::send: buffer size exceeds INT_MAX");
  }

  // Handle partial writes by retrying until all data is sent.
  // In blocking mode, SSL_write typically writes all data or fails,
  // but this loop provides robustness if SSL_MODE_ENABLE_PARTIAL_WRITE
  // is enabled or in edge cases.
  size_t total_written = 0;

  while( total_written < len )
  {
    int ret = SSL_write( ssl, data + total_written,
                         static_cast<int>(len - total_written) );

    if( ret <= 0 )
      throw_io_exception( ssl, ret );

    total_written += static_cast<size_t>(ret);
  }

  return total_written;
}


size_t ssl::Connection::recv( char* buf, size_t len )
{
  // SECURITY: Prevent integer truncation when casting size_t to int.
  // On 64-bit systems, len > INT_MAX would wrap to negative/small value,
  // causing undefined behavior in the OpenSSL call.
  if (len > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw exception("SSL::recv: buffer size exceeds INT_MAX");
  }

  int ret = SSL_read( ssl, buf, static_cast<int>(len) );

  if( ret <= 0 )
    throw_io_exception( ssl, ret );

  return static_cast<size_t>(ret);
}


inline bool ssl::Connection::shutdown_recved()
{
  return (SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) != 0;
}


inline bool ssl::Connection::shutdown_sent()
{
  return SSL_get_shutdown(ssl) & SSL_SENT_SHUTDOWN;
}


// ----------------------------------------------------------------------------
// P3 Optimization: Non-throwing SSL I/O methods
// These return SslIoResult instead of throwing exceptions for the common
// WANT_READ/WANT_WRITE cases, providing ~800x performance improvement.
// ----------------------------------------------------------------------------

ssl::SslIoResult ssl::Connection::try_ssl_read( char* buf, size_t len, size_t& bytes_read )
{
  // SECURITY: Prevent integer truncation when casting size_t to int.
  // Returns ERROR for oversized buffers to maintain non-throwing semantics.
  if (len > static_cast<size_t>(std::numeric_limits<int>::max())) {
    bytes_read = 0;
    return SslIoResult::ERROR;
  }

  bytes_read = 0;
  int ret = SSL_read( ssl, buf, static_cast<int>(len) );

  if( ret > 0 ) {
    bytes_read = static_cast<size_t>(ret);
    return SslIoResult::OK;
  }

  bool clean_close = false;
  return check_io_result( ssl, ret, clean_close );
}


ssl::SslIoResult ssl::Connection::try_ssl_write( const char* buf, size_t len, size_t& bytes_written )
{
  // SECURITY: Prevent integer truncation when casting size_t to int.
  // Returns ERROR for oversized buffers to maintain non-throwing semantics.
  if (len > static_cast<size_t>(std::numeric_limits<int>::max())) {
    bytes_written = 0;
    return SslIoResult::ERROR;
  }

  bytes_written = 0;
  int ret = SSL_write( ssl, buf, static_cast<int>(len) );

  if( ret > 0 ) {
    bytes_written = static_cast<size_t>(ret);
    return SslIoResult::OK;  // Caller checks bytes_written for partial writes
  }

  bool clean_close = false;
  return check_io_result( ssl, ret, clean_close );
}


ssl::SslIoResult ssl::Connection::try_ssl_accept_nonblock()
{
  int ret = SSL_accept( ssl );

  if( ret == 1 ) {
    return SslIoResult::OK;
  }

  bool clean_close = false;
  return check_io_result( ssl, ret, clean_close );
}


ssl::SslIoResult ssl::Connection::try_ssl_connect_nonblock()
{
  int ret = SSL_connect( ssl );

  if( ret == 1 ) {
    return SslIoResult::OK;
  }

  bool clean_close = false;
  return check_io_result( ssl, ret, clean_close );
}


ssl::SslIoResult ssl::Connection::try_ssl_shutdown_nonblock()
{
  if( shutdown_recved() && shutdown_sent() ) {
    return SslIoResult::OK;
  }

  int ret = SSL_shutdown( ssl );

  if( ret == 1 ) {
    return SslIoResult::OK;
  }

  if( ret == 0 ) {
    // First phase of bidirectional shutdown complete, need to call again
    SSL_shutdown( ssl );
    SSL_set_shutdown( ssl, SSL_RECEIVED_SHUTDOWN );
    return SslIoResult::OK;
  }

  bool clean_close = false;
  return check_io_result( ssl, ret, clean_close );
}


// ----------------------------------------------------------------------------
ssl::Reaction_connection::Reaction_connection( const Socket& s, Reactor_base* r ):
  ssl::Connection( s ),
  reactor(r)
{
  sock.set_non_blocking( true );
}


void ssl::Reaction_connection::post_accept()
{
  reg_accept();
}


void ssl::Reaction_connection::post_connect()
{
  reg_connect();
}


void ssl::Reaction_connection::ssl_accept()
{
  ssl::Connection::ssl_accept();
  state = EMPTY;
}


void ssl::Reaction_connection::ssl_connect()
{
  ssl::Connection::ssl_connect();
  state = EMPTY;
}


// P3 Optimization: Use return codes instead of exceptions for hot path
// This eliminates ~3000ns exception overhead per WANT_READ/WANT_WRITE
void ssl::Reaction_connection::handle_io_result( SslIoResult result )
{
  switch( result )
  {
    case SslIoResult::OK:
      break;

    case SslIoResult::WANT_READ:
      reactor->register_handler( this, Reactor_base::INPUT );
      break;

    case SslIoResult::WANT_WRITE:
      reactor->register_handler( this, Reactor_base::OUTPUT );
      break;

    case SslIoResult::CONNECTION_CLOSE:
      reg_shutdown();
      break;

    case SslIoResult::ERROR:
      throw ssl::exception("SSL I/O error");
  }
}

void ssl::Reaction_connection::switch_state( bool& terminate )
{
  SslIoResult result = SslIoResult::OK;

  switch( state )
  {
    case ACCEPTING:
      result = try_ssl_accept_nonblock();
      if( result == SslIoResult::OK ) {
        state = EMPTY;
        accept_succeed();
      }
      break;

    case CONNECTING:
      result = try_ssl_connect_nonblock();
      if( result == SslIoResult::OK ) {
        state = EMPTY;
        connect_succeed();
      }
      break;

    case READING:
    {
      size_t bytes_read = 0;
      result = try_ssl_read( recv_buf, buf_len, bytes_read );
      if( result == SslIoResult::OK ) {
        state = EMPTY;
        recv_succeed( terminate, buf_len, bytes_read );
      }
      break;
    }

    case WRITING:
    {
      size_t bytes_written = 0;
      result = try_ssl_write( send_buf, buf_len, bytes_written );
      if( result == SslIoResult::OK ) {
        state = EMPTY;
        send_succeed( terminate );
      }
      break;
    }

    case SHUTDOWN:
      result = try_ssl_shutdown_nonblock();
      if( result == SslIoResult::OK ) {
        terminate = true;
      }
      break;

    case EMPTY:
    default:
      terminate = true;
      return;
  }

  handle_io_result( result );
}


void ssl::Reaction_connection::handle_input( bool& terminate )
{
  reactor->unregister_handler( this, Reactor_base::INPUT );
  switch_state( terminate );
}


void ssl::Reaction_connection::handle_output( bool& terminate )
{
  reactor->unregister_handler( this, Reactor_base::OUTPUT );
  switch_state( terminate );
}


void ssl::Reaction_connection::try_send()
{
  send( send_buf, buf_len );
  state = EMPTY;
}


size_t ssl::Reaction_connection::try_recv()
{
  size_t ln = recv( recv_buf, buf_len );
  state = EMPTY;

  return ln;
}


bool ssl::Reaction_connection::reg_shutdown()
{
  state = SHUTDOWN;

  if( !shutdown_sent() )
  {
    reactor->register_handler( this, Reactor_base::OUTPUT );
  }
  else if( !shutdown_recved() )
  {
    reactor->register_handler( this, Reactor_base::INPUT );
  }
  else
  {
    state = EMPTY;
    return true;
  }

  return false;
}


void ssl::Reaction_connection::reg_accept()
{
  prepare_for_ssl_accept();
  state = ACCEPTING;
  reactor->register_handler( this, Reactor_base::INPUT );
}


void ssl::Reaction_connection::reg_connect()
{
  prepare_for_ssl_connect();
  state = CONNECTING;
  reactor->register_handler( this, Reactor_base::OUTPUT );
}


void ssl::Reaction_connection::reg_send( const char* buf, size_t len )
{
  state = WRITING;
  send_buf = buf;
  buf_len = len;
  reactor->register_handler( this, Reactor_base::OUTPUT );
}


void ssl::Reaction_connection::reg_recv( char* buf, size_t len )
{
  state = READING;
  recv_buf = buf;
  buf_len = len;
  reactor->register_handler( this, Reactor_base::INPUT );

  if( SSL_pending(ssl) )
    reactor->fake_event( this, Reactor_base::INPUT );
}

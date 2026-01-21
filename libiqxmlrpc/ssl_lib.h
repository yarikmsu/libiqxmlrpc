//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef LIBIQNET_SSL_LIB_H
#define LIBIQNET_SSL_LIB_H

#include "api_export.h"

#include <cstdint>
#include <memory>
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

//! Result codes for SSL I/O operations (P3 optimization).
/*! Using return codes instead of exceptions for expected non-blocking
    states (WANT_READ/WANT_WRITE) provides ~800x performance improvement.
    These states occur frequently in non-blocking SSL I/O and using
    exceptions for normal control flow is expensive.
*/
enum class SslIoResult : std::uint8_t {
  OK,               //!< Operation completed successfully
  WANT_READ,        //!< Need to wait for socket readable, then retry
  WANT_WRITE,       //!< Need to wait for socket writable, then retry
  CONNECTION_CLOSE, //!< Connection was closed (clean or unclean)
  ERROR             //!< Actual error occurred - caller should throw
};

//! Check SSL operation result without throwing for expected states.
/*! Returns SslIoResult for the common non-blocking cases.
    For SslIoResult::ERROR, caller should call throw_io_exception()
    to get the appropriate exception.
    \param ssl The SSL connection
    \param ret The return value from SSL_read/SSL_write/etc.
    \param clean_close Output parameter set to true if connection was closed cleanly
    \return The result code
*/
SslIoResult LIBIQXMLRPC_API check_io_result( SSL* ssl, int ret, bool& clean_close );

class LIBIQXMLRPC_API ConnectionVerifier {
public:
  virtual ~ConnectionVerifier();

  int verify(bool preverified_ok, X509_STORE_CTX*) const;

protected:
  static std::string cert_finger_sha256(X509_STORE_CTX*);

private:
  virtual int do_verify(bool preverified_ok, X509_STORE_CTX*) const = 0;
};

//! SSL context class. Initializes SSL library.
/*!
  \code
  using namespace iqnet;
  ssl::ctx = ssl::Ctx::client_server( "/path/to/cert", "/path/to/key" );
  //...
  \endcode
*/
class LIBIQXMLRPC_API Ctx {
public:
  static Ctx* client_server( const std::string& cert_path, const std::string& key_path );
  static Ctx* server_only( const std::string& cert_path, const std::string& key_path );
  static Ctx* client_only();

  ~Ctx();

  SSL_CTX* context();

  void verify_server(ConnectionVerifier*);
  void verify_client(bool require_certificate, ConnectionVerifier*);
  void prepare_verify(SSL*, bool server);

  //! Configure SSL session caching.
  /*! Session caching speeds up reconnects from returning clients.
      \param enable Whether to enable session caching (default: true for servers)
      \param cache_size Maximum number of sessions to cache (default: 1024)
      \param timeout_sec Session lifetime in seconds (default: 300)
  */
  void set_session_cache(bool enable, int cache_size = 1024, int timeout_sec = 300);

  //! Load trusted CA certificates from file and/or directory.
  /*! SECURITY: Required for proper certificate chain verification.
      \param ca_file Path to CA certificate file (PEM format), or empty string
      \param ca_dir Path to directory containing CA certificates, or empty string
      \return true on success, false on failure
  */
  bool load_verify_locations(const std::string& ca_file, const std::string& ca_dir = "");

  //! Use system default CA certificate store.
  /*! SECURITY: Loads the default trusted CA certificates from the operating system.
      \return true on success, false on failure
  */
  bool use_default_verify_paths();

  //! Enable hostname verification for client connections.
  /*! SECURITY: Verifies that server certificate matches the expected hostname.
      Checks both Common Name (CN) and Subject Alternative Names (SAN).
      Requires OpenSSL 1.1.0+.
      \param enable Whether to enable hostname verification (default: true)
  */
  void set_hostname_verification(bool enable);

  //! Set the expected hostname for verification.
  /*! Call this before connecting to set the hostname to verify against.
      Must be called per-connection if hostnames differ.
      \param hostname The expected server hostname
  */
  void set_expected_hostname(const std::string& hostname);

  //! Prepare hostname verification on SSL connection (internal use).
  void prepare_hostname_verify(SSL* ssl);

  //! Set SNI (Server Name Indication) on SSL connection (internal use).
  /*! SECURITY: SNI tells the server which hostname the client is connecting to,
      enabling proper virtual hosting and certificate selection.
  */
  void prepare_sni(SSL* ssl);

private:
  Ctx( const std::string&, const std::string&, bool init_client );
  Ctx();

  struct Impl;
  std::shared_ptr<Impl> impl_;
};

#ifdef _MSC_VER
#pragma warning(disable: 4251)
#endif

//! Exception class to wrap errors generated by openssl library.
class LIBIQXMLRPC_API exception: public std::exception {
  unsigned long ssl_err;
  std::string msg;

public:
  exception() noexcept;
  explicit exception( unsigned long ssl_err ) noexcept;
  explicit exception( const std::string& msg ) noexcept;
  ~exception() noexcept override = default;

  const char*   what() const noexcept override { return msg.c_str(); }
  unsigned long code() const noexcept { return ssl_err; }
};

class LIBIQXMLRPC_API not_initialized: public ssl::exception {
public:
  not_initialized():
    exception( "Libiqnet::ssl not initialized." ) {}
};

class LIBIQXMLRPC_API connection_close: public ssl::exception {
  bool clean;
public:
  explicit connection_close( bool clean_ ):
    exception( "Connection has been closed." ),
    clean(clean_) {}

  bool is_clean() const { return clean; }
};

class LIBIQXMLRPC_API io_error: public ssl::exception {
public:
  explicit io_error( int err ):
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

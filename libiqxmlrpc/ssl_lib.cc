//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#ifdef WIN32
#include <winsock2.h>
#endif

#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <atomic>
#include <cerrno>
#include <mutex>
#include <cstdio>

#include "ssl_lib.h"
#include "net_except.h"

namespace iqnet {
namespace ssl {

Ctx* ctx = nullptr;
std::once_flag ssl_init;
int iqxmlrpc_ssl_data_idx = 0;

void
init_library()
{
  // OpenSSL 1.1.0+ initializes automatically
  OPENSSL_init_ssl(0, nullptr);

  iqxmlrpc_ssl_data_idx = SSL_get_ex_new_index(0, const_cast<void*>(static_cast<const void*>("iqxmlrpc verifier")), nullptr, nullptr, nullptr);
}

//
// Ctx
//

Ctx* Ctx::client_server( const std::string& cert_path, const std::string& key_path )
{
  return new Ctx( cert_path, key_path, true );
}


Ctx* Ctx::server_only( const std::string& cert_path, const std::string& key_path )
{
  return new Ctx( cert_path, key_path, false );
}


Ctx* Ctx::client_only()
{
  return new Ctx;
}

namespace {

void
set_common_options(SSL_CTX* ctx)
{
  // Enforce TLS 1.2 as minimum version (TLS 1.2 released 2008, widely supported)
  // TLS 1.0 and 1.1 are deprecated and have known security weaknesses

  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
}

// Server-only cipher configuration for optimal performance
// Not applied to client contexts to avoid restricting outbound connections
void
set_server_cipher_options(SSL_CTX* ctx)
{
  // Prefer hardware-accelerated AES-GCM ciphers (uses AES-NI on modern CPUs)
  // Can provide significant speedup on CPUs with hardware AES support
  // TLS 1.2 cipher list
  int ret = SSL_CTX_set_cipher_list(ctx,
      "ECDHE-ECDSA-AES128-GCM-SHA256:"
      "ECDHE-RSA-AES128-GCM-SHA256:"
      "ECDHE-ECDSA-AES256-GCM-SHA384:"
      "ECDHE-RSA-AES256-GCM-SHA384:"
      "ECDHE-ECDSA-CHACHA20-POLY1305:"
      "ECDHE-RSA-CHACHA20-POLY1305");

  if (ret == 0) {
    // Cipher list rejected (FIPS mode, old OpenSSL, etc.)
    // Fall back to OpenSSL defaults rather than failing
    ERR_clear_error();
  }

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  // TLS 1.3 ciphersuites (OpenSSL 1.1.1+)
  // Default order already prefers AES-GCM, but be explicit
  SSL_CTX_set_ciphersuites(ctx,
      "TLS_AES_128_GCM_SHA256:"
      "TLS_AES_256_GCM_SHA384:"
      "TLS_CHACHA20_POLY1305_SHA256");
#endif

  // Server prefers its own cipher order (for consistent performance)
  SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
}

int
iqxmlrpc_SSL_verify(int prev_ok, X509_STORE_CTX* ctx)
{
  auto ssl = reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
  auto v = reinterpret_cast<const ConnectionVerifier*>(SSL_get_ex_data(ssl, iqxmlrpc_ssl_data_idx));
  return v->verify(prev_ok, ctx);
}

} // anonymous namespace

//
// ConnectionVerifier
//

ConnectionVerifier::~ConnectionVerifier() = default;

int
ConnectionVerifier::verify(bool prev_ok, X509_STORE_CTX* ctx) const
{
  try {
    return do_verify(prev_ok, ctx);
  } catch (...) {
    // TODO: log ability?
    return 0;
  }
}

std::string
ConnectionVerifier::cert_finger_sha256(X509_STORE_CTX* ctx)
{
  X509* x = X509_STORE_CTX_get_current_cert(ctx);
  if (!x) {
    return "";  // No certificate available at this verification stage
  }
  const EVP_MD* digest = EVP_get_digestbyname("sha256");
  unsigned int n = 0;
  unsigned char md[EVP_MAX_MD_SIZE];
  X509_digest(x, digest, md, &n);

  // Defensive check: SHA256 must produce exactly 32 bytes
  if (n != 32) {
    throw iqnet::network_error("SHA256 digest produced unexpected size");
  }

  // Use zero-padded hex format (%02x) for proper SHA256 fingerprints.
  // This differs from the original ostringstream implementation which used std::hex
  // without zero-padding, producing variable-length output (missing leading zeros).
  // The zero-padded format is the standard representation for cryptographic hashes
  // and ensures consistent 64-character output for SHA256.
  char hex_buf[65];  // 32 bytes * 2 chars/byte + null terminator
  for(size_t i = 0; i < 32; i++) {
    int written = snprintf(&hex_buf[i * 2], 3, "%02x", md[i]);
    if (written < 0 || written >= 3) {
      throw iqnet::network_error("Failed to format certificate fingerprint");
    }
  }
  hex_buf[64] = '\0';
  return std::string(hex_buf, 64);
}

//
// Ctx
//

struct Ctx::Impl {
  SSL_CTX* ctx;
  ConnectionVerifier* server_verifier;
  ConnectionVerifier* client_verifier;
  bool require_client_cert;
  std::atomic<bool> verify_peer;  // SECURITY: When true, SSL_VERIFY_PEER is set on client connections even without a custom verifier
  std::atomic<bool> hostname_verification;  // SECURITY: Verify hostname against certificate
  std::string expected_hostname;

  Impl():
    ctx(nullptr),
    server_verifier(nullptr),
    client_verifier(nullptr),
    require_client_cert(false),
    verify_peer(false),
    hostname_verification(true),  // SECURITY: Default to verifying hostname
    expected_hostname()
  {
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  ~Impl()
  {
    if (ctx) {
      SSL_CTX_free(ctx);
    }
  }
};

Ctx::Ctx( const std::string& cert_path, const std::string& key_path, bool client ):
  impl_(new Impl)
{
  std::call_once(ssl_init, init_library);
  impl_->ctx = SSL_CTX_new( client ? SSLv23_method() : SSLv23_server_method() );
  set_common_options(impl_->ctx);

  // Server cipher optimization (applies to both client_server and server_only contexts
  // since both can act as servers; does not apply to client_only contexts)
  set_server_cipher_options(impl_->ctx);

  // Enable TLS session caching for faster reconnects from returning clients
  // Server-side session cache can reduce handshake time for returning clients
  SSL_CTX_set_session_cache_mode(impl_->ctx, SSL_SESS_CACHE_SERVER);
  SSL_CTX_sess_set_cache_size(impl_->ctx, 1024);  // Cache up to 1024 sessions
  SSL_CTX_set_timeout(impl_->ctx, 300);           // 5 minute session lifetime

  if(
    !SSL_CTX_use_certificate_file( impl_->ctx, cert_path.c_str(), SSL_FILETYPE_PEM ) ||
    !SSL_CTX_use_PrivateKey_file( impl_->ctx, key_path.c_str(), SSL_FILETYPE_PEM ) ||
    !SSL_CTX_check_private_key( impl_->ctx )
  )
    throw exception();
}


Ctx::Ctx():
  impl_(new Impl)
{
  std::call_once(ssl_init, init_library);
  impl_->ctx = SSL_CTX_new( SSLv23_client_method() );
  set_common_options( impl_->ctx );
}


Ctx* Ctx::client_verified()
{
  std::unique_ptr<Ctx> c(new Ctx);
  if (!c->use_default_verify_paths())
    throw ssl::exception("Failed to load system CA certificates");
  c->set_verify_peer(true);
  return c.release();
}


Ctx::~Ctx() = default;

SSL_CTX*
Ctx::context()
{
  return impl_->ctx;
}

void
Ctx::verify_server(ConnectionVerifier* v)
{
  impl_->server_verifier = v;
}

void
Ctx::verify_client(bool require_certificate, ConnectionVerifier* v)
{
  impl_->require_client_cert = require_certificate;
  impl_->client_verifier = v;
}

void
Ctx::prepare_verify(SSL* ssl, bool server)
{
  ConnectionVerifier* v = server ? impl_->client_verifier : impl_->server_verifier;

  // verify_peer only applies client-side; server-side uses verify_client() instead
  bool need_verify = v || (!server && impl_->verify_peer);
  int mode = need_verify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE;

  if (server && impl_->require_client_cert)
    mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;

  if (v) {
    SSL_set_verify(ssl, mode, iqxmlrpc_SSL_verify);
    SSL_set_ex_data(ssl, iqxmlrpc_ssl_data_idx, const_cast<void*>(static_cast<const void*>(v)));
  } else {
    SSL_set_verify(ssl, mode, nullptr);
  }
}

void
Ctx::set_session_cache(bool enable, int cache_size, int timeout_sec)
{
  if (enable) {
    SSL_CTX_set_session_cache_mode(impl_->ctx, SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(impl_->ctx, cache_size);
    SSL_CTX_set_timeout(impl_->ctx, timeout_sec);
  } else {
    SSL_CTX_set_session_cache_mode(impl_->ctx, SSL_SESS_CACHE_OFF);
  }
}

bool
Ctx::load_verify_locations(const std::string& ca_file, const std::string& ca_dir)
{
  const char* file = ca_file.empty() ? nullptr : ca_file.c_str();
  const char* dir = ca_dir.empty() ? nullptr : ca_dir.c_str();

  if (!file && !dir) {
    return false;  // At least one must be provided
  }

  return SSL_CTX_load_verify_locations(impl_->ctx, file, dir) == 1;
}

bool
Ctx::use_default_verify_paths()
{
  return SSL_CTX_set_default_verify_paths(impl_->ctx) == 1;
}

void
Ctx::set_verify_peer(bool enable)
{
  impl_->verify_peer = enable;
}

bool
Ctx::verify_peer_enabled() const
{
  return impl_->verify_peer;
}

void
Ctx::set_hostname_verification(bool enable)
{
  impl_->hostname_verification = enable;
}

void
Ctx::set_expected_hostname(const std::string& hostname)
{
  impl_->expected_hostname = hostname;
}

bool
Ctx::hostname_verification_enabled() const
{
  return impl_->hostname_verification;
}

void
Ctx::prepare_hostname_verify(SSL* ssl)
{
  if (!impl_->hostname_verification || impl_->expected_hostname.empty()) {
    return;
  }

  X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
  if (!param)
    throw ssl::exception("Failed to get SSL verify parameters");

  X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
  if (!X509_VERIFY_PARAM_set1_host(param, impl_->expected_hostname.c_str(),
                                   impl_->expected_hostname.length()))
    throw ssl::exception("Failed to set hostname for verification");
}

void
Ctx::prepare_sni(SSL* ssl)
{
  if (impl_->expected_hostname.empty()) {
    return;
  }

  // Set Server Name Indication (SNI) extension
  // This tells the server which hostname we're connecting to, enabling:
  // - Virtual hosting (multiple sites on one IP)
  // - Correct certificate selection on the server side
  if (!SSL_set_tlsext_host_name(ssl, impl_->expected_hostname.c_str()))
    throw ssl::exception("Failed to set SNI hostname");
}

// ----------------------------------------------------------------------------
exception::exception() noexcept:
  ssl_err( ERR_get_error() ),
  msg()
{
  // SECURITY: ERR_reason_error_string() returns nullptr when no error is queued
  // (ssl_err == 0). Constructing std::string from nullptr is undefined behavior
  // and causes a crash. Handle this case by providing a fallback message.
  const char* reason = ERR_reason_error_string(ssl_err);
  msg = reason ? reason : "unknown SSL error";
  msg.insert(0, "SSL: ");
}


exception::exception( unsigned long err ) noexcept:
  ssl_err(err),
  msg()
{
  const char* reason = ERR_reason_error_string(ssl_err);
  msg = reason ? reason : "unknown error";
  msg.insert(0, "SSL: ");
}


exception::exception( const std::string& msg_ ) noexcept:
  ssl_err(0),
  msg( msg_ )
{
  msg.insert(0, "SSL: ");
}


// ----------------------------------------------------------------------------
// P3 Optimization: Return codes for expected non-blocking states
// Using return codes instead of exceptions for WANT_READ/WANT_WRITE provides
// ~800x performance improvement (measured: ~3ns vs ~3000ns per operation)
// ----------------------------------------------------------------------------
SslIoResult check_io_result( SSL* ssl, int ret, bool& clean_close )
{
  clean_close = false;

  // Save errno/WSAGetLastError() before SSL_get_error() which may clobber it
#ifdef WIN32
  int saved_wsa_err = WSAGetLastError();
#else
  int saved_errno = errno;
#endif

  int code = SSL_get_error( ssl, ret );

  switch( code )
  {
    case SSL_ERROR_NONE:
      return SslIoResult::OK;

    case SSL_ERROR_WANT_READ:
      return SslIoResult::WANT_READ;

    case SSL_ERROR_WANT_WRITE:
      return SslIoResult::WANT_WRITE;

    case SSL_ERROR_ZERO_RETURN:
      clean_close = (SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) != 0;
      return SslIoResult::CONNECTION_CLOSE;

    case SSL_ERROR_SYSCALL:
      if( !ret ) {
        // ret == 0: EOF observed, peer closed connection
        clean_close = false;
        return SslIoResult::CONNECTION_CLOSE;
      }
      // ret < 0: Check error code to distinguish real errors from normal shutdown
#ifdef WIN32
      if( saved_wsa_err == WSAEWOULDBLOCK ) {
        // Socket would block - retry later
        return SslIoResult::WANT_READ;
      }
      if( saved_wsa_err == 0 || saved_wsa_err == WSAECONNRESET || saved_wsa_err == WSAECONNABORTED || saved_wsa_err == WSAENOTCONN ) {
        // Peer closed connection (with or without proper shutdown)
        clean_close = false;
        return SslIoResult::CONNECTION_CLOSE;
      }
#else
      if( saved_errno == EAGAIN || saved_errno == EWOULDBLOCK ) {
        // Socket would block - retry later
        return SslIoResult::WANT_READ;
      }
      if( saved_errno == 0 || saved_errno == EPIPE || saved_errno == ECONNRESET || saved_errno == ENOTCONN ) {
        // Peer closed connection (errno==0 means unexpected EOF without close_notify)
        clean_close = false;
        return SslIoResult::CONNECTION_CLOSE;
      }
#endif
      return SslIoResult::ERROR;

    default:
      return SslIoResult::ERROR;
  }
}

// ----------------------------------------------------------------------------
void throw_io_exception( SSL* ssl, int ret )
{
  int code = SSL_get_error( ssl, ret );
  switch( code )
  {
    case SSL_ERROR_NONE:
      return;

    case SSL_ERROR_ZERO_RETURN:
    {
      bool clean = (SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) != 0;
      throw connection_close( clean );
    }

    case SSL_ERROR_WANT_READ:
      throw need_read();

    case SSL_ERROR_WANT_WRITE:
      throw need_write();

    case SSL_ERROR_SYSCALL:
      if( !ret )
        throw connection_close( false );
      else
        throw iqnet::network_error( "iqnet::ssl::throw_io_exception" );

    case SSL_ERROR_SSL:
      throw exception();

    default:
      throw io_error( code );
  }
}

} // namespace ssl
} // namespace iqnet

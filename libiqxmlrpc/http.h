//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#ifndef LIBIQXMLRPC_HTTP_H
#define LIBIQXMLRPC_HTTP_H

#include "except.h"
#include "inet_addr.h"
#include "xheaders.h"

#include <cstdint>
#include <functional>
#include <map>  // For Validators (std::multimap)
#include <unordered_map>  // For Options
#include <memory>
#include <string>

namespace iqxmlrpc {

class Auth_Plugin_base;

//! XML-RPC HTTP transport-independent infrastructure.
/*! Contains classes which responsible for transport-indepenent
    HTTP collaboration functionality. Such as packet parsing/constructing,
    wrapping XML-RPC message into HTTP-layer one and vice versa.
*/
namespace http {

//! The level of HTTP sanity checks.
enum Verification_level : std::uint8_t { HTTP_CHECK_WEAK, HTTP_CHECK_STRICT };

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

//! HTTP header type tags for efficient type checking (replaces dynamic_cast)
enum class HeaderType : std::uint8_t {
  BASE,
  REQUEST,
  RESPONSE,
  ERROR_RESPONSE
};

//! HTTP header. Responsible for parsing,
//! creating generic HTTP headers.
class LIBIQXMLRPC_API Header {
public:
  //! Get the header type tag (O(1) type check, replaces dynamic_cast)
  virtual HeaderType header_type() const { return HeaderType::BASE; }

  //! Set a custom server header (e.g., "MyServer/1.0").
  /*! Call before creating any Response_header instances.
      \param header Custom server identifier (empty to use default)
  */
  static void set_server_header(const std::string& header);

  //! Hide the server version from HTTP responses.
  /*! When enabled, the "Server:" header will be omitted entirely.
      Call before creating any Response_header instances.
      \param hide Whether to hide the server version
  */
  static void hide_server_version(bool hide);

  //! Enable HSTS (HTTP Strict Transport Security) header.
  /*! SECURITY: Only enable for HTTPS servers. Tells browsers to only
      connect via HTTPS for the specified duration.
      \param enable Whether to add HSTS header
      \param max_age Duration in seconds (default: 31536000 = 1 year)
  */
  static void enable_hsts(bool enable, int max_age = 31536000);

  //! Set a custom Content-Security-Policy header.
  /*! SECURITY: Helps prevent XSS attacks by restricting resource loading.
      \param policy CSP policy string (empty to disable, e.g., "default-src 'self'")
  */
  static void set_content_security_policy(const std::string& policy);

  Header(Verification_level = HTTP_CHECK_WEAK);
  virtual ~Header();

  unsigned  content_length()  const;
  bool      conn_keep_alive() const;
  bool      expect_continue() const;

  void set_content_length( size_t ln );
  void set_conn_keep_alive( bool );
  void set_option(const std::string& name, const std::string& value);

  void get_xheaders(iqxmlrpc::XHeaders& xheaders) const;
  void set_xheaders(const iqxmlrpc::XHeaders& xheaders);

  //! Return text representation of header including final CRLF.
  std::string dump() const;

protected:
  bool option_exists(const std::string&) const;
  void set_option_default(const std::string& name, const std::string& value);
  void set_option_default(const std::string& name, unsigned value);
  void set_option_checked(const std::string& name, const std::string& value);
  void set_option(const std::string& name, size_t value);

  const std::string&  get_head_line() const { return head_line_; }
  std::string         get_string(const std::string& name) const;
  unsigned            get_unsigned(const std::string& name) const;

  //
  // Parser interface
  //

  typedef std::function<void (const std::string&)> Option_validator_fn;

  void register_validator(
    const std::string&,
    Option_validator_fn,
    Verification_level);

  void parse(const std::string&);

private:
  template <class T>
  T get_option(const std::string& name) const;

  virtual std::string dump_head() const = 0;

private:
  struct Option_validator {
    Verification_level level;
    Option_validator_fn fn;
  };

  typedef std::unordered_map<std::string, std::string> Options;
  typedef std::multimap<std::string, Option_validator> Validators;

  std::string head_line_;
  Options options_;
  Validators validators_;
  Verification_level ver_level_;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

//! HTTP request's header.
class LIBIQXMLRPC_API Request_header: public Header {
  std::string uri_;

public:
  HeaderType header_type() const override { return HeaderType::REQUEST; }

  Request_header( Verification_level, const std::string& to_parse );
  Request_header( const std::string& uri, const std::string& vhost, int port );

  const std::string& uri() const { return uri_; }
  std::string host()  const;
  std::string agent() const;

  bool has_authinfo() const;
  void get_authinfo(std::string& user, std::string& password) const;
  void set_authinfo(const std::string& user, const std::string& password);

private:
  std::string dump_head() const override;
};

//! HTTP response's header.
class LIBIQXMLRPC_API Response_header: public Header {
  int code_;
  std::string phrase_;

public:
  HeaderType header_type() const override { return HeaderType::RESPONSE; }

  Response_header( Verification_level, const std::string& to_parse );
  explicit Response_header( int = 200, const std::string& = "OK" );

  int code() const { return code_; }
  const std::string& phrase() const { return phrase_; }
  std::string server() const;

private:
  static std::string current_date();
  std::string dump_head() const override;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

//! HTTP packet: Header + Content.
class LIBIQXMLRPC_API Packet {
protected:
  std::shared_ptr<http::Header> header_;
  std::string content_;

public:
  Packet( http::Header* header, const std::string& content );
  //! Move-enabled constructor for efficient response creation.
  Packet( http::Header* header, std::string&& content );
  virtual ~Packet();

  //! Sets header option "connection: {keep-alive|close}".
  //! By default connection is close.
  void set_keep_alive( bool = true );

  const http::Header* header()  const { return header_.get(); }
  const std::string&  content() const { return content_; }

  std::string dump() const
  {
    return header_->dump() + content_;
  }
};

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(disable: 4251)
#endif

//! Helper that responsible for constructing HTTP packets of specified type
//! (request or response).
class Packet_reader {
  std::string header_cache;
  std::string content_cache;
  Header* header;
  Verification_level ver_level_;
  bool constructed;
  size_t pkt_max_sz;
  size_t header_max_sz;  // SECURITY: Limit header size independently
  size_t total_sz;
  bool continue_sent_;
  bool reading_response_;

public:
  Packet_reader(const Packet_reader&) = delete;
  Packet_reader& operator=(const Packet_reader&) = delete;

  Packet_reader():
    header_cache(),
    content_cache(),
    header(nullptr),
    ver_level_(HTTP_CHECK_WEAK),
    constructed(false),
    pkt_max_sz(0),
    header_max_sz(16384),  // SECURITY: Default 16KB header limit
    total_sz(0),
    continue_sent_(false),
    reading_response_(false)
  {
  }

  ~Packet_reader()
  {
    if( !constructed )
      delete header;
  }

  void set_verification_level(Verification_level lev)
  {
    ver_level_ = lev;
  }

  void set_max_size( size_t m )
  {
    pkt_max_sz = m;
  }

  //! Set maximum response size for client-side enforcement.
  /*! Configures the reader as a response reader, so that size limit
      violations throw Response_too_large instead of Request_too_large.
      This flag is sticky — once set, the reader stays in response mode
      for its lifetime (correct for client connections where one
      Packet_reader is owned per Client_connection).
      \param m Maximum response size in bytes. 0 means unlimited.
  */
  void set_max_response_size( size_t m )
  {
    pkt_max_sz = m;
    reading_response_ = true;
  }

  //! Set maximum header size to prevent header-based DoS attacks.
  /*! Default: 16384 (16KB). Set to 0 to disable limit.
      \param m Maximum header size in bytes
  */
  void set_max_header_size( size_t m )
  {
    header_max_sz = m;
  }

  bool expect_continue() const;
  Packet* read_request( const std::string& );
  Packet* read_response( const std::string&, bool read_header_only );
  void set_continue_sent(); 

private:
  void clear();
  void check_sz( size_t );
  bool read_header( const std::string& );

  template <class Header_type>
  Packet* read_packet( const std::string&, bool = false );
};


//! Exception which is thrown on syntax error during HTTP packet parsing.
class LIBIQXMLRPC_API Malformed_packet: public Exception {
public:
  Malformed_packet():
    Exception( "Malformed HTTP packet received.") {}

  explicit Malformed_packet(const std::string& problem_domain):
    Exception( "Malformed HTTP packet received (" + problem_domain + ")." ) {}
};

//! Exception related to HTTP protocol.
//! Can be sent as error response to client.
class LIBIQXMLRPC_API Error_response: public Packet, public Exception {
public:
  Error_response( const std::string& phrase, int code ):
    Packet( new Response_header(code, phrase), "" ),
    Exception( "HTTP: " + phrase ) {}

  ~Error_response() noexcept override = default;

  const Response_header* response_header() const
  {
    // Use type tag instead of dynamic_cast for performance (M3 optimization)
    // This is error path so impact is low, but good for consistency
    const Header* hdr = header();
    if (hdr && hdr->header_type() == HeaderType::RESPONSE)
      return static_cast<const Response_header*>(hdr);
    return nullptr;
  }

  // deprecated
  std::string dump_error_response() const { return dump(); }
};

} // namespace http
} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

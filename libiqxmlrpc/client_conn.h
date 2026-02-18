//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_CLIENT_CONN_H
#define IQXMLRPC_CLIENT_CONN_H

#include <memory>
#include <string>
#include <vector>
#include "connection.h"
#include "http.h"
#include "request.h"
#include "response.h"

namespace iqxmlrpc {

class Client_options;

//! Transport independent base class for XML-RPC client's connection.
class LIBIQXMLRPC_API Client_connection {
public:
  Client_connection();
  virtual ~Client_connection();

  Client_connection(const Client_connection&) = delete;
  Client_connection& operator=(const Client_connection&) = delete;

  void set_options(const Client_options& o) { options = &o; }

  //! Set expected hostname for SSL hostname verification (no-op for non-SSL).
  virtual void set_ssl_expected_hostname(const std::string&) {}

  Response process_session(const Request&, const XHeaders& xheaders = XHeaders());

protected:
  //! \returns Fully-assembled Packet once enough data has been received,
  //!          or nullptr if the HTTP packet is still incomplete (more data expected).
  //!          When \p read_hdr_only is true, nullptr is still returned until the full
  //!          HTTP header section has been accumulated; the returned Packet then contains
  //!          only the parsed header with an empty content string.
  //! \throws  iqxmlrpc::http::Response_too_large if accumulated data exceeds the configured limit.
  //! \throws  iqxmlrpc::http::Malformed_packet   if the received data is malformed (bad response
  //!           status line, missing colon in a header line, non-numeric Content-Length,
  //!           empty first data chunk, etc.).
  //! \throws  iqxmlrpc::http::Http_header_error  if a response header name or value contains
  //!           CRLF characters (header injection detection), or if the server response includes
  //!           a Transfer-Encoding header (not supported by this implementation).
  //! \throws  iqxmlrpc::http::Length_required    if the response header contains no Content-Length.
  //!           Note: this is a server-side HTTP 411 exception type reused on the client path;
  //!           the embedded response packet carries no meaning here.
  std::unique_ptr<http::Packet> read_response( const std::string&, bool read_hdr_only = false );
  virtual std::unique_ptr<http::Packet> do_process_session( const std::string& ) = 0;

  const Client_options& opts() const { return *options; }

  char* read_buf() { return &read_buf_[0]; }
  size_t read_buf_sz() const { return read_buf_.size(); }

private:
  virtual std::string decorate_uri() const;

  http::Packet_reader preader;
  const Client_options* options = nullptr;
  std::vector<char> read_buf_;
};

//! Exception which be thrown by client when timeout occured.
class LIBIQXMLRPC_API Client_timeout: public iqxmlrpc::Exception {
public:
  Client_timeout():
    Exception( "Connection timeout." ) {}
};

} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

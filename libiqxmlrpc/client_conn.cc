//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include <memory>

#include "client_conn.h"
#include "client_opts.h"
#include "http.h"

namespace iqxmlrpc {

Client_connection::Client_connection():
  preader(),
  read_buf_(65536, '\0')
{
}

Client_connection::~Client_connection() = default;

Response Client_connection::process_session( const Request& req, const XHeaders& xheaders )
{
  using namespace http;

  std::string req_xml_str( dump_request(req) );

  std::unique_ptr<Request_header> req_h(
    new Request_header(
      decorate_uri(),
      opts().vhost(),
      opts().addr().get_port() ));

  if (opts().has_authinfo())
    req_h->set_authinfo( opts().auth_user(), opts().auth_passwd() );

  req_h->set_xheaders( opts().xheaders() );
  req_h->set_xheaders( xheaders );

  Packet req_p( req_h.release(), req_xml_str );
  req_p.set_keep_alive( opts().keep_alive() );

  auto res_p = do_process_session(req_p.dump());

  const auto* res_h =
    static_cast<const Response_header*>(res_p->header());

  if( res_h->code() != 200 )
    throw Error_response( res_h->phrase(), res_h->code() );

  return parse_response( res_p->content() );
}

std::unique_ptr<http::Packet> Client_connection::read_response( const std::string& s, bool hdr_only )
{
  // Re-apply the limit before each chunk so that any change to
  // max_response_sz() takes effect immediately.  Packet_reader
  // accumulates total_sz across reads, so cumulative size enforcement
  // is preserved even when the limit value is re-set.  This mirrors
  // the server pattern in server_conn.cc where set_max_size() precedes
  // each read_request().
  preader.set_max_response_size( opts().max_response_sz() );
  // nullptr is a valid return meaning the packet is incomplete (more data expected).
  // The caller must supply subsequent data chunks via further calls until a non-null
  // packet is returned; this may happen within a read loop or across multiple
  // event-handler callbacks depending on the I/O model.
  return preader.read_response( s, hdr_only );
}

std::string Client_connection::decorate_uri() const
{
  return opts().uri();
}

} // namespace iqxmlrpc

// vim:ts=2:sw=2:et

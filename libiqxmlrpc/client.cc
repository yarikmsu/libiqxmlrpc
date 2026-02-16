//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include "client.h"
#include "client_conn.h"
#include "client_opts.h"

#include <exception>

namespace iqxmlrpc {

//
// Client_base::Impl
//

class Client_base::Impl {
public:
  Impl(
    const iqnet::Inet_addr& addr,
    const std::string& uri,
    const std::string& vhost
  ):
    opts(addr, uri, vhost),
    conn_cache(),
    expected_hostname() {}

  Client_options opts;
  std::unique_ptr<Client_connection> conn_cache;
  std::string expected_hostname;
};

//
// Auto_conn
//

class Auto_conn {
public:
  Auto_conn(const Auto_conn&) = delete;
  Auto_conn& operator=(const Auto_conn&) = delete;

  Auto_conn( Client_base::Impl& client_impl, Client_base& client ):
    client_impl_(client_impl),
    tmp_conn_(),
    conn_ptr_(nullptr),
    uncaught_on_entry_(std::uncaught_exceptions())
  {
    if (opts().keep_alive())
    {
      if (!cimpl().conn_cache)
        cimpl().conn_cache.reset( create_connection(client) );

      conn_ptr_ = cimpl().conn_cache.get();

    } else {
      tmp_conn_.reset( create_connection(client) );
      conn_ptr_ = tmp_conn_.get();
    }
  }

  ~Auto_conn()
  {
    // Drop the cached connection if keep-alive is off, OR if we are
    // unwinding due to an exception (e.g. Response_too_large).  A mid-
    // response abort leaves Packet_reader and the socket in an
    // indeterminate state; reusing the connection would corrupt the
    // next request.
    if (!cimpl().opts.keep_alive() ||
        std::uncaught_exceptions() > uncaught_on_entry_)
      cimpl().conn_cache.reset();
  }

  Client_connection* operator ->()
  {
    return conn_ptr_;
  }

private:
  static Client_connection* create_connection(Client_base& client)
  {
    return client.get_connection();
  }

  const Client_options& opts()
  {
    return client_impl_.opts;
  }

  Client_base::Impl& cimpl()
  {
    return client_impl_;
  }

  Client_base::Impl& client_impl_;
  std::unique_ptr<Client_connection> tmp_conn_;
  Client_connection* conn_ptr_;
  int uncaught_on_entry_;
};

//
// Client_base
//

Client_base::Client_base(
  const iqnet::Inet_addr& addr,
  const std::string& uri,
  const std::string& vhost
):
  impl_(new Impl(addr, uri, vhost))
{
}

Client_base::~Client_base() = default;

void Client_base::set_proxy( const iqnet::Inet_addr& addr )
{
  do_set_proxy( addr );
}

void Client_base::set_timeout( int seconds )
{
  impl_->opts.set_timeout(seconds);
}

int Client_base::timeout() const
{
  return impl_->opts.timeout();
}

//! Set connection keep-alive flag
void Client_base::set_keep_alive( bool keep_alive )
{
  impl_->opts.set_keep_alive(keep_alive);

  if (!keep_alive && impl_->conn_cache)
    impl_->conn_cache.reset();
}

void Client_base::set_authinfo( const std::string& u, const std::string& p )
{
  impl_->opts.set_authinfo( u, p );
}

void Client_base::set_xheaders( const XHeaders& xheaders)
{
  impl_->opts.set_xheaders(xheaders);
}

void Client_base::set_expected_hostname( const std::string& hostname )
{
  impl_->expected_hostname = hostname;
}

void Client_base::set_max_response_sz( size_t sz )
{
  impl_->opts.set_max_response_sz(sz);
}

size_t Client_base::get_max_response_sz() const
{
  return impl_->opts.max_response_sz();
}

const std::string& Client_base::expected_hostname() const
{
  return impl_->expected_hostname;
}

Response Client_base::execute(
  const std::string& method, const Param_list& pl, const XHeaders& xheaders )
{
  Request req( method, pl );

  Auto_conn conn( *impl_, *this );
  conn->set_options(impl_->opts);

  return conn->process_session( req, xheaders );
}

} // namespace iqxmlrpc

// vim:ts=2:sw=2:et

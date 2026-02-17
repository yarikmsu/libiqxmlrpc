//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_CLIENT_OPTS_H
#define IQXMLRPC_CLIENT_OPTS_H

#include <string>
#include "inet_addr.h"
#include "xheaders.h"

namespace iqxmlrpc {

class LIBIQXMLRPC_API Client_options {
public:
  Client_options(
    const iqnet::Inet_addr& addr,
    const std::string& uri,
    const std::string& vhost
  ):
    addr_(addr),
    uri_(uri),
    vhost_(vhost.empty() ? addr.get_host_name() : vhost),
    keep_alive_(false),
    timeout_(-1),
    non_blocking_flag_(false),
    auth_user_(),
    auth_passwd_(),
    xheaders_(),
    max_response_sz_(0)
  {
  }

  const iqnet::Inet_addr&  addr()         const { return addr_; }
  const std::string&       uri()          const { return uri_; }
  const std::string&       vhost()        const { return vhost_; }
  int                      timeout()      const { return timeout_; }
  bool                     non_blocking() const { return non_blocking_flag_; }
  bool                     keep_alive()   const { return keep_alive_; }

  bool                     has_authinfo() const { return !auth_user_.empty(); }
  const std::string&       auth_user()    const { return auth_user_; }
  const std::string&       auth_passwd()  const { return auth_passwd_; }
  const XHeaders&          xheaders()     const { return xheaders_; }

  void set_timeout( int seconds )
  {
    timeout_ = seconds;
    if( timeout_ > 0 )
      non_blocking_flag_ = true;
  }

  void set_keep_alive( bool keep_alive )
  {
    keep_alive_ = keep_alive;
  }

  void set_authinfo( const std::string& user, const std::string& password )
  {
    auth_user_ = user;
    auth_passwd_ = password;
  }

  void set_xheaders( const XHeaders& xheaders )
  {
    xheaders_ = xheaders;
  }

  size_t max_response_sz() const { return max_response_sz_; }

  void set_max_response_sz( size_t sz ) { max_response_sz_ = sz; }

private:
  iqnet::Inet_addr addr_;
  std::string      uri_;
  std::string      vhost_;
  bool             keep_alive_;

  int              timeout_;
  bool             non_blocking_flag_;

  std::string      auth_user_;
  std::string      auth_passwd_;

  XHeaders         xheaders_;

  size_t           max_response_sz_;
};

} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

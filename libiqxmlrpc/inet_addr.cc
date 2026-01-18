//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include <mutex>
#include <optional>
#include "inet_addr.h"
#include "net_except.h"

namespace iqnet {

namespace {
  // Global mutex to serialize DNS lookups. While gethostbyname_r provides
  // thread-local result storage, glibc's internal NSS implementation has
  // shared state (dynarray) that causes TSan warnings. Serializing DNS
  // lookups eliminates this race with minimal performance impact since
  // DNS results are typically cached by the system resolver.
  std::mutex& dns_mutex()
  {
    static std::mutex mtx;
    return mtx;
  }
}

std::string get_host_name()
{
  char buf[1024];
  buf[1023] = 0;
  ::gethostname( buf, sizeof(buf) );

  return buf;
}


#if defined(WIN32) || defined(__APPLE__)
#define IQXMLRPC_GETHOSTBYNAME(_h) \
  hent = ::gethostbyname( _h ); \
  if( !hent ) { \
    throw network_error( "gethostbyname" ); \
  }
#endif

#if !defined(IQXMLRPC_GETHOSTBYNAME)
#define IQXMLRPC_GETHOSTBYNAME_PREP \
  struct hostent hent_local; \
  char buf[1024]; \
  int local_h_errno = 0;
#endif

#if !defined(IQXMLRPC_GETHOSTBYNAME)
#define IQXMLRPC_GETHOSTBYNAME_POST \
  if( !hent ) { \
    throw network_error( "gethostbyname: " + std::string(hstrerror(local_h_errno)), false ); \
  }
#endif

#if defined(__sun) || defined(sun)
#define IQXMLRPC_GETHOSTBYNAME(_h) \
  IQXMLRPC_GETHOSTBYNAME_PREP \
  hent = ::gethostbyname_r( _h, &hent_local, buf, sizeof(buf), &local_h_errno ); \
  IQXMLRPC_GETHOSTBYNAME_POST
#endif

#if !defined(IQXMLRPC_GETHOSTBYNAME)
#define IQXMLRPC_GETHOSTBYNAME(_h) \
  IQXMLRPC_GETHOSTBYNAME_PREP \
  ::gethostbyname_r( _h, &hent_local, buf, sizeof(buf), &hent, &local_h_errno ); \
  IQXMLRPC_GETHOSTBYNAME_POST
#endif

typedef struct sockaddr_in SystemSockAddrIn;

struct Inet_addr::Impl {
  mutable std::optional<SystemSockAddrIn> sa;
  mutable std::once_flag sa_init_flag;
  std::string host;
  int port;

  explicit Impl( const SystemSockAddrIn& );
  Impl( const std::string& host, int port );
  explicit Impl( int port );

  void init_sockaddr() const;
};

Inet_addr::Impl::Impl( const std::string& h, int p ):
  sa(), sa_init_flag(), host(h), port(p)
{
  if (h.find_first_of("\n\r") != std::string::npos)
    throw network_error("Hostname must not contain CR LF characters", false);
}

Inet_addr::Impl::Impl( int p ):
  sa(SystemSockAddrIn()), sa_init_flag(), host("0.0.0.0"), port(p)
{
  sa->sin_family = PF_INET;
  sa->sin_port = htons(port);
  sa->sin_addr.s_addr = INADDR_ANY;
}

Inet_addr::Impl::Impl( const SystemSockAddrIn& s ):
  sa(s),
  sa_init_flag(),
  host(inet_ntoa( sa->sin_addr )),
  port(ntohs( sa->sin_port ))
{
}

void
Inet_addr::Impl::init_sockaddr() const
{
  sa = SystemSockAddrIn();
  // cppcheck-suppress constVariablePointer
  struct hostent* hent = nullptr;
  {
    std::lock_guard<std::mutex> lock(dns_mutex());
    IQXMLRPC_GETHOSTBYNAME(host.c_str());
    sa->sin_family = PF_INET;
    sa->sin_port = htons(port);
    // Validate h_length to prevent buffer overflow from malformed DNS response
    if (hent->h_length < 0 ||
        static_cast<size_t>(hent->h_length) > sizeof(struct in_addr)) {
      throw network_error("Invalid address length from DNS lookup", false);
    }
    memcpy( static_cast<void*>(&(sa->sin_addr)), static_cast<const void*>(hent->h_addr), hent->h_length );
  }
}

Inet_addr::Inet_addr( const std::string& host, int port ):
  impl_(new Impl(host, port))
{
}

Inet_addr::Inet_addr( int port ):
  impl_(new Impl(port))
{
}

Inet_addr::Inet_addr( const SystemSockAddrIn& sa ):
  impl_(new Impl(sa))
{
}

const SystemSockAddrIn*
Inet_addr::get_sockaddr() const
{
  std::call_once(impl_->sa_init_flag, [this]() {
    if (!impl_->sa) {
      impl_->init_sockaddr();
    }
  });

  if (!impl_->sa.has_value()) {
    throw network_error("Socket address initialization failed", false);
  }
  return &(*impl_->sa);
}

const std::string&
Inet_addr::get_host_name() const
{
  return impl_->host;
}

int
Inet_addr::get_port() const
{
  return impl_->port;
}

} // namespace iqnet

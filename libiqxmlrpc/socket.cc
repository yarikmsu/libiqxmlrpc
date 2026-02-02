//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include <cerrno>
#include <limits>
#include "socket.h"
#include "net_except.h"

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <netinet/tcp.h>
#endif

using namespace iqnet;

Socket::Socket():
  sock(-1),
  peer()
{
  // cppcheck-suppress useInitializationList
  // Cannot use initializer list: need to check socket() return value for errors
  sock = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
  if( sock == -1 )
    throw network_error( "Socket::Socket" );

#ifndef WIN32
  {
  // SO_REUSEADDR allows immediate reuse of the port after server restart.
  // Return value intentionally ignored - this is a "best effort" optimization
  // that should not prevent socket creation if it fails.
  int enable = 1;
  (void)setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable) );
  }
#endif //WIN32

#if defined(__APPLE__)
  {
  // SO_NOSIGPIPE prevents SIGPIPE on write to closed socket (macOS-specific).
  // Return value intentionally ignored - fallback is to catch SIGPIPE or
  // handle EPIPE error on send().
  int enable = 1;
  (void)setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable) );
  }
#endif

  // Enable TCP_NODELAY by default for RPC workloads.
  // Disables Nagle's algorithm to reduce latency for small messages.
  set_nodelay(true);
}

Socket::Socket( Socket::Handler h, const Inet_addr& addr ):
  sock(h),
  peer(addr)
{
}

void Socket::shutdown()
{
  if (is_valid()) {
    ::shutdown(sock, 2);
  }
}

void Socket::close()
{
#ifdef WIN32
  closesocket(sock);
#else
  ::close( sock );
#endif //WIN32
}

void Socket::set_non_blocking( bool flag )
{
#ifdef WIN32
  unsigned long f = flag ? 1 : 0;
  if( ioctlsocket(sock, FIONBIO, &f) != 0 )
    throw network_error( "Socket::set_non_blocking");
#else
  if( !flag )
    return;

  if( fcntl( sock, F_SETFL, O_NDELAY ) == -1 )
    throw network_error( "Socket::set_non_blocking" );
#endif //WIN32
}

void Socket::set_nodelay( bool enable )
{
  // TCP_NODELAY disables Nagle's algorithm for lower latency.
  // Return value intentionally ignored - this is a performance optimization
  // that should not prevent communication if it fails.
  int flag = enable ? 1 : 0;
  (void)setsockopt( sock, IPPROTO_TCP, TCP_NODELAY,
                    reinterpret_cast<const char*>(&flag), sizeof(flag) );
}

#if defined(MSG_NOSIGNAL)
#define IQXMLRPC_NOPIPE MSG_NOSIGNAL
#else
#define IQXMLRPC_NOPIPE 0
#endif

size_t Socket::send( const char* data, size_t len )
{
  // SECURITY: Prevent integer truncation when casting size_t to int.
  // On 64-bit systems, len > INT_MAX would wrap to negative/small value,
  // causing undefined behavior in the system call.
  if (len > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw network_error("Socket::send: buffer size exceeds INT_MAX");
  }

  int ret = ::send( sock, data, static_cast<int>(len), IQXMLRPC_NOPIPE);

  if( ret == -1 )
    throw network_error( "Socket::send" );

  return static_cast<size_t>(ret);
}

size_t Socket::recv( char* buf, size_t len )
{
  // SECURITY: Prevent integer truncation when casting size_t to int.
  // On 64-bit systems, len > INT_MAX would wrap to negative/small value,
  // causing undefined behavior in the system call.
  if (len > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw network_error("Socket::recv: buffer size exceeds INT_MAX");
  }

  int ret = ::recv( sock, buf, static_cast<int>(len), 0 );

  if( ret == -1 )
    throw network_error( "Socket::recv" );

  return static_cast<size_t>(ret);
}

void Socket::send_shutdown( const char* data, size_t len )
{
  send(data, len);
  // SO_LINGER with zero timeout enables abortive close (RST instead of FIN).
  // Return value intentionally ignored - if it fails, the socket will still
  // close normally with graceful TCP teardown.
  const struct linger ling = {1, 0};
  (void)::setsockopt( sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&ling), sizeof(ling) );
  ::shutdown( sock, 1 );
}

void Socket::bind( const Inet_addr& addr )
{
  auto saddr = reinterpret_cast<const sockaddr*>(addr.get_sockaddr());

  if( ::bind( sock, saddr, sizeof(sockaddr_in) ) == -1 )
    throw network_error( "Socket::bind" );
}

void Socket::listen( unsigned blog )
{
  if( ::listen( sock, blog ) == -1 )
    throw network_error( "Socket::listen" );
}

Socket Socket::accept()
{
  sockaddr_in addr;
  socklen_t len = sizeof(sockaddr_in);

  Handler new_sock = ::accept( sock, reinterpret_cast<sockaddr*>(&addr), &len );
  if( new_sock == -1 )
    throw network_error( "Socket::accept" );

  Socket accepted_socket( new_sock, Inet_addr(addr) );
  accepted_socket.set_nodelay(true);  // Enable TCP_NODELAY for RPC workloads
  return accepted_socket;
}

bool Socket::connect( const iqnet::Inet_addr& peer_addr )
{
  auto saddr = reinterpret_cast<const sockaddr*>(peer_addr.get_sockaddr());

  int code = ::connect(sock, saddr, sizeof(sockaddr_in));
  bool wouldblock = false;

  if( code == -1 ) {
#ifndef WIN32
    wouldblock = errno == EINPROGRESS;
#else
    wouldblock = get_last_error() == WSAEWOULDBLOCK;
#endif

    if (!wouldblock)
      throw network_error( "Socket::connect" );
  }

  peer = peer_addr;
  return !wouldblock;
}

Inet_addr Socket::get_addr() const
{
  sockaddr_in saddr;
  socklen_t saddr_len = sizeof(saddr);

  if (::getsockname(sock, reinterpret_cast<sockaddr*>(&saddr), &saddr_len) == -1)
    throw network_error( "Socket::get_addr" );

  return Inet_addr(reinterpret_cast<const sockaddr_in&>(saddr));
}

int Socket::get_last_error()
{
  int err = 0;
#ifndef WIN32
  socklen_t int_sz = sizeof(err);
  // If getsockopt fails, fall back to errno which likely contains
  // the actual error that caused the socket operation to fail.
  if (::getsockopt( sock, SOL_SOCKET, SO_ERROR, &err, &int_sz ) != 0) {
    err = errno;
  }
#else
  err=WSAGetLastError();
#endif
  return err;
}

#ifndef _libiqnet_inet_addr_h_
#define _libiqnet_inet_addr_h_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

namespace iqnet 
{
  class Inet_addr;
};


//! An object representation of internet address.
/*! A wrapper for sockaddr_in system structure. */
class iqnet::Inet_addr {
  struct sockaddr_in sa;
  std::string host;
  int port;
  
public:
  Inet_addr( const struct sockaddr_in& );
  Inet_addr( const std::string& host, int port );
  Inet_addr( int port );

  virtual ~Inet_addr() {}
  
  const struct sockaddr_in* get_sockaddr() const { return &sa; }
  const std::string& get_host_name() const { return host; }
  int get_port() const { return port; }
};

#endif
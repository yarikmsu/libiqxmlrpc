//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#if _MSC_VER >= 1700
#include <windows.h>
#endif

#include <cstring>
#include "net_except.h"

namespace {

inline std::string
exception_message(const std::string& prefix, bool use_errno, int myerrno)
{
  std::string retval = prefix;

  if (use_errno)
  {
    retval += ": ";

    char buf[256];
    buf[255] = 0;

#if defined WIN32
    strerror_s( buf, sizeof(buf) - 1, WSAGetLastError() );
    retval += std::string(buf);
#else
    int err = myerrno ? myerrno : errno;
#if defined _GNU_SOURCE
    char* b = strerror_r( err, buf, sizeof(buf) - 1 );
    retval += std::string(b);
#else
    strerror_r( err, buf, sizeof(buf) - 1 );
    retval += std::string(buf);
#endif
#endif
  }

  return retval;
}

}

iqnet::network_error::network_error( const std::string& msg, bool use_errno, int myerrno ):
  std::runtime_error( exception_message(msg, use_errno, myerrno) )
{
}

// vim:ts=2:sw=2:et

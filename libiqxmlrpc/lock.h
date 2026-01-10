//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _libiqnet_lock_h_
#define _libiqnet_lock_h_

#include "api_export.h"

namespace iqnet
{

//! Class which provides null synchronization.
//! Methods are intentionally non-static for template compatibility with boost::mutex.
class LIBIQXMLRPC_API Null_lock {
public:
  struct scoped_lock {
    explicit scoped_lock(Null_lock&) {}
    ~scoped_lock() {}

    // cppcheck-suppress functionStatic
    void lock() {}
    // cppcheck-suppress functionStatic
    void unlock() {}
  };

  // cppcheck-suppress functionStatic
  void lock() {}
  // cppcheck-suppress functionStatic
  void unlock() {}
};

} // namespace iqnet

#endif

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef LIBIQNET_LOCK_H
#define LIBIQNET_LOCK_H

#include "api_export.h"

#include <mutex>

namespace iqnet
{

//! Class which provides null synchronization.
//! Methods are intentionally non-static for template compatibility with std::mutex.
//! Compatible with std::unique_lock<Null_lock>.
class LIBIQXMLRPC_API Null_lock {
public:
  // cppcheck-suppress functionStatic
  void lock() {}
  // cppcheck-suppress functionStatic
  void unlock() {}
};

//! Type alias for scoped lock, works with both Null_lock and std::mutex
template<typename Lock>
using scoped_lock = std::unique_lock<Lock>;

} // namespace iqnet

#endif

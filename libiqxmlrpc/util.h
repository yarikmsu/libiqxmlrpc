//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _iqxmlrpc_util_h_
#define _iqxmlrpc_util_h_

#include "lock.h"

#include <functional>
#include <memory>

namespace iqxmlrpc {
namespace util {

template <class M>
class Select2nd
{
public:
  using argument_type = typename M::value_type;
  using result_type = typename M::mapped_type;

  typename M::mapped_type operator ()(const typename M::value_type& i)
  {
    return i.second;
  }
};

template <class Iter>
void delete_ptrs(Iter first, Iter last)
{
  for(; first != last; ++first)
    delete *first;
}

template <class Iter, class AccessOp>
void delete_ptrs(Iter first, Iter last, AccessOp op)
{
  for(; first != last; ++first)
    delete op(*first);
}

template <class Ptr>
class ExplicitPtr {
  Ptr p_;

public:
  explicit ExplicitPtr(Ptr p): p_(p) {}

  ExplicitPtr(ExplicitPtr& ep):
    p_(ep.release())
  {
  }

  ExplicitPtr& operator=(const ExplicitPtr&) = delete;

  ~ExplicitPtr()
  {
    delete release();
  }

  Ptr release()
  {
    Ptr p(p_);
    p_ = nullptr;
    return p;
  }
};

//! Provides serialized access to some bool value
template <class Lock>
class LockedBool {
  bool val;
  Lock lock;

public:
  LockedBool(const LockedBool&) = delete;
  LockedBool& operator=(const LockedBool&) = delete;

  explicit LockedBool(bool default_):
    val(default_) {}

  ~LockedBool() {}

  operator bool()
  {
    iqnet::scoped_lock<Lock> lk(lock);
    return val;
  }

  LockedBool& operator =(bool b)
  {
    iqnet::scoped_lock<Lock> lk(lock);
    val = b;
    return *this;
  }
};

} // namespace util
} // namespace iqxmlrpc

#endif

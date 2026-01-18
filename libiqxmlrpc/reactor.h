//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef LIBIQNET_REACTOR_H
#define LIBIQNET_REACTOR_H

#include "lock.h"
#include "net_except.h"
#include "socket.h"

#include <cstdint>
#include <list>
#include <memory>

namespace iqnet
{

//! Base class for event-driven communication classes.
class LIBIQXMLRPC_API Event_handler {
public:
  virtual ~Event_handler() = default;

  //! If this handler used as Reactor stopper.
  virtual bool is_stopper() const { return false; }

  virtual void handle_input( bool& /* terminate */) {}
  virtual void handle_output( bool& /* terminate */) {}

  //! Invoked by Reactor when handle_X()
  //! sets terminate variable to true.
  virtual void finish() {};

  //! Whether reactor should catch its exceptions.
  virtual bool catch_in_reactor() const { return false; }
  //! Log its exception catched in an external object.
  virtual void log_exception( const std::exception& ) {};
  //! Log its exception catched in an external object.
  virtual void log_unknown_exception() {};

  virtual Socket::Handler get_handler() const = 0;
};

//! Abstract base for Reactor template.
//! It defines interface, standard exceptions and
//! general data structures for all implementations.
class LIBIQXMLRPC_API Reactor_base {
public:
  class No_handlers: public iqnet::network_error {
  public:
    No_handlers():
      network_error( "iqnet::Reactor: no handlers given.", false ) {}
  };

  enum Event_mask : std::uint8_t { INPUT=1, OUTPUT=2 };

  struct HandlerState {
    Socket::Handler fd;
    short           mask;
    short           revents;

    explicit HandlerState( Socket::Handler fd_ = 0 ):
      fd(fd_), mask(0), revents(0) {}

    HandlerState( Socket::Handler fd_, Event_mask m ):
      fd(fd_), mask(m), revents(0) {}

    bool operator ==(const HandlerState& hs)
    {
      return fd == hs.fd;
    }
  };

  typedef std::list<HandlerState> HandlerStateList;
  typedef int Timeout;

  //! Copy-on-write wrapper for handler list to avoid O(N) copies on hot path.
  //! Readers get immutable snapshots (shared_ptr<const>), writers copy if needed.
  //! Thread safety: All methods except snapshot() must be called with the external
  //! lock held. snapshot() may be called under lock to safely share data with
  //! readers who will access it without the lock.
  template<typename T>
  class CowList {
  public:
    using List = std::list<T>;
    using Snapshot = std::shared_ptr<const List>;

    CowList() : data_(std::make_shared<List>()) {}

    //! Get immutable snapshot - just refcount increment, no copy
    Snapshot snapshot() const { return data_; }

    //! Copy before write if readers hold references (caller must hold lock)
    void copy_for_write() {
      if (data_.use_count() != 1) {
        data_ = std::make_shared<List>(*data_);
      }
    }

    // Iterator access (for compatibility with existing code)
    auto begin() { return data_->begin(); }
    auto end() { return data_->end(); }
    auto begin() const { return data_->begin(); }
    auto end() const { return data_->end(); }

    // Container methods
    size_t size() const { return data_->size(); }

    // Modification methods (caller must call copy_for_write() first)
    void push_back(const T& val) { data_->push_back(val); }
    void erase(typename List::iterator it) { data_->erase(it); }

  private:
    std::shared_ptr<List> data_;
  };

  virtual ~Reactor_base() = default;

  virtual void register_handler( Event_handler*, Event_mask )   = 0;
  virtual void unregister_handler( Event_handler*, Event_mask ) = 0;
  virtual void unregister_handler( Event_handler* ) = 0;
  virtual void fake_event( Event_handler*, Event_mask ) = 0;

  //! \return true if any handle was invoked, false on timeout.
  /*! Throws Reactor::No_handlers when no one handler has been registered. */
  virtual bool handle_events( Timeout ms = -1 ) = 0;
};

} // namespace iqnet

#endif

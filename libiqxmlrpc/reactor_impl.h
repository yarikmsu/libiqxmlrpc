//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_REACTOR_IMPL_H
#define IQXMLRPC_REACTOR_IMPL_H

#include "config.h"
#include "reactor.h"

#ifdef HAVE_POLL
#include "reactor_poll_impl.h"
  namespace iqnet
  {
    typedef Reactor_poll_impl ReactorImpl;
  }
#else
#include "reactor_select_impl.h"
  namespace iqnet
  {
    typedef Reactor_select_impl ReactorImpl;
  }
#endif // HAVE_POLL

#include <assert.h>
#include <map>
#include <algorithm>

namespace iqnet
{

//! The Reactor template class.
//! Lock param can be either std::mutex or iqnet::Null_lock.
template <class Lock>
class Reactor: public Reactor_base {
public:
  Reactor(const Reactor&) = delete;
  Reactor& operator=(const Reactor&) = delete;

  Reactor();
  ~Reactor() override = default;

  void register_handler( Event_handler*, Event_mask ) override;
  void unregister_handler( Event_handler*, Event_mask ) override;
  void unregister_handler( Event_handler* ) override;

  void fake_event( Event_handler*, Event_mask ) override;

  bool handle_events( Timeout ms = -1 ) override;

private:
  typedef iqnet::scoped_lock<Lock>          scoped_lock;
  typedef std::map<Socket::Handler, Event_handler*> EventHandlersMap;
  typedef EventHandlersMap::iterator        h_iterator;
  typedef HandlerStateList::const_iterator  hs_const_iterator;
  typedef HandlerStateList::iterator        hs_iterator;

  size_t            size()  const { return handlers_states.size(); }
  hs_const_iterator begin() const { return handlers_states.begin(); }
  hs_iterator       begin()       { return handlers_states.begin(); }
  hs_const_iterator end()   const { return handlers_states.end(); }
  hs_iterator       end()         { return handlers_states.end(); }

  Event_handler* find_handler(Socket::Handler);
  hs_iterator find_handler_state(const Event_handler*);

  void handle_user_events();
  bool handle_system_events( Timeout );

  static void invoke_clients_handler( Event_handler*, const HandlerState&, bool& terminate );
  static void invoke_servers_handler( Event_handler*, const HandlerState&, bool& terminate );
  void invoke_event_handler( const HandlerState& );

private:
  Lock lock;
  ReactorImpl impl;

  EventHandlersMap handlers;
  CowList<HandlerState> handlers_states;  // COW for zero-copy reads on hot path

  unsigned num_stoppers;
};


// ----------------------------- Implementation -----------------------------

template <class Lock>
Reactor<Lock>::Reactor():
  lock(),
  impl(),
  handlers(),
  handlers_states(),
  num_stoppers(0)
{
}

template <class Lock>
typename Reactor<Lock>::hs_iterator
Reactor<Lock>::find_handler_state(const Event_handler* eh)
{
  return std::find(begin(), end(), HandlerState(eh->get_handler()));
}

template <class Lock>
iqnet::Event_handler* Reactor<Lock>::find_handler(Socket::Handler fd)
{
  scoped_lock lk(lock);
  auto i = handlers.find(fd);
  return i == handlers.end() ? nullptr : i->second;
}

template <class Lock>
void Reactor<Lock>::register_handler( Event_handler* eh, Event_mask mask )
{
  scoped_lock lk(lock);

  if (eh->is_stopper())
    num_stoppers++;

  Socket::Handler fd = eh->get_handler();

  if( handlers.find(fd) == handlers.end() )
  {
    handlers_states.copy_for_write();  // COW: copy if readers hold references
    handlers_states.push_back( HandlerState(fd, mask) );
    handlers[fd] = eh;
  }
  else
  {
    handlers_states.copy_for_write();  // COW: copy if readers hold references
    auto i = find_handler_state(eh);
    i->mask |= mask;
  }
}

template <class Lock>
void Reactor<Lock>::unregister_handler( Event_handler* eh, Event_mask mask )
{
  scoped_lock lk(lock);
  handlers_states.copy_for_write();  // COW: copy if readers hold references
  auto i = find_handler_state( eh );

  if( i != end() )
  {
    int newmask = (i->mask &= ~mask);

    if( !newmask )
    {
      handlers.erase(eh->get_handler());
      handlers_states.erase(i);

      if (eh->is_stopper())
        num_stoppers--;
    }
  }
}

template <class Lock>
void Reactor<Lock>::unregister_handler( Event_handler* eh )
{
  scoped_lock lk(lock);
  auto i = handlers.find(eh->get_handler());

  if( i != handlers.end() )
  {
    handlers_states.copy_for_write();  // COW: copy if readers hold references
    handlers.erase(i);
    handlers_states.erase(find_handler_state(eh));

    if (eh->is_stopper())
      num_stoppers--;
  }
}

template <class Lock>
void Reactor<Lock>::fake_event( Event_handler* eh, Event_mask mask )
{
  scoped_lock lk(lock);
  handlers_states.copy_for_write();  // COW: copy if readers hold references
  auto i = find_handler_state( eh );

  if( i != end() )
    i->revents |= mask;
}

template <class Lock>
void Reactor<Lock>::invoke_clients_handler(
  Event_handler* handler, const HandlerState& hs, bool& terminate )
{
  bool in  = (hs.revents & Reactor_base::INPUT) != 0;
  bool out = (hs.revents & Reactor_base::OUTPUT) != 0;

  if( in )
    handler->handle_input( terminate );
  else if( out )
    handler->handle_output( terminate );
}

template <class Lock>
void Reactor<Lock>::invoke_servers_handler(
    Event_handler* handler, const HandlerState& hs, bool& terminate )
{
  try {
    invoke_clients_handler( handler, hs, terminate );
  }
  catch( const std::exception& e )
  {
    handler->log_exception( e );
    terminate = true;
  }
  catch( ... )
  {
    handler->log_unknown_exception();
    terminate = true;
  }
}

// THREADING SAFETY NOTE:
// This function is ONLY called from the reactor thread. Handler registration
// and unregistration also only occur from the reactor thread. This single-threaded
// access pattern guarantees that the handler pointer remains valid throughout
// this function's execution, even though find_handler() releases the lock.
// If this threading model changes, consider using shared_ptr for handlers.
template <class Lock>
void Reactor<Lock>::invoke_event_handler( const Reactor_base::HandlerState& hs )
{
  bool terminate = false;

  Event_handler* handler = find_handler(hs.fd);
  assert(handler);

  if( handler->catch_in_reactor() )
    invoke_servers_handler( handler, hs, terminate );
  else
    invoke_clients_handler( handler, hs, terminate );

  if( terminate )
  {
    unregister_handler( handler );
    handler->finish();
  }
}

template <class Lock>
void Reactor<Lock>::handle_user_events()
{
  HandlerStateList called_by_user;
  scoped_lock lk(lock);

  handlers_states.copy_for_write();  // COW: copy if readers hold references
  for( auto i = begin(); i != end(); ++i )
  {
    if( i->revents && (i->mask | i->revents) )
    {
      called_by_user.push_back( *i );
      i->revents &= ~i->mask;
    }
  }

  lk.unlock();

  while( !called_by_user.empty() )
  {
    HandlerState hs(called_by_user.front());
    called_by_user.pop_front();
    invoke_event_handler(hs);
  }
}

template <class Lock>
bool Reactor<Lock>::handle_system_events(Reactor_base::Timeout ms)
{
  // COW optimization: get immutable snapshot instead of O(N) copy
  CowList<HandlerState>::Snapshot snapshot;
  {
    scoped_lock lk(lock);
    snapshot = handlers_states.snapshot();  // Just refcount++, no copy!
  }

  // if all events were of "user" type
  if (snapshot->empty())
    return true;

  impl.reset(*snapshot);
  HandlerStateList ret;
  bool succ = impl.poll(ret, ms);

  if (!succ)
    return false;

  while (!ret.empty())
  {
    HandlerState hs(ret.front());
    ret.pop_front();
    invoke_event_handler(hs);
  }

  return true;
}

template <class Lock>
bool Reactor<Lock>::handle_events(Reactor_base::Timeout ms)
{
  {
    scoped_lock lk(lock);
    if (handlers.empty())
      return false;

    if (handlers.size() <= num_stoppers)
      throw No_handlers();
  }

  handle_user_events();
  return handle_system_events(ms);
}

} // namespace iqnet

#endif

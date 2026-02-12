//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include "executor.h"
#include "except.h"
#include "reactor_impl.h"
#include "response.h"
#include "server.h"
#include "server_conn.h"
#include "util.h"

#include <cassert>
#include <cstdio>
#include <memory>

using namespace iqxmlrpc;
typedef std::unique_lock<std::mutex> scoped_lock;

Executor::Executor( Method* m, Server* s, Server_connection* cb ):
  method(m),
  interceptors(nullptr),
  server(s),
  conn(cb),
  conn_guard_()
{
}


Executor::~Executor() = default;


void Executor::schedule_response( const Response& resp )
{
  // Pool executors use the guard for safe cross-thread delivery;
  // serial executors use the raw conn (same thread as reactor).
  if (conn_guard_) {
    server->schedule_response( resp, conn_guard_, this );
  } else {
    server->schedule_response( resp, conn, this );
  }
}


void Executor::interrupt_server()
{
  server->interrupt();
}

// ----------------------------------------------------------------------------
void Serial_executor::execute( const Param_list& params )
{
  try {
    std::unique_ptr<Value> result(new Value(0));
    method->process_execution( interceptors, params, *result );
    schedule_response( Response(result.release()) );
  }
  catch( const Fault& f )
  {
    schedule_response( Response( f.code(), f.what() ) );
  }
}


Executor* Serial_executor_factory::create(
  Method* m, Server* s, Server_connection* c )
{
  return new Serial_executor( m, s, c );
}


iqnet::Reactor_base* Serial_executor_factory::create_reactor()
{
  return new iqnet::Reactor<iqnet::Null_lock>;
}


// ----------------------------------------------------------------------------
#ifndef DOXYGEN_SHOULD_SKIP_THIS
class Pool_executor_factory::Pool_thread {
  Pool_executor_factory* pool;

public:
  explicit Pool_thread( Pool_executor_factory* pool_ ):
    pool(pool_)
  {
  }

  // thread's entry point
  void operator ()();
};
#endif


void Pool_executor_factory::Pool_thread::operator ()()
{
  Pool_executor_factory* pool_ptr = this->pool;

  for (;;)
  {
    Pool_executor* executor = nullptr;

    // Lock-free dequeue loop
    while (!pool_ptr->req_queue.pop(executor))
    {
      // Check shutdown flag (lock-free)
      if (pool_ptr->is_being_destructed())
        return;

      // Wait for work or shutdown.
      std::unique_lock<std::mutex> lk(pool_ptr->wait_mutex);
      pool_ptr->wait_cond.wait(lk, [pool_ptr] {
        return pool_ptr->pending_count.load(std::memory_order_acquire) > 0
            || pool_ptr->is_being_destructed();
      });

      if (pool_ptr->is_being_destructed())
        return;
    }

    // Successfully dequeued - decrement pending count.
    // Relaxed: pending_count is a wake-up hint, not a synchronization barrier.
    pool_ptr->pending_count.fetch_sub(1, std::memory_order_relaxed);

    // RAII guard: decrement outstanding_count even if execution throws,
    // then wake drain() waiter when count reaches zero.
    struct Drain_guard {
      Pool_executor_factory* p;
      ~Drain_guard() {
        if (p->outstanding_count.fetch_sub(1, std::memory_order_release) == 1) {
          std::lock_guard<std::mutex> lk(p->drain_mutex);
          p->drain_cond.notify_one();
        }
      }
    } drain_guard{pool_ptr};

    // process_actual_execution() calls schedule_response(), which deletes
    // the executor. Do not use executor after this call.
    executor->process_actual_execution();
  }
}


// ----------------------------------------------------------------------------
Pool_executor_factory::Pool_executor_factory(unsigned numthreads):
  threads(),
  pool(),
  req_queue(),
  wait_mutex(),
  wait_cond(),
  pending_count(0),
  in_destructor(false),
  drain_timeout_(DEFAULT_DRAIN_TIMEOUT),
  outstanding_count(0),
  drain_mutex(),
  drain_cond()
{
  add_threads(numthreads);
}


Pool_executor_factory::~Pool_executor_factory()
{
  destruction_started();
  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // Drain remaining items from lock-free queue (single-threaded after join).
  Pool_executor* remaining = nullptr;
  while (req_queue.pop(remaining)) {
    outstanding_count.fetch_sub(1, std::memory_order_relaxed);
    delete remaining;
  }
  auto final_count = outstanding_count.load(std::memory_order_relaxed);
  if (final_count != 0) {
    (void)std::fprintf(stderr,
      "iqxmlrpc: BUG: ~Pool_executor_factory outstanding_count=%zu "
      "(expected 0)\n", final_count);
  }
  assert(final_count == 0
         && "outstanding_count leak: increment without matching decrement");
}


Executor* Pool_executor_factory::create(
  Method* m, Server* s, Server_connection* c )
{
  return new Pool_executor( this, m, s, c );
}


iqnet::Reactor_base* Pool_executor_factory::create_reactor()
{
  return new iqnet::Reactor<std::mutex>;
}


void Pool_executor_factory::add_threads( unsigned num )
{
  for( unsigned i = 0; i < num; ++i )
  {
    auto t = std::make_unique<Pool_thread>(this);
    Pool_thread* raw_ptr = t.get();
    pool.push_back(std::move(t));
    threads.emplace_back([raw_ptr]() { (*raw_ptr)(); });
  }
}


void Pool_executor_factory::register_executor( Pool_executor* executor )
{
  // Increment outstanding BEFORE push so drain() never observes a transient zero.
  // Decremented by Drain_guard (after execution) or ~Pool_executor_factory
  // (queued-but-unexecuted items during shutdown).
  outstanding_count.fetch_add(1, std::memory_order_release);
  while (!req_queue.push(executor)) {
    std::this_thread::yield();  // Queue full - rare with proper sizing
  }

  pending_count.fetch_add(1, std::memory_order_release);

  // Wake one sleeping worker
  {
    std::lock_guard<std::mutex> lk(wait_mutex);
    wait_cond.notify_one();
  }
}


void Pool_executor_factory::destruction_started()
{
  in_destructor.store(true, std::memory_order_release);

  // Wake all workers to exit
  {
    std::lock_guard<std::mutex> lk(wait_mutex);
    wait_cond.notify_all();
  }
}


bool Pool_executor_factory::is_being_destructed()
{
  return in_destructor.load(std::memory_order_acquire);
}


void Pool_executor_factory::drain()
{
  std::unique_lock<std::mutex> lk(drain_mutex);
  auto total_waited = std::chrono::milliseconds(0);
  while (outstanding_count.load(std::memory_order_acquire) != 0) {
    bool completed = drain_cond.wait_for(lk, drain_timeout_, [this] {
      return outstanding_count.load(std::memory_order_acquire) == 0;
    });
    if (!completed) {
      total_waited += drain_timeout_;
      (void)std::fprintf(stderr,
        "iqxmlrpc: WARNING: drain() still waiting (outstanding_count=%zu, "
        "total_wait=%lldms)\n",
        outstanding_count.load(std::memory_order_relaxed),
        static_cast<long long>(total_waited.count()));
    }
  }
}


// ----------------------------------------------------------------------------
Pool_executor::Pool_executor(
    Pool_executor_factory* p, Method* m, Server* s, Server_connection* c
  ):
    Executor( m, s, c ),
    pool(p),
    params()
{
  if (c)
    set_connection_guard(c->connection_guard());
}


Pool_executor::~Pool_executor()
{
  if (pool->is_being_destructed())
    return;  // Pool is shutting down — Server is already destroyed, interrupt_server() would be UAF

  try {
    interrupt_server();
  } catch (const std::exception& e) {
    (void)std::fprintf(stderr,
      "iqxmlrpc: WARNING: ~Pool_executor::interrupt_server() failed: %s\n",
      e.what());
  } catch (...) {
    (void)std::fprintf(stderr,
      "iqxmlrpc: WARNING: ~Pool_executor::interrupt_server() failed "
      "(unknown exception)\n");
  }
}


void Pool_executor::execute( const Param_list& params_ )
{
  params = params_;
  pool->register_executor( this );
}


void Pool_executor::execute( Param_list&& params_ )
{
  params = std::move(params_);
  pool->register_executor( this );
}


void Pool_executor::process_actual_execution()
{
  try {
    std::unique_ptr<Value> result(new Value(0));
    method->process_execution( interceptors, params, *result );
    schedule_response( Response(result.release()) );
  }
  catch( const Fault& f )
  {
    schedule_response( Response( f.code(), f.what() ) );
  }
  catch( const std::exception& e )
  {
    schedule_response( Response( -1, e.what() ) );
  }
  catch( ... )
  {
    schedule_response( Response( -1, "Unknown Error" ) );
  }
}

// vim:ts=2:sw=2:et

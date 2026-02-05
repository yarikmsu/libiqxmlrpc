//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#include "executor.h"
#include "except.h"
#include "reactor_impl.h"
#include "response.h"
#include "server.h"
#include "util.h"

#include <memory>

using namespace iqxmlrpc;
typedef std::unique_lock<std::mutex> scoped_lock;

Executor::Executor( Method* m, Server* s, Server_connection* cb ):
  method(m),
  interceptors(nullptr),
  server(s),
  conn(cb)
{
}


Executor::~Executor() = default;


void Executor::schedule_response( const Response& resp )
{
  server->schedule_response( resp, conn, this );
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

      // Wait for work or shutdown using condition variable with predicate.
      // The predicate re-checks on spurious wakeups and prevents lost
      // wakeups when notify arrives between the check and the wait.
      std::unique_lock<std::mutex> lk(pool_ptr->wait_mutex);
      pool_ptr->wait_cond.wait(lk, [pool_ptr] {
        return pool_ptr->pending_count.load(std::memory_order_acquire) > 0
            || pool_ptr->is_being_destructed();
      });

      if (pool_ptr->is_being_destructed())
        return;
    }

    // Successfully dequeued - decrement pending count
    pool_ptr->pending_count.fetch_sub(1, std::memory_order_relaxed);

    // Execute the work (no lock held)
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
  in_destructor(false)
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

  // pool contains unique_ptr<Pool_thread>, auto-cleaned when vector destructs

  // Drain remaining items from lock-free queue (single-threaded after join)
  Pool_executor* remaining = nullptr;
  while (req_queue.pop(remaining)) {
    delete remaining;
  }
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
  // Lock-free enqueue
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


// ----------------------------------------------------------------------------
Pool_executor::Pool_executor(
    Pool_executor_factory* p, Method* m, Server* s, Server_connection* c
  ):
    Executor( m, s, c ),
    pool(p),
    params()
{
}


Pool_executor::~Pool_executor()
{
  try {
    interrupt_server();
  } catch (...) { // NOLINT(bugprone-empty-catch)
    // Suppress exceptions: destructors are implicitly noexcept (C++11).
    // interrupt_server() may throw network_error if the interrupter
    // socket is already closed during shutdown.
  }
}


void Pool_executor::execute( const Param_list& params_ )
{
  params = params_;
  pool->register_executor( this );
}


void Pool_executor::execute( Param_list&& params_ )
{
  // PERFORMANCE: Move params to avoid cloning all Values
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

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

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
  for(;;)
  {
    scoped_lock lk(pool->req_queue_lock);

    // Check BEFORE waiting to prevent race where notification arrives
    // while thread is processing a request (after unlock, before wait)
    if (pool->is_being_destructed())
      return;

    if (pool->req_queue.empty())
    {
      pool->req_queue_cond.wait(lk);

      if (pool->is_being_destructed())
        return;

      if (pool->req_queue.empty())
        continue;
    }

    Pool_executor* executor = pool->req_queue.front();
    pool->req_queue.pop_front();
    lk.unlock();

    executor->process_actual_execution();
  }
}


// ----------------------------------------------------------------------------
Pool_executor_factory::Pool_executor_factory(unsigned numthreads):
  threads(),
  pool(),
  req_queue(),
  req_queue_lock(),
  req_queue_cond(),
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

  scoped_lock lk(req_queue_lock);
  util::delete_ptrs(req_queue.begin(), req_queue.end());
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
  scoped_lock lk(req_queue_lock);
  req_queue.push_back(executor);
  req_queue_cond.notify_one();
}


void Pool_executor_factory::destruction_started()
{
  in_destructor.store(true, std::memory_order_release);

  scoped_lock lk(req_queue_lock);
  req_queue_cond.notify_all();
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
  interrupt_server();
}


void Pool_executor::execute( const Param_list& params_ )
{
  params = params_;
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

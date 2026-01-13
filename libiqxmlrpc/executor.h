//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _iqxmlrpc_executor_h_
#define _iqxmlrpc_executor_h_

#include "lock.h"
#include "method.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <deque>
#include <vector>

namespace iqnet
{
  class Reactor_base;
}

namespace iqxmlrpc {

class Server;
class Server_connection;
class Response;

class Serial_executor_factory;
class Pool_executor_factory;

struct Serial_executor_traits
{
  typedef Serial_executor_factory Executor_factory;
  typedef iqnet::Null_lock Lock;
};

struct Pool_executor_traits
{
  typedef Pool_executor_factory Executor_factory;
  typedef std::mutex Lock;
};

//! Abstract executor class. Defines the policy for method execution.
class LIBIQXMLRPC_API Executor {
public:
  Executor(const Executor&) = delete;
  Executor& operator=(const Executor&) = delete;

protected:
  std::unique_ptr<Method> method;
  Interceptor* interceptors;

private:
  Server* server;
  Server_connection* conn;

public:
  Executor( Method*, Server*, Server_connection* );
  virtual ~Executor();

  void set_interceptors(Interceptor* ic) { interceptors = ic; }

  //! Start method execution.
  virtual void execute( const Param_list& params ) = 0;

protected:
  void schedule_response( const Response& );
  void interrupt_server();
};


//! Abstract base for Executor's factories.
class LIBIQXMLRPC_API Executor_factory_base {
public:
  virtual ~Executor_factory_base() {}

  virtual Executor* create(
    Method*,
    Server*,
    Server_connection*
  ) = 0;

  virtual iqnet::Reactor_base* create_reactor() = 0;
};


//! Single thread executor.
class LIBIQXMLRPC_API Serial_executor: public Executor {
public:
  Serial_executor( Method* m, Server* s, Server_connection* c ):
    Executor( m, s, c ) {}

  void execute( const Param_list& ) override;
};


//! Factory class for Serial_executor.
class LIBIQXMLRPC_API Serial_executor_factory: public Executor_factory_base {
public:
  Executor* create( Method* m, Server* s, Server_connection* c ) override;
  iqnet::Reactor_base* create_reactor() override;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

//! An Executor which plans request to be executed by a pool of threads.
class LIBIQXMLRPC_API Pool_executor: public Executor {
  Pool_executor_factory* pool;
  Param_list params;

public:
  Pool_executor( Pool_executor_factory*, Method*, Server*, Server_connection* );
  ~Pool_executor() override;

  Pool_executor(const Pool_executor&) = delete;
  Pool_executor& operator=(const Pool_executor&) = delete;

  void execute( const Param_list& ) override;
  void process_actual_execution();
};

//! Factory for Pool_executor objects. It is also serves as a pool of threads.
class LIBIQXMLRPC_API Pool_executor_factory: public Executor_factory_base {
  class Pool_thread;
  friend class Pool_thread;

  std::vector<std::thread>  threads;
  std::vector<std::unique_ptr<Pool_thread>> pool;

  // Objects Pool_thread works with
  std::deque<Pool_executor*> req_queue;
  std::mutex                 req_queue_lock;
  std::condition_variable    req_queue_cond;

  std::atomic<bool> in_destructor;

public:
  explicit Pool_executor_factory(unsigned num_threads);
  ~Pool_executor_factory() override;

  Executor* create( Method* m, Server* s, Server_connection* c ) override;
  iqnet::Reactor_base* create_reactor() override;

  //! Add some threads to the pool.
  void add_threads(unsigned num);

  void register_executor( Pool_executor* );

private:
  // Pool_thread interface
  bool is_being_destructed();

private:
  void destruction_started();
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace iqxmlrpc

#endif
// vim:ts=2:sw=2:et

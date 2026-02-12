//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov
//  Copyright (C) 2019-2026 Yaroslav Gorbunov

#ifndef IQXMLRPC_EXECUTOR_H
#define IQXMLRPC_EXECUTOR_H

#include "lock.h"
#include "method.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/lockfree/queue.hpp>

namespace iqnet
{
  class Reactor_base;
}

namespace iqxmlrpc {

class Server;
class Server_connection;
class Response;
class ConnectionGuard;
using ConnectionGuardPtr = std::shared_ptr<ConnectionGuard>;

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
  ConnectionGuardPtr conn_guard_;

public:
  Executor( Method*, Server*, Server_connection* );
  virtual ~Executor();

  void set_interceptors(Interceptor* ic) { interceptors = ic; }

  //! Start method execution.
  virtual void execute( const Param_list& params ) = 0;

  //! Start method execution (move overload). Default forwards to const-ref version.
  virtual void execute( Param_list&& params ) { execute(params); }

protected:
  void schedule_response( const Response& );
  void interrupt_server();
  void log_err_msg( const std::string& msg );
  void set_connection_guard(ConnectionGuardPtr g) { conn_guard_ = std::move(g); }
};


//! Abstract base for Executor's factories.
class LIBIQXMLRPC_API Executor_factory_base {
public:
  virtual ~Executor_factory_base() = default;

  virtual Executor* create(
    Method*,
    Server*,
    Server_connection*
  ) = 0;

  virtual iqnet::Reactor_base* create_reactor() = 0;

  //! Wait for all in-flight executors to complete. No-op for serial execution.
  virtual void drain() {}
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
  void execute( Param_list&& ) override;
  void process_actual_execution();
};

//! Factory for Pool_executor objects. It is also serves as a pool of threads.
class LIBIQXMLRPC_API Pool_executor_factory: public Executor_factory_base {
  class Pool_thread;
  friend class Pool_thread;

  std::vector<std::thread>  threads;
  std::vector<std::unique_ptr<Pool_thread>> pool;

  // Lock-free work queue for reduced contention under high load
  static constexpr size_t QUEUE_CAPACITY = 1024;
  boost::lockfree::queue<Pool_executor*, boost::lockfree::capacity<QUEUE_CAPACITY>> req_queue;

  // Wait coordination (only for sleeping workers, not on hot path)
  std::mutex              wait_mutex;
  std::condition_variable wait_cond;
  std::atomic<size_t>     pending_count{0};

  std::atomic<bool> in_destructor;

  // In-flight executor tracking for drain().
  // drain() blocks until outstanding_count reaches zero, logging a warning
  // every drain_timeout_ interval. Default 30s; typical requests complete in <1s.
  static constexpr std::chrono::seconds DEFAULT_DRAIN_TIMEOUT{30};
  std::chrono::milliseconds drain_timeout_{DEFAULT_DRAIN_TIMEOUT};
  std::atomic<size_t>     outstanding_count{0};
  std::mutex              drain_mutex;
  std::condition_variable drain_cond;

public:
  explicit Pool_executor_factory(unsigned num_threads);
  ~Pool_executor_factory() override;

  Executor* create( Method* m, Server* s, Server_connection* c ) override;
  iqnet::Reactor_base* create_reactor() override;
  void drain() override;

  //! Add some threads to the pool.
  void add_threads(unsigned num);

  void register_executor( Pool_executor* );

  //! Set drain timeout. Must be called before server starts or between
  //! drain() calls. Not thread-safe with concurrent drain().
  void set_drain_timeout(std::chrono::milliseconds t) { drain_timeout_ = t; }

  //! Returns true if the pool factory is being destroyed.
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

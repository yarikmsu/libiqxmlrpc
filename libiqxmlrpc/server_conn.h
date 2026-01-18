//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef IQXMLRPC_SERVER_CONN_H
#define IQXMLRPC_SERVER_CONN_H

#include <chrono>
#include <mutex>
#include <optional>
#include <vector>
#include "connection.h"
#include "conn_factory.h"
#include "http.h"

namespace iqnet
{
  class Reactor_base;
}

namespace iqxmlrpc {

class Server;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

//! Base class for XML-RPC server connections.
class LIBIQXMLRPC_API Server_connection {
protected:
  iqnet::Inet_addr peer_addr;
  Server *server;
  http::Packet_reader preader;
  std::string response;
  size_t response_offset;  // Track send position instead of erasing
  bool keep_alive;

public:
  explicit Server_connection( const iqnet::Inet_addr& );
  virtual ~Server_connection() = 0;

  Server_connection(const Server_connection&) = delete;
  Server_connection& operator=(const Server_connection&) = delete;

  const iqnet::Inet_addr& get_peer_addr() const { return peer_addr; }

  void set_server( Server* s )
  {
    server = s;
  }

  void schedule_response( http::Packet* );

  //! Called when connection starts waiting for input (idle state).
  void start_idle();

  //! Called when connection stops waiting for input (processing request).
  void stop_idle();

  //! Check if connection is in idle state (waiting for input).
  bool is_idle() const {
    std::lock_guard<std::mutex> lock(idle_mutex_);
    return is_waiting_input_;
  }

  //! Check if idle timeout has expired.
  //! Returns true only if connection is idle AND timeout has exceeded.
  bool is_idle_timeout_expired(std::chrono::milliseconds timeout) const;

  //! Atomically check if still idle and mark as non-idle.
  //! Returns true if was idle (and should be terminated), false if no longer idle.
  //! This prevents TOCTOU race between checking idle state and terminating.
  bool try_claim_for_termination();

  //! Terminate this connection due to idle timeout.
  //! Called by the server when idle timeout expires.
  virtual void terminate_idle() = 0;

protected:
  http::Packet* read_request( const std::string& );

  char* read_buf() { return &read_buf_[0]; }
  size_t read_buf_sz() const { return read_buf_.size(); }

  virtual void do_schedule_response() = 0;

private:
  std::vector<char> read_buf_;

  // Mutex for idle state - provides defensive synchronization.
  // Currently all access is from the reactor thread, but mutex protects
  // against future changes or API misuse.
  mutable std::mutex idle_mutex_;
  bool is_waiting_input_ = false;
  std::optional<std::chrono::steady_clock::time_point> idle_since_;
};

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(disable: 4251)
#endif

//! Server connections factory.
template < class Transport >
class Server_conn_factory: public iqnet::Serial_conn_factory<Transport>
{
  Server* server;
  iqnet::Reactor_base* reactor;

public:
  Server_conn_factory():
    server(nullptr), reactor(nullptr) {}

  Server_conn_factory(const Server_conn_factory&) = delete;
  Server_conn_factory& operator=(const Server_conn_factory&) = delete;

  void post_init( Server* s, iqnet::Reactor_base* r )
  {
    server = s;
    reactor = r;
  }

  void post_create( Transport* c ) override
  {
    c->set_server( server );
    c->set_reactor( reactor );
  }
};

} // namespace iqxmlrpc

#endif

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include <memory>

#include "server_conn.h"
#include "auth_plugin.h"
#include "http_errors.h"
#include "server.h"

using namespace iqxmlrpc;

Server_connection::Server_connection( const iqnet::Inet_addr& a ):
  peer_addr(a),
  server(nullptr),
  preader(),
  response(),
  response_offset(0),
  keep_alive(false),
  read_buf_(65536, '\0'),
  idle_mutex_(),
  is_waiting_input_(false),
  idle_since_(std::nullopt)
{
}


Server_connection::~Server_connection()
{
}


http::Packet* Server_connection::read_request( const std::string& s )
{
  try
  {
    preader.set_verification_level( server->get_verification_level() );
    preader.set_max_size( server->get_max_request_sz() );
    http::Packet* r = preader.read_request(s);

    if( r ) {
      keep_alive = r->header()->conn_keep_alive();
    } else if( preader.expect_continue() ) {
      response = "HTTP/1.1 100\r\n\r\n";
      keep_alive = true;
      do_schedule_response();
      preader.set_continue_sent();
    }

    return r;
  }
  catch( const http::Malformed_packet& )
  {
    throw http::Bad_request();
  }
}


void Server_connection::schedule_response( http::Packet* pkt )
{
  std::unique_ptr<http::Packet> p(pkt);
  p->set_keep_alive( keep_alive );
  response = p->dump();
  response_offset = 0;  // Reset offset for new response
  do_schedule_response();
}


void Server_connection::start_idle()
{
  std::lock_guard<std::mutex> lock(idle_mutex_);
  is_waiting_input_ = true;
  idle_since_ = std::chrono::steady_clock::now();
}


void Server_connection::stop_idle()
{
  std::lock_guard<std::mutex> lock(idle_mutex_);
  is_waiting_input_ = false;
  idle_since_ = std::nullopt;
}


bool Server_connection::is_idle_timeout_expired(std::chrono::milliseconds timeout) const
{
  std::lock_guard<std::mutex> lock(idle_mutex_);
  if (!is_waiting_input_ || !idle_since_) {
    return false;
  }

  auto elapsed = std::chrono::steady_clock::now() - *idle_since_;
  return elapsed > timeout;
}


bool Server_connection::try_claim_for_termination()
{
  std::lock_guard<std::mutex> lock(idle_mutex_);
  if (!is_waiting_input_) {
    // Connection is no longer idle (received data since check)
    return false;
  }
  // Atomically mark as non-idle and claim for termination
  is_waiting_input_ = false;
  idle_since_ = std::nullopt;
  return true;
}

// vim:ts=2:sw=2:et

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#ifndef _libiqxmlrpc_firewall_h_
#define _libiqxmlrpc_firewall_h_

#include "api_export.h"
#include "inet_addr.h"

#include <memory>
#include <string>

namespace iqnet {

//! Firewall base class.
/*! Used by Acceptor to find out whether it should
    accept XML-RPC requests from specific IP.
*/
class LIBIQXMLRPC_API Firewall_base {
public:
  virtual ~Firewall_base() {}

  //! Must return bool to grant client to send request.
  virtual bool grant( const iqnet::Inet_addr& ) = 0;

  //! Called when a connection is closed (optional override).
  /*! Override this to track connection lifecycle for rate limiting. */
  virtual void release( const iqnet::Inet_addr& ) {}

  //! Override this method for custom good-bye message.
  /*! Return empty string for closing connection silently. */
  virtual std::string message()
  {
    return "HTTP/1.0 403 Forbidden\r\n";
  }
};

//! Rate-limiting firewall with per-IP and total connection limits.
/*! SECURITY: Prevents DoS attacks by limiting connections and request rates.
    - max_connections_per_ip: Limits connections from a single IP (default: 10)
    - max_connections_total: Limits total concurrent connections (default: 1000)
    - max_requests_per_second: Limits request rate per IP (default: 100)

    Usage:
    \code
    auto firewall = std::make_unique<RateLimitingFirewall>(10, 500);
    firewall->set_request_rate_limit(50);  // 50 requests/sec per IP
    server.set_firewall(firewall.release());
    \endcode

    Note: Call release() when connections close for accurate tracking.
    Note: Call check_request_allowed() before processing each request.

    IMPORTANT: For long-running servers, call cleanup_stale_entries() periodically
    (e.g., every 60 seconds) to remove rate limiter entries for inactive IPs.
    Without cleanup, memory usage grows with the number of unique client IPs.
*/
class LIBIQXMLRPC_API RateLimitingFirewall : public Firewall_base {
public:
  //! Create a rate-limiting firewall.
  /*! \param max_connections_per_ip Maximum connections per IP address (0 = unlimited)
      \param max_connections_total Maximum total connections (0 = unlimited)
  */
  explicit RateLimitingFirewall(
    size_t max_connections_per_ip = 10,
    size_t max_connections_total = 1000
  );

  ~RateLimitingFirewall() override;

  bool grant(const Inet_addr&) override;
  void release(const Inet_addr&) override;

  //! Get current connection count for an IP.
  size_t connections_from(const Inet_addr&) const;

  //! Get current total connection count.
  size_t total_connections() const;

  //! Set maximum requests per second per IP address.
  /*! SECURITY: Limits request rate to prevent application-level DoS.
      \param max_rps Maximum requests per second (0 = unlimited, default: 100)
  */
  void set_request_rate_limit(size_t max_rps);

  //! Check if a request from the given IP is allowed based on rate limits.
  /*! Call this before processing each request.
      \param addr Client IP address
      \return true if request is allowed, false if rate limit exceeded
  */
  bool check_request_allowed(const Inet_addr& addr);

  //! Get current request rate for an IP (requests in last second).
  size_t request_rate(const Inet_addr& addr) const;

  //! Clean up stale rate limiter entries to prevent memory growth.
  /*! Removes entries for IPs that have no recent requests.
      Call periodically (e.g., every minute) in long-running servers.
      \return Number of entries removed
  */
  size_t cleanup_stale_entries();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace iqnet

#endif

//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011 Anton Dedov

#include "firewall.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace iqnet {

// Request timestamp tracking for rate limiting
struct RequestTracker {
  // Mutable to allow cleanup in const methods (count_recent modifies deque)
  mutable std::deque<std::chrono::steady_clock::time_point> timestamps;

  // NOLINTNEXTLINE(modernize-use-equals-default) - explicit init required for -Weffc++
  RequestTracker() : timestamps() {}

  void add_request() {
    timestamps.push_back(std::chrono::steady_clock::now());
  }

  size_t count_recent(std::chrono::seconds window = std::chrono::seconds(1)) const {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - window;

    // Remove old timestamps
    while (!timestamps.empty() && timestamps.front() < cutoff) {
      timestamps.pop_front();
    }

    return timestamps.size();
  }
};

struct RateLimitingFirewall::Impl {
  size_t max_per_ip;
  size_t max_total;
  std::atomic<size_t> max_rps;  // Maximum requests per second per IP (atomic for thread-safe config)

  mutable std::mutex map_mutex;
  std::unordered_map<std::string, size_t> ip_counts;
  std::atomic<size_t> total_count;

  mutable std::mutex rate_mutex;
  std::unordered_map<std::string, RequestTracker> request_trackers;

  Impl(size_t per_ip, size_t total)
    : max_per_ip(per_ip)
    , max_total(total)
    , max_rps(100)  // Default: 100 requests/sec per IP
    , map_mutex()
    , ip_counts()
    , total_count(0)
    , rate_mutex()
    , request_trackers()
  {}
};

RateLimitingFirewall::RateLimitingFirewall(
  size_t max_connections_per_ip,
  size_t max_connections_total
)
  : impl_(std::make_unique<Impl>(max_connections_per_ip, max_connections_total))
{
}

RateLimitingFirewall::~RateLimitingFirewall() = default;

bool RateLimitingFirewall::grant(const Inet_addr& addr)
{
  // Check total limit first (fast path without lock)
  if (impl_->max_total > 0 && impl_->total_count.load() >= impl_->max_total) {
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->map_mutex);

  // Re-check total under lock to avoid race
  if (impl_->max_total > 0 && impl_->total_count.load() >= impl_->max_total) {
    return false;
  }

  // Check per-IP limit
  if (impl_->max_per_ip > 0) {
    const std::string& ip = addr.get_host_name();
    auto it = impl_->ip_counts.find(ip);
    size_t current = (it != impl_->ip_counts.end()) ? it->second : 0;

    if (current >= impl_->max_per_ip) {
      return false;
    }

    impl_->ip_counts[ip] = current + 1;
  }

  impl_->total_count.fetch_add(1);
  return true;
}

void RateLimitingFirewall::release(const Inet_addr& addr)
{
  const std::string& ip = addr.get_host_name();

  std::lock_guard<std::mutex> lock(impl_->map_mutex);

  auto it = impl_->ip_counts.find(ip);
  if (it != impl_->ip_counts.end()) {
    if (it->second > 1) {
      it->second--;
    } else {
      impl_->ip_counts.erase(it);
    }
  }

  // Decrement total, but don't go below zero
  size_t expected = impl_->total_count.load();
  while (expected > 0 &&
         !impl_->total_count.compare_exchange_weak(expected, expected - 1)) {
    // Retry until successful or zero
  }
}

size_t RateLimitingFirewall::connections_from(const Inet_addr& addr) const
{
  const std::string& ip = addr.get_host_name();

  std::lock_guard<std::mutex> lock(impl_->map_mutex);
  auto it = impl_->ip_counts.find(ip);
  return (it != impl_->ip_counts.end()) ? it->second : 0;
}

size_t RateLimitingFirewall::total_connections() const
{
  return impl_->total_count.load();
}

void RateLimitingFirewall::set_request_rate_limit(size_t max_rps)
{
  impl_->max_rps.store(max_rps, std::memory_order_relaxed);
}

bool RateLimitingFirewall::check_request_allowed(const Inet_addr& addr)
{
  size_t limit = impl_->max_rps.load(std::memory_order_relaxed);
  if (limit == 0) {
    return true;  // No rate limit
  }

  const std::string& ip = addr.get_host_name();

  std::lock_guard<std::mutex> lock(impl_->rate_mutex);

  auto& tracker = impl_->request_trackers[ip];
  size_t recent = tracker.count_recent();

  if (recent >= limit) {
    return false;  // Rate limit exceeded
  }

  tracker.add_request();
  return true;
}

size_t RateLimitingFirewall::request_rate(const Inet_addr& addr) const
{
  const std::string& ip = addr.get_host_name();

  std::lock_guard<std::mutex> lock(impl_->rate_mutex);

  auto it = impl_->request_trackers.find(ip);
  if (it == impl_->request_trackers.end()) {
    return 0;
  }

  return it->second.count_recent();
}

size_t RateLimitingFirewall::cleanup_stale_entries()
{
  std::lock_guard<std::mutex> lock(impl_->rate_mutex);

  size_t removed = 0;
  auto it = impl_->request_trackers.begin();
  while (it != impl_->request_trackers.end()) {
    // count_recent() cleans up old timestamps and returns current count
    if (it->second.count_recent() == 0) {
      it = impl_->request_trackers.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }

  return removed;
}

} // namespace iqnet

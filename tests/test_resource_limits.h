#ifndef IQXMLRPC_TEST_RESOURCE_LIMITS_H
#define IQXMLRPC_TEST_RESOURCE_LIMITS_H

#include <algorithm>
#include <cstdlib>

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

namespace iqxmlrpc_test {

constexpr size_t kMinFdLimitForIntegration = 512;
constexpr const char* kDisableKeepAliveEnv = "IQXMLRPC_TEST_DISABLE_KEEP_ALIVE";
constexpr const char* kForceKeepAliveEnv = "IQXMLRPC_TEST_FORCE_KEEP_ALIVE";
constexpr const char* kMinFdLimitEnv = "IQXMLRPC_TEST_MIN_FD";
constexpr const char* kForcePoolThreadsEnv = "IQXMLRPC_TEST_FORCE_POOL_THREADS";
constexpr const char* kForceThreadCountEnv = "IQXMLRPC_TEST_FORCE_THREAD_COUNT";

inline bool env_flag_set(const char* name) {
  const char* value = std::getenv(name);  // NOLINT(concurrency-mt-unsafe)
  if (!value || value[0] == '\0') {
    return false;
  }
  const char c = value[0];
  return c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T';
}

inline size_t env_fd_limit(size_t fallback) {
  const char* value = std::getenv(kMinFdLimitEnv);  // NOLINT(concurrency-mt-unsafe)
  if (!value) {
    return fallback;
  }
  char* end = nullptr;
  long parsed = std::strtol(value, &end, 10);
  if (end == value || *end != '\0' || parsed <= 0) {
    return fallback;
  }
  return static_cast<size_t>(parsed);
}

inline int env_thread_override() {
  const char* value = std::getenv(kForceThreadCountEnv);  // NOLINT(concurrency-mt-unsafe)
  if (!value) {
    return -1;
  }
  char* end = nullptr;
  long parsed = std::strtol(value, &end, 10);
  if (end == value || *end != '\0' || parsed <= 0) {
    return -1;
  }
  return static_cast<int>(parsed);
}

inline size_t get_fd_soft_limit() {
#if defined(_WIN32)
  return 0;
#else
  struct rlimit limit;
  if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
    return 0;
  }
  return static_cast<size_t>(limit.rlim_cur);
#endif
}

inline size_t get_fd_hard_limit() {
#if defined(_WIN32)
  return 0;
#else
  struct rlimit limit;
  if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
    return 0;
  }
  return static_cast<size_t>(limit.rlim_max);
#endif
}

inline void ensure_fd_limit(size_t min_limit = kMinFdLimitForIntegration) {
  min_limit = env_fd_limit(min_limit);
#if !defined(_WIN32)
  const size_t soft = get_fd_soft_limit();
  if (soft == 0 || soft >= min_limit) {
    return;
  }
  const size_t hard = get_fd_hard_limit();
  if (hard == 0) {
    return;
  }
  const size_t target = std::min(min_limit, hard);
  struct rlimit limit;
  limit.rlim_cur = static_cast<rlim_t>(target);
  limit.rlim_max = static_cast<rlim_t>(hard);
  (void)setrlimit(RLIMIT_NOFILE, &limit);
#endif
}

inline bool should_disable_keep_alive(size_t min_limit = kMinFdLimitForIntegration) {
  if (env_flag_set(kForceKeepAliveEnv)) {
    return false;
  }
  if (env_flag_set(kDisableKeepAliveEnv)) {
    return true;
  }
  min_limit = env_fd_limit(min_limit);
  const size_t limit = get_fd_soft_limit();
  return limit != 0 && limit < min_limit;
}

inline int tuned_thread_count(int requested, size_t per_thread_budget = 32) {
  const int override_threads = env_thread_override();
  if (override_threads > 0) {
    return override_threads;
  }
  if (env_flag_set(kForcePoolThreadsEnv)) {
    return requested;
  }
  if (requested <= 1) {
    return requested;
  }
  const size_t limit = get_fd_soft_limit();
  if (limit == 0 || per_thread_budget == 0) {
    return requested;
  }
  const size_t max_threads = std::max<size_t>(1, limit / per_thread_budget);
  return static_cast<int>(std::min<size_t>(max_threads, static_cast<size_t>(requested)));
}

} // namespace iqxmlrpc_test

#endif // IQXMLRPC_TEST_RESOURCE_LIMITS_H

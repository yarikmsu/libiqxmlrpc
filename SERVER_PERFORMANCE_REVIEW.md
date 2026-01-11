# Server Performance Improvement Plan: libiqxmlrpc

**Created:** 2026-01-10
**Last Updated:** 2026-01-11
**Scope:** Library-side optimizations only (C++ code changes)

## Revision History

| Date | PR | Change |
|------|-----|--------|
| 2026-01-11 | #60 | P1b: TLS cipher optimization with AES-NI |
| 2026-01-11 | #59 | P1a: TLS session caching enabled |
| 2026-01-10 | - | Initial plan created |

---

## Executive Summary

This plan identifies performance improvements that can be implemented within the libiqxmlrpc library to reduce latency and improve throughput for HTTPS XML-RPC servers.

---

## 1. TLS Session Resumption (High Impact) ✅ Done (PR #59)

**Status:** ✅ Implemented and merged in PR #59

**Previous State:** Library didn't configure TLS session caching. Each new connection required full TLS handshake (~100-200ms).

**Location:** `libiqxmlrpc/ssl_lib.cc`

**Proposed Change:**
```cpp
// Add to Ctx::Ctx() or Ctx::client_server() after SSL_CTX creation
SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
SSL_CTX_sess_set_cache_size(ctx, 1024);  // Cache up to 1024 sessions
SSL_CTX_set_timeout(ctx, 300);           // 5 minute session lifetime
```

**Impact:**
- Resumed TLS handshake: ~1 round-trip vs 2+ for full handshake
- **20-30% reduction** in handshake time for returning clients

**Effort:** Low (10 lines of code)

---

## 2. TLS Cipher Suite Optimization (Medium Impact) ✅ Done (PR #60)

**Status:** ✅ Implemented and merged in PR #60

**Previous State:** Used OpenSSL defaults, which may include slower ciphers.

**Location:** `libiqxmlrpc/ssl_lib.cc`

**Implemented Change (PR #60):**
```cpp
// Server-only cipher configuration - applied to server contexts only
// (client contexts use OpenSSL defaults to avoid restricting outbound connections)

void set_server_cipher_options(SSL_CTX* ctx) {
    // TLS 1.2 - prefer hardware-accelerated ciphers
    int ret = SSL_CTX_set_cipher_list(ctx,
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        ...);

    if (ret == 0) {
        ERR_clear_error();  // Fallback to defaults
    }

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    // TLS 1.3 ciphersuites
    SSL_CTX_set_ciphersuites(ctx, "TLS_AES_128_GCM_SHA256:...");
#endif

    SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
}
```

**Benchmark Results:**
- AES-128-GCM: ~9,300 ns/64KB (6.8 GB/s)
- AES-256-GCM: ~11,000 ns/64KB (5.8 GB/s)
- ChaCha20-Poly1305: ~44,000 ns/64KB (1.5 GB/s)
- AES-256-CBC: ~58,000 ns/64KB (1.1 GB/s)

**Impact:**
- AES-GCM uses hardware AES-NI on modern CPUs
- **5-6x faster** encryption throughput vs CBC mode

**Effort:** Low (20 lines of code)

---

## 3. Connection Idle Timeout (Medium Impact)

**Current State:** No idle timeout - zombie connections can accumulate indefinitely.

**Location:** `libiqxmlrpc/server_conn.h`, `libiqxmlrpc/http_server.cc`

**Proposed Change:**

Add idle timeout tracking to `Server_connection`:
```cpp
// In server_conn.h
class Server_connection {
protected:
    std::chrono::steady_clock::time_point last_activity_;
    static constexpr std::chrono::seconds IDLE_TIMEOUT{60};

public:
    void touch() { last_activity_ = std::chrono::steady_clock::now(); }
    bool is_idle() const {
        return (std::chrono::steady_clock::now() - last_activity_) > IDLE_TIMEOUT;
    }
};
```

Add periodic idle check in reactor or use `SO_RCVTIMEO`/`SO_SNDTIMEO` socket options.

**Impact:**
- Prevents resource exhaustion from abandoned connections
- Frees file descriptors for new connections

**Effort:** Medium (requires reactor integration)

---

## 4. Configurable Read Buffer Size (Low Impact)

**Current State:** Fixed 64KB read buffer per connection (`server_conn.cc:20`).

**Location:** `libiqxmlrpc/server_conn.cc`

**Current Code:**
```cpp
read_buf_(65536, '\0')  // 64KB per connection
```

**Proposed Change:**
```cpp
// Make configurable, start smaller, grow on demand
static constexpr size_t INITIAL_BUFFER_SIZE = 4096;   // 4KB initial
static constexpr size_t MAX_BUFFER_SIZE = 65536;      // 64KB max

// Grow buffer when needed
void ensure_buffer_capacity(size_t needed) {
    if (read_buf_.size() < needed && read_buf_.size() < MAX_BUFFER_SIZE) {
        read_buf_.resize(std::min(needed, MAX_BUFFER_SIZE));
    }
}
```

**Impact:**
- Memory reduction: 60KB saved per connection for small requests
- With 100 connections: **6MB memory savings**

**Effort:** Medium

---

## 5. Request Size Limit (Already Implemented)

**Current State:** Available via `Server::set_max_request_sz()`

**Location:** `libiqxmlrpc/server.cc:121-124`

**Usage:**
```cpp
server.set_max_request_sz(1024 * 1024);  // 1MB limit
```

**Documentation:** Add to README/examples that this should be configured for DoS protection.

**Effort:** None (documentation only)

---

## 6. Improve Exception Flow in SSL Path (Medium Impact)

**Current State:** Uses exceptions for normal TLS state transitions (`ssl_lib.cc:330-334`).

**Location:** `libiqxmlrpc/ssl_lib.cc`

**Current Code:**
```cpp
case SSL_ERROR_WANT_READ:
    throw need_read();
case SSL_ERROR_WANT_WRITE:
    throw need_write();
```

**Problem:** Exception throwing for normal flow has ~5-10% CPU overhead.

**Proposed Change:** Return error codes instead of throwing for expected states:
```cpp
enum class SslResult { OK, WANT_READ, WANT_WRITE, ERROR };

SslResult Ctx::handle_ssl_result(SSL* ssl, int ret) {
    int err = SSL_get_error(ssl, ret);
    switch (err) {
        case SSL_ERROR_NONE: return SslResult::OK;
        case SSL_ERROR_WANT_READ: return SslResult::WANT_READ;
        case SSL_ERROR_WANT_WRITE: return SslResult::WANT_WRITE;
        default: return SslResult::ERROR;
    }
}
```

**Impact:**
- **5-10% CPU reduction** in SSL hot path
- Cleaner code flow

**Effort:** High (requires refactoring ssl_connection.cc state machine)

---

## Implementation Priority

| Priority | Item | Effort | Impact | Status |
|----------|------|--------|--------|--------|
| ~~**P1**~~ | ~~TLS session caching~~ | ~~Low~~ | ~~High~~ | ✅ Done (PR #59) |
| ~~**P1**~~ | ~~TLS cipher optimization~~ | ~~Low~~ | ~~Medium~~ | ✅ Done (PR #60) |
| **P2** | Connection idle timeout | Medium | Medium | Pending |
| **P2** | Configurable buffer size | Medium | Low | Pending |
| **P3** | Exception-free SSL flow | High | Medium | Pending |

---

## Benchmarks Required

Before implementing, add benchmarks to `tests/test_performance.cc`:

```cpp
// TLS handshake performance
PERF_BENCHMARK("perf_tls_handshake_full", 100, {
    // Measure full TLS handshake time
});

PERF_BENCHMARK("perf_tls_handshake_resumed", 100, {
    // Measure resumed TLS handshake time (after session caching)
});

// SSL read/write performance
PERF_BENCHMARK("perf_ssl_write_1kb", ITERS, {
    // Measure SSL_write for 1KB payload
});

PERF_BENCHMARK("perf_ssl_write_100kb", ITERS, {
    // Measure SSL_write for 100KB payload
});
```

---

## ~~Quick Implementation: TLS Session Caching~~ ✅ Completed

~~This is the highest-impact, lowest-effort change.~~

**Status:** ✅ Both P1 optimizations have been implemented and merged.

**PR #59:** TLS session caching - Adds server-side session cache (1024 sessions, 5-minute lifetime)

**PR #60:** TLS cipher optimization - Prefers AES-GCM ciphers with AES-NI acceleration (server contexts only)

**Actual Implementation in `ssl_lib.cc`:**

```cpp
// TLS session caching (PR #59) - in server context constructors
SSL_CTX_set_session_cache_mode(impl_->ctx, SSL_SESS_CACHE_SERVER);
SSL_CTX_sess_set_cache_size(impl_->ctx, 1024);
SSL_CTX_set_timeout(impl_->ctx, 300);

// TLS cipher optimization (PR #60) - server-only function
void set_server_cipher_options(SSL_CTX* ctx) {
    // TLS 1.2 cipher list - prefers hardware-accelerated AES-GCM
    int ret = SSL_CTX_set_cipher_list(ctx,
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305");

    if (ret == 0) {
        ERR_clear_error();  // Fallback to defaults if rejected
    }

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    // TLS 1.3 ciphersuites (OpenSSL 1.1.1+)
    SSL_CTX_set_ciphersuites(ctx,
        "TLS_AES_128_GCM_SHA256:"
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256");
#endif

    SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
}
```

---

## Summary

| Change | Expected Improvement | Status |
|--------|---------------------|--------|
| TLS session caching | 20-30% faster reconnects | ✅ Done (PR #59) |
| Cipher optimization | 10-30% faster encryption | ✅ Done (PR #60) |
| Idle timeout | Prevents resource exhaustion | Pending |
| Buffer optimization | 60KB/connection memory savings | Pending |
| Exception-free SSL | 5-10% CPU reduction | Pending |

**Progress:** 2/5 optimizations completed

**P1 Results (TLS Optimizations):**
- PR #59 added TLS session caching for returning client speedup
- PR #60 added cipher optimization with AES-NI hardware acceleration
- Cipher throughput benchmark shows AES-128-GCM is 5-6x faster than AES-256-CBC

**Remaining potential improvement (P2-P3):** 10-20% additional latency reduction for HTTPS workloads.

# Hardening Guide

Production deployment guide for securing libiqxmlrpc servers and clients.

## Overview

Out of the box, libiqxmlrpc defaults to permissive settings for backward compatibility. This guide covers every security knob and provides complete, production-ready examples.

**Key defaults that need attention:**
- `max_request_sz` = 0 (unlimited request bodies)
- `idle_timeout` = 0 (connections held forever)
- `max_response_sz` = 0 (unlimited client-side response buffering)
- The legacy `Ctx::client_only()` factory does not verify server certificates

---

## Server Limits

### Request Size Limit

Prevents memory exhaustion from oversized request bodies (Finding #8).

```cpp
server.set_max_request_sz(10 * 1024 * 1024);  // 10 MB
```

**Recommendation:** 10 MB covers most XML-RPC payloads. Set lower if your methods only accept small parameters.

**Memory note:** XML parsing temporarily requires ~2x the value size in memory
(libxml2 internal buffer + application string). A 100 MB request body may use
~200 MB of heap during parsing. Factor this into your `max_request_sz` setting
relative to available server memory.

### Idle Connection Timeout

Prevents Slow Loris attacks — connections that send data slowly to hold server resources (Finding #8).

```cpp
using namespace std::chrono_literals;
server.set_idle_timeout(30s);  // 30 seconds
```

**Recommendation:** 30 seconds is appropriate for most deployments. Increase for slow networks or large file transfers.

---

## Client Limits

### Response Size Limit

Prevents a malicious (or misconfigured) server from causing out-of-memory on the client (Finding #8).

```cpp
client->set_max_response_sz(10 * 1024 * 1024);  // 10 MB
```

Throws `http::Response_too_large` if the response exceeds the limit. Set to 0 (default) for unlimited.

**Proxy caveat:** When using an HTTPS proxy (`set_proxy()`), the response size limit also applies to the proxy CONNECT handshake response. Avoid setting the limit below ~1 KB when using proxies, as the CONNECT response headers alone may be several hundred bytes.

```cpp
try {
    Response r = client->execute("my_method", params);
} catch (const http::Response_too_large&) {
    // Server sent a response larger than our limit
}
```

### Connection Timeout

Prevents the client from blocking indefinitely on unresponsive servers.

```cpp
client->set_timeout(30);  // 30 seconds
```

Throws `Client_timeout` on expiry.

---

## TLS Configuration

### Server TLS

Use `Ctx::server_only()` (server certificate only) or `Ctx::client_server()` (mutual TLS):

```cpp
// Server-only TLS (most common)
iqnet::ssl::ctx = iqnet::ssl::Ctx::server_only(
    "/path/to/cert.pem", "/path/to/key.pem");

// Mutual TLS (clients must present certificates)
iqnet::ssl::ctx = iqnet::ssl::Ctx::client_server(
    "/path/to/cert.pem", "/path/to/key.pem");
```

### Client TLS

Use `Ctx::client_verified()` for production clients — it loads system CA certificates and enables `SSL_VERIFY_PEER`:

```cpp
iqnet::ssl::ctx = iqnet::ssl::Ctx::client_verified();
```

For per-connection hostname verification:

```cpp
client->set_expected_hostname("api.example.com");
```

**Warning:** The legacy `Ctx::client_only()` does **not** verify server certificates (Finding #1). Only use it for testing.

---

## Authentication

### Basic Auth Plugin

Implement `Auth_Plugin_base` and use `constant_time_compare()` for credential checks:

```cpp
class MyAuth : public iqxmlrpc::Auth_Plugin_base {
    bool do_authenticate(
        const std::string& user,
        const std::string& password) const override
    {
        std::string stored_hash = lookup_password_hash(user);
        std::string provided_hash = hash(password);
        return iqxmlrpc::constant_time_compare(stored_hash, provided_hash);
    }

    bool do_authenticate_anonymous() const override {
        return false;  // Require authentication
    }
};

MyAuth auth;
server.set_auth_plugin(auth);
```

**Warning:** HTTP Basic auth transmits credentials in Base64 (not encrypted). Always use HTTPS when authentication is enabled (Finding #11).

### Enforce TLS for Authentication (Recommended)

Call `require_tls_for_auth()` before `set_auth_plugin()` to ensure
auth is never accidentally configured on a plain HTTP server:

```cpp
iqxmlrpc::Https_server server(addr, &ef);
server.require_tls_for_auth();    // require TLS for set_auth_plugin()
server.set_auth_plugin(my_auth);  // safe — server is HTTPS
```

Without this call, `set_auth_plugin()` works on any server type for
backward compatibility.

### Client-Side Auth

```cpp
client->set_authinfo("username", "password");
```

---

## Firewall & Rate Limiting

### Rate-Limiting Firewall

Limits concurrent connections per IP address:

```cpp
// Max 10 connections per IP, 500 total connections
auto* fw = new iqnet::RateLimitingFirewall(10, 500);
server.set_firewall(fw);
```

### Custom Firewall

Implement `iqnet::Firewall_base` for IP allowlisting:

```cpp
class AllowlistFirewall : public iqnet::Firewall_base {
    std::set<std::string> allowed_;
public:
    explicit AllowlistFirewall(std::set<std::string> ips)
        : allowed_(std::move(ips)) {}

    bool grant(const iqnet::Inet_addr& addr) override {
        return allowed_.count(addr.get_host_name()) > 0;
    }
};

server.set_firewall(
    new AllowlistFirewall({"192.168.1.0", "10.0.0.1"}));
```

---

## HTTP Hardening

### Strict Header Validation

Rejects malformed HTTP headers and unsupported content types:

```cpp
server.set_verification_level(iqxmlrpc::http::HTTP_CHECK_STRICT);
```

### Server Version Disclosure

Hide the `Server:` header to prevent fingerprinting:

```cpp
iqxmlrpc::http::Header::hide_server_version(true);
```

Or set a custom value:

```cpp
iqxmlrpc::http::Header::set_server_header("MyService");
```

### Security Headers

Enable HSTS for HTTPS servers:

```cpp
iqxmlrpc::http::Header::enable_hsts(true, 31536000);  // 1 year
```

Set Content Security Policy:

```cpp
iqxmlrpc::http::Header::set_content_security_policy("default-src 'none'");
```

**Note:** `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, and `Cache-Control: no-store` are set automatically on all responses.

---

## Complete Server Example

```cpp
#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/https_server.h"
#include "libiqxmlrpc/executor.h"
#include "libiqxmlrpc/auth_plugin.h"
#include "libiqxmlrpc/firewall.h"
#include "libiqxmlrpc/ssl_lib.h"

using namespace std::chrono_literals;

int main() {
    // --- TLS ---
    iqnet::ssl::ctx = iqnet::ssl::Ctx::server_only(
        "/etc/myservice/cert.pem",
        "/etc/myservice/key.pem");

    // --- HTTP Hardening ---
    iqxmlrpc::http::Header::hide_server_version(true);
    iqxmlrpc::http::Header::enable_hsts(true);
    iqxmlrpc::http::Header::set_content_security_policy(
        "default-src 'none'");

    // --- Server Setup ---
    iqxmlrpc::Pool_executor_factory exec_factory(4);
    iqxmlrpc::Https_server server(
        iqnet::Inet_addr("0.0.0.0", 8443), &exec_factory);

    // --- Resource Limits ---
    server.set_max_request_sz(10 * 1024 * 1024);   // 10 MB
    server.set_idle_timeout(30s);                    // 30s
    server.set_verification_level(
        iqxmlrpc::http::HTTP_CHECK_STRICT);

    // --- Authentication ---
    server.require_tls_for_auth();  // reject auth plugin on non-HTTPS
    // MyAuth auth;
    // server.set_auth_plugin(auth);

    // --- Rate Limiting ---
    // Max 10 connections per IP, 500 total connections
    server.set_firewall(
        new iqnet::RateLimitingFirewall(10, 500));

    // --- Register Methods ---
    // iqxmlrpc::register_method(server, "my.method", handler);

    server.work();
    return 0;
}
```

## Complete Client Example

```cpp
#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/https_client.h"
#include "libiqxmlrpc/ssl_lib.h"

int main() {
    // --- TLS with Certificate Verification ---
    iqnet::ssl::ctx = iqnet::ssl::Ctx::client_verified();

    // --- Client Setup ---
    iqxmlrpc::Client<iqxmlrpc::Https_client_connection> client(
        iqnet::Inet_addr("api.example.com", 8443));

    // --- Security Settings ---
    client.set_expected_hostname("api.example.com");
    client.set_timeout(30);
    client.set_max_response_sz(10 * 1024 * 1024);  // 10 MB
    client.set_authinfo("user", "password");

    // --- RPC Call ---
    try {
        iqxmlrpc::Response r = client.execute(
            "my.method", iqxmlrpc::Value("data"));
        if (!r.is_fault()) {
            // Process response
        }
    } catch (const iqxmlrpc::http::Response_too_large&) {
        // Response exceeded size limit
    } catch (const iqxmlrpc::Client_timeout&) {
        // Connection timed out
    }

    return 0;
}
```

---

## Quick Reference

| Setting | Server | Client | Default | Recommended |
|---------|--------|--------|---------|-------------|
| Max request size | `set_max_request_sz()` | — | 0 (unlimited) | 10 MB |
| Max response size | — | `set_max_response_sz()` | 0 (unlimited) | 10 MB |
| Idle timeout | `set_idle_timeout()` | — | 0 (none) | 30s |
| Connection timeout | — | `set_timeout()` | -1 (infinite) | 30s |
| TLS context | `Ctx::server_only()` | `Ctx::client_verified()` | `client_only()` | See above |
| Hostname verify | — | `set_expected_hostname()` | — | Set always |
| HTTP strictness | `set_verification_level()` | — | `WEAK` | `STRICT` |
| Rate limiting | `set_firewall()` | — | None | 10 conn/IP |
| Server header | `hide_server_version()` | — | Shown | Hidden |
| HSTS | `enable_hsts()` | — | Off | On (HTTPS) |

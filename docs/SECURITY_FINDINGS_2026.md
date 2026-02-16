# Zero-Day Vulnerability Hunt Report: libiqxmlrpc

**Date:** 2026-02-07
**Scope:** Full codebase audit across 4 attack surfaces (XML parsing, HTTP layer, memory safety/concurrency, SSL/TLS/authentication)
**Findings:** 18 total — 1 Critical, 4 High, 6 Medium, 7 Low

---

## CRITICAL (1)

### #1: Client-Side TLS Certificate Validation Disabled by Default

- **Category:** SSL/TLS — Improper Certificate Validation
- **Affected:** `libiqxmlrpc/ssl_lib.cc:226-232` (constructor), `ssl_lib.cc:256-271` (`prepare_verify()`)
- **Description:** The `Ctx::Ctx()` client-only constructor never loads CA certificates (`SSL_CTX_set_default_verify_paths()` is never called) and never enables peer verification. When no custom `server_verifier` is set (the default), `prepare_verify()` sets `SSL_VERIFY_NONE`, meaning the client accepts **any** certificate — self-signed, expired, wrong hostname. Although `hostname_verification` defaults to `true`, OpenSSL's hostname check is only enforced when `SSL_VERIFY_PEER` is active; with `VERIFY_NONE` it is a no-op.
- **Impact:** A MITM attacker presents any certificate and intercepts all XML-RPC traffic including authentication credentials.
- **Recommendation:** Load system CA store by default in `client_only()` constructor; set `SSL_VERIFY_PEER` as the default mode. Provide `set_verify_peer(false)` opt-out for legacy self-signed setups.

---

## HIGH (4)

### #2: Use-After-Free — Pool_executor Accesses Connection After Deletion

- **Category:** Memory Safety — Use-After-Free (CWE-416)
- **Affected:** `libiqxmlrpc/executor.cc:253-271`, `libiqxmlrpc/executor.h:57-58`
- **Description:** `Pool_executor` stores raw `Server_connection* conn` and `Server* server` pointers. Between enqueue into the lock-free work queue and dequeue by the pool thread, the reactor thread can close and `delete` the connection (client disconnect triggers `finish()` → `delete this`). The pool thread then calls `schedule_response()` which dereferences the dangling pointer. The destructor path also has a related issue: `Pool_executor::~Pool_executor()` calls `interrupt_server()` which accesses `server` — if the server is destroyed before all queued executors finish, this is also use-after-free.
- **Impact:** Crash or remote code execution via heap corruption from dangling pointer dereference.
- **Recommendation:** Use `shared_ptr` or weak reference with validity check for connection lifetime management.

### #3: Race Condition on Global SSL Context Hostname

- **Category:** Concurrency — TOCTOU Race (CWE-367)
- **Affected:** `libiqxmlrpc/ssl_lib.cc:310-330`, `libiqxmlrpc/ssl_connection.cc:11-14`, `libiqxmlrpc/ssl_lib.cc:27` (global `ctx` pointer)
- **Description:** The SSL context `iqnet::ssl::ctx` is a global singleton. All connections share it. `expected_hostname` is stored in the shared `Ctx::Impl`. Concurrent client connections to different hosts race on `set_expected_hostname()` / `prepare_hostname_verify()`, causing hostname verification against the wrong domain. Thread A sets "host-a.com", Thread B sets "host-b.com", Thread A verifies against "host-b.com".
- **Impact:** MITM via hostname verification bypass in multi-threaded clients connecting to different hosts.
- **Recommendation:** Move `expected_hostname` to per-connection state rather than shared global context.

### #4: Race Condition on Global Mutable ValueOptions

- **Category:** Concurrency — Data Race (CWE-362)
- **Affected:** `libiqxmlrpc/value.cc:20-24`
- **Description:** Global non-atomic variables (`default_int`, `default_int64`, `omit_string_tag_in_responses`) are read/written from multiple threads without synchronization. Pool executor threads read these during serialization while any thread can write them via `Value::set_default_int()`, `Value::omit_string_tag_in_responses(bool)`, etc.
- **Impact:** Undefined behavior per C++ standard (data race on non-atomic types). Practically: corrupted output or crash.
- **Recommendation:** Make these `std::atomic` or protect with a mutex; alternatively, make them truly immutable after server startup.

### #5: No Method-Level Authorization — WON'T FIX (by design)

- **Category:** Authorization — Missing Function-Level Access Control (OWASP A01:2021)
- **Affected:** `libiqxmlrpc/server.cc:264-317`
- **Description:** Authentication is binary pass/fail. After auth succeeds, any authenticated user can call **any** registered method. No role/permission system exists. `system.listMethods` reveals all available methods for reconnaissance. The `Method` base class has `authname()` accessor but it is informational only — the method has already been dispatched.
- **Impact:** Privilege escalation — any authenticated user can access admin methods.
- **Resolution:** Won't fix. Method-level authorization is the responsibility of the application that uses the library, not the library itself. The library provides authentication infrastructure (`auth_plugin`, `authname()` accessor); applications implement authorization logic within their `Method` subclasses using `authname()` to make per-method access decisions.

---

## MEDIUM (6)

### #6: HTTP Request Smuggling via Missing Transfer-Encoding Handling

- **Category:** HTTP — Request Smuggling (CWE-444)
- **Affected:** `libiqxmlrpc/http.cc:202-212`
- **Description:** The server does not recognize `Transfer-Encoding: chunked`. No validator is registered for it. Behind a reverse proxy (nginx, HAProxy), an attacker sends both `Transfer-Encoding: chunked` and `Content-Length`. The proxy uses TE, the library uses CL, creating a classic CL.TE desynchronization. The fuzz corpus already contains test cases for this (`fuzz/corpus/http/smuggle_te_cl.txt`), confirming it is a known concern.
- **Impact:** Request smuggling behind reverse proxy deployments.
- **Recommendation:** Reject requests containing `Transfer-Encoding` header, or implement chunked transfer decoding.

### #7: Integer Truncation — `size_t` to `int` on XML Buffer

- **Category:** Integer Handling — Integer Truncation (CWE-197)
- **Affected:** `libiqxmlrpc/parser2.cc:144`
- **Description:** `static_cast<int>(str.size())` truncates 64-bit `size_t` to 32-bit `int`. With `max_req_sz` defaulting to 0 (unlimited), a >2GB payload reaches the parser. libxml2's `xmlReaderForMemory` receives a truncated (possibly negative) size, parsing only a fragment. An attacker crafts a payload where the first INT_MAX bytes are benign XML but malicious content lies after the truncation boundary.
- **Impact:** Parser sees truncated input; trailing malicious content is silently ignored, or undefined behavior if truncated value is negative.
- **Recommendation:** Add an explicit check that `str.size() <= INT_MAX` before casting; return error if exceeded.

### #8: Memory Exhaustion — No Default Size Limits

- **Category:** Denial of Service — Resource Exhaustion (CWE-400)
- **Affected:** `libiqxmlrpc/server.cc:71` (`max_req_sz=0`), `libiqxmlrpc/server.cc:76` (`idle_timeout_ms=0`), `libiqxmlrpc/client_conn.cc:12`
- **Description:** Three related sub-issues:
  - **Server:** `max_req_sz` defaults to 0 — unlimited request body buffering. An attacker sends `Content-Length: 2147483647` and the server buffers it all.
  - **Client:** No API to set max response size — unlimited response buffering from a malicious server.
  - **Slow Loris:** `idle_timeout_ms` defaults to 0 — connections held indefinitely in header-reading state.
  - Combined with `XML_PARSE_HUGE` (parser2.cc:148) and 10M element limit, ~4-5x memory amplification is achievable (200MB wire → ~1GB heap).
- **Impact:** OOM denial of service from a single connection.
- **Recommendation:** Set secure defaults (`max_req_sz` = 10MB, `idle_timeout_ms` = 30000); add client-side `max_response_sz` API.

### #9: Firewall Use-After-Free via Atomic Swap — FIXED

- **Category:** Memory Safety — Use-After-Free (CWE-416)
- **Affected:** `Acceptor::set_firewall()` in `acceptor.cc`, `Server::unregister_connection()` in `server.cc`
- **Description:** `set_firewall()` atomically swaps and immediately `delete`s the old firewall. Between `firewall.load()` and `fw->grant()` (or `fw->release()`) in the reactor thread, another thread can delete the firewall object. Same pattern in `Server::unregister_connection()` where `fw->release()` can race with `set_firewall()`.
- **Impact:** Use-after-free crash if firewall is reconfigured while server is running.
- **Resolution:** Replaced `std::atomic<Firewall_base*>` with `std::shared_ptr<Firewall_base>` using C++17 `std::atomic_load()`/`std::atomic_store()` free functions. Readers get a local `shared_ptr` copy that prevents deletion during use. Public API `Server::set_firewall(Firewall_base*)` unchanged — wraps raw pointer in `shared_ptr` at the boundary.

### #10: SSL Error Messages Leaked to Clients

- **Category:** Information Disclosure (CWE-209)
- **Affected:** `libiqxmlrpc/ssl_lib.cc:347-357`, `libiqxmlrpc/server.cc:299-316`
- **Description:** OpenSSL error strings (`ERR_reason_error_string`) are captured in `ssl::exception::what()` and forwarded directly to clients in XML-RPC fault responses via the generic `catch(std::exception& e)` handler that passes `e.what()` into the response. Error messages like `"SSL: certificate verify failed"` or `"SSL: sslv3 alert handshake failure"` are sent to the client.
- **Impact:** Fingerprinting of OpenSSL version and TLS configuration; aids reconnaissance for further attacks.
- **Recommendation:** Return generic error messages to clients; log detailed SSL errors server-side only.

### #11: Auth Credentials Over Plain HTTP

- **Category:** Credential Protection — Cleartext Transmission (CWE-319)
- **Affected:** `libiqxmlrpc/server.cc:230-261`, `libiqxmlrpc/auth_plugin.h:40-42`
- **Description:** `set_auth_plugin()` is available on the base `Server` class (parent of both `Http_server` and `Https_server`). No compile-time or runtime guard prevents setting an auth plugin on `Http_server`. HTTP Basic Authentication transmits Base64-encoded credentials (not encrypted) in the `Authorization` header. A comment in `auth_plugin.h:40-42` warns about this but there is no enforcement.
- **Impact:** Passive network sniffing captures Base64-encoded credentials.
- **Recommendation:** Emit a runtime warning (or refuse) when `set_auth_plugin()` is called on a non-TLS server; or require explicit opt-in for auth over HTTP.

---

## LOW (7)

### #12: `XML_PARSE_HUGE` Removes libxml2 Internal Limits

- **Category:** Defense in Depth — Removed Safety Limits
- **Affected:** `libiqxmlrpc/parser2.cc:148`
- **Description:** The `XML_PARSE_HUGE` flag disables libxml2's internal size limits for text nodes, attribute values, and other internal buffers (normally 10MB per text content). This allows an attacker to send XML with extremely large individual text nodes that would normally be rejected. The text gets copied into `std::string` (line 272) and again into a `String` value object, doubling memory usage for large values.
- **Impact:** Amplifies the impact of Finding #8 (memory exhaustion); a single `<string>` element can consume gigabytes.
- **Recommendation:** Remove `XML_PARSE_HUGE` flag unless explicitly needed; or add application-level per-value size limits.

### #13: Null Dereference on Moved-From `Value::type_name()` — FIXED

- **Category:** Memory Safety — Null Pointer Dereference (CWE-476)
- **Affected:** `libiqxmlrpc/value.cc` — `Value::type_name()` (line 228 at time of discovery)
- **Description:** `Value::type_name()` dereferences `value` without a null check. After a move operation (`Value(Value&&)` sets `v.value = nullptr`), calling `type_name()` on the moved-from object is undefined behavior. Unlike the `is_*()` methods which check for null, and unlike `cast<T>()` which throws `Bad_cast`, `type_name()` had no guard.
- **Impact:** Crash if user code calls `type_name()` on a moved-from `Value`. Caller error, but the API is inconsistent with the null-safe `is_*()` methods.
- **Resolution:** Added `if (!value) throw Bad_cast();` guard, consistent with `cast<T>()`. Also guarded the copy constructor (`Value(const Value&)`) which had the same null dereference via `v.value->clone()`.

### #14: Null Dereference on Moved-From `Value::apply_visitor()` — FIXED

- **Category:** Memory Safety — Null Pointer Dereference (CWE-476)
- **Affected:** `libiqxmlrpc/value.cc` — `Value::apply_visitor()` (line 384 at time of discovery)
- **Description:** Same as #13 — `Value::apply_visitor()` dereferences `value` without null check. Called from `value_to_xml()` and `print_value()` which are public API entry points.
- **Impact:** Crash if applied to moved-from Value.
- **Resolution:** Added `if (!value) throw Bad_cast();` guard, consistent with `cast<T>()`. Combined with #13 copy constructor fix, all `Value` methods that dereference the internal pointer now handle moved-from state consistently.

### #15: Pipelined HTTP Data Silently Discarded

- **Category:** HTTP — Data Loss (CWE-20)
- **Affected:** `libiqxmlrpc/http.cc:708` (`clear()`), `libiqxmlrpc/http.cc:822` (`content_cache.erase()`)
- **Description:** When reading a packet, excess data beyond `Content-Length` is erased at line 822. On keep-alive connections, if the client pipelines a second request in the same TCP segment, the `Packet_reader::clear()` discards any buffered data from the next request. The server does not support HTTP pipelining — once a packet is complete, it processes it and discards any remaining buffered data.
- **Impact:** Data corruption/loss if clients attempt HTTP/1.1 pipelining. Could be chained with smuggling attacks.
- **Recommendation:** Document that pipelining is not supported; optionally buffer excess data for the next request.

### #16: All Headers Forwarded to Methods — WON'T FIX (by design)

- **Category:** Input Validation — Header Injection (CWE-113)
- **Affected:** `libiqxmlrpc/http.cc:490`
- **Description:** `Header::get_headers()` copies ALL parsed headers to the method handler's `XHeaders` map. Applications use custom headers without the `x-*` prefix, so filtering by prefix would break existing deployments.
- **Impact:** Negligible. Header total size is bounded by `header_max_sz` (16KB default), CRLF injection is blocked by `validate_header_crlf()`, and the `XHeaders` map is a per-request copy.
- **Resolution:** Won't fix. Forwarding all headers is by design — applications rely on non-`x-*` headers. The 16KB header size limit bounds both count and total size, making namespace pollution impractical. Renamed `Header::get_xheaders()`/`set_xheaders()` to `Header::get_headers()`/`set_headers()` to eliminate the naming mismatch that implied `x-*` filtering.

### #17: Unbounded Method Name Length

- **Category:** Input Validation — Unbounded Input (CWE-20)
- **Affected:** `libiqxmlrpc/request_parser.cc:44`
- **Description:** The method name read from `<methodName>` has no length limit applied. A multi-megabyte method name would be accepted and stored. The `Unknown_method` exception sanitizes the name (limiting to 128 characters in error output), but the name itself is stored at full size until the lookup fails. Bounded by HTTP layer `max_req_sz` if configured, and by `MAX_ELEMENT_COUNT`.
- **Impact:** Minor memory waste; bounded by HTTP layer if configured.
- **Recommendation:** Add an explicit method name length limit (e.g., 256 bytes).

### #18: `gethostname()` Return Value Unchecked

- **Category:** Error Handling — Unchecked Return Value (CWE-252)
- **Affected:** `libiqxmlrpc/inet_addr.cc:26-33`
- **Description:** The return value of `gethostname()` is not checked. If it fails (returns -1), `buf` may contain uninitialized data. The manual null termination at `buf[1023] = 0` prevents buffer overrun, but the returned string would contain garbage. If the hostname is exactly 1024 bytes, `gethostname()` may not null-terminate on some platforms (implementation-defined per POSIX). The uninitialized content between the last written byte and position 1023 would become part of the returned string.
- **Impact:** Garbage hostname returned on failure; information leak of uninitialized stack memory in edge cases.
- **Recommendation:** Check `gethostname()` return value; zero-initialize `buf` or handle errors.

---

## Confirmed NOT Vulnerable

| Area | Why |
|------|-----|
| XXE / Billion Laughs | `XML_PARSER_SUBST_ENTITIES=0` + `XML_PARSE_NONET` |
| CRLF Header Injection | `validate_header_crlf()` on all `set_option()` |
| Protocol Downgrade | `TLS1_2_VERSION` minimum enforced |
| Timing Attack on Auth | `constant_time_compare()` provided |
| Weak Cipher Suites | ECDHE+AES-GCM+CHACHA20 only |
| XML Type Confusion | State machine rejects unknown tags |
| Recursive Stack Overflow | `MAX_PARSE_DEPTH=32` effective |
| Base64 Buffer Overflow | Safe math + correct pre-allocation |
| Content-Length Negative | `from_chars<unsigned>` rejects |
| Integer Overflow in Size Checks | `safe_math::would_overflow_add` |
| Path Traversal | URI stored as opaque string, never used for file access |
| Null Bytes in XML | libxml2 rejects null bytes |

---

## Cross-Cutting Insight

The most severe vulnerabilities (#1, #2, #3, #4) share a common root cause: **shared mutable global state** without synchronization. The global SSL context (`ssl::ctx`), global `ValueOptions`, and raw `Server_connection*` pointers all assume single-threaded access in a library that explicitly supports multi-threaded execution via `Pool_executor`.

The "secure by default" gap (findings #7, #8, #11) stems from the server defaulting to insecure configuration (`max_req_sz=0`, `idle_timeout=0`, auth over HTTP). The library requires application developers to opt-in to safety rather than opt-out.

---

## Remediation Priority

| Priority | Findings | Rationale |
|----------|----------|-----------|
| **P0 — Immediate** | #1 | MITM on every client connection; simple fix |
| **P1 — Next sprint** | #2, #3, #4 | Concurrency bugs exploitable under load |
| **P2 — Near-term** | #6, #7, #8 | Requires API changes or new defaults |
| **P3 — Backlog** | #9, #10, #11 | Moderate risk, workarounds exist |
| **P4 — Low priority** | #12–#18 | Defense in depth, edge cases |

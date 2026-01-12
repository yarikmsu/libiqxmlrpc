# Bug Review Report: libiqxmlrpc

## Executive Summary

Comprehensive code review identified **26 potential bugs** across the codebase:
- **3 CRITICAL** - Can cause crashes/data corruption in production
- **10 HIGH** - Significant issues requiring attention
- **9 MEDIUM** - Should be addressed
- **4 LOW** - Minor issues/code quality

---

## CRITICAL Issues (Fix Immediately)

### 1. Race Condition in Server Connection Set
**File:** `libiqxmlrpc/server.cc:44, 164-172, 300-319`

```cpp
// Line 164-171 - NO LOCK PROTECTION
void Server::register_connection(Server_connection* conn) {
  impl->connections.insert(conn);  // Called from worker threads
}
void Server::unregister_connection(Server_connection* conn) {
  impl->connections.erase(conn);   // Called from worker threads
}

// Line 300-319 - Iterated from reactor thread without lock
std::copy_if(impl->connections.begin(), impl->connections.end(), ...);
```

**Impact:** In Pool executor mode, worker threads modify `connections` while reactor thread iterates it → crashes, iterator invalidation, data corruption.

---

### 2. Bitwise Operation Bug in Reactor
**File:** `libiqxmlrpc/reactor_impl.h:141`

```cpp
int newmask = (i->mask &= !mask);  // BUG: uses ! instead of ~
```

**Impact:** `!mask` returns boolean (0 or 1), not bitwise complement. Should be `~mask`. Works by accident but is dangerous for maintenance.

---

### 3. Memory Leak / Double-Free in Packet_reader
**File:** `libiqxmlrpc/http.cc:612-651, 207`

```cpp
// Line 625 - Header allocated with new
header = new Header_type(ver_level_, header_cache);

// Line 635, 644 - Packet takes ownership via shared_ptr
return new Packet(header, std::string());

// Line 207 - Destructor deletes if not constructed
~Packet_reader() { if (!constructed) delete header; }
```

**Impact:** Complex ownership between `Packet_reader::header` member and `Packet`'s `shared_ptr`. Potential double-free when destruction timing is wrong.

---

## HIGH Severity Issues

### 4. Incorrect SSL_write Partial Write Handling
**Files:** `libiqxmlrpc/ssl_connection.cc:87-95, 142-150`

```cpp
// Line 89 - Treats any short write as error
if (static_cast<size_t>(ret) != len)
  throw_io_exception(ssl, ret);
```

**Impact:** OpenSSL allows partial writes (ret > 0 but ret < len). Code incorrectly throws on valid partial writes.

---

### 5. Null Pointer / Buffer Over-read in SSL Certificate
**File:** `libiqxmlrpc/ssl_lib.cc:225-238`

```cpp
X509* x = X509_STORE_CTX_get_current_cert(ctx);  // Can return NULL
// ... no null check ...
for(int i = 0; i < 32; i++)  // Hard-coded 32 instead of actual 'n'
   ss << std::hex << int(md[i]);
```

**Impact:** Null dereference if no cert. Buffer over-read if SHA256 returns < 32 bytes.

---

### 6. Data Race on Idle Connection State
**File:** `libiqxmlrpc/server_conn.cc:68-90`

```cpp
void Server_connection::start_idle() {
  is_waiting_input_ = true;   // NO SYNCHRONIZATION
  idle_since_ = std::chrono::steady_clock::now();
}
```

**Impact:** Reactor thread reads `is_waiting_input_` and `idle_since_` while worker threads write them → undefined behavior.

---

### 7. Handler Invocation Use-After-Free Risk
**File:** `libiqxmlrpc/reactor_impl.h:213-230`

```cpp
Event_handler* handler = find_handler(hs.fd);  // Lock released after find
// ... handler invoked without lock ...
if (terminate) {
  unregister_handler(handler);
  handler->finish();  // Handler may be deleted by another thread
}
```

**Impact:** Between `find_handler()` and `finish()`, another thread could delete the handler.

---

### 8. Memory Leak in RequestBuilder::get()
**File:** `libiqxmlrpc/request_parser.cc:52-59`

```cpp
Request* RequestBuilder::get() {
  return new Request(*method_name_, params_);  // Raw owning pointer returned
}
```

**Impact:** Caller must manually delete. If exception thrown before deletion → memory leak.

---

### 9. Iterator Invalidation in Reactor
**File:** `libiqxmlrpc/reactor_impl.h:98-101, 128, 137`

**Impact:** `find_handler_state()` returns iterator that can be invalidated if `handlers_states` is modified by another thread before use.

---

### 10. HTTP Authentication Logic Error
**File:** `libiqxmlrpc/http.cc:460-463`

```cpp
size_t colon_it = data.find_first_of(":");
pw = colon_it < std::string::npos ?  // Should be != not <
  data.substr(colon_it + 1, ...) : std::string();
```

**Impact:** Comparison `colon_it < npos` is always true when colon found. Logic works but is confusing.

---

### 11-13. Additional HIGH Issues
- **Double-delete via self-delete** (`http_server.cc:79-83`, `https_server.cc:71-75`)
- **TOCTOU race in find_handler_state** (`reactor_impl.h:98-101`)
- **Idle timeout check race** (`server.cc:308`)

---

## MEDIUM Severity Issues

| # | File | Line | Issue |
|---|------|------|-------|
| 14 | `value.cc` | 29-46 | `get_default_int()` returns raw owning pointer |
| 15 | `value_type.cc` | 283 | `const_iterator` used in non-const method |
| 16 | `inet_addr.cc` | 92-102 | Null check hidden in macro |
| 17 | `executor.cc` | 94-114 | Pool executor exception handling edge case |
| 18 | `executor.cc` | 131-143 | `join()` can throw in destructor |
| 19 | `http.cc` | 643 | Implicit packet parsing behavior |
| 20 | `http.cc` | 625 | Exception safety in header construction |
| 21 | `socket.cc` | 26 | `setsockopt()` return value ignored |
| 22 | `http.cc` | 570 | Integer overflow check could be clearer |

---

## LOW Severity Issues

| # | File | Line | Issue |
|---|------|------|-------|
| 23 | `parser2.cc` | 202 | Edge case in colon parsing |
| 24 | `https_server.cc` | 141 | `.data()` vs `.c_str()` style |
| 25 | `value_parser.cc` | 59 | Defensive coding order |
| 26 | `reactor_poll_impl.cc` | 65 | `unsigned` vs `size_t` mismatch |

---

## Recommended Fix Priority

1. **Immediate (CRITICAL):**
   - Add mutex to `Server::impl->connections`
   - Fix `&= !mask` → `&= ~mask` in reactor
   - Refactor `Packet_reader` ownership model

2. **Soon (HIGH):**
   - Fix SSL partial write handling
   - Add null checks in SSL certificate code
   - Add synchronization to idle connection state
   - Use smart pointers in `RequestBuilder::get()`

3. **Planned (MEDIUM/LOW):**
   - Modernize raw pointer APIs to return `unique_ptr`
   - Add error checking for `setsockopt()` calls
   - Clean up iterator type mismatches

---

## Verification Plan

After fixes:
1. Run `make check` - all tests pass
2. Run `make perf-test` - no performance regression
3. Run with ASan/TSan: `cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=thread"` + `make check`
4. Review CI: `gh pr view <PR> --json statusCheckRollup`

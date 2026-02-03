# Security Bug Hunt Findings - February 1, 2026

## Executive Summary

After comprehensive code review of libiqxmlrpc, I've identified several potential security issues ranging from informational to potentially medium severity. The codebase has a strong defensive posture with XXE prevention, CRLF validation, safe integer math, and TLS 1.2+ enforcement. Most issues found are edge cases rather than fundamental design flaws.

---

## Finding 1: Integer Truncation in Socket Send/Recv (Medium)

**Location**: `libiqxmlrpc/socket.cc:104-121`

**Description**: The `Socket::send()` and `Socket::recv()` functions cast `size_t` to `int` when calling the underlying system calls:

```cpp
size_t Socket::send( const char* data, size_t len )
{
  int ret = ::send( sock, data, static_cast<int>(len), IQXMLRPC_NOPIPE);  // Line 106
  ...
}

size_t Socket::recv( char* buf, size_t len )
{
  int ret = ::recv( sock, buf, static_cast<int>(len), 0 );  // Line 116
  ...
}
```

**Impact**: On 64-bit systems, if `len > INT_MAX` (2,147,483,647), the value wraps to a negative number. The system call behavior with negative length is undefined:
- On Linux, `send()` with negative length typically returns `EINVAL`
- On some systems, it may attempt to read from invalid memory

**Trigger Scenario**:
1. A malicious or buggy XML-RPC response with `Content-Length` > 2GB
2. The code calls `content_cache.length() >= header->content_length()` (http.cc:773)
3. If Content-Length is parsed as `unsigned` (line 416), values up to 4GB pass validation
4. When the data is sent/received, truncation occurs

**Proof of Concept**: A response with `Content-Length: 3000000000` (3 billion bytes) would truncate to a small positive or negative value.

**Severity**: Medium - Requires large request to trigger, but could cause denial of service or memory corruption.

**Recommended Fix**:
```cpp
size_t Socket::send( const char* data, size_t len )
{
  if (len > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw network_error("Send buffer too large");
  }
  int ret = ::send( sock, data, static_cast<int>(len), IQXMLRPC_NOPIPE);
  ...
}
```

---

## Finding 2: SSL Integer Truncation in Read/Write (Medium)

**Location**: `libiqxmlrpc/ssl_connection.cc:100-101, 115, 145, 160`

**Description**: Same issue as Finding 1, but with SSL_read/SSL_write:

```cpp
int ret = SSL_write( ssl, data + total_written,
                     static_cast<int>(len - total_written) );  // Line 100-101

int ret = SSL_read( ssl, buf, static_cast<int>(len) );  // Line 115
```

**Impact**: Same truncation issue - could cause partial data transmission or buffer overflows if the truncated length causes writes beyond intended boundaries.

**Severity**: Medium

---

## Finding 3: Content-Length Maximum Value Boundary (Low)

**Location**: `libiqxmlrpc/http.cc:411-416`

**Description**: The `content_length()` function returns `unsigned`:

```cpp
unsigned Header::content_length() const
{
  if (!option_exists(names::content_length))
    throw Length_required();

  return get_unsigned(names::content_length);  // Returns unsigned (32-bit on most systems)
}
```

The validator only checks that it's a valid unsigned number:

```cpp
void unsigned_number(const std::string& val)
{
  if (!all_digits(val))
    throw Malformed_packet(errmsg);

  try {
    num_conv::from_string<unsigned>(val);  // Only checks if it fits in unsigned
  } catch (const num_conv::conversion_error&) {
    throw Malformed_packet(errmsg);
  }
}
```

**Impact**: A Content-Length of `UINT_MAX` (4,294,967,295) passes validation but may cause issues in downstream size calculations, especially when combined with Finding 1.

**Severity**: Low - Protected by `safe_math` in most places, but edge cases may exist.

---

## Finding 4: SSL Exception Constructor Without Error Queue (Low)

**Location**: `libiqxmlrpc/ssl_lib.cc:347-352`

**Description**: The `ssl::exception` default constructor calls `ERR_get_error()`:

```cpp
exception::exception() noexcept:
  ssl_err( ERR_get_error() ),
  msg( ERR_reason_error_string(ssl_err) )
{
  msg.insert(0, "SSL: ");
}
```

**Issue**: If no error is queued in OpenSSL's error queue, `ERR_get_error()` returns 0 and `ERR_reason_error_string(0)` returns `nullptr`. The `std::string` constructor receiving `nullptr` causes undefined behavior.

**Trigger**: Any code path that constructs `ssl::exception()` when no SSL error is pending.

**Workaround in Code**: The codebase already has `ssl::exception(const std::string&)` constructor and `ssl::exception(unsigned long)` that handle this case. However, calling the default constructor in the wrong context is still possible.

**Severity**: Low - Would cause crash, not security bypass. Documented in common-pitfalls.md.

---

## Finding 5: Firewall Rate Limiter Cleanup Race (Informational)

**Location**: `libiqxmlrpc/firewall.cc:183-200`

**Description**: The `cleanup_stale_entries()` function locks `rate_mutex` while iterating and erasing from `request_trackers`:

```cpp
size_t RateLimitingFirewall::cleanup_stale_entries()
{
  std::lock_guard<std::mutex> lock(impl_->rate_mutex);

  size_t removed = 0;
  auto it = impl_->request_trackers.begin();
  while (it != impl_->request_trackers.end()) {
    if (it->second.count_recent() == 0) {
      it = impl_->request_trackers.erase(it);  // Safe but holds lock for entire cleanup
      ++removed;
    } else {
      ++it;
    }
  }
  return removed;
}
```

**Impact**: With many tracked IPs, cleanup holds the lock for an extended period, potentially causing latency spikes for `check_request_allowed()` calls.

**Severity**: Informational - Not a security vulnerability, but could enable slowloris-style DoS if cleanup is triggered at high load.

---

## Finding 6: Parser xml_depth() Returns int but Compared to Constexpr (Informational)

**Location**: `libiqxmlrpc/parser2.cc:65-67` and `libiqxmlrpc/parser2.h:19`

**Description**:
```cpp
// parser2.h
static constexpr int MAX_PARSE_DEPTH = 32;

// parser2.cc
void BuilderBase::visit_element(const std::string& tag)
{
  depth_++;
  int xml_depth = parser_.xml_depth();  // xmlTextReaderDepth returns int
  if (xml_depth > MAX_PARSE_DEPTH) {
    throw Parse_depth_error(xml_depth, MAX_PARSE_DEPTH);
  }
  do_visit_element(tag);
}
```

**Issue**: `xmlTextReaderDepth()` can return -1 on error. The comparison `xml_depth > MAX_PARSE_DEPTH` would not catch -1.

**Impact**: If libxml2 is in an error state, depth checking may be bypassed. However, libxml2 typically throws/returns errors before this becomes exploitable.

**Severity**: Informational - Defense in depth issue, not directly exploitable.

---

## Finding 7: Potential Denial of Service via Wide XML Structures (Low)

**Location**: `libiqxmlrpc/value_parser.cc` (entire file)

**Description**: The parser enforces `MAX_PARSE_DEPTH = 32` for nested elements, but does NOT limit:
1. Number of sibling elements at each level
2. Total number of elements in the document
3. Total memory allocation

**Attack**: An XML-RPC request with 1 million sibling `<member>` elements inside a `<struct>`:
```xml
<struct>
  <member><name>a0</name><value><i4>0</i4></value></member>
  <member><name>a1</name><value><i4>0</i4></value></member>
  ... (repeat 1 million times)
</struct>
```

This passes depth checks but causes:
1. O(n) memory allocation
2. O(n) parsing time
3. O(n) dispatch processing

**Mitigation**: The `max_req_sz` limit on HTTP packet size provides partial protection, but a maximally-dense payload can still cause significant CPU load.

**Severity**: Low - Protected by request size limits but not CPU time limits.

---

## Finding 8: Request Smuggling Potential (Low)

**Location**: `libiqxmlrpc/http.cc:697-743`

**Description**: The HTTP parser handles multiple line ending styles for robustness:

```cpp
size_t sep_pos = header_cache.find("\r\n\r\n");
size_t sep_len = 4;

if (sep_pos == std::string::npos) {
  sep_pos = header_cache.find("\r\n\n");  // Mixed
  sep_len = 3;
}

if (sep_pos == std::string::npos) {
  sep_pos = header_cache.find("\n\n");    // Unix-style
  sep_len = 2;
}
```

**Impact**: This flexible parsing could potentially be exploited in request smuggling attacks when libiqxmlrpc is behind a reverse proxy that handles line endings differently.

**Scenario**:
1. Attacker sends: `POST / HTTP/1.1\r\nContent-Length: 0\r\n\nPOST /admin HTTP/1.1\r\n...`
2. A strict proxy sees one request with `\r\n\r\n` terminator
3. libiqxmlrpc sees the `\n\n` and processes the second "smuggled" request

**Severity**: Low - Only exploitable with specific proxy configurations, and libiqxmlrpc typically runs standalone.

---

## Finding 9: Hostname CRLF Check Before DNS Resolution (Mitigated)

**Location**: `libiqxmlrpc/inet_addr.cc:90-91`

**Description**: CRLF validation is already present:

```cpp
Inet_addr::Impl::Impl( const std::string& h, int p ):
  sa(), sa_init_flag(), host(h), port(p)
{
  if (h.find_first_of("\n\r") != std::string::npos)
    throw network_error("Hostname must not contain CR LF characters", false);
}
```

**Status**: This was flagged in the exploration phase but is already fixed. The implementation correctly rejects CRLF in hostnames.

**Severity**: N/A - Already mitigated.

---

## Summary Table

| # | Finding | Severity | CVSS Est. | Status |
|---|---------|----------|-----------|--------|
| 1 | Socket send/recv integer truncation | Medium | 5.3 | New |
| 2 | SSL read/write integer truncation | Medium | 5.3 | New |
| 3 | Content-Length boundary value | Low | 3.1 | New |
| 4 | SSL exception constructor crash | Low | 3.7 | Known |
| 5 | Rate limiter cleanup lock contention | Info | 2.1 | New |
| 6 | Parser depth check negative value | Info | 2.1 | New |
| 7 | Wide XML structure DoS | Low | 4.3 | New |
| 8 | Request smuggling potential | Low | 3.4 | New |
| 9 | Hostname CRLF injection | N/A | - | Mitigated |

---

## Recommendations

### High Priority
1. Add bounds checking to `Socket::send()`, `Socket::recv()`, `SSL_read()`, `SSL_write()` for sizes > INT_MAX
2. Consider adding element count limits to XML parser

### Medium Priority
3. Add explicit check for `xml_depth() < 0` error condition
4. Review request smuggling scenarios if used behind reverse proxy

### Low Priority
5. Consider batch cleanup for rate limiter to reduce lock contention
6. Add configuration option for maximum XML elements per document

---

## Verification Steps

To verify Findings 1 and 2:
```bash
# Build with ASan
mkdir build_asan && cd build_asan
cmake -DCMAKE_BUILD_TYPE=Debug -DSANITIZE_ADDRESS=ON ..
make

# Run fuzz target with large inputs (would require adding to fuzz corpus)
```

The existing fuzz infrastructure covers most parsing paths but doesn't specifically test INT_MAX boundary conditions for size parameters.

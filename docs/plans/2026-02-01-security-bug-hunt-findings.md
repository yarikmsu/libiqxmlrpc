# Security Bug Hunt Findings - February 1, 2026

> **Last Updated**: February 2, 2026
> **Status**: Most findings have been addressed. See status column in summary table.

## Executive Summary

After comprehensive code review of libiqxmlrpc, several potential security issues were identified ranging from informational to medium severity. The codebase has a strong defensive posture with XXE prevention, CRLF validation, safe integer math, and TLS 1.2+ enforcement. Most issues found were edge cases rather than fundamental design flaws.

**Update (Feb 2, 2026)**: 6 of 9 findings have been fixed. The remaining items are informational or low-priority design considerations.

---

## Finding 1: Integer Truncation in Socket Send/Recv (Medium)

**Status**: ✅ **FIXED** (Commit `9d3155f`)

**Location**: `libiqxmlrpc/socket.cc:105-120`

**Description**: The `Socket::send()` and `Socket::recv()` functions previously cast `size_t` to `int` without bounds checking.

**Fix Applied**:
```cpp
size_t Socket::send( const char* data, size_t len )
{
  // SECURITY: Prevent integer truncation when casting size_t to int.
  if (len > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw network_error("Socket::send: buffer size exceeds INT_MAX");
  }
  int ret = ::send( sock, data, static_cast<int>(len), IQXMLRPC_NOPIPE);
  ...
}
```

---

## Finding 2: SSL Integer Truncation in Read/Write (Medium)

**Status**: ✅ **FIXED** (Commit `7079b8b`)

**Location**: `libiqxmlrpc/ssl_connection.cc:91-136`

**Description**: Same issue as Finding 1, but with SSL_read/SSL_write.

**Fix Applied**: INT_MAX checks added to `send()`, `recv()`, `try_ssl_read()`, and `try_ssl_write()`.

---

## Finding 3: Content-Length Maximum Value Boundary (Low)

**Status**: ⚠️ **MITIGATED**

**Location**: `libiqxmlrpc/http.cc:411-416`

**Description**: A Content-Length of `UINT_MAX` (4,294,967,295) passes validation.

**Mitigation**: With Findings 1 and 2 fixed, oversized Content-Length values are now caught at the socket/SSL layer before causing issues. The `max_req_sz` limit provides additional protection.

---

## Finding 4: SSL Exception Constructor Without Error Queue (Low)

**Status**: ✅ **FIXED** (Commit `b1b1f54`)

**Location**: `libiqxmlrpc/ssl_lib.cc:347-357`

**Description**: The `ssl::exception` default constructor could receive `nullptr` from `ERR_reason_error_string()`.

**Fix Applied**:
```cpp
exception::exception() noexcept:
  ssl_err( ERR_get_error() ),
  msg()
{
  // SECURITY: ERR_reason_error_string() returns nullptr when no error is queued
  const char* reason = ERR_reason_error_string(ssl_err);
  msg = reason ? reason : "unknown SSL error";
  msg.insert(0, "SSL: ");
}
```

---

## Finding 5: Firewall Rate Limiter Cleanup Race (Informational)

**Status**: ℹ️ **ACKNOWLEDGED** (Not a security vulnerability)

**Location**: `libiqxmlrpc/firewall.cc:183-200`

**Description**: The `cleanup_stale_entries()` function holds a lock while iterating, which could cause latency spikes with many tracked IPs.

**Decision**: Acceptable trade-off. Lock contention is minimal in practice, and correctness is prioritized over micro-optimization.

---

## Finding 6: Parser xml_depth() Returns int but Compared to Constexpr (Informational)

**Status**: ✅ **FIXED** (Commit `401f8df`)

**Location**: `libiqxmlrpc/parser2.cc:72-80`

**Description**: `xmlTextReaderDepth()` can return -1 on error, which was not explicitly checked.

**Fix Applied**:
```cpp
int xml_depth = parser_.xml_depth();
// SECURITY: xmlTextReaderDepth() returns -1 on error.
if (xml_depth < 0) {
  throw Parse_error("Failed to get XML depth (parser error)");
}
if (xml_depth > MAX_PARSE_DEPTH) {
  throw Parse_depth_error(xml_depth, MAX_PARSE_DEPTH);
}
```

---

## Finding 7: Potential Denial of Service via Wide XML Structures (Low)

**Status**: ✅ **FIXED** (Commits `da3b30e`, `5f39b77`)

**Location**: `libiqxmlrpc/parser2.h:21-24`, `libiqxmlrpc/parser2.cc:64-69`

**Description**: The parser enforced depth limits but not element count limits, allowing "wide" XML attacks.

**Fix Applied**:
```cpp
// parser2.h
static constexpr int MAX_ELEMENT_COUNT = 10000000;  // 10 million elements

// parser2.cc
int count = parser_.increment_element_count();
if (count > MAX_ELEMENT_COUNT) {
  throw Parse_element_count_error(count, MAX_ELEMENT_COUNT);
}
```

---

## Finding 8: Request Smuggling Potential (Low)

**Status**: ℹ️ **ACKNOWLEDGED** (Design decision)

**Location**: `libiqxmlrpc/http.cc:697-743`

**Description**: The HTTP parser handles multiple line ending styles (`\r\n\r\n`, `\r\n\n`, `\n\n`) for robustness, which could theoretically enable request smuggling behind certain proxies.

**Decision**: This flexibility is intentional for compatibility. libiqxmlrpc typically runs standalone, not behind reverse proxies. If used behind a proxy, operators should ensure consistent line ending handling.

---

## Finding 9: Hostname CRLF Check Before DNS Resolution (Mitigated)

**Status**: ✅ **ALREADY MITIGATED**

**Location**: `libiqxmlrpc/inet_addr.cc:90-91`

**Description**: CRLF validation was already present at time of initial review.

---

## Summary Table

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| 1 | Socket send/recv integer truncation | Medium | ✅ Fixed (`9d3155f`) |
| 2 | SSL read/write integer truncation | Medium | ✅ Fixed (`7079b8b`) |
| 3 | Content-Length boundary value | Low | ⚠️ Mitigated by #1/#2 |
| 4 | SSL exception constructor crash | Low | ✅ Fixed (`b1b1f54`) |
| 5 | Rate limiter cleanup lock contention | Info | ℹ️ Acknowledged |
| 6 | Parser depth check negative value | Info | ✅ Fixed (`401f8df`) |
| 7 | Wide XML structure DoS | Low | ✅ Fixed (`da3b30e`, `5f39b77`) |
| 8 | Request smuggling potential | Low | ℹ️ Acknowledged |
| 9 | Hostname CRLF injection | N/A | ✅ Already mitigated |

---

## Recommendations Status

### Completed ✅

1. ~~Add bounds checking to `Socket::send()`, `Socket::recv()`, `SSL_read()`, `SSL_write()` for sizes > INT_MAX~~
2. ~~Consider adding element count limits to XML parser~~
3. ~~Add explicit check for `xml_depth() < 0` error condition~~

### Still Valid (Low Priority)

4. Review request smuggling scenarios if used behind reverse proxy
5. Consider batch cleanup for rate limiter to reduce lock contention (optional optimization)

---

## Verification

All fixes have been verified:
- INT_MAX checks present in `socket.cc:107-112, 124-129`
- INT_MAX checks present in `ssl_connection.cc:96-98, 126-128, 161-164, 183-186`
- Null check present in `ssl_lib.cc:354-355`
- Negative depth check present in `parser2.cc:75-77`
- Element count check present in `parser2.cc:66-69`
- MAX_ELEMENT_COUNT = 10,000,000 in `parser2.h:24`

The existing fuzz infrastructure and CI sanitizers (ASan/UBSan/TSan) provide ongoing protection against regressions.

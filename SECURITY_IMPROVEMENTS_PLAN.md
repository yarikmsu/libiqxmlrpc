# Security Improvements Plan

**Source:** Extracted from stale branch `claude/recommend-project-agents-3xIlN` before deletion.
**Date:** 2026-01-11

## Overview

Four security hardening features were identified that are not currently implemented in master. These should be implemented in a fresh feature branch with proper testing.

---

## 1. XML Parsing Depth Limit

**Purpose:** Prevent stack exhaustion attacks via deeply nested XML documents.

**Implementation Location:** `libiqxmlrpc/parser2.cc`, `libiqxmlrpc/parser2.h`

**Approach:**
```cpp
// In parser2.h - add constant
static constexpr int MAX_XML_DEPTH = 100;  // Configurable limit

// In BuilderBase::visit_element() - add check
void BuilderBase::visit_element(const std::string& tag) {
    depth_++;
    if (depth_ > MAX_XML_DEPTH) {
        throw Parse_error("XML nesting depth exceeds maximum allowed limit");
    }
    do_visit_element(tag);
}
```

**Test:** Create XML with 101+ nested elements, verify Parse_error is thrown.

---

## 2. Disable TLS 1.0 and TLS 1.1 ✅ COMPLETED

**Purpose:** Disable deprecated/insecure TLS versions (PCI-DSS compliance, modern security).

**Status:** Implemented in branch `security/disable-legacy-tls`

**Changes Made:**
- `libiqxmlrpc/ssl_lib.cc`: Added `SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1` to `set_common_options()`
- `tests/test_integration.cc`: Added `ssl_disables_legacy_tls_versions` test case

**Test:** `ssl_disables_legacy_tls_versions` verifies all legacy protocol options are set.

---

## 3. Server Connection Idle Timeout

**Purpose:** Prevent slowloris-style denial-of-service attacks by closing idle connections.

**Implementation Location:** `libiqxmlrpc/server.h`, `libiqxmlrpc/server.cc`, `libiqxmlrpc/server_conn.cc`

**Approach:**
```cpp
// In server.h - add API
void set_conn_timeout(int seconds);  // 0 = disabled (default)
int get_conn_timeout() const;
// Recommended: 30-300 seconds

// In server_conn.cc - track last activity, close if exceeded
```

**Test:** Open connection, send no data, verify timeout and closure.

---

## 4. Integer Overflow Protection for Buffer Sizes

**Purpose:** Prevent integer overflow when calculating buffer sizes.

**Implementation Location:** `libiqxmlrpc/http.cc` and related buffer handling code

**Approach:**
```cpp
// Before buffer allocation, validate size won't overflow
if (size > MAX_BUFFER_SIZE || size < 0) {
    throw std::runtime_error("Invalid buffer size");
}
```

**Test:** Attempt to trigger overflow with malicious Content-Length headers.

---

## Implementation Checklist

- [x] ~~Create feature branch: `security/hardening-2026-01`~~ → `security/disable-legacy-tls`
- [ ] Implement XML depth limit with tests
- [x] Implement TLS 1.0/1.1 disabling with tests
- [ ] Implement connection timeout with tests
- [ ] Implement integer overflow protection with tests
- [ ] Run full test suite including ASan/UBSan
- [ ] Update documentation
- [ ] Create PR for review

## Priority

| Feature | Priority | Rationale |
|---------|----------|-----------|
| TLS 1.0/1.1 disable | High | Compliance requirement, easy win |
| Connection timeout | High | DoS protection |
| XML depth limit | Medium | Defense in depth |
| Integer overflow | Medium | Edge case hardening |

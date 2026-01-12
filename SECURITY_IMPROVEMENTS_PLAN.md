# Security Improvements Plan

**Source:** Extracted from stale branch `claude/recommend-project-agents-3xIlN` before deletion.
**Date:** 2026-01-11

## Overview

Four security hardening features were identified that are not currently implemented in master. These should be implemented in a fresh feature branch with proper testing.

---

## 1. XML Parsing Depth Limit ✅ COMPLETED

**Purpose:** Prevent stack exhaustion attacks via deeply nested XML documents.

**Status:** Implemented in PR #68, branch `security/xml-depth-limit`

**Changes Made:**
- `libiqxmlrpc/except.h`: Added `Parse_depth_error` exception class (fault code -32700)
- `libiqxmlrpc/parser2.h`: Added `MAX_PARSE_DEPTH` constant (32 levels) and `xml_depth()` method
- `libiqxmlrpc/parser2.cc`: Added depth check in `BuilderBase::visit_element()` using libxml2's global depth tracking
- `tests/parser2.cc`: Added 9 comprehensive unit tests

**Implementation Details:**
```cpp
// In parser2.h - BuilderBase class
static constexpr int MAX_PARSE_DEPTH = 32;  // XML-RPC typically needs ~10 levels

// In parser2.cc - uses libxml2's actual XML depth (not builder-local depth)
void BuilderBase::visit_element(const std::string& tag) {
    depth_++;
    int xml_depth = parser_.xml_depth();
    if (xml_depth > MAX_PARSE_DEPTH) {
        throw Parse_depth_error(xml_depth, MAX_PARSE_DEPTH);
    }
    do_visit_element(tag);
}
```

**Tests:** 9 test cases covering arrays, structs, mixed nesting, boundary conditions, error messages, and request/response parsing contexts.

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

- [x] ~~Create feature branch: `security/hardening-2026-01`~~ → Individual feature branches
- [x] Implement XML depth limit with tests → PR #68 (`security/xml-depth-limit`)
- [x] Implement TLS 1.0/1.1 disabling with tests → `security/disable-legacy-tls`
- [ ] Implement connection timeout with tests
- [ ] Implement integer overflow protection with tests
- [x] Run full test suite including ASan/UBSan (for completed items)
- [ ] Update documentation
- [x] Create PRs for review (XML depth: #68)

## Priority

| Feature | Priority | Status | Rationale |
|---------|----------|--------|-----------|
| TLS 1.0/1.1 disable | High | ✅ Done | Compliance requirement, easy win |
| Connection timeout | High | Pending | DoS protection |
| XML depth limit | Medium | ✅ Done (PR #68) | Defense in depth |
| Integer overflow | Medium | Pending | Edge case hardening |

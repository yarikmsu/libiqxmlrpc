# Security Checklist

OWASP-based security checklist for libiqxmlrpc code reviews.

## XML/Parsing
- [ ] Depth limit enforced (MAX_PARSE_DEPTH = 32)
- [ ] Entity expansion disabled (XXE prevention)
- [ ] Size limits before allocation

## Memory Safety
- [ ] Integer overflow protection (`safe_math.h`)
- [ ] Bounds checking on all buffers
- [ ] No `sprintf`/`strcpy` (use safe alternatives)
- [ ] Smart pointers for ownership

## TLS/SSL
- [ ] TLS 1.2 minimum enforced
- [ ] Certificate validation enabled
- [ ] Hostname verification for clients

## Input Validation
- [ ] Content-Length validated against actual size
- [ ] Headers sanitized
- [ ] No user input in exceptions/logs (info disclosure)

## DoS Prevention
- [ ] Connection timeouts configured
- [ ] Request size limits enforced
- [ ] Resource cleanup on errors
- [ ] Rate limiting (RateLimitingFirewall)

## Defensive Coding Patterns

- [ ] **No `assert()` for security checks** - `assert()` compiles away in Release builds (NDEBUG)
  - Use runtime checks: `if (!ptr) return;` or `if (!ptr) throw ...;`
  - Example: `reactor_impl.h` line 235 - replaced `assert(handler)` with null check
- [ ] **Element count limits** - MAX_ELEMENT_COUNT prevents "wide" XML attacks
  - Depth limits alone don't protect against structures with many siblings
- [ ] **Canonical IP addresses** - Use `inet_ntop()` for consistent IP representation
  - Prevents rate limiter bypass via different representations of same IP

## Key Files

| File | Security Feature |
|------|------------------|
| `safe_math.h` | Overflow-checked arithmetic |
| `parser2.h` | XML depth + element count limits |
| `ssl_lib.cc` | TLS configuration |
| `firewall.h` | Rate limiting, connection limits |
| `http.h` | Header size limits |
| `reactor_impl.h` | Null handler safety check |

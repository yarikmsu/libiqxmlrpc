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

## Key Files

| File | Security Feature |
|------|------------------|
| `safe_math.h` | Overflow-checked arithmetic |
| `parser2.h` | XML depth limit |
| `ssl_lib.cc` | TLS configuration |
| `firewall.h` | Rate limiting, connection limits |
| `http.h` | Header size limits |

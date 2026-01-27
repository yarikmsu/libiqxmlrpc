# Performance Review: libiqxmlrpc

**Last Updated:** 2026-01-27

## Summary

**31 performance optimizations** implemented across the library:

| Category | Key Improvements |
|----------|-----------------|
| Number conversion | `boost::lexical_cast` → `std::to_chars` (1.2x-12.5x faster) |
| Type checking | `dynamic_cast` → type tags (1.8x-10.5x faster) |
| HTTP parsing | Single-pass parser (2.9x-3.3x faster), `string_view` tokenization (~28% faster) |
| HTTP generation | `ostringstream` → `snprintf` for response header (2.97x faster) |
| String formatting | `ostringstream` → stack buffer/concat for SSL hex, proxy URI, host:port |
| Data structures | `std::map` → `unordered_map` for Struct (39-56% faster), HTTP options (2.54x faster), method dispatcher (3.21x faster), reactor handlers, XHeaders |
| Base64 | Lookup table decoding (4.1x-4.7x faster), direct buffer writes (~70% faster decode) |
| SSL/TLS | Exception-free I/O (~850x faster per event), session caching, AES-NI |
| Network | TCP_NODELAY enabled (40-400ms latency reduction), offset tracking for sends |
| Thread pool | Lock-free queue with `boost::lockfree::queue` (better multi-core scaling) |
| Reactor | Copy-on-write handler list (zero-copy reads on hot path) |
| XML | Zero-copy `content_view()` for XML builder output, `Array::reserve()` |

## Completed Optimizations

### PR #180: M5 State Machine Cleanup (2026-01-27)

| ID | Optimization | Result |
|----|--------------|--------|
| M5 | State machine lookup: linear scan vs hash map | Benchmarked: hash map 4-5x slower for 3-11 entry tables. Linear scan is optimal. Early-return cleanup applied. |

### PR #179: M6 HTTP Tokenization (2026-01-27)

| ID | Optimization | Speedup |
|----|--------------|---------|
| M6 | HTTP line tokenization with `string_view` | ~28% faster request/response parsing |

### PR #178: Array::reserve() + XML Benchmarks (2026-01-26)

| ID | Optimization | Impact |
|----|--------------|--------|
| — | `Array::reserve()` method | Eliminates reallocations when array size is known |
| — | Comprehensive XML parsing benchmark suite | 10 benchmark sections, 69 benchmarks total |

### PR #177: Base64 Decode Optimization (2026-01-26)

| ID | Optimization | Speedup |
|----|--------------|---------|
| — | Base64 decode with direct buffer writes | ~70% faster |

### PR #176: E1-E3 String Formatting (2026-01-25)

| ID | Optimization | Before | After |
|----|--------------|--------|-------|
| E1 | SSL hex formatting | `ostringstream` | Stack buffer with `snprintf` |
| E2 | Proxy URI construction | `ostringstream` | String concat + `reserve()` |
| E3 | Host:port construction | `ostringstream` | String concat |

### PR #169: Quick Wins (2026-01-21)

| ID | Optimization | Before | After | Impact |
|----|--------------|--------|-------|--------|
| QW1 | XHeaders `unordered_map` | `std::map` O(log n) | `std::unordered_map` O(1) | Faster header lookup |
| QW2 | HTTP/HTTPS offset tracking | `string.erase(0, sz)` O(n) | Offset counter O(1) | Avoids memory moves |
| QW3 | XML Builder `content_view()` | Copy only | + `string_view` zero-copy | Avoids large XML copies |

### PR #168: M1-M4 Optimizations (2026-01-20)

| ID | Optimization | Speedup |
|----|--------------|---------|
| M1 | Struct `unordered_map` | 39-56% faster |
| M2 | HTTP options `unordered_map` | 2.54x faster |
| M3 | Method dispatcher `unordered_map` | 3.21x faster |
| M4 | Reactor handler `unordered_map` | O(1) lookup |

### Previous Optimizations (2025-2026)

See git history for full details. Key improvements:
- Number conversion with `std::to_chars`
- Type tags replacing `dynamic_cast`
- Single-pass HTTP parsing
- Base64 lookup tables
- SSL exception-free I/O
- Lock-free thread pool
- TCP_NODELAY

## Remaining Opportunities

### Moderate Effort (Medium Complexity)

| ID | File | Current | Proposed | Status |
|----|------|---------|----------|--------|
| M7 | `http.cc:645` | Header + content concat | `writev()` scatter/gather | Pending |

### Major Refactor (High Complexity)

| ID | File | Current | Proposed | Status |
|----|------|---------|----------|--------|
| R1 | `value.cc:70-73` | Deep clone on copy | COW with shared_ptr | Deferred |

### Investigated & Rejected

| ID | Proposal | Reason |
|----|----------|--------|
| M5 | Hash map for state machine lookup | Hash map 4-5x slower than linear scan for 3-11 entry tables. Construction cost (115-344 ns) dominates. Linear scan is cache-friendly and optimal. |
| — | Socket buffers (SO_RCVBUF/SNDBUF) | OS defaults sufficient, adds complexity |
| — | HTTP version consistency | Current behavior is intentional (1.0 client, 1.1 server) |

## Running Benchmarks

```bash
cd build
make perf-test                    # Main benchmarks (69 tests)
make perf-http-tokenize           # M6 HTTP tokenization benchmark
make perf-state-machine           # M5 state machine lookup benchmark
```

CI includes benchmark regression testing (>15% threshold).

See `docs/PERFORMANCE_GUIDE.md` for performance rules and guidelines.

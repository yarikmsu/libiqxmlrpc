# Performance Review: libiqxmlrpc

**Last Updated:** 2026-01-21

## Summary

**28 performance optimizations** implemented across the library:

| Category | Key Improvements |
|----------|-----------------|
| Number conversion | `boost::lexical_cast` → `std::to_chars` (1.2x-12.5x faster) |
| Type checking | `dynamic_cast` → type tags (1.8x-10.5x faster) |
| HTTP parsing | Single-pass parser (2.9x-3.3x faster) |
| HTTP generation | `ostringstream` → `snprintf` for response header (2.97x faster) |
| Data structures | `std::map` → `unordered_map` for Struct (39-56% faster), HTTP options (2.54x faster), method dispatcher (3.21x faster), reactor handlers, XHeaders |
| Base64 | Lookup table decoding (4.1x-4.7x faster) |
| SSL/TLS | Exception-free I/O (~850x faster per event), session caching, AES-NI |
| Network | TCP_NODELAY enabled (40-400ms latency reduction), offset tracking for sends |
| Thread pool | Lock-free queue with `boost::lockfree::queue` (better multi-core scaling) |
| Reactor | Copy-on-write handler list (zero-copy reads on hot path) |
| XML | Zero-copy `content_view()` for XML builder output |

## Completed Optimizations

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

### Easy Fixes (Low Complexity)

| ID | File | Current | Proposed | Status |
|----|------|---------|----------|--------|
| E1 | `ssl_lib.cc:143-148` | `ostringstream` for hex | Stack buffer | Pending |
| E2 | `http_client.cc:78-88` | `ostringstream` for proxy URI | String concat + reserve() | Pending |
| E3 | `http.cc:455-457` | `ostringstream` for host:port | String concat | Pending |

### Moderate Effort (Medium Complexity)

| ID | File | Current | Proposed | Status |
|----|------|---------|----------|--------|
| M5 | `parser2.cc:305-321` | Linear state search | Hash map by (state, tag) | Pending |
| M6 | `http.cc:54-90` | `substr()` allocates | Return `string_view` tokens | Pending |
| M7 | `http.cc:645` | Header + content concat | `writev()` scatter/gather | Pending |

### Major Refactor (High Complexity)

| ID | File | Current | Proposed | Status |
|----|------|---------|----------|--------|
| R1 | `value.cc:70-73` | Deep clone on copy | COW with shared_ptr | Deferred |

### Skipped (Not Recommended)

| Item | Reason |
|------|--------|
| Socket buffers (SO_RCVBUF/SNDBUF) | OS defaults sufficient, adds complexity |
| HTTP version consistency | Current behavior is intentional (1.0 client, 1.1 server) |

## Running Benchmarks

```bash
cd build
make perf-test
```

CI includes benchmark regression testing (>20% threshold).

See `docs/PERFORMANCE_GUIDE.md` for performance rules and guidelines.

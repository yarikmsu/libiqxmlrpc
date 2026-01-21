# Performance Review: libiqxmlrpc

**Status:** All major optimizations completed (2026-01-21)

## Summary

20 performance optimizations were implemented across the library:

| Category | Key Improvements |
|----------|-----------------|
| Number conversion | `boost::lexical_cast` → `std::to_chars` (1.2x-12.5x faster) |
| Type checking | `dynamic_cast` → type tags (1.8x-10.5x faster) |
| HTTP parsing | Single-pass parser (2.9x-3.3x faster) |
| Data structures | `std::map` → `unordered_map` for Struct (39-56% faster) |
| Base64 | Lookup table decoding (4.1x-4.7x faster) |
| SSL/TLS | Exception-free I/O (~850x faster per event), session caching, AES-NI |
| Network | TCP_NODELAY enabled (40-400ms latency reduction) |
| Thread pool | Lock-free queue with `boost::lockfree::queue` (better multi-core scaling) |
| Reactor | Copy-on-write handler list (zero-copy reads on hot path) |

## Remaining Opportunities

These are lower-priority items that could be addressed if needed:

| Priority | Area | Recommendation | Impact |
|----------|------|----------------|--------|
| Medium | Handler lookup | `unordered_map` indexed by socket descriptor | O(1) vs O(log n) lookup |
| Low | Socket buffers | Configurable `SO_RCVBUF`/`SO_SNDBUF` | Throughput tuning |
| Low | Vectored I/O | `writev` for header+body | Minor efficiency gain |
| Low | Parser | `std::string_view` for XML parsing | Reduced allocations |
| Low | HTTP version | Make consistent (1.0 vs 1.1) | Correctness |

## Running Benchmarks

```bash
cd build
make perf-test
```

See `docs/PERFORMANCE_GUIDE.md` for performance rules and guidelines.

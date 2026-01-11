# Performance Guide

## Running Benchmarks

```bash
cd build
make perf-test
```

Results are saved to `tests/performance_baseline.txt`.

## Performance Change Requirements

**Every performance improvement MUST include:**

1. **Benchmark** — Add or use existing benchmark in `tests/test_performance.cc`
2. **Before/after measurements** — Run benchmarks before and after the change
3. **Documented results** — Include measured results in PR description
4. **Reproducible** — Ensure benchmarks can be re-run with `make perf-test`

### PR Template for Performance Changes

```markdown
## Performance Results
| Operation | Before | After | Improvement |
|-----------|--------|-------|-------------|
| benchmark_name | X ns | Y ns | Z% faster |

## How to reproduce
1. Checkout commit before change
2. Run: make perf-test
3. Apply change
4. Run: make perf-test
5. Compare results
```

## Hot Path Guidelines

### Avoid in Hot Paths

| Pattern | Why | Alternative |
|---------|-----|-------------|
| `boost::lexical_cast` | 8-12x slower than alternatives | `num_conv::to_string/from_string` |
| `dynamic_cast` for type checks | 10x slower for mismatches | `value->type_tag()` comparison |
| `std::locale` construction | 3-4x overhead | `strftime` directly |
| `substr()` for parsing | Allocates new string | `std::string_view` or index math |
| Exceptions for control flow | ~850x slower than return codes | Return codes or error flags |

### Prefer

| Pattern | Why |
|---------|-----|
| `std::to_string()` for integers | Highly optimized in modern libc++ |
| `std::from_chars()` for parsing | No locale overhead |
| `ValueTypeTag` enum comparison | O(1) type identification |
| Stack buffers with `snprintf`/`strftime` | Avoids heap allocation |
| Lookup tables for decoding | O(1) vs O(n) search |

## Key Optimizations Applied

| Optimization | Speedup | PR |
|--------------|---------|-----|
| `boost::lexical_cast` → `std::to_chars` | 1.2x-12.5x | #29 |
| `dynamic_cast` → type tags | 1.8x-10.5x | #33 |
| `std::locale` → `strftime` | 2.7x-3.7x | #34 |
| Base64 decode lookup table | 4.1x-4.7x | #42 |
| `std::map` → `unordered_map` (Struct) | 39-56% | #44 |
| Single-pass HTTP parsing | 2.9x-3.3x | #54 |
| Exception-free SSL I/O | ~850x per event | #62 |
| TCP_NODELAY enabled | 40-400ms latency | #45 |

## Remaining Opportunities

| Priority | Area | Recommendation |
|----------|------|----------------|
| Medium | Handler list copy | Copy-on-write for high connection counts |
| Medium | Queue contention | Lock-free queue for thread pool |
| Low | Vectored I/O | `writev` for header+body |
| Low | String allocations | `std::string_view` in parser |

## Benchmarking Tools

- **Built-in**: `make perf-test` (Boost.Test benchmarks)
- **Memory**: Valgrind/ASan for allocation tracking
- **CPU**: `perf` on Linux, Instruments on macOS
- **HTTP load**: `wrk` or `wrk2`

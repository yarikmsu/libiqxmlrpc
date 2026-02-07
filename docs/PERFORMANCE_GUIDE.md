# Performance Guide

## Running Benchmarks

```bash
cd build
make perf-test
```

Results are saved to `build/performance_baseline.txt`.

## CI Performance Regression Detection

The Benchmark CI workflow automatically runs on PRs that modify:
- `libiqxmlrpc/**` — Library source code
- `tests/test_performance.cc` — Benchmark test file
- `tests/perf_utils.h` — Benchmark utilities
- `scripts/benchmark_utils.py` — Shared parsing utilities
- `scripts/compare_benchmarks.py` — Comparison script
- `scripts/select_minimum_results.py` — Minimum result selector
- `.github/workflows/benchmark.yml` — Workflow itself

### How It Works

1. Builds and benchmarks the **PR branch** (3 runs, selects minimum)
2. Builds and benchmarks the **master branch** (3 runs, selects minimum)
3. Compares PR against master — both measured in same CI environment
4. Posts results as a PR comment
5. Fails if any regression exceeds the threshold (default: 20%)

This approach ensures accurate comparison by using identical hardware and configuration for both measurements.

### Manual Trigger with Custom Threshold

You can manually trigger the benchmark workflow with a custom threshold:

1. Go to Actions → Benchmark → Run workflow
2. Enter a custom threshold percentage (e.g., `5` for stricter, `20` for looser)
3. Click "Run workflow"

### Local Benchmark Comparison

Compare your changes against a base branch on the same machine:

```bash
# From repository root (requires build directory)
./scripts/local_benchmark_compare.sh           # Default 20% threshold, compare to master
./scripts/local_benchmark_compare.sh 5         # Stricter 5% threshold
./scripts/local_benchmark_compare.sh 10 main   # Compare to 'main' branch instead
```

The script:
1. Benchmarks your current branch (3 runs)
2. Stashes changes and checks out the base branch (default: master)
3. Benchmarks the base branch (3 runs)
4. Returns to your branch and shows comparison

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
| Copy-on-write handler list | 617x-5788x snapshot | #88 |
| Lock-free thread pool queue | Eliminates mutex contention | — |

## Remaining Opportunities

| Priority | Area | Recommendation |
|----------|------|----------------|
| Medium | Serialization | Custom XML writer bypassing libxml2 `xmlTextWriter` |
| Medium | Parse overhead | Reader pooling with rotation (discard every N uses) |

See `docs/PERFORMANCE_OPTIMIZATION_LOG.md` § "Recommended Path to +30% RPS" for details.

## RPS Benchmarking

For end-to-end throughput measurement, use the RPS benchmark script:

```bash
# Start benchmark server
./build_release/tests/benchmark-server --port 8091 --numthreads 4

# Run RPS benchmark (in another terminal)
python3 scripts/rps_benchmark.py --port 8091 --requests 5000 --clients 4
```

### Realistic Payloads

Always use realistic production payloads for benchmarks. The `rps_benchmark.py` uses a struct
matching the smallest real-world request pattern (auth credentials, server, method, params array).
Synthetic string payloads (e.g., `"x" * 100`) do not reflect actual XML-RPC parsing costs.

### C++ Microbenchmarks vs RPS

- **C++ microbenchmarks** (`make perf-test`) isolate specific operations (parse, serialize). Variance: ±3%.
- **RPS benchmarks** measure end-to-end throughput including HTTP, dispatch, network. Variance: ±10%.
- Use C++ microbenchmarks for parser-level improvements; RPS for server-level changes.
- Always run 3+ iterations and average results.

## Benchmarking Tools

- **Built-in**: `make perf-test` (Boost.Test benchmarks)
- **RPS**: `python3 scripts/rps_benchmark.py` (end-to-end throughput)
- **Memory**: Valgrind/ASan for allocation tracking
- **CPU**: `perf` on Linux, Instruments on macOS
- **HTTP load**: `wrk` or `wrk2`

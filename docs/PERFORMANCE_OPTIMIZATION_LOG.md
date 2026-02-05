# Performance Optimization Log

This document records performance optimization efforts, findings, and recommendations for libiqxmlrpc.

## Summary

As of February 2026, libiqxmlrpc has been optimized to near-maximum performance for XML-RPC with libxml2. Key improvements:

| PR | Optimization | Impact |
|----|--------------|--------|
| #213 | Move semantics for Value containers | **+17% RPS** |
| #214 | Move semantics for text parsing | **+1-2% RPS** |

**Current bottleneck:** 77% of CPU time is inside libxml2's XML parsing functions, which cannot be optimized without changing the XML library.

---

## Profiling Methodology

### Tools Used

- **macOS `sample` command** - CPU sampling profiler
- **Python RPS benchmark** (`scripts/rps_benchmark.py`) - Throughput measurement
- **C++ performance tests** (`make perf-test`) - Micro-benchmarks

### Benchmark Configuration

```bash
# Start benchmark server
./build_release/tests/benchmark-server --port 8090 --numthreads 4

# Run RPS benchmark
python3 scripts/rps_benchmark.py --port 8090 --requests 5000 --clients 4

# Profile during load (10 seconds)
sample <PID> -f /tmp/profile.txt 10
```

### Key Metrics

- **RPS** - Requests per second (higher is better)
- **p50/p90/p99 latency** - Response time percentiles (lower is better)
- **CPU samples** - Where time is spent (from profiler)

---

## Current Performance Profile

### CPU Time Distribution (as of 2026-02)

| Component | CPU % | Function | Optimizable? |
|-----------|-------|----------|--------------|
| XML Parsing | 77% | `xmlParseGetLasts`, `xmlTextReaderRead` | ❌ Inside libxml2 |
| XML Serialization | 23% | `xmlEncodeSpecialChars`, `xmlStrlen` | ❌ Inside libxml2 |

### Detailed Breakdown

**Parsing (77% of CPU):**
- `xmlParseGetLasts` - 60% (libxml2 parser state machine)
- `xmlStrdup` - 15% (string duplication in libxml2)
- `xmlParseCharData` - 17% (character data parsing)
- Other - 8%

**Serialization (23% of CPU):**
- `xmlEncodeSpecialChars` - 65% (escaping `<`, `>`, `&`, etc.)
- `xmlStrlen` - 28% (computing string lengths)
- `xmlTextWriterWriteRawLen` - 7% (writing to buffer)

---

## Optimizations Implemented

### PR #213: Move Semantics for Value Containers

**Problem:** When parsing XML-RPC requests, Values were being cloned when pushed into Arrays/Structs, even though the parser creates them as temporaries.

**Solution:** Changed `value_parser.cc` to construct Values directly instead of wrapping in `unique_ptr`, enabling the rvalue overloads of `push_back` and `insert`:

```cpp
// Before (created unique_ptr, then copied Value internally):
proxy_->push_back(std::make_unique<Value>(tmp.release()));

// After (constructs temporary Value, binds to rvalue overload):
proxy_->push_back(Value(tmp.release()));
```

The key insight is that `Value(tmp.release())` creates a temporary (rvalue) which binds to `Array::push_back(Value&&)`, enabling move semantics instead of copy.

**Impact:** +17% RPS improvement

**Files changed:**
- `libiqxmlrpc/value_parser.cc` - ArrayBuilder and StructBuilder

---

### PR #214: Move Semantics for Text Parsing

**Problem:** Parsed text content was being copied through the visitor chain even though it's a temporary string.

**Solution:** Added move overloads throughout the parser visitor chain:

1. `BuilderBase::visit_text(std::string&&)` - Entry point
2. `BuilderBase::do_visit_text(std::string&&)` - Virtual method (default forwards to const-ref)
3. `ValueBuilder::do_visit_text(std::string&&)` - Moves string into Scalar
4. `Scalar(T&&)` constructor with conditional `noexcept`

**Impact:** +1-2% RPS improvement

**Files changed:**
- `libiqxmlrpc/parser2.h` - Added move overload declarations
- `libiqxmlrpc/parser2.cc` - Implemented move forwarding
- `libiqxmlrpc/value_parser.h` - Added override declaration
- `libiqxmlrpc/value_parser.cc` - Implemented move version
- `libiqxmlrpc/value_type.h` - Added `noexcept` to Scalar move constructor

**Key design decisions:**
- Default `do_visit_text(std::string&&)` forwards to const-ref version for backward compatibility
- Conditional `noexcept(std::is_nothrow_move_constructible_v<T>)` enables STL container optimizations

---

## Optimizations Attempted But Abandoned

### XML Raw Write for Unescaped Strings

**Hypothesis:** Pre-check if string needs escaping; use `xmlTextWriterWriteRaw` for safe strings.

**Implementation:** Added `needs_xml_escaping()` function with 5 `memchr()` calls.

**Result:** No improvement. The overhead of pre-checking was similar to letting libxml2 escape.

**Reason:** libxml2's `xmlEncodeSpecialChars` is already optimized with SIMD on modern platforms.

---

### XmlBuilder Buffer Pre-allocation

**Hypothesis:** Pre-allocate 1KB buffer to avoid reallocations during serialization.

**Implementation:** Changed `xmlBufferCreate()` to `xmlBufferCreateSize(1024)`.

**Result:** No measurable improvement (within noise margin).

**Reason:**
- Small responses (~150 bytes) fit in default 256-byte buffer
- Reallocation cost is negligible compared to character escaping
- Larger pre-allocation wastes memory for small responses

---

## Future Optimization Opportunities

### High Impact, High Effort

| Optimization | Est. Impact | Effort | Risk |
|--------------|-------------|--------|------|
| Replace libxml2 with RapidXML | +30-50% | High | Medium (API changes) |
| Replace libxml2 with pugixml | +30-50% | High | Medium (API changes) |
| Add binary protocol option | +100%+ | Very High | Low (new feature) |

**RapidXML/pugixml benefits:**
- Header-only, no external dependency
- Faster parsing (no SAX overhead)
- In-place parsing possible (zero-copy)

**Considerations:**
- libxml2 provides better XML validation
- libxml2 handles edge cases (DTD, namespaces) that simpler parsers may not

---

### Medium Impact, Medium Effort

| Optimization | Est. Impact | Effort | Risk |
|--------------|-------------|--------|------|
| Response caching | Varies | Medium | Low |
| Parser context pooling | +5-10% | Medium | Low |
| Custom string escaping | +5% | Medium | Medium |

**Response caching:**
- Cache serialized XML for repeated responses
- Best for methods that return the same value frequently

**Parser context pooling:**
- Reuse `xmlTextReader` instances instead of creating new ones
- Saves initialization overhead

---

### Low Impact, Low Effort

| Optimization | Est. Impact | Effort | Risk |
|--------------|-------------|--------|------|
| String length hints | +1-2% | Low | Low |
| Avoid unnecessary copies | Done | - | - |

---

## Benchmarking Tips

### Reducing Variance

1. **Close other applications** - Background processes affect results
2. **Run multiple iterations** - Average at least 3 runs
3. **Use consistent request counts** - 3000-5000 requests per measurement
4. **Warm up before measuring** - The benchmark script does 100 warmup requests

### Interpreting Results

- **±5% variance is normal** - Don't chase improvements smaller than noise
- **±15% variance under load** - System scheduling affects multi-threaded benchmarks
- **Profile, don't guess** - Always profile before optimizing

### Expected Performance

With optimizations as of 2026-02:

| Scenario | Expected RPS (4 threads) |
|----------|-------------------------|
| Small payload (echo "Hello") | 9,000-11,000 |
| Large payload (1000-element array) | 150-250 |

---

## References

- [libxml2 documentation](http://xmlsoft.org/)
- [RapidXML](http://rapidxml.sourceforge.net/)
- [pugixml](https://pugixml.org/)
- `docs/PERFORMANCE_GUIDE.md` - General performance guidelines
- `docs/CPP_STYLE.md` - C++ style and anti-patterns

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

### Thread-Local xmlTextReader Pooling

**Hypothesis:** Cache one `xmlTextReader` per thread using `thread_local` storage. Reinitialize with `xmlReaderNewMemory()` instead of creating/destroying per parse. Preserves the interned string dictionary (`xmlDict`) across parses for hash hits on repeated tag names.

**Implementation:** `CachedReader` struct in anonymous namespace, `Impl` constructor tries `xmlReaderNewMemory()` first with fallback to `xmlReaderForMemory()`. Destructor returns reader to TLS cache. XXE protection re-applied unconditionally.

**Result:** +8% parse improvement on small payloads (C++ microbenchmark). No measurable RPS improvement (within noise).

**Reason abandoned:** libxml2's `xmlDict` accumulates interned strings permanently across parses. `xmlCtxtReset()` does not clear the dictionary, and libxml2 provides no `xmlDictReset()` API — only `xmlDictFree()`, which destroys the reader. On long-lived server threads processing untrusted input, an attacker can send requests with unique element names to grow the dictionary without bound (~25 bytes per entry). While XML-RPC's fixed vocabulary (~20 tags) means normal traffic reaches steady state, the unbounded growth under adversarial input is a behavioral regression from the previous per-parse `xmlFreeTextReader()` approach.

**Possible future approach:** Discard and recreate the cached reader after N uses (e.g., 10,000) to bound dictionary growth while preserving most of the pooling benefit.

---

## Future Optimization Opportunities

### Recommended Path to +30% RPS (Without Replacing libxml2)

Profiling shows 100% of CPU is inside libxml2: 77% parsing, 23% serialization. Two
optimizations stack to an estimated +30-35% RPS improvement while keeping libxml2 for parsing.

#### 1. Custom XML Serializer (+22-25% RPS)

Replace `XmlBuilder` (libxml2 `xmlTextWriter`) with direct `std::string` building for response
and request serialization. Keep libxml2 for parsing only.

**Why it works:** XML-RPC output has a fixed vocabulary of ~18 tags. Generating it requires only
`std::string::append()` for tags and a simple escape function for 3 characters (`<`, `>`, `&`).
The full `xmlTextWriter` API is pure overhead:

- `xmlEncodeSpecialChars` — 65% of serialization CPU (~15% total) — replaced by a trivial scan
- `xmlStrlen` — 28% of serialization CPU (~6.5% total) — eliminated (we already know `std::string::size()`)
- `xmlTextWriterWriteRawLen` — 7% of serialization CPU (~1.6% total) — replaced by `append()`

**Approach:**

- New `FastXmlWriter` class: `std::string` with `reserve(256)`, direct `append()` for tags
- New `Value_type_to_xml` visitor (or template specialization) targeting `FastXmlWriter`
- Custom `escape_xml()` scanning only for `<`, `>`, `&` in string content
- `std::to_chars` for int/double (matches existing codebase patterns)
- `dump_response()` / `dump_request()` select `FastXmlWriter` instead of `XmlBuilder`

**Amdahl's law:** Serialization 5x faster → 23% CPU becomes ~5% → total 81.6/100 → **+22.5% RPS**

**Risk:** Low. Existing roundtrip tests in `test_request_response.cc` (1377 lines) validate
output correctness. `XmlBuilder` remains available as fallback.

#### 2. Reader Pooling with Rotation (+6-8% RPS, additive)

Re-attempt thread-local `xmlTextReader` caching with a use counter: discard and recreate the
reader every N parses (e.g., 10,000) to bound `xmlDict` memory growth.

**Why it works:** Reuses the interned string dictionary (`xmlDict`) across parses, getting hash
hits on the ~20 repeated XML-RPC tag names. Previously measured at +8% on parse microbenchmarks.

**Why rotation solves the memory problem:**

- Normal traffic: ~20 unique tags → steady state at ~500 bytes, never grows
- Adversarial input: 10,000 × ~25 bytes/entry = 250KB worst case, then reader is discarded
- The original attempt (reverted) had *unbounded* growth; rotation makes it O(N) bounded

**Approach:**

- `thread_local CachedReader` struct with use counter in `parser2.cc`
- `Impl` constructor: try `xmlReaderNewMemory()` on cached reader, fallback to `xmlReaderForMemory()`
- Increment counter on each use; when counter reaches N, set cached reader to null
- Destructor returns reader to TLS cache (or frees if counter expired)
- Re-apply XXE protection unconditionally (`xmlCtxtReset()` resets `replaceEntities` to 0)

**Amdahl's law:** Stacked on serializer: parsing 77/1.08 = 71.3 + serialization 4.6 = 75.9/100 → **+31.8% RPS**

**Risk:** Medium. `xmlDict` behavior is well-understood from prior attempt. Needs memory bound
test to verify rotation works correctly.

#### Combined Projection

| Optimization | Mechanism | Est. Impact | Effort | Risk |
|---|---|---|---|---|
| Custom serializer | Bypass libxml2 `xmlTextWriter` | +22-25% | Medium (~150 LOC) | Low |
| Reader pooling w/ rotation | Reuse `xmlTextReader`, discard every N uses | +6-8% | Medium (~50 LOC) | Medium |
| **Combined** | | **+30-35%** | | |

**Recommended order:** Custom serializer first (larger impact, lower risk, verifiable with
existing roundtrip tests), then reader pooling (measured independently against new baseline).

---

### Other Opportunities

#### High Impact, High Effort

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

#### Low Impact

| Optimization | Est. Impact | Effort | Risk |
|--------------|-------------|--------|------|
| Response caching | Varies by workload | Medium | Low |

Note: `writev` for header+body (<2%) and `string_view` in parser (<2%) were removed.
`writev` saves one concatenation of a ~200-byte header — negligible. `string_view` was
already applied in HTTP parsing and number conversion; the XML parser copies from libxml2
`const xmlChar*` pointers that are invalidated on each `xmlTextReaderRead()`, making
`string_view` inapplicable there.

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

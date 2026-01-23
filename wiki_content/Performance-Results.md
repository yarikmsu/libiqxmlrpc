# Performance Results

This fork includes **28 performance optimizations** implemented and validated through CI benchmarks.

## Highlights

### Number Conversion (12.5x faster)
- **Before**: `boost::lexical_cast<int>("12345")` - 297 ns
- **After**: `iqxmlrpc::string_to_int("12345")` - **24 ns**
- **Speedup**: **12.5x faster** ✨

Uses C++17 `std::to_chars` / `std::from_chars` on hot paths.

### Type Checking (10.5x faster)
- **Before**: `dynamic_cast` for type identification - 8.7 ns
- **After**: `ValueTypeTag` enum check - **0.8 ns**
- **Speedup**: **10.5x faster** ✨

### HTTP Parsing (3.3x faster)
- **Before**: Multiple passes for header parsing
- **After**: Single-pass parser with zero-copy views
- **Speedup**: **3.3x faster** ✨

### Data Structures (39-400% faster)
- **Struct**: `std::map` → `std::unordered_map` (**39% faster**)
- **HTTP headers**: `std::map` → `std::unordered_map` (**400% faster**)
- **Reactor handlers**: Linear search → `unordered_map` lookup

### SSL/TLS Exception Handling (850x faster)
- **Before**: Exceptions on every SSL_read/write call
- **After**: Exception-free I/O on hot path
- **Impact**: **~850x fewer exceptions** per request

## Complete Results

For detailed analysis of all 28 optimizations, see:
- [PERFORMANCE_REVIEW.md](/docs/PERFORMANCE_REVIEW.md) - Full optimization summary
- [BENCHMARK_COMPARISON.md](../BENCHMARK_COMPARISON.md) - Latest PR results

## Optimization Categories

| Category | Optimizations | Avg Speedup |
|----------|--------------|-------------|
| **Number Conversion** | 3 | 1.2x - 12.5x |
| **Type Checking** | 4 | 1.8x - 10.5x |
| **Data Structures** | 8 | 1.4x - 5.0x |
| **String Operations** | 6 | 2.2x - 3.3x |
| **Memory Management** | 4 | 1.5x - 2.1x |
| **Exception Handling** | 3 | 850x events |

## Benchmark CI

Every PR runs performance regression tests. Regressions >15% are automatically flagged.

See [Performance Guide](/docs/PERFORMANCE_GUIDE.md) for:
- Benchmarking best practices
- Hot path identification
- Optimization opportunities

## When to Use Fast APIs

Not all code paths need optimization. Focus on hot paths for maximum impact.

### High Priority (Hot Paths)

Optimize these aggressively - they're called frequently:

- **XML-RPC request parsing** - Every incoming request
- **Response serialization** - Every outgoing response
- **Value type checking in loops** - Can execute thousands of times per request
- **Number conversion** - Common in data processing
- **Header parsing** - Every HTTP request/response

### Medium Priority

Optimize if profiling shows these are bottlenecks:

- **Method dispatch** - Once per request (usually fast enough)
- **Connection setup** - One-time per connection
- **Value construction** - Moderate frequency

### Low Priority

Don't optimize unless absolutely necessary:

- **One-time configuration** - Initialization code
- **Error handling paths** - Rarely executed
- **Debug/logging code** - Usually disabled in production

**Rule of thumb**: If code runs >1000 times per second, use fast APIs. Otherwise, prioritize readability.

## API Recommendations

For best performance in your code:

```cpp
// ✅ Fast: Use num_conv for number conversion
#include <iqxmlrpc/num_conv.h>
int val = iqxmlrpc::string_to_int("12345");

// ❌ Slow: boost::lexical_cast (12.5x slower)
int val = boost::lexical_cast<int>("12345");

// ✅ Fast: Use ValueTypeTag for type checks
if (value.type_tag() == iqxmlrpc::ValueTypeTag::INT) { ... }

// ❌ Slow: dynamic_cast (10.5x slower)
if (dynamic_cast<Int*>(value.get_impl())) { ... }

// ✅ Fast: Use content_view() for zero-copy reads
std::string_view sv = builder.content_view();

// ❌ Slow: content() creates string copy
std::string s = builder.content();
```

## Run Benchmarks Yourself

Want to see the performance improvements firsthand?

### Build with Benchmarks

```bash
# Clone the repository
git clone https://github.com/yarikmsu/libiqxmlrpc.git
cd libiqxmlrpc

# Build in Release mode with optimizations
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run benchmarks
cd build
./tests/benchmark_num_conv      # Number conversion benchmarks
./tests/benchmark_value_type    # Type checking benchmarks
./tests/benchmark_http_parser   # HTTP parsing benchmarks
```

### Expected Output

```
Benchmark: string_to_int vs lexical_cast
  string_to_int:     24.3 ns ±0.5 ns
  lexical_cast:     297.1 ns ±2.1 ns
  Speedup:          12.2x faster ✨

Benchmark: ValueTypeTag vs dynamic_cast
  type_tag():        0.8 ns ±0.1 ns
  dynamic_cast:      8.7 ns ±0.3 ns
  Speedup:          10.9x faster ✨
```

### Compare with Upstream

Want to compare with the original upstream performance?

```bash
# Build this fork
cd libiqxmlrpc
cmake -B build-fork -DCMAKE_BUILD_TYPE=Release
cmake --build build-fork
./build-fork/tests/benchmark_num_conv > /tmp/fork_results.txt

# Clone and build upstream
cd ..
git clone https://github.com/adedov/libiqxmlrpc.git upstream
cd upstream
# Note: Upstream uses older build system, may need adjustments
```

See [PERFORMANCE_GUIDE.md](/docs/PERFORMANCE_GUIDE.md) for:
- How to profile your application
- Identifying hot paths
- Creating custom benchmarks

# What's New in This Fork

Key changes from the upstream adedov/libiqxmlrpc repository.

## Performance Improvements

**28 optimizations** delivered 1.2x to 12.5x speedups on key operations.

See [Performance Results](Performance-Results) for details.

## New APIs

### Fast Number Conversion
```cpp
#include <iqxmlrpc/num_conv.h>

// String to integer (12.5x faster than boost::lexical_cast)
int val = iqxmlrpc::string_to_int("12345");

// Integer to string (1.2x faster)
std::string str = iqxmlrpc::int_to_string(12345);
```

### Value Type Checking
```cpp
// Fast type identification (10.5x faster than dynamic_cast)
iqxmlrpc::ValueTypeTag tag = value.type_tag();
if (tag == iqxmlrpc::ValueTypeTag::INT) { ... }
```

### Zero-Copy String Views
```cpp
// Avoid string copies when reading XML content
std::string_view sv = xml_builder.content_view();
```

### Overflow-Safe Math
```cpp
#include <iqxmlrpc/safe_math.h>

// Throws std::overflow_error on overflow
size_t result = iqxmlrpc::safe_add(a, b);
size_t result = iqxmlrpc::safe_multiply(x, y);
```

## Updated Requirements

| Component | Upstream | Fork |
|-----------|----------|------|
| **C++ Standard** | C++11 | **C++17** |
| **CMake** | 2.8+ | 3.10+ (3.21+ recommended) |
| **OpenSSL** | 1.0.2+ | **1.1.0+ required** |
| **Compiler** | GCC 4.8+ | GCC 7+ / Clang 5+ |

### Why C++17?

The fork requires C++17 to enable significant performance and safety improvements:

**Performance Features**:
- **`std::string_view`** - Zero-copy string operations (eliminates thousands of allocations per request)
- **`std::to_chars` / `std::from_chars`** - Fast number conversion (12.5x faster than boost::lexical_cast)
- **Structured bindings** - Cleaner code for container iteration
- **`if constexpr`** - Compile-time branch elimination

**Safety Features**:
- **`std::optional`** - Explicit handling of optional values (replaces error-prone pointers)
- **Inline variables** - Safer header-only constants
- **`[[nodiscard]]`** - Compiler warnings for ignored return values

**Example - Zero-Copy Parsing**:
```cpp
// C++11: Must copy string (allocation + copy)
std::string header = parser.get_header("Content-Type");

// C++17: No allocation, just a view
std::string_view header = parser.get_header_view("Content-Type");
```

The performance gains from C++17 features are **measurable and significant** - not just theoretical improvements.

## Security Enhancements

- **TLS 1.2+** required (TLS 1.0/1.1 disabled)
- **Overflow protection** via `safe_math.h`
- **OWASP-aligned** security checklist for PRs
- **Fuzz testing** via OSS-Fuzz integration

## Development Improvements

- **CI/CD**: GitHub Actions with 12 check types (ASan, TSan, Valgrind, coverage, etc.)
- **Coverage**: 95% line coverage target
- **Benchmarks**: Automated performance regression detection (15% threshold)
- **Documentation**: 12 internal guides in `/docs/`

## Breaking Changes

### Removed APIs
None - all upstream APIs remain supported.

### Deprecated Patterns
These still work but are slower:
- `boost::lexical_cast` → Use `iqxmlrpc::string_to_int()`
- `dynamic_cast` for type checks → Use `.type_tag()`
- `.content()` string copies → Use `.content_view()`

## Migration Guide

### For Existing Users

**Step 1: Update Build Environment**
1. Update compiler to GCC 7+ or Clang 5+
2. Update OpenSSL to 1.1.0+ (1.0.2 is EOL since 2019)
3. Update CMake to 3.10+ (3.21+ recommended)
4. Recompile with `-std=c++17`

**Step 2: Test Existing Code**
- No API changes required - all upstream APIs still work
- Run your existing test suite
- Verify that everything compiles and runs correctly

**Step 3 (Optional): Adopt Performance Improvements**

Use this checklist to identify optimization opportunities in your codebase:

#### Performance Migration Checklist

**Number Conversion** (12.5x speedup):
- [ ] Find: `boost::lexical_cast<int>(str)` → Replace: `iqxmlrpc::string_to_int(str)`
- [ ] Find: `boost::lexical_cast<std::string>(num)` → Replace: `iqxmlrpc::int_to_string(num)`
- [ ] Add: `#include <iqxmlrpc/num_conv.h>` where needed

**Type Checking** (10.5x speedup):
- [ ] Find: `dynamic_cast<Int*>(value.get_impl())` → Replace: `value.type_tag() == ValueTypeTag::INT`
- [ ] Find: `if (dynamic_cast<String*>(...))` → Replace: `if (value.type_tag() == ValueTypeTag::STRING)`
- [ ] Review: All type checking in loops or hot paths

**Zero-Copy Operations** (eliminates allocations):
- [ ] Find: `builder.content()` in read-only contexts → Replace: `builder.content_view()`
- [ ] Find: `std::string header = parser.get_header(...)` → Replace: `std::string_view header = parser.get_header_view(...)`
- [ ] Review: Any string parameter that's only read, not modified

**Overflow Safety** (security improvement):
- [ ] Find: Manual size calculations like `size_t total = a + b;` → Replace: `size_t total = safe_add(a, b);`
- [ ] Add: `#include <iqxmlrpc/safe_math.h>` for buffer size calculations
- [ ] Review: Any arithmetic that determines buffer sizes or array indices

**Priority**: Focus on hot paths first (request parsing, response serialization). See [Performance Results](Performance-Results#when-to-use-fast-apis) for prioritization guidance.

**Validation**: Run benchmarks before and after to measure impact:
```bash
# Profile your application before changes
perf record -g ./your_app
perf report

# Make optimizations from checklist

# Profile again and compare
perf record -g ./your_app_optimized
perf report
```

### For New Users
Start with [Quick Start](Quick-Start) guide.

## License

No license changes - remains **BSD 2-Clause**.

**Copyright**:
- Original: 2011 Anton Dedov
- Fork: 2019-2026 Yaroslav Gorbunov

See [License](../License) file for full text.

# C++ Style Guidelines

Project-specific C++ patterns for libiqxmlrpc development.

## Performance-Critical Patterns

**Avoid in hot paths:**

| Anti-Pattern | Alternative | Why |
|--------------|-------------|-----|
| `boost::lexical_cast` | `num_conv::to_string/from_string` | 8-12x faster |
| `dynamic_cast` for type checks | `value->type_tag()` | 10x faster |
| `std::locale` construction | `strftime` directly | 3-4x faster |
| `substr()` for parsing | `std::string_view` | Avoids allocation |

**Prefer:**
- `std::to_string()` for integers (highly optimized in modern libc++)
- `std::from_chars()` for parsing (no locale overhead)
- `ValueTypeTag` enum comparison for type checks
- Stack buffers with `snprintf`/`strftime` for formatting

## Code Patterns

### Value System
- `Value` - proxy class for all XML-RPC types
- `ValueTypeTag` - enum for O(1) type identification
- Use `value->type_tag()` instead of `dynamic_cast`

### Number Conversion
- `num_conv::to_string<T>()` - fast integer-to-string
- `num_conv::from_string<T>()` - fast string-to-integer
- `num_conv::double_to_string()` - IEEE 754 precision (17 digits)

## Thread Safety Patterns

### Simple flags/pointers shared across threads
- Use `std::atomic<bool>` for flags (e.g., `exit_flag`)
- Use `std::atomic<T*>` for pointers set from one thread, read from another

### Lazy initialization
- Use `std::call_once` with `std::once_flag` for thread-safe one-time init
- Initialize `once_flag` in constructor initializer list (`-Weffc++` requirement)

### Collections
- Protect with `std::mutex` and `std::lock_guard`
- Minimize lock scope - copy data out, release lock, then process

### DNS lookups
- Use `dns_mutex()` in `inet_addr.cc` - glibc's resolver has internal shared state

## Key Files

| File | Purpose |
|------|---------|
| `libiqxmlrpc/num_conv.h` | Fast number conversions |
| `libiqxmlrpc/value.h` | Value proxy class |
| `libiqxmlrpc/value_type.h` | ValueTypeTag enum |
| `libiqxmlrpc/safe_math.h` | Overflow-safe arithmetic |

# Code Review Checklist

Standards for reviewing C++ code in libiqxmlrpc.

## Code Quality
- [ ] Functions under 50 lines
- [ ] Meaningful variable/function names
- [ ] No dead code or commented-out code
- [ ] Clear separation of concerns

## Performance (Hot Path)
- [ ] No `boost::lexical_cast` in hot paths (use `num_conv.h`)
- [ ] No `dynamic_cast` where `ValueTypeTag` works
- [ ] No `std::locale` operations (use C locale functions)
- [ ] Avoid unnecessary allocations

## Thread Safety
- [ ] Shared state protected by mutex or atomic
- [ ] No data races (verify with TSan)
- [ ] RAII for lock management (`std::lock_guard`)

## Memory Safety
- [ ] Smart pointers for ownership
- [ ] `safe_math.h` for arithmetic that could overflow
- [ ] Bounds checking on arrays/buffers

## Testing
- [ ] New functionality has tests
- [ ] Edge cases covered (null, empty, overflow)
- [ ] Error paths tested

## Severity Levels

| Level | Action Required |
|-------|-----------------|
| **High** | Must fix: security, crash, data corruption |
| **Medium** | Should fix: performance, maintainability |
| **Low** | Optional: style, minor improvements |

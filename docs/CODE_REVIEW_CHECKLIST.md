# Code Review Checklist

Standards for reviewing C++ code in libiqxmlrpc.

## Code Quality
- [ ] Meaningful variable/function names
- [ ] Clear separation of concerns

## Thread Safety
- [ ] Shared state protected by mutex or atomic
- [ ] No data races (verify with TSan)
- [ ] RAII for lock management (`std::lock_guard`)

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

## Related Checklists
- **Simplicity**: `docs/CODE_SIMPLIFICATION.md` — function length, nesting, dead code
- **Security/Memory**: `docs/SECURITY_CHECKLIST.md` — memory safety, input validation, DoS, XML/TLS
- **Performance**: `docs/PERFORMANCE_GUIDE.md` — benchmarking, hot path patterns, profiling

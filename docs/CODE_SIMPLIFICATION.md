# Code Simplification Guide

Patterns for reducing unnecessary complexity in libiqxmlrpc.

## Structure Rules
- [ ] Functions under 50 lines
- [ ] Nesting max 3 levels (use guard clauses)
- [ ] Single responsibility per function

## Dead Code
- [ ] No unused variables/parameters
- [ ] No commented-out code
- [ ] No backwards-compatibility shims for removed features

## Over-Engineering
- [ ] No single-use helper functions
- [ ] No premature abstractions
- [ ] Three similar lines > premature abstraction

## Guard Clause Pattern

```cpp
// Before (deep nesting)
if (conn) {
    if (conn->is_valid()) {
        process(conn);
    }
}

// After (guard clause)
if (!conn || !conn->is_valid())
    return;
process(conn);
```

## Boolean Extraction

```cpp
// Before (complex condition)
if (state == READY && !queue.empty() && timeout > 0)

// After (extracted variable)
bool can_process = (state == READY && !queue.empty() && timeout > 0);
if (can_process)
```

## Long Method Refactoring

```cpp
// Before (60+ line function)
void process_request(Request& req) {
    // validation code (15 lines)
    // parsing code (20 lines)
    // processing code (25 lines)
}

// After (extracted helpers)
void process_request(Request& req) {
    validate_request(req);
    auto data = parse_request(req);
    execute_request(data);
}
```

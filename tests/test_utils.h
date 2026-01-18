#ifndef IQXMLRPC_TEST_UTILS_H
#define IQXMLRPC_TEST_UTILS_H

// Shared test utilities for libiqxmlrpc tests
// Provides RAII guards for global state restoration

#include "libiqxmlrpc/value.h"

namespace iqxmlrpc {
namespace test {

// RAII guard for Value::omit_string_tag_in_responses()
// Saves current value on construction, restores it on destruction.
// Ensures the setting is restored even if the test fails/throws.
class OmitStringTagGuard {
    bool saved_;
public:
    OmitStringTagGuard() : saved_(Value::omit_string_tag_in_responses()) {}
    ~OmitStringTagGuard() { Value::omit_string_tag_in_responses(saved_); }

    // Non-copyable, non-movable
    OmitStringTagGuard(const OmitStringTagGuard&) = delete;
    OmitStringTagGuard& operator=(const OmitStringTagGuard&) = delete;
    OmitStringTagGuard(OmitStringTagGuard&&) = delete;
    OmitStringTagGuard& operator=(OmitStringTagGuard&&) = delete;
};

// RAII guard for Value::set_default_int() / drop_default_int()
// Note: Intentionally drops any existing default on construction to ensure
// a clean test state. This differs from OmitStringTagGuard which preserves
// the original value. Use this pattern when tests should start with no default.
// Ensures the default is dropped even if the test fails/throws.
class DefaultIntGuard {
public:
    DefaultIntGuard() { Value::drop_default_int(); }
    ~DefaultIntGuard() { Value::drop_default_int(); }

    // Non-copyable, non-movable
    DefaultIntGuard(const DefaultIntGuard&) = delete;
    DefaultIntGuard& operator=(const DefaultIntGuard&) = delete;
    DefaultIntGuard(DefaultIntGuard&&) = delete;
    DefaultIntGuard& operator=(DefaultIntGuard&&) = delete;
};

// RAII guard for Value::set_default_int64() / drop_default_int64()
// Note: Intentionally drops any existing default on construction to ensure
// a clean test state. This differs from OmitStringTagGuard which preserves
// the original value. Use this pattern when tests should start with no default.
// Ensures the default is dropped even if the test fails/throws.
class DefaultInt64Guard {
public:
    DefaultInt64Guard() { Value::drop_default_int64(); }
    ~DefaultInt64Guard() { Value::drop_default_int64(); }

    // Non-copyable, non-movable
    DefaultInt64Guard(const DefaultInt64Guard&) = delete;
    DefaultInt64Guard& operator=(const DefaultInt64Guard&) = delete;
    DefaultInt64Guard(DefaultInt64Guard&&) = delete;
    DefaultInt64Guard& operator=(DefaultInt64Guard&&) = delete;
};

// Maximum method name length constant (mirrors dispatcher_manager.cc)
// Used in security tests to verify length validation
constexpr size_t MAX_METHOD_NAME_LEN = 256;

} // namespace test
} // namespace iqxmlrpc

#endif // IQXMLRPC_TEST_UTILS_H

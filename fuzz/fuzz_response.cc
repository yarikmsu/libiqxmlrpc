// Fuzz target for XML-RPC response parsing
// Copyright (C) 2026 libiqxmlrpc contributors

#include "libiqxmlrpc/response.h"
#include "libiqxmlrpc/value.h"
#include <cstdint>
#include <cstddef>
#include <string>

// Recursively exercise all Value type conversions and accessors
static void exercise_value(const iqxmlrpc::Value& v, int depth = 0) {
  // Prevent stack overflow on deeply nested structures
  if (depth > 100) return;

  // Exercise type checking methods
  (void)v.is_nil();
  (void)v.is_int();
  (void)v.is_double();
  (void)v.is_bool();
  (void)v.is_string();
  (void)v.is_binary();
  (void)v.is_datetime();
  (void)v.is_array();
  (void)v.is_struct();

  // Exercise type conversion methods (each may throw on type mismatch)
  try { (void)v.get_int(); } catch (...) {}
  try { (void)v.get_double(); } catch (...) {}
  try { (void)v.get_bool(); } catch (...) {}
  try { (void)v.get_string(); } catch (...) {}
  try { (void)v.get_binary(); } catch (...) {}

  // Recursively exercise arrays
  if (v.is_array()) {
    try {
      size_t sz = v.size();
      for (size_t i = 0; i < sz && i < 1000; ++i) {
        exercise_value(v[i], depth + 1);
      }
    } catch (...) {}
  }

  // Exercise struct member access
  if (v.is_struct()) {
    try {
      (void)v.size();
    } catch (...) {}
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > 64 * 1024) return 0;

  try {
    std::string input(reinterpret_cast<const char*>(data), size);
    iqxmlrpc::Response resp = iqxmlrpc::parse_response(input);

    // Exercise the parsed response
    (void)resp.is_fault();
    if (resp.is_fault()) {
      (void)resp.fault_code();
      (void)resp.fault_string();
    } else {
      // Deep traversal of response value
      exercise_value(resp.value());
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }
  return 0;
}

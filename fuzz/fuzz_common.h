// Common utilities for fuzz targets
// Copyright (C) 2026 libiqxmlrpc contributors

#ifndef IQXMLRPC_FUZZ_COMMON_H
#define IQXMLRPC_FUZZ_COMMON_H

#include "libiqxmlrpc/value.h"

namespace fuzz {

// Maximum recursion depth to prevent stack overflow
constexpr int MAX_DEPTH = 100;

// Maximum input size to prevent slow units
constexpr size_t MAX_INPUT_SIZE = 64 * 1024;

// Maximum array elements to iterate (prevents slow units on huge arrays)
constexpr size_t MAX_ARRAY_ELEMENTS = 1000;

// Recursively exercise all Value type conversions and accessors
// This ensures fuzzing covers the entire Value API surface
inline void exercise_value(const iqxmlrpc::Value& v, int depth = 0) {
  // Prevent stack overflow on deeply nested structures
  if (depth > MAX_DEPTH) return;

  // Exercise all type checking methods
  (void)v.is_nil();
  (void)v.is_int();
  (void)v.is_int64();
  (void)v.is_double();
  (void)v.is_bool();
  (void)v.is_string();
  (void)v.is_binary();
  (void)v.is_datetime();
  (void)v.is_array();
  (void)v.is_struct();

  // Exercise all type conversion methods (each may throw on type mismatch)
  try { (void)v.get_int(); } catch (...) {}
  try { (void)v.get_int64(); } catch (...) {}
  try { (void)v.get_double(); } catch (...) {}
  try { (void)v.get_bool(); } catch (...) {}
  try { (void)v.get_string(); } catch (...) {}
  try { (void)v.get_binary(); } catch (...) {}
  try { (void)v.get_datetime(); } catch (...) {}

  // Recursively exercise arrays
  if (v.is_array()) {
    try {
      size_t sz = v.size();
      for (size_t i = 0; i < sz && i < MAX_ARRAY_ELEMENTS; ++i) {
        exercise_value(v[i], depth + 1);
      }
    } catch (...) {}
  }

  // Exercise struct member access - iterate all members
  if (v.is_struct()) {
    try {
      (void)v.size();
      const iqxmlrpc::Struct& s = v.the_struct();
      size_t count = 0;
      for (auto it = s.begin(); it != s.end() && count < MAX_ARRAY_ELEMENTS; ++it, ++count) {
        exercise_value(*(it->second), depth + 1);
      }
    } catch (...) {}
  }
}

} // namespace fuzz

#endif // IQXMLRPC_FUZZ_COMMON_H

// Fuzz target for XML-RPC Value type handling
// Copyright (C) 2026 libiqxmlrpc contributors

#include "libiqxmlrpc/value.h"
#include "libiqxmlrpc/request.h"
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
  try { (void)v.get_datetime(); } catch (...) {}

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
      // Try accessing with common member names
      try { exercise_value(v["faultCode"], depth + 1); } catch (...) {}
      try { exercise_value(v["faultString"], depth + 1); } catch (...) {}
      try { exercise_value(v["name"], depth + 1); } catch (...) {}
      try { exercise_value(v["value"], depth + 1); } catch (...) {}
    } catch (...) {}
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > 64 * 1024) return 0;

  // Wrap input in a minimal XML-RPC request to get Value parsing
  std::string input(reinterpret_cast<const char*>(data), size);
  std::string wrapped =
    "<?xml version=\"1.0\"?><methodCall><methodName>x</methodName>"
    "<params><param><value>" + input + "</value></param></params></methodCall>";

  try {
    iqxmlrpc::Request* req = iqxmlrpc::parse_request(wrapped);
    if (req) {
      const iqxmlrpc::Param_list& params = req->get_params();
      for (size_t i = 0; i < params.size(); ++i) {
        exercise_value(params[i]);
      }
      delete req;
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  return 0;
}

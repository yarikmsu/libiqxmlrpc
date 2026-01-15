// Fuzz target for number conversion functions
// Copyright (C) 2026 libiqxmlrpc contributors
//
// This fuzzer tests the num_conv namespace functions which parse
// numbers from strings. These are exposed to untrusted XML content.

#include "fuzz_common.h"
#include "libiqxmlrpc/num_conv.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <limits>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Test from_string<int>
  try {
    int val = iqxmlrpc::num_conv::from_string<int>(input);
    // Round-trip test
    std::string back = iqxmlrpc::num_conv::to_string(val);
    (void)back;
  } catch (...) {
    // Exceptions are expected for invalid input
  }

  // Test from_string<int64_t>
  try {
    int64_t val = iqxmlrpc::num_conv::from_string<int64_t>(input);
    // Round-trip test
    std::string back = iqxmlrpc::num_conv::to_string(val);
    (void)back;
  } catch (...) {
    // Exceptions are expected for invalid input
  }

  // Test from_string<uint32_t>
  try {
    uint32_t val = iqxmlrpc::num_conv::from_string<uint32_t>(input);
    std::string back = iqxmlrpc::num_conv::to_string(val);
    (void)back;
  } catch (...) {
    // Exceptions are expected for invalid input
  }

  // Test from_string<uint64_t>
  try {
    uint64_t val = iqxmlrpc::num_conv::from_string<uint64_t>(input);
    std::string back = iqxmlrpc::num_conv::to_string(val);
    (void)back;
  } catch (...) {
    // Exceptions are expected for invalid input
  }

  // Test string_to_double
  try {
    double val = iqxmlrpc::num_conv::string_to_double(input);
    // Round-trip test
    std::string back = iqxmlrpc::num_conv::double_to_string(val);
    (void)back;
  } catch (...) {
    // Exceptions are expected for invalid input
  }

  // Test to_string with boundary values constructed from input
  if (size >= 4) {
    try {
      int32_t val = static_cast<int32_t>(data[0]) |
                    (static_cast<int32_t>(data[1]) << 8) |
                    (static_cast<int32_t>(data[2]) << 16) |
                    (static_cast<int32_t>(data[3]) << 24);
      std::string str = iqxmlrpc::num_conv::to_string(val);
      int32_t back = iqxmlrpc::num_conv::from_string<int32_t>(str);
      (void)back;
    } catch (...) {}
  }

  if (size >= 8) {
    try {
      int64_t val = static_cast<int64_t>(data[0]) |
                    (static_cast<int64_t>(data[1]) << 8) |
                    (static_cast<int64_t>(data[2]) << 16) |
                    (static_cast<int64_t>(data[3]) << 24) |
                    (static_cast<int64_t>(data[4]) << 32) |
                    (static_cast<int64_t>(data[5]) << 40) |
                    (static_cast<int64_t>(data[6]) << 48) |
                    (static_cast<int64_t>(data[7]) << 56);
      std::string str = iqxmlrpc::num_conv::to_string(val);
      int64_t back = iqxmlrpc::num_conv::from_string<int64_t>(str);
      (void)back;
    } catch (...) {}

    // Test double conversion with raw bits
    try {
      double val;
      static_assert(sizeof(double) == 8, "double must be 8 bytes");
      std::memcpy(&val, data, sizeof(double));

      // Skip NaN and infinity which have special representation
      if (std::isfinite(val)) {
        std::string str = iqxmlrpc::num_conv::double_to_string(val);
        double back = iqxmlrpc::num_conv::string_to_double(str);
        (void)back;
      }
    } catch (...) {}
  }

  return 0;
}

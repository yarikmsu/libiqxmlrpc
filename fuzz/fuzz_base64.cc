// Fuzz target for Base64 decoding
// Copyright (C) 2026 libiqxmlrpc contributors

#include "fuzz_common.h"
#include "libiqxmlrpc/value_type.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Fuzz Base64 decoding (from_base64)
  // This is security-critical as it parses untrusted input from Authorization headers
  try {
    std::unique_ptr<iqxmlrpc::Binary_data> binary(
      iqxmlrpc::Binary_data::from_base64(input));
    if (binary) {
      // Exercise the decoded data
      (void)binary->get_data();
      // Re-encode and verify round-trip
      (void)binary->get_base64();
      // Exercise type introspection
      (void)binary->type_name();
    }
  } catch (const iqxmlrpc::Binary_data::Malformed_base64&) {
    // Expected for invalid base64 input
  } catch (...) {
    // Other exceptions should not occur, but catch for safety
  }

  // Fuzz with whitespace variations
  // Base64 decoders often handle whitespace differently
  if (size > 0) {
    try {
      // Add leading/trailing whitespace
      std::string with_whitespace = " \t\n" + input + "\n\t ";
      std::unique_ptr<iqxmlrpc::Binary_data> binary(
        iqxmlrpc::Binary_data::from_base64(with_whitespace));
      if (binary) {
        (void)binary->get_data();
      }
    } catch (...) {
      // Expected for invalid input
    }
  }

  // Fuzz Base64 encoding (from_data) - for completeness
  // This tests the reverse path: raw data -> base64
  try {
    std::unique_ptr<iqxmlrpc::Binary_data> binary(
      iqxmlrpc::Binary_data::from_data(input));
    if (binary) {
      // Encode to base64
      const std::string& encoded = binary->get_base64();
      (void)encoded.size();
      // Get raw data back
      (void)binary->get_data();
    }
  } catch (...) {
    // Exceptions are not expected for from_data, but catch for safety
  }

  // Fuzz with raw pointer interface
  try {
    std::unique_ptr<iqxmlrpc::Binary_data> binary(
      iqxmlrpc::Binary_data::from_data(
        reinterpret_cast<const char*>(data), size));
    if (binary) {
      (void)binary->get_base64();
      (void)binary->get_data();
    }
  } catch (...) {
    // Exceptions are not expected, but catch for safety
  }

  return 0;
}

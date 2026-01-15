// Fuzz target for XML-RPC Value type handling
// Copyright (C) 2026 libiqxmlrpc contributors

#include "fuzz_common.h"
#include "libiqxmlrpc/request.h"
#include "libiqxmlrpc/value_type.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>

// XML wrapper overhead: ~130 bytes for the methodCall envelope
constexpr size_t XML_WRAPPER_OVERHEAD = 200;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  // Account for XML wrapper overhead to prevent oversized wrapped strings
  if (size > fuzz::MAX_INPUT_SIZE - XML_WRAPPER_OVERHEAD) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Test 1: Wrap input in a minimal XML-RPC request to get Value parsing
  std::string wrapped =
    "<?xml version=\"1.0\"?><methodCall><methodName>x</methodName>"
    "<params><param><value>" + input + "</value></param></params></methodCall>";

  try {
    std::unique_ptr<iqxmlrpc::Request> req(iqxmlrpc::parse_request(wrapped));
    if (req) {
      const iqxmlrpc::Param_list& params = req->get_params();
      for (size_t i = 0; i < params.size(); ++i) {
        fuzz::exercise_value(params[i]);
      }
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Test 2: Direct Date_time string parsing
  // This exercises the ISO8601 parser directly without XML wrapping
  try {
    iqxmlrpc::Date_time dt(input);
    // Exercise the Date_time accessors
    (void)dt.to_string();
    (void)dt.get_tm();
  } catch (...) {
    // Exceptions are expected for malformed date strings
  }

  // Test 3: Date_time with common date formats
  // Try multiple interpretations of the input as date components
  if (size >= 8) {
    try {
      // Construct an ISO8601-like string from input bytes
      char date_buf[32];
      std::snprintf(date_buf, sizeof(date_buf),
                    "%04d%02d%02dT%02d:%02d:%02d",
                    1900 + (data[0] % 200),  // Year 1900-2099
                    1 + (data[1] % 12),       // Month 1-12
                    1 + (data[2] % 28),       // Day 1-28
                    data[3] % 24,             // Hour 0-23
                    data[4] % 60,             // Minute 0-59
                    data[5] % 60);            // Second 0-59

      std::string date_str(date_buf);
      iqxmlrpc::Date_time dt(date_str);
      (void)dt.to_string();
      (void)dt.get_tm();
    } catch (...) {
      // Exceptions are expected
    }
  }

  // Test 4: Binary_data type construction and manipulation
  if (size > 0) {
    try {
      // from_data creates Binary_data from raw bytes
      std::unique_ptr<iqxmlrpc::Binary_data> bin(
          iqxmlrpc::Binary_data::from_data(
              reinterpret_cast<const char*>(data), size));
      if (bin) {
        (void)bin->get_data();
        (void)bin->get_base64();
      }
    } catch (...) {
      // Exceptions are expected for invalid input
    }

    // Also test from_base64 path
    try {
      std::unique_ptr<iqxmlrpc::Binary_data> bin(
          iqxmlrpc::Binary_data::from_base64(input));
      if (bin) {
        (void)bin->get_data();
        (void)bin->get_base64();
      }
    } catch (...) {
      // Exceptions are expected for invalid base64
    }
  }

  return 0;
}

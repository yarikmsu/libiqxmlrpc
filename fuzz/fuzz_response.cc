// Fuzz target for XML-RPC response parsing
// Copyright (C) 2024 libiqxmlrpc contributors

#include "libiqxmlrpc/response.h"
#include <cstdint>
#include <cstddef>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  try {
    std::string input(reinterpret_cast<const char*>(data), size);
    iqxmlrpc::Response resp = iqxmlrpc::parse_response(input);
    // Exercise the parsed response
    (void)resp.is_fault();
    if (resp.is_fault()) {
      (void)resp.fault_code();
      (void)resp.fault_string();
    } else {
      (void)resp.value();
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }
  return 0;
}

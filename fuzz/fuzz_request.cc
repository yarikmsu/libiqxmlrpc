// Fuzz target for XML-RPC request parsing
// Copyright (C) 2026 libiqxmlrpc contributors

#include "fuzz_common.h"
#include "libiqxmlrpc/request.h"
#include <cstdint>
#include <cstddef>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  try {
    std::string input(reinterpret_cast<const char*>(data), size);
    iqxmlrpc::Request* req = iqxmlrpc::parse_request(input);
    if (req) {
      // Exercise the parsed request
      (void)req->get_name();
      const iqxmlrpc::Param_list& params = req->get_params();

      // Deep traversal of all parameters
      for (size_t i = 0; i < params.size(); ++i) {
        fuzz::exercise_value(params[i]);
      }
      delete req;
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }
  return 0;
}

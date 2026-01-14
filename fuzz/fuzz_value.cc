// Fuzz target for XML-RPC Value type handling
// Copyright (C) 2026 libiqxmlrpc contributors

#include "fuzz_common.h"
#include "libiqxmlrpc/request.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  // Wrap input in a minimal XML-RPC request to get Value parsing
  std::string input(reinterpret_cast<const char*>(data), size);
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

  return 0;
}

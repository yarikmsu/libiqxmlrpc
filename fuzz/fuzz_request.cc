// Fuzz target for XML-RPC request parsing
// Copyright (C) 2024 libiqxmlrpc contributors

#include "libiqxmlrpc/request.h"
#include <cstdint>
#include <cstddef>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  try {
    std::string input(reinterpret_cast<const char*>(data), size);
    iqxmlrpc::Request* req = iqxmlrpc::parse_request(input);
    if (req) {
      // Exercise the parsed request
      (void)req->get_name();
      (void)req->get_params();
      delete req;
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }
  return 0;
}

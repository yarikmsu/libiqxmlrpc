// Fuzz target for HTTP header parsing
// Copyright (C) 2024 libiqxmlrpc contributors

#include "libiqxmlrpc/http.h"
#include <cstdint>
#include <cstddef>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);

  // Fuzz HTTP request header parsing
  try {
    iqxmlrpc::http::Request_header req_hdr(
      iqxmlrpc::http::HTTP_CHECK_WEAK, input);
    (void)req_hdr.uri();
    (void)req_hdr.content_length();
    (void)req_hdr.conn_keep_alive();
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz HTTP response header parsing
  try {
    iqxmlrpc::http::Response_header resp_hdr(
      iqxmlrpc::http::HTTP_CHECK_WEAK, input);
    (void)resp_hdr.code();
    (void)resp_hdr.phrase();
    (void)resp_hdr.content_length();
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  return 0;
}

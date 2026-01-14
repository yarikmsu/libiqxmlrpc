// Fuzz target for HTTP header parsing
// Copyright (C) 2026 libiqxmlrpc contributors

#include "libiqxmlrpc/http.h"
#include <cstdint>
#include <cstddef>
#include <string>

// Maximum input size to prevent slow units
constexpr size_t MAX_INPUT_SIZE = 64 * 1024;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > MAX_INPUT_SIZE) return 0;

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

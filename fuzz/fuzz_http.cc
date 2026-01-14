// Fuzz target for HTTP header parsing
// Copyright (C) 2026 libiqxmlrpc contributors

#include "fuzz_common.h"
#include "libiqxmlrpc/http.h"
#include <cstdint>
#include <cstddef>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Fuzz HTTP request header parsing (weak verification)
  try {
    iqxmlrpc::http::Request_header req_hdr(
      iqxmlrpc::http::HTTP_CHECK_WEAK, input);
    (void)req_hdr.uri();
    (void)req_hdr.content_length();
    (void)req_hdr.conn_keep_alive();
    (void)req_hdr.host();
    (void)req_hdr.agent();
    // Exercise auth parsing - security-critical Base64 decoding
    if (req_hdr.has_authinfo()) {
      std::string user, password;
      req_hdr.get_authinfo(user, password);
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz HTTP request header parsing (strict verification)
  try {
    iqxmlrpc::http::Request_header req_hdr_strict(
      iqxmlrpc::http::HTTP_CHECK_STRICT, input);
    (void)req_hdr_strict.uri();
    (void)req_hdr_strict.content_length();
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz HTTP response header parsing (weak verification)
  try {
    iqxmlrpc::http::Response_header resp_hdr(
      iqxmlrpc::http::HTTP_CHECK_WEAK, input);
    (void)resp_hdr.code();
    (void)resp_hdr.phrase();
    (void)resp_hdr.content_length();
    (void)resp_hdr.server();
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz HTTP response header parsing (strict verification)
  try {
    iqxmlrpc::http::Response_header resp_hdr_strict(
      iqxmlrpc::http::HTTP_CHECK_STRICT, input);
    (void)resp_hdr_strict.code();
    (void)resp_hdr_strict.phrase();
    (void)resp_hdr_strict.content_length();
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  return 0;
}

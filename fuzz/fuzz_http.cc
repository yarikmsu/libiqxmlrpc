// Fuzz target for HTTP header parsing
// Copyright (C) 2026 libiqxmlrpc contributors

#include "fuzz_common.h"
#include "libiqxmlrpc/http.h"
#include "libiqxmlrpc/xheaders.h"
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
    (void)req_hdr.expect_continue();
    // Exercise auth parsing - security-critical Base64 decoding
    if (req_hdr.has_authinfo()) {
      std::string user, password;
      req_hdr.get_authinfo(user, password);
    }
    // Exercise custom header parsing
    iqxmlrpc::XHeaders xhdrs;
    req_hdr.get_headers(xhdrs);
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz set_authinfo - tests Base64 encoding construction
  // This exercises the path where auth info is SET rather than GET
  if (size > 1) {
    try {
      iqxmlrpc::http::Request_header req_hdr("/RPC2", "localhost", 8080);
      // Split input into user and password
      size_t split = data[0] % size;
      std::string user(reinterpret_cast<const char*>(data + 1),
                       split > 0 ? split - 1 : 0);
      std::string password;
      if (split + 1 < size) {
        password = std::string(reinterpret_cast<const char*>(data + split),
                               size - split);
      }
      // Set auth info (constructs Base64 internally)
      req_hdr.set_authinfo(user, password);
      // Verify round-trip
      if (req_hdr.has_authinfo()) {
        std::string out_user, out_pass;
        req_hdr.get_authinfo(out_user, out_pass);
      }
      // Dump the header to exercise serialization
      (void)req_hdr.dump();
    } catch (...) {
      // Exceptions are expected for malformed input
    }
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
    (void)resp_hdr.conn_keep_alive();
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

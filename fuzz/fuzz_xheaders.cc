// Fuzz target for XHeaders validation
// Copyright (C) 2026 libiqxmlrpc contributors
//
// This fuzzer tests the XHeaders::validate() static method and
// the XHeaders container operations with arbitrary input.

#include "fuzz_common.h"
#include "libiqxmlrpc/xheaders.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <map>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Test XHeaders::validate() with fuzzed input
  try {
    bool valid = iqxmlrpc::XHeaders::validate(input);
    (void)valid;
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Test XHeaders container operations
  try {
    iqxmlrpc::XHeaders xheaders;

    // Split input into key-value pairs
    if (size > 1) {
      size_t split = data[0] % size;
      std::string key(reinterpret_cast<const char*>(data + 1), split);
      std::string value;
      if (split + 1 < size) {
        value = std::string(reinterpret_cast<const char*>(data + 1 + split), size - 1 - split);
      }

      // Try to set the header (may throw for invalid values)
      if (iqxmlrpc::XHeaders::validate(value)) {
        xheaders[key] = value;
      }
    }

    // Exercise container methods
    (void)xheaders.size();
    for (auto it = xheaders.begin(); it != xheaders.end(); ++it) {
      (void)it->first;
      (void)it->second;
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Test assignment from map
  try {
    std::map<std::string, std::string> map_input;

    // Parse input as multiple key-value pairs
    size_t offset = 0;
    size_t count = 0;
    while (offset < size && count < 10) {
      if (offset >= size) break;
      size_t key_len = data[offset] % 32;
      offset++;

      if (offset + key_len >= size) break;
      std::string key(reinterpret_cast<const char*>(data + offset), key_len);
      offset += key_len;

      if (offset >= size) break;
      size_t val_len = data[offset] % 64;
      offset++;

      if (offset + val_len > size) val_len = size - offset;
      std::string val(reinterpret_cast<const char*>(data + offset), val_len);
      offset += val_len;

      // Only add if value is valid
      if (iqxmlrpc::XHeaders::validate(val)) {
        map_input[key] = val;
      }
      count++;
    }

    iqxmlrpc::XHeaders xheaders;
    xheaders = map_input;

    // Exercise find with various keys
    for (const auto& kv : map_input) {
      auto it = xheaders.find(kv.first);
      (void)it;
    }

    // Test find with non-existent key
    auto it = xheaders.find("__nonexistent__");
    (void)(it == xheaders.end());
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  return 0;
}

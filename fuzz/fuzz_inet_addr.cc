// Fuzz target for Inet_addr hostname/IP parsing
// Copyright (C) 2026 libiqxmlrpc contributors
//
// This fuzzer tests the Inet_addr class which parses hostnames and IP addresses.
// It avoids actual DNS lookups by using:
// - IP address strings (127.x.x.x, 10.x.x.x, 192.168.x.x)
// - The loopback hostname "localhost"
// - Invalid hostnames that should be rejected
//
// Security relevance: Hostname parsing can be vulnerable to buffer overflows
// and injection attacks (CRLF injection in hostnames).

#include "fuzz_common.h"
#include "libiqxmlrpc/inet_addr.h"
#include <cstdint>
#include <cstddef>
#include <string>

namespace {

// Helper to extract a port number from two fuzz bytes (little-endian)
int extract_port(const uint8_t* data) {
  return (data[0] | (data[1] << 8)) % 65536;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;
  if (size == 0) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Test 1: Parse input directly as hostname with fuzzed port
  // This tests CRLF injection rejection and hostname validation
  {
    int port = (size >= 2) ? extract_port(data) : 80;

    try {
      iqnet::Inet_addr addr(input, port);
      // If parsing succeeded, exercise accessors
      (void)addr.get_host_name();
      (void)addr.get_port();
      (void)addr.get_sockaddr();
    } catch (...) {
      // Exceptions are expected for invalid hostnames
      // (CRLF injection, invalid characters, DNS failures)
    }
  }

  // Test 2: Construct IP address string from fuzz bytes (avoids DNS)
  // This exercises the IP parsing path without network calls
  if (size >= 4) {
    try {
      char ip_buf[24];
      std::snprintf(ip_buf, sizeof(ip_buf), "%d.%d.%d.%d",
                    data[0], data[1], data[2], data[3]);

      int port = (size >= 6) ? extract_port(data + 4) : 8080;

      iqnet::Inet_addr addr(std::string(ip_buf), port);
      (void)addr.get_host_name();
      (void)addr.get_port();
      (void)addr.get_sockaddr();
    } catch (...) {
      // Some IP addresses may fail (e.g., 0.0.0.0)
    }
  }

  // Test 3: Test port-only constructor
  if (size >= 2) {
    try {
      int port = extract_port(data);
      iqnet::Inet_addr addr(port);
      (void)addr.get_port();
      (void)addr.get_sockaddr();
    } catch (...) {
      // Exceptions for invalid ports
    }
  }

  // Test 4: Test with known-safe loopback addresses (no DNS)
  // This ensures we test the parsing paths without network effects
  {
    static const char* safe_hosts[] = {
      "127.0.0.1",
      "127.0.0.2",
      "127.255.255.254",
      "0.0.0.0",
      "255.255.255.255",
    };

    if (size >= 1) {
      size_t idx = data[0] % (sizeof(safe_hosts) / sizeof(safe_hosts[0]));
      int port = (size >= 3) ? extract_port(data + 1) : 8080;

      try {
        iqnet::Inet_addr addr(safe_hosts[idx], port);
        (void)addr.get_host_name();
        (void)addr.get_port();
      } catch (...) {
        // Some may fail
      }
    }
  }

  // Test 5: Test copy construction
  if (size >= 4) {
    try {
      char ip_buf[24];
      std::snprintf(ip_buf, sizeof(ip_buf), "10.%d.%d.%d",
                    data[0] % 256, data[1] % 256, data[2] % 256);

      iqnet::Inet_addr addr1(std::string(ip_buf), data[3] % 65536);
      iqnet::Inet_addr addr2(addr1);  // Copy construction
      (void)addr2.get_host_name();
      (void)addr2.get_port();
    } catch (...) {
      // Expected for some inputs
    }
  }

  // Test 6: Test hostname injection patterns (should be rejected)
  // These test security-critical validation
  {
    static const char* injection_patterns[] = {
      "host\nname",      // Newline injection
      "host\rname",      // CR injection
      "host\r\nname",    // CRLF injection
      "host\x00name",    // Null byte injection
      "host name",       // Space in hostname
      "-hostname",       // Leading dash
      "hostname-",       // Trailing dash (valid)
      ".hostname",       // Leading dot
      "hostname.",       // Trailing dot (may be valid)
      "host..name",      // Double dot
    };

    if (size >= 1) {
      size_t idx = data[0] % (sizeof(injection_patterns) / sizeof(injection_patterns[0]));
      try {
        iqnet::Inet_addr addr(injection_patterns[idx], 80);
        // If it didn't throw, exercise accessors
        (void)addr.get_host_name();
      } catch (...) {
        // Most should throw - that's the expected secure behavior
      }
    }
  }

  return 0;
}

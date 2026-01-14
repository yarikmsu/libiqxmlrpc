// Fuzz target for HTTP packet streaming parser
// Copyright (C) 2026 libiqxmlrpc contributors

#include "fuzz_common.h"
#include "libiqxmlrpc/http.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Fuzz Packet_reader for HTTP request parsing (weak verification)
  // This tests incremental/streaming packet construction
  try {
    iqxmlrpc::http::Packet_reader reader;
    reader.set_verification_level(iqxmlrpc::http::HTTP_CHECK_WEAK);
    reader.set_max_size(fuzz::MAX_INPUT_SIZE);

    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader.read_request(input));
    if (pkt) {
      (void)pkt->header();
      (void)pkt->content();
      (void)pkt->dump();
    }
    // Check if reader expects 100-continue
    (void)reader.expect_continue();
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz Packet_reader for HTTP request parsing (strict verification)
  try {
    iqxmlrpc::http::Packet_reader reader_strict;
    reader_strict.set_verification_level(iqxmlrpc::http::HTTP_CHECK_STRICT);
    reader_strict.set_max_size(fuzz::MAX_INPUT_SIZE);

    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader_strict.read_request(input));
    if (pkt) {
      (void)pkt->header();
      (void)pkt->content();
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz Packet_reader for HTTP response parsing (weak, full packet)
  try {
    iqxmlrpc::http::Packet_reader reader;
    reader.set_verification_level(iqxmlrpc::http::HTTP_CHECK_WEAK);
    reader.set_max_size(fuzz::MAX_INPUT_SIZE);

    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader.read_response(input, false));
    if (pkt) {
      (void)pkt->header();
      (void)pkt->content();
      (void)pkt->dump();
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz Packet_reader for HTTP response parsing (weak, header only)
  try {
    iqxmlrpc::http::Packet_reader reader;
    reader.set_verification_level(iqxmlrpc::http::HTTP_CHECK_WEAK);
    reader.set_max_size(fuzz::MAX_INPUT_SIZE);

    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader.read_response(input, true));
    if (pkt) {
      (void)pkt->header();
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz Packet_reader for HTTP response parsing (strict verification)
  try {
    iqxmlrpc::http::Packet_reader reader_strict;
    reader_strict.set_verification_level(iqxmlrpc::http::HTTP_CHECK_STRICT);
    reader_strict.set_max_size(fuzz::MAX_INPUT_SIZE);

    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader_strict.read_response(input, false));
    if (pkt) {
      (void)pkt->header();
      (void)pkt->content();
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Test incremental parsing by splitting input into chunks
  // This simulates real network conditions where data arrives in pieces
  if (size > 1) {
    try {
      iqxmlrpc::http::Packet_reader reader;
      reader.set_verification_level(iqxmlrpc::http::HTTP_CHECK_WEAK);
      reader.set_max_size(fuzz::MAX_INPUT_SIZE);

      // Feed data incrementally (byte-by-byte)
      for (size_t i = 1; i <= size; ++i) {
        std::string partial(reinterpret_cast<const char*>(data), i);
        std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader.read_request(partial));
        if (pkt) {
          // Packet is complete
          (void)pkt->header();
          (void)pkt->content();
          break;
        }
      }
    } catch (...) {
      // Exceptions are expected for malformed input
    }
  }

  // Test with random chunk boundaries using input bytes as entropy
  // This catches bugs that only manifest with specific split points
  if (size > 4) {
    try {
      iqxmlrpc::http::Packet_reader reader;
      reader.set_verification_level(iqxmlrpc::http::HTTP_CHECK_WEAK);
      reader.set_max_size(fuzz::MAX_INPUT_SIZE);

      // Use first bytes as chunk size seeds
      size_t chunk_size = (data[0] % 64) + 1;  // 1-64 byte chunks
      size_t offset = 0;

      while (offset < size) {
        size_t end = offset + chunk_size;
        if (end > size) end = size;

        std::string partial(reinterpret_cast<const char*>(data), end);
        std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader.read_request(partial));
        if (pkt) {
          (void)pkt->header();
          (void)pkt->content();
          break;
        }

        offset = end;
        // Vary chunk size using next byte
        if (offset < size) {
          chunk_size = (data[offset] % 64) + 1;
        }
      }
    } catch (...) {
      // Exceptions are expected for malformed input
    }
  }

  // Test Packet_reader reuse - parse multiple packets sequentially
  // This tests state reset behavior between packets
  if (size > 2) {
    try {
      iqxmlrpc::http::Packet_reader reader;
      reader.set_verification_level(iqxmlrpc::http::HTTP_CHECK_WEAK);
      reader.set_max_size(fuzz::MAX_INPUT_SIZE);

      // First packet attempt
      std::unique_ptr<iqxmlrpc::http::Packet> pkt1(reader.read_request(input));
      if (pkt1) {
        (void)pkt1->header();
        (void)pkt1->content();
      }

      // Reuse reader for second packet (split input differently)
      size_t split = size / 2;
      std::string second_input(reinterpret_cast<const char*>(data + split), size - split);
      std::unique_ptr<iqxmlrpc::http::Packet> pkt2(reader.read_request(second_input));
      if (pkt2) {
        (void)pkt2->header();
        (void)pkt2->content();
      }
    } catch (...) {
      // Exceptions are expected for malformed input
    }
  }

  return 0;
}

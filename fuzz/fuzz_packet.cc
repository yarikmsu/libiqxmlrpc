// Fuzz target for HTTP packet streaming parser
// Copyright (C) 2026 libiqxmlrpc contributors

#include "fuzz_common.h"
#include "libiqxmlrpc/http.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

namespace {

// Helper to create a configured Packet_reader, reducing setup duplication
std::unique_ptr<iqxmlrpc::http::Packet_reader> create_reader(
    iqxmlrpc::http::Verification_level level,
    size_t max_size = fuzz::MAX_INPUT_SIZE) {
  auto reader = std::make_unique<iqxmlrpc::http::Packet_reader>();
  reader->set_verification_level(level);
  reader->set_max_size(max_size);
  return reader;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Fuzz Packet_reader for HTTP request parsing (weak verification)
  // This tests incremental/streaming packet construction
  try {
    auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK);
    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_request(input));
    if (pkt) {
      (void)pkt->header();
      (void)pkt->content();
      (void)pkt->dump();
    }
    // Check if reader expects 100-continue
    (void)reader->expect_continue();
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz Packet_reader with continue_sent state toggled
  // This exercises the 100-continue response path
  try {
    auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK);
    reader->set_continue_sent();  // Toggle continue_sent state before parsing
    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_request(input));
    if (pkt) {
      (void)pkt->header();
      (void)pkt->content();
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz Packet_reader for HTTP request parsing (strict verification)
  try {
    auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_STRICT);
    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_request(input));
    if (pkt) {
      (void)pkt->header();
      (void)pkt->content();
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz Packet_reader for HTTP response parsing (weak, full packet)
  try {
    auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK);
    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_response(input, false));
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
    auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK);
    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_response(input, true));
    if (pkt) {
      (void)pkt->header();
    }
  } catch (...) {
    // Exceptions are expected for malformed input
  }

  // Fuzz Packet_reader for HTTP response parsing (strict verification)
  try {
    auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_STRICT);
    std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_response(input, false));
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
      auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK);
      // Feed data incrementally (byte-by-byte)
      for (size_t i = 1; i <= size; ++i) {
        std::string partial(reinterpret_cast<const char*>(data), i);
        std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_request(partial));
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
      auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK);
      // Use first bytes as chunk size seeds
      size_t chunk_size = (data[0] % 64) + 1;  // 1-64 byte chunks
      size_t offset = 0;

      while (offset < size) {
        size_t end = offset + chunk_size;
        if (end > size) end = size;

        std::string partial(reinterpret_cast<const char*>(data), end);
        std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_request(partial));
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
      auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK);
      // First packet attempt
      std::unique_ptr<iqxmlrpc::http::Packet> pkt1(reader->read_request(input));
      if (pkt1) {
        (void)pkt1->header();
        (void)pkt1->content();
      }

      // Reuse reader for second packet (split input differently)
      size_t split = size / 2;
      std::string second_input(reinterpret_cast<const char*>(data + split), size - split);
      std::unique_ptr<iqxmlrpc::http::Packet> pkt2(reader->read_request(second_input));
      if (pkt2) {
        (void)pkt2->header();
        (void)pkt2->content();
      }
    } catch (...) {
      // Exceptions are expected for malformed input
    }
  }

  // Test chunked transfer-encoding body parsing
  // This constructs HTTP requests with chunked bodies using fuzz input
  if (size > 10) {
    try {
      // Construct a chunked HTTP request
      std::string chunked_request =
          "POST /RPC2 HTTP/1.1\r\n"
          "Host: localhost\r\n"
          "Transfer-Encoding: chunked\r\n"
          "\r\n";

      // Add chunk(s) from fuzz input
      size_t chunk_offset = 0;
      size_t chunk_count = 0;
      while (chunk_offset < size && chunk_count < 10) {
        // Use first byte at offset to determine chunk size
        size_t chunk_size = data[chunk_offset] % 256;
        chunk_offset++;

        if (chunk_offset + chunk_size > size) {
          chunk_size = size - chunk_offset;
        }

        // Write chunk header (hex size)
        char chunk_header[32];
        std::snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", chunk_size);
        chunked_request += chunk_header;

        // Write chunk data
        if (chunk_size > 0) {
          chunked_request.append(reinterpret_cast<const char*>(data + chunk_offset), chunk_size);
          chunk_offset += chunk_size;
        }

        // Write chunk terminator
        chunked_request += "\r\n";
        chunk_count++;

        // If chunk_size was 0, that's the final chunk
        if (chunk_size == 0) break;
      }

      // Add final chunk if not already added
      if (chunk_count > 0 && chunked_request.find("0\r\n\r\n") == std::string::npos) {
        chunked_request += "0\r\n\r\n";
      }

      auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK, fuzz::MAX_INPUT_SIZE * 2);
      std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_request(chunked_request));
      if (pkt) {
        (void)pkt->header();
        (void)pkt->content();
        (void)pkt->dump();
      }
    } catch (...) {
      // Exceptions expected for malformed chunked encoding
    }
  }

  // Test raw chunked input (not constructed, direct fuzz)
  // This catches issues with malformed chunk headers
  if (size > 0) {
    try {
      // Prepend a minimal chunked request header to fuzz input
      std::string raw_chunked =
          "POST /RPC2 HTTP/1.1\r\n"
          "Host: localhost\r\n"
          "Transfer-Encoding: chunked\r\n"
          "\r\n" + input;

      auto reader = create_reader(iqxmlrpc::http::HTTP_CHECK_WEAK, fuzz::MAX_INPUT_SIZE * 2);
      std::unique_ptr<iqxmlrpc::http::Packet> pkt(reader->read_request(raw_chunked));
      if (pkt) {
        (void)pkt->content();
      }
    } catch (...) {
      // Expected for malformed chunks
    }
  }

  return 0;
}

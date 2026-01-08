# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Libiqxmlrpc is an object-oriented C++ XML-RPC library (v0.13.6) providing client/server API with HTTP/HTTPS transports, single-threaded and multi-threaded server execution models, and method interceptors. Licensed under BSD 2-Clause.

## Build Commands

```bash
# Standard build
mkdir build && cd build
cmake ..
cmake --build .

# Build with tests
cmake -Dbuild_tests=ON ..
cmake --build .

# Build with documentation
cmake -Dbuild_docs=ON ..
```

## Running Tests

Tests use Boost Unit Test Framework. After building with `-Dbuild_tests=ON`:

```bash
cd build
ctest                              # Run all tests
./tests/value-test                 # Run single test
./tests/server-test --log_level=all  # Verbose output
```

Available test executables: `value-test`, `server-test`, `client-test`, `client-stress-test`, `parser-test` (Unix only), `xheaders-test`.

## Architecture

### Core Namespaces
- `iqxmlrpc` - Main XML-RPC implementation
- `iqnet` - Low-level networking abstractions

### Key Components

**Value System** (`value.h`, `value_type.h`)
- Type-safe XML-RPC value wrapper supporting: `int`/`i4`, `int64_t`/`i8`, `bool`, `double`, `string`, `Array`, `Struct`, `Binary_data`, `Date_time`, `Nil`
- `Value` class provides proxy access with implicit conversions

**Server** (`server.h`, `http_server.h`, `https_server.h`)
- `Http_server`/`Https_server` - Concrete server implementations
- `Executor_factory_base` with `Serial_executor_factory` (single-threaded) or `Pool_executor_factory` (thread pool)
- `Method` base class for RPC method implementations
- `Interceptor` for cross-cutting concerns (logging, auth)

**Client** (`client.h`, `http_client.h`, `https_client.h`)
- `Client<TRANSPORT>` template for HTTP/HTTPS clients
- `Client_connection` manages server connections

**Reactor/Event Loop** (`reactor.h`, `reactor_poll_impl.h`, `reactor_select_impl.h`)
- Platform-aware: uses `poll()` where available, falls back to `select()`
- Template-based locking with `boost::mutex` or `Null_lock`

**HTTP Transport** (`http.h`)
- `http::Packet`, `http::Request_header`, `http::Response_header`
- `http::Packet_reader` for parsing incoming HTTP data

**XML Parsing** (`parser2.h`, `value_parser.h`, `xml_builder.h`)
- Built on libxml2 xmlreader/xmlwriter

**SSL/TLS** (`ssl_lib.h`, `ssl_connection.h`)
- OpenSSL-based HTTPS support; SSLv2/SSLv3 disabled

### Dependencies
- **Boost** (≥1.41.0): `date_time`, `thread`, `system`
- **libxml2**: XML parsing
- **OpenSSL**: HTTPS support

### C++ Standard
Codebase uses C++11 (`CMAKE_CXX_STANDARD 11`). Uses `std::unique_ptr`, `std::move`, and other modern C++ features.

## Documentation

- Wiki: https://github.com/adedov/libiqxmlrpc/wiki
- Sample code in `doc/samples/` (server.cc, client.cc)

## Commit Guidelines

- Do not include `Co-Authored-By: Claude` in commit messages

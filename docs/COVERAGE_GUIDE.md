# Code Coverage Guide

## Coverage Targets
- **Lines:** 95%
- **Branches:** 60%

## Configuration

Coverage is configured via `codecov.yml`:
- Test files (`tests/**`) are excluded from coverage metrics
- Only `libiqxmlrpc/**` counts toward coverage totals
- `threshold: 1%` allows coverage to drop by up to 1% without failing

## Running Coverage Locally

```bash
cd build
cmake .. -DENABLE_COVERAGE=ON
make check

# Generate HTML report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
open coverage_html/index.html
```

## Effective Test Patterns

### 1. Error Path Testing

Target catch blocks and error handling:

```cpp
BOOST_AUTO_TEST_CASE(ssl_invalid_cert_path)
{
    bool exception_thrown = false;
    try {
        iqnet::ssl::Ctx::client_server("/invalid/path", "/invalid/key");
    } catch (...) {
        exception_thrown = true;
    }
    BOOST_CHECK(exception_thrown);
}
```

### 2. Network Error Simulation

Use non-routable IPs for timeouts:

```cpp
// 192.0.2.x (TEST-NET-1) won't route, perfect for timeout testing
Inet_addr addr("192.0.2.1", 12345);
client.set_timeout(1);  // Short timeout
```

### 3. Raw Socket Testing

Send malformed HTTP to trigger error paths:

```cpp
std::string send_raw_http(const std::string& host, int port, const std::string& data)
{
    iqnet::Socket sock;
    sock.connect(iqnet::Inet_addr(host, port));
    sock.send(data.c_str(), data.length());
    // ... recv response
}
```

### 4. Exception Type Verification

When you can't trigger the full path:

```cpp
BOOST_CHECK_NO_THROW({
    try {
        throw iqnet::Reactor_base::No_handlers();
    } catch (const std::exception& e) {
        BOOST_CHECK(e.what() != nullptr);
    }
});
```

## Common Pitfalls

### Memory Leaks with Abrupt Disconnects
- Don't use 0-second timeouts that disconnect before server cleanup
- ASan will catch leaked connections from incomplete handshakes
- Always let connections complete gracefully in tests

### SSL Exception Constructor Issue
- `ssl::exception()` default constructor can crash when no SSL error is queued
- Always catch with `catch (...)` when testing SSL errors
- Create invalid cert files instead of using non-existent paths

### IntegrationFixture Server Cleanup
- Always call `stop_server()` in test cleanup
- Use unique port offsets for each test to avoid conflicts
- Example: `start_server(1, 120)` uses port TEST_PORT + 120

### Branch Coverage Challenges
- Each `if` creates 2 branches; error tests only cover one side
- System call failures (send/recv errors) are hard to trigger without mocks
- Focus on reachable error paths first

## Key Files for Coverage Improvement

| File | Typical Coverage | Key Uncovered Areas |
|------|------------------|---------------------|
| `connector.cc` | ~85% | Timeout handling, connection errors |
| `http_client.cc` | ~75% | Request timeout, peer disconnect |
| `ssl_lib.cc` | ~80% | Cert loading errors, SSL_ERROR_SYSCALL |
| `http_server.cc` | ~82% | Error logging, malformed packets |
| `server_conn.cc` | ~76% | Expect-continue, Malformed_packet catch |
| `reactor_*.cc` | ~70% | poll()/select() errors, POLLERR/POLLHUP |

## Finding Uncovered Lines

```bash
gcov -b libiqxmlrpc/CMakeFiles/iqxmlrpc.dir/connector.cc.gcno
grep -n "#####" connector.cc.gcov  # Shows uncovered lines
```

# SSL/HTTPS Testing Guide

## Embedded Test Certificates

Tests use embedded self-signed certificates defined in `tests/test_integration.cc`:
- `EMBEDDED_TEST_CERT` - PEM certificate for localhost
- `EMBEDDED_TEST_KEY` - Corresponding private key
- Use `create_temp_cert_files()` to write them to /tmp for SSL context creation

## SSL Context Setup Pattern

```cpp
class HttpsIntegrationFixture {
    iqnet::ssl::Ctx* saved_ctx_ = nullptr;
    iqnet::ssl::Ctx* test_ctx_ = nullptr;

    bool setup_ssl_context() {
        auto paths = create_temp_cert_files();
        test_ctx_ = iqnet::ssl::Ctx::client_server(paths.first, paths.second);
        saved_ctx_ = iqnet::ssl::ctx;
        iqnet::ssl::ctx = test_ctx_;
        return true;
    }

    void cleanup_ssl() {
        iqnet::ssl::ctx = saved_ctx_;
        delete test_ctx_;
    }
};
```

## Testing SSL Verification Callbacks

To cover `ssl_lib.cc` verify callback paths:
1. Create a `TrackingVerifier` that counts `do_verify()` calls
2. Perform actual HTTPS handshake (not just context setup)
3. Verify `call_count > 0` after handshake completes

## SSL Error Testing

When testing SSL errors:
- Always catch with `catch (...)` - the `ssl::exception()` default constructor can crash when no SSL error is queued
- Create invalid cert files instead of using non-existent paths
- Use short timeouts to avoid hanging on SSL handshake failures

## Example: Testing Certificate Loading Failure

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

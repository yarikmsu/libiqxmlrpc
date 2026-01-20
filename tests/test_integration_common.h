//  Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2011-2026 Anton Dedov
//
//  Shared fixtures and helpers for integration tests

#ifndef IQXMLRPC_TEST_INTEGRATION_COMMON_H
#define IQXMLRPC_TEST_INTEGRATION_COMMON_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "libiqxmlrpc/auth_plugin.h"
#include "libiqxmlrpc/firewall.h"
#include "libiqxmlrpc/https_server.h"
#include "libiqxmlrpc/https_client.h"
#include "libiqxmlrpc/executor.h"
#include "libiqxmlrpc/ssl_lib.h"
#include "libiqxmlrpc/connection.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "methods.h"
#include "test_resource_limits.h"

namespace iqxmlrpc_test {

//=============================================================================
// Port Constants for Test Fixtures
//=============================================================================
// Each fixture uses a unique port range to avoid conflicts when tests run in parallel
// Base ports allocation:
//   19876 - IntegrationFixture (test_common.h)
//   19950 - Thread safety tests, HttpsIntegrationFixture

constexpr int HTTPS_FIXTURE_BASE_PORT = 19950;

// Timing constants
constexpr int SERVER_SETTLE_TIME_MS = 100;  // Extra time after server signals ready

//=============================================================================
// Auth Plugins for Testing
//=============================================================================
// WARNING: These auth plugins contain hardcoded test credentials.
// DO NOT use in production code or copy these patterns to production configs.

// Simple auth plugin for testing
// WARNING: Contains hardcoded credentials - TEST USE ONLY
class TestAuthPlugin : public iqxmlrpc::Auth_Plugin_base {
  bool allow_anonymous_;
public:
  explicit TestAuthPlugin(bool allow_anonymous = true)
    : allow_anonymous_(allow_anonymous) {}

  // WARNING: Hardcoded test credentials - DO NOT USE IN PRODUCTION
  bool do_authenticate(const std::string& user, const std::string& password) const override {
    return user == "testuser" && password == "testpass";
  }

  bool do_authenticate_anonymous() const override {
    return allow_anonymous_;
  }
};

// Firewall that blocks all connections
class BlockAllFirewall : public iqnet::Firewall_base {
public:
  bool grant(const iqnet::Inet_addr&) override { return false; }
};

// Firewall with custom message
class CustomMessageFirewall : public iqnet::Firewall_base {
public:
  bool grant(const iqnet::Inet_addr&) override { return false; }
  std::string message() override { return "HTTP/1.0 403 Custom Forbidden\r\n\r\n"; }
};

// Firewall that allows all connections
class AllowAllFirewall : public iqnet::Firewall_base {
public:
  bool grant(const iqnet::Inet_addr&) override { return true; }
};

//=============================================================================
// Embedded Test Certificates
//=============================================================================

// Embedded self-signed certificate for testing
// Generated with: openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 36500 -nodes -subj "/CN=localhost"
inline const char* EMBEDDED_TEST_CERT = R"(-----BEGIN CERTIFICATE-----
MIIDCzCCAfOgAwIBAgIUXzkbleG5HOcIm3Ke/qrw3JCCCVMwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MDEwOTIxMTYxNVoYDzIxMjUx
MjE2MjExNjE1WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEB
AQUAA4IBDwAwggEKAoIBAQDWUSUBs2Am6ptXHZkz3zZAwzA06jF+r5PMCFmhf2ZY
o54a0dUgh2XElgpo7saEWFNnt5EgTxJQpUCRHs7QKkB39/6itjg/4rmR7C7nXj1n
q1jkYUXiPXlihkHwycXp4jUh0zgLFAtQNYBl6AajlsZcxkLUB+4pFxTmtCXuOX6E
fh4iiougQgkzUL89dNC/+PViUOKkO3WxZ3ZcuLEaiyBEBfuqLH/YBKp45nIaFr8H
iFyEx6Y5nuZ1grPDDbZZ4MXmdm+aC6OUNTrIYtSzaP2wO3BiJhLVshDB/cIDmYsX
H80aB3zbrKWClTTAVxFgn/y83lNAIciP90XvDQSP59EDAgMBAAGjUzBRMB0GA1Ud
DgQWBBQo6uxnhPB3W3xFzqQ42Xgzg//+wjAfBgNVHSMEGDAWgBQo6uxnhPB3W3xF
zqQ42Xgzg//+wjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQCa
iBhA4uamOdZAulJQV3/VKOlqzCPyzokSwh+D7H2fgvJRf4dt4CvZYlFtM2iK7+EW
h7wYNJ5qo4pq88/iAfDgIe8Vbpbr9IpwcHw1hLfVxqOys845Z4bXRrvFaE4GaaAa
Nx+Zbr+asm0eL2w/df8HHcp78vHYZSDZL04skyv1Ybx1buoFY3G59kl/I2v7SRXi
73m7JurSbDWaVXV9M2k/znSPifdx9bqOKHX8zX7liitHcSyVGG9DWl1yB+2iP0dM
0eioGoqxoNt3Gws8wSieB11r2k5cfqcGFLbjEfV6YDenjRs2FB2xVfrmocBrbJ9V
5ntzlSfNSe7ZowUs1202
-----END CERTIFICATE-----
)";

inline const char* EMBEDDED_TEST_KEY = R"(-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDWUSUBs2Am6ptX
HZkz3zZAwzA06jF+r5PMCFmhf2ZYo54a0dUgh2XElgpo7saEWFNnt5EgTxJQpUCR
Hs7QKkB39/6itjg/4rmR7C7nXj1nq1jkYUXiPXlihkHwycXp4jUh0zgLFAtQNYBl
6AajlsZcxkLUB+4pFxTmtCXuOX6Efh4iiougQgkzUL89dNC/+PViUOKkO3WxZ3Zc
uLEaiyBEBfuqLH/YBKp45nIaFr8HiFyEx6Y5nuZ1grPDDbZZ4MXmdm+aC6OUNTrI
YtSzaP2wO3BiJhLVshDB/cIDmYsXH80aB3zbrKWClTTAVxFgn/y83lNAIciP90Xv
DQSP59EDAgMBAAECggEAUcqzIGSIUCHeOg+SPgE0j9/OQIuWax5v/gC70E4yTabX
+q1VNO5nkPCgNW7XNYAOCLm+ecGjoEKJEzlaPYi6hO6Q8CEx83PAVaf5OJS3Q57Z
tINJK/BBKLBLby1aSonptCiLrXKvZKOehoXYLsumlZaWv5vtMSJdeDSNe07W8ZIL
VxlKFsVANHPMP9wK/NIx2z0G+Qd/e8UJukuLccN5G+oL/oPfGdMtxY3onHlSQdL0
X20v5dcbTKRwO+kYMK9nLz6ZF9sL/MDi3/AmlCyPQ87Vaz/LTw2t8JlSe9hqHoZ9
hJat8c6KRnRvL6hhs3YFuXnh5uecs6SdsltXrf6UBQKBgQD0Bg6rP1OTv/BIFY3p
CT8M/Eop49eM3d5jIkWGEo0LDZp6TVQ6geWIhTYXB24D7zzk/FlhUiWrYlCSJhjc
NFff7ysdbZft0gVtYRddepEgN2JafJqs8R1+GoYubrxUcFz/v4qIkt8NXs54Z+J3
TCQqIf8aEK0XO1gN3qlITzZS9wKBgQDg1dlMFDGrSUdu19vnXK85t/dQvroyrnKZ
MyObUceSLSkYNbOJAplI48LMTVApmUccG370WNg/qGiZhBdw90UxdHLPdt3Ca/C5
3wmGUNakg5bDfdFhmHsooQlh6wvbJ1SX3O9UApWDqLMstSaUZqppbVgjpbrHG9AV
e/94Vo2jVQKBgQC7Ye9ftsgh+9CyOcL4QL5m5VC57Bi4NiMwMr/6XUJrS23lHn5g
UyED/W70riLf6JT1LYYhAmiku2EtaQ3MAnG8JrcP6PkyiQTb4iOEB7trZrwiye4o
gRppnEqPWz9JA+OWC+qAR2/6n2Oi9/riKtjWdbajuEyCO3K5a9LIEPOhLwKBgQCk
P/Wn25TRgg4aTr2Kjq4/50JYjY0vGzwC6VYY0KyQAEfmNMz8yZY7ppAXel+WlDBb
u0aKsSEBmEEZ7WLGlw3IbD63iynEL+DDmMm3gvTbaHpKRG8i8ib+7m4RR4n4xwnI
i5GXeO/LKAIFJi2R+lKCBGyAVkFV1d6040olmm2MpQKBgBEkhuUdBaSkNBt8YJxM
BU2PiriNuFw5UMWFRRcysMKO3oA9UWeXEHEX7z4jyThCmLl2+X0Q9KvAezhKdRjP
H/+tEBbXrHM9aOHqPvhkMe6foDk3VZdXwiU/XO+gBidrsQVoHRuz3TA5xMYflvHg
rK0fmiWyi5lQX70lb9kyDkqP
-----END PRIVATE KEY-----
)";

//=============================================================================
// SSL Certificate Helpers
//=============================================================================

// Write embedded cert/key to temp files and return their paths
// Security measures:
//   - Uses O_EXCL to prevent symlink/TOCTOU attacks
//   - Sets restrictive permissions (0600) on private key file
//   - Includes PID and random suffix for uniqueness
inline std::pair<std::string, std::string> create_temp_cert_files() {
  // Include PID and timestamp for uniqueness across parallel test runs
  std::string suffix = std::to_string(getpid()) + "_" +
    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  std::string cert_path = "/tmp/iqxmlrpc_test_cert_" + suffix + ".pem";
  std::string key_path = "/tmp/iqxmlrpc_test_key_" + suffix + ".pem";

  // Create cert file with O_EXCL to prevent TOCTOU/symlink attacks
  // Certificate can be world-readable (0644)
  size_t cert_len = strlen(EMBEDDED_TEST_CERT);
  int cert_fd = open(cert_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (cert_fd < 0) {
    throw std::runtime_error("Failed to create temp cert file (may already exist): " + cert_path);
  }
  ssize_t cert_written = write(cert_fd, EMBEDDED_TEST_CERT, cert_len);
  close(cert_fd);
  if (cert_written < 0 || static_cast<size_t>(cert_written) != cert_len) {
    (void)std::remove(cert_path.c_str());
    throw std::runtime_error("Failed to write temp cert file: " + cert_path);
  }

  // Create key file with O_EXCL and restrictive permissions (0600)
  // Private keys must NOT be world-readable
  size_t key_len = strlen(EMBEDDED_TEST_KEY);
  int key_fd = open(key_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (key_fd < 0) {
    (void)std::remove(cert_path.c_str());
    throw std::runtime_error("Failed to create temp key file (may already exist): " + key_path);
  }
  ssize_t key_written = write(key_fd, EMBEDDED_TEST_KEY, key_len);
  close(key_fd);
  if (key_written < 0 || static_cast<size_t>(key_written) != key_len) {
    (void)std::remove(cert_path.c_str());
    (void)std::remove(key_path.c_str());
    throw std::runtime_error("Failed to write temp key file: " + key_path);
  }

  return std::make_pair(cert_path, key_path);
}

// Check if external test certificates are available (for backward compat tests)
inline bool ssl_certs_available() {
  std::ifstream cert("../tests/data/cert.pem");
  std::ifstream key("../tests/data/pk.pem");
  return cert.good() && key.good();
}

//=============================================================================
// SSL Verification Helpers
//=============================================================================

// Verifier that tracks how many times it was called
class TrackingVerifier : public iqnet::ssl::ConnectionVerifier {
  mutable std::atomic<int> call_count_{0};

  int do_verify(bool, X509_STORE_CTX*) const override {
    ++call_count_;
    return 1;  // Accept all
  }

public:
  int get_call_count() const { return call_count_.load(); }
  void reset() { call_count_ = 0; }
};

//=============================================================================
// HTTPS Integration Fixture
//=============================================================================

// HTTPS integration test fixture with embedded certificates
class HttpsIntegrationFixture {
protected:
  std::unique_ptr<iqxmlrpc::Https_server> server_;
  std::unique_ptr<iqxmlrpc::Executor_factory_base> exec_factory_;
  std::thread server_thread_;
  std::mutex ready_mutex_;
  std::condition_variable ready_cond_;
  std::mutex server_error_mutex_;
  bool server_ready_ = false;
  std::atomic<bool> server_error_{false};
  std::atomic<bool> server_running_{false};
  int port_ = HTTPS_FIXTURE_BASE_PORT;
  iqnet::ssl::Ctx* saved_ctx_ = nullptr;
  iqnet::ssl::Ctx* test_ctx_ = nullptr;
  std::string temp_cert_path_;
  std::string temp_key_path_;
  std::string server_error_message_;

public:
  HttpsIntegrationFixture(const HttpsIntegrationFixture&) = delete;
  HttpsIntegrationFixture& operator=(const HttpsIntegrationFixture&) = delete;

  HttpsIntegrationFixture()
    : server_()
    , exec_factory_()
    , server_thread_()
    , ready_mutex_()
    , ready_cond_()
    , server_error_mutex_()
    , saved_ctx_(iqnet::ssl::ctx)
    , temp_cert_path_()
    , temp_key_path_()
    , server_error_message_()
  {}

  ~HttpsIntegrationFixture() {
    stop_server();
    cleanup_ssl();
    cleanup_temp_files();
  }

  bool setup_ssl_context() {
    try {
      auto paths = create_temp_cert_files();
      temp_cert_path_ = paths.first;
      temp_key_path_ = paths.second;

      test_ctx_ = iqnet::ssl::Ctx::client_server(temp_cert_path_, temp_key_path_);
      iqnet::ssl::ctx = test_ctx_;
      return true;
    } catch (...) {
      return false;
    }
  }

  void cleanup_ssl() {
    iqnet::ssl::ctx = saved_ctx_;
    if (test_ctx_) {
      delete test_ctx_;
      test_ctx_ = nullptr;
    }
  }

  void cleanup_temp_files() {
    if (!temp_cert_path_.empty()) (void)std::remove(temp_cert_path_.c_str());
    if (!temp_key_path_.empty()) (void)std::remove(temp_key_path_.c_str());
  }

  void start_server(int port_offset = 0) {
    ensure_fd_limit();
    port_ = HTTPS_FIXTURE_BASE_PORT + port_offset;
    server_error_ = false;

    exec_factory_ = std::make_unique<iqxmlrpc::Serial_executor_factory>();

    server_ = std::make_unique<iqxmlrpc::Https_server>(
      iqnet::Inet_addr("127.0.0.1", port_),
      exec_factory_.get());

    register_user_methods(*server_);

    server_running_ = true;
    server_thread_ = std::thread([this]() {
      {
        std::unique_lock<std::mutex> lk(ready_mutex_);
        server_ready_ = true;
        ready_cond_.notify_one();
      }
      try {
        server_->work();
      } catch (const std::exception& ex) {
        {
          std::lock_guard<std::mutex> lk(server_error_mutex_);
          server_error_message_ = ex.what();
        }
        server_error_ = true;
      } catch (...) {
        {
          std::lock_guard<std::mutex> lk(server_error_mutex_);
          server_error_message_ = "unknown exception";
        }
        server_error_ = true;
      }
      server_running_ = false;
    });

    std::unique_lock<std::mutex> lk(ready_mutex_);
    ready_cond_.wait_for(lk,
      std::chrono::seconds(5),
      [this]{ return server_ready_; });

    std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_SETTLE_TIME_MS));
  }

  void stop_server() {
    if (server_ && server_running_) {
      server_->set_exit_flag();
      server_->interrupt();
      if (server_thread_.joinable()) {
        server_thread_.join();
      }
    }
    server_.reset();
    exec_factory_.reset();
    server_ready_ = false;
    server_running_ = false;
  }

  std::unique_ptr<iqxmlrpc::Client_base> create_client() {
    if (server_error_) {
      std::string msg;
      {
        std::lock_guard<std::mutex> lk(server_error_mutex_);
        msg = server_error_message_;
      }
      throw std::runtime_error("Server thread failed: " + msg);
    }
    auto client = std::unique_ptr<iqxmlrpc::Client_base>(
      new iqxmlrpc::Client<iqxmlrpc::Https_client_connection>(
        iqnet::Inet_addr("127.0.0.1", port_)));
    if (should_disable_keep_alive()) {
      client->set_keep_alive(false);
    }
    return client;
  }

  iqnet::ssl::Ctx* get_context() { return test_ctx_; }
  int port() const { return port_; }
};

//=============================================================================
// SSL Factory Test Helpers (Test-build only)
//=============================================================================

#ifdef IQXMLRPC_TESTING

// RAII guard for mock proxy socket and thread cleanup
struct SslFactoryTestProxyGuard {
  iqnet::Socket& sock;
  std::thread& thr;
  ~SslFactoryTestProxyGuard() {
    if (thr.joinable()) thr.join();
    try { sock.close(); } catch (...) {}
  }
};

// Note: SimpleTunnelProxyFixture and SslTunnelProxyFixture classes were removed
// as they were unused. The existing https_proxy_ssl_factory_* tests in
// test_integration.cc provide adequate coverage using inline proxy patterns
// with SslFactoryTestProxyGuard above.

#endif // IQXMLRPC_TESTING

} // namespace iqxmlrpc_test

#endif // IQXMLRPC_TEST_INTEGRATION_COMMON_H

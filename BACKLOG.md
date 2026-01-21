# libiqxmlrpc Product Backlog

> **Context**: This library serves as an XML-RPC server for API calls from Java EE / Spring Boot microservices to a C++ monolith.

## Priority Legend
- **P0** - Critical: Blocking issues or essential features for production readiness
- **P1** - High: Significant improvements for reliability, performance, or usability
- **P2** - Medium: Nice-to-have improvements and optimizations
- **P3** - Low: Future considerations and long-term enhancements

---

## 1. Observability & Monitoring

Modern microservice environments require comprehensive observability. These items enable better debugging, monitoring, and tracing across the Java/C++ boundary.

### P0 - Critical

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| OBS-001 | Structured Logging Framework | Implement structured logging (JSON format) with configurable log levels | - JSON log output option<br>- Log levels: TRACE, DEBUG, INFO, WARN, ERROR<br>- Include request_id, method, duration, status in logs<br>- Thread-safe logging |
| OBS-002 | Request/Response Correlation IDs | Support passing correlation IDs from Java clients for distributed tracing | - Accept X-Correlation-ID / X-Request-ID headers<br>- Propagate through interceptor chain<br>- Include in all log messages |

### P1 - High

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| OBS-003 | Prometheus Metrics Endpoint | Expose metrics in Prometheus format for Kubernetes/cloud-native monitoring | - /metrics endpoint with request counts, latencies, error rates<br>- Histogram for response times<br>- Connection pool stats<br>- Thread pool utilization |
| OBS-004 | OpenTelemetry Tracing Support | Integrate with OpenTelemetry for distributed tracing | - Span creation for each RPC call<br>- Context propagation from incoming headers<br>- Trace parent/state header support |
| OBS-005 | Health Check Endpoints | Implement health/readiness/liveness endpoints for Kubernetes | - /health, /ready, /live endpoints<br>- Configurable health checks<br>- Dependency health reporting |

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| OBS-006 | SSL/TLS Logging Enhancement | Implement the TODO in ssl_lib.cc for OpenSSL logging | - Configurable SSL debug logging<br>- Certificate validation logging<br>- Handshake event logging |
| OBS-007 | Request/Response Payload Logging | Optional debug logging of XML payloads | - Configurable payload size limit<br>- Sensitive data masking support<br>- Performance impact < 5% when enabled |

---

## 2. Reliability & Error Handling

Robust error handling is crucial when serving as the integration point between microservices and a monolith.

### P0 - Critical

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| REL-001 | Graceful Shutdown | Implement proper graceful shutdown with connection draining | - Finish in-flight requests<br>- Configurable drain timeout<br>- SIGTERM handling<br>- Complete server-stop-test TODO |
| REL-002 | Circuit Breaker Pattern | Implement circuit breaker for downstream dependency failures | - Configurable failure threshold<br>- Half-open state for recovery<br>- Per-method circuit breaker support |

### P1 - High

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| REL-003 | Request Timeout Enforcement | Server-side request timeout to prevent hanging requests | - Configurable per-method timeouts<br>- Global default timeout<br>- Proper resource cleanup on timeout |
| REL-004 | Retry-After Header Support | Return Retry-After header on rate limiting/503 responses | - RFC 7231 compliant<br>- Dynamic calculation based on load |
| REL-005 | Enhanced Error Responses | Structured error responses with error codes and details | - Machine-readable error codes<br>- Localized error messages<br>- Stack trace option for development |

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| REL-006 | Automatic Rate Limiter Cleanup | Scheduled cleanup of stale rate limiter entries | - Background cleanup thread<br>- Configurable cleanup interval<br>- Memory usage monitoring |
| REL-007 | Connection Keep-Alive Tuning | Optimize keep-alive settings for microservice patterns | - Configurable idle timeout<br>- Max requests per connection<br>- Connection pool statistics |

---

## 3. Performance & Scalability

While v0.14.0 includes 21 performance optimizations, there are additional opportunities for microservice workloads.

### P1 - High

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| PERF-001 | Connection Pooling Improvements | Enhanced connection pool management for high-throughput scenarios | - Configurable pool sizes<br>- Connection warm-up<br>- Pool exhaustion handling |
| PERF-002 | Request Batching Support | Support XML-RPC system.multicall for batched requests | - Atomic batch execution option<br>- Per-request error handling<br>- Batch size limits |
| PERF-003 | Response Compression | gzip/deflate compression for large responses | - Accept-Encoding negotiation<br>- Configurable compression threshold<br>- Compression level tuning |

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| PERF-004 | epoll Reactor Implementation | Add epoll reactor for better Linux scalability | - Automatic selection on Linux<br>- Edge-triggered mode<br>- Benchmark vs poll |
| PERF-005 | Memory Pool Allocator | Custom allocator for frequent small allocations | - Reduce allocation overhead<br>- Thread-local pools<br>- Configurable pool sizes |
| PERF-006 | Zero-Copy Response Path | Minimize data copies in response serialization | - Direct buffer writes<br>- Scatter-gather I/O |

### P3 - Low

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| PERF-007 | io_uring Support | Modern Linux async I/O for highest performance | - Linux 5.1+ support<br>- Fallback to epoll/poll |

---

## 4. Security Enhancements

Additional security features for enterprise microservice environments.

### P0 - Critical

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| SEC-001 | mTLS Support | Mutual TLS authentication for service-to-service calls | - Client certificate validation<br>- Certificate chain verification<br>- CRL/OCSP checking |
| SEC-002 | Request Size Limits | Configurable limits on request body size | - Reject oversized requests early<br>- Configurable per-method limits<br>- Memory protection |

### P1 - High

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| SEC-003 | JWT Token Validation | Validate JWT tokens from Spring Security | - RS256/ES256 signature verification<br>- Claims validation<br>- JWKS endpoint support |
| SEC-004 | API Key Authentication | Simple API key-based authentication option | - Header-based API keys<br>- Key rotation support<br>- Rate limiting per key |
| SEC-005 | Audit Logging | Security-focused audit log for compliance | - Authentication events<br>- Authorization decisions<br>- Sensitive operation logging |

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| SEC-006 | Certificate Rotation | Hot-reload of TLS certificates without restart | - File change detection<br>- Graceful certificate swap<br>- Validation before activation |
| SEC-007 | Security Headers | Add security headers to HTTP responses | - X-Content-Type-Options<br>- X-Frame-Options<br>- Content-Security-Policy |

---

## 5. Java/Spring Integration

Features specifically aimed at improving integration with Java EE and Spring Boot clients.

### P1 - High

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| JAVA-001 | Java Client Documentation | Comprehensive documentation for Java client integration | - Spring RestTemplate examples<br>- WebClient (reactive) examples<br>- Connection pool configuration |
| JAVA-002 | Spring Boot Starter Compatibility Matrix | Document compatible Spring Boot / Java versions | - Tested version combinations<br>- Known issues/workarounds<br>- Migration guides |
| JAVA-003 | OpenAPI/Swagger Spec Generation | Generate API documentation from method registry | - OpenAPI 3.0 format<br>- Type mapping documentation<br>- Example requests/responses |

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| JAVA-004 | Wire Protocol Test Suite | Interop tests with Apache XML-RPC (Java) | - Round-trip compatibility<br>- Edge case handling<br>- Type coercion rules |
| JAVA-005 | Java Type Mapping Guide | Document XML-RPC to Java type mappings | - Primitive types<br>- Collections<br>- Custom type handling |

---

## 6. Configuration & Operations

Operational improvements for production deployments.

### P1 - High

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| OPS-001 | Environment Variable Configuration | Configure server via environment variables | - All settings via env vars<br>- Docker/K8s friendly<br>- Prefix convention (IQXMLRPC_) |
| OPS-002 | Configuration File Support | YAML/JSON configuration file support | - Single config file<br>- Config validation on startup<br>- Hot-reload for safe settings |
| OPS-003 | Runtime Configuration Updates | Change certain settings without restart | - Log levels<br>- Rate limits<br>- Timeouts |

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| OPS-004 | Admin API | Internal admin endpoints for operations | - Config inspection<br>- Connection stats<br>- Method registry listing |
| OPS-005 | Systemd Integration | Better systemd service integration | - Notify socket support<br>- Watchdog integration<br>- Journal logging |

---

## 7. Developer Experience

Improvements to make the library easier to use and debug.

### P1 - High

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| DX-001 | Quick Start Guide | Step-by-step guide for common use cases | - Basic server setup<br>- HTTPS configuration<br>- Authentication setup<br>- Performance tuning |
| DX-002 | Docker Example | Ready-to-run Docker example | - Dockerfile<br>- docker-compose.yml with Java client<br>- Volume mounts for certs |
| DX-003 | Error Message Improvements | More helpful error messages with context | - Suggestion for resolution<br>- Related documentation links<br>- Configuration hints |

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| DX-004 | Debug Mode | Comprehensive debug mode for development | - Verbose request logging<br>- Timing breakdowns<br>- Memory usage reporting |
| DX-005 | Method Introspection Enhancement | Richer method metadata | - Parameter descriptions<br>- Return type documentation<br>- Version info |

---

## 8. Code Quality & Maintenance

Technical debt and code quality improvements.

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| CODE-001 | Complete Binary_data operator== | Fix incomplete comparison noted in tests | - Full equality comparison<br>- Unit tests |
| CODE-002 | Const Correctness Audit | Review and improve const correctness | - Mark const methods<br>- Use const references |
| CODE-003 | Noexcept Audit | Add noexcept specifications where appropriate | - Move operations<br>- Swap functions<br>- Destructors |

### P3 - Low

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| CODE-004 | C++20 Migration Preparation | Prepare for eventual C++20 upgrade | - Identify C++20 opportunities<br>- Deprecation warnings cleanup |
| CODE-005 | Module Support (C++20) | Prepare for C++20 modules | - Header organization<br>- Forward declaration cleanup |

---

## 9. Protocol Extensions

Optional protocol enhancements for specific use cases.

### P2 - Medium

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| PROTO-001 | JSON-RPC Support | Alternative JSON-RPC protocol support | - JSON-RPC 2.0 compliant<br>- Content-Type negotiation<br>- Batch requests |
| PROTO-002 | HTTP/2 Support | HTTP/2 protocol support | - Multiplexing<br>- Header compression<br>- Server push (optional) |

### P3 - Low

| ID | Item | Description | Acceptance Criteria |
|----|------|-------------|---------------------|
| PROTO-003 | gRPC Gateway | Optional gRPC interface | - Protobuf definitions<br>- Bidirectional streaming |
| PROTO-004 | WebSocket Support | WebSocket transport for real-time use cases | - Persistent connections<br>- Server-initiated messages |

---

## Prioritized Roadmap Suggestion

### Phase 1: Production Hardening
Focus: Reliability and observability for production microservice environments
- OBS-001: Structured Logging
- OBS-002: Correlation IDs
- REL-001: Graceful Shutdown
- SEC-001: mTLS Support
- SEC-002: Request Size Limits

### Phase 2: Cloud-Native Integration
Focus: Kubernetes and cloud-native ecosystem integration
- OBS-003: Prometheus Metrics
- OBS-004: OpenTelemetry Tracing
- OBS-005: Health Check Endpoints
- OPS-001: Environment Variable Configuration
- JAVA-001: Java Client Documentation

### Phase 3: Performance & Scale
Focus: High-throughput optimizations
- PERF-001: Connection Pooling
- PERF-002: Request Batching
- PERF-003: Response Compression
- REL-002: Circuit Breaker
- PERF-004: epoll Reactor

### Phase 4: Enterprise Features
Focus: Enterprise security and compliance
- SEC-003: JWT Validation
- SEC-004: API Key Authentication
- SEC-005: Audit Logging
- OPS-002: Configuration File Support
- JAVA-003: OpenAPI Generation

### Phase 5: Future Enhancements
Focus: Protocol modernization and long-term improvements
- PROTO-001: JSON-RPC Support
- PROTO-002: HTTP/2 Support
- CODE-004: C++20 Preparation

---

## Appendix: Current State Summary

### Completed Optimizations (v0.14.0)
- 21 major performance optimizations
- Lock-free thread pool with boost::lockfree::queue
- TLS session caching
- Exception-free SSL I/O path
- std::to_chars/from_chars number conversion (2-12x faster)
- Type tags for O(1) type checking
- Single-pass HTTP parser
- unordered_map for Struct and Reactor handlers

### Current Test Coverage
- >95% code coverage target
- 85+ test files
- 11 OSS-Fuzz targets
- Wire compatibility tests with Python xmlrpc

### Security Posture
- XXE prevention
- CRLF injection protection
- Integer overflow protection
- TLS 1.2+ enforcement
- Rate limiting firewall
- Coverity and CodeQL scanning in CI

---

*Last Updated: 2026-01-21*
*Library Version: 0.14.0*

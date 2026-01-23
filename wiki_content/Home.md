# libiqxmlrpc - High-Performance Fork

This is Yaroslav Gorbunov's actively maintained fork of Anton Dedov's libiqxmlrpc,
optimized for performance and modern C++ practices.

## Why This Fork?

- **28 performance optimizations** (1.2x to 12.5x faster on key operations)
- **Modern C++17** with exception-free hot paths
- **Enhanced security** (TLS 1.2+, overflow protection, OWASP-aligned)
- **Active maintenance** (2019-2026, CI/CD, fuzz testing, 95% coverage)

## Quick Comparison

|  | Upstream | This Fork |
|--|----------|-----------|
| **Last Updated** | 2015 | 2026 |
| **C++ Standard** | C++11 | C++17 |
| **Performance** | Baseline | 1.2x-12.5x faster |
| **OpenSSL** | 1.0.2+ | 1.1.0+ (TLS 1.2+) |
| **CI/CD** | Travis CI (inactive) | GitHub Actions (12 checks) |
| **Coverage** | ~75% | 95% target |
| **Fuzz Testing** | None | OSS-Fuzz integrated |

## Who Should Use This Fork?

**✅ Choose this fork if you:**
- Need better performance for high-throughput XML-RPC services
- Want modern C++17 features and cleaner APIs
- Require active maintenance and security updates
- Are starting a new project with XML-RPC

**⚠️ Stay with upstream if you:**
- Have legacy C++11 codebase that can't upgrade
- Need OpenSSL 1.0.2 compatibility (EOL 2019)
- Have deeply integrated dependencies on upstream-specific behavior

## 🚀 Quick Links

**Getting Started**
- [Quick Start Guide](Quick-Start)
- [Building from Source](Building)
- [Performance Results](Performance-Results)

**📚 Core Documentation** (Upstream - Still Valid)
- [Writing a Client](https://github.com/adedov/libiqxmlrpc/wiki/Writing-Client)
- [Writing a Server](https://github.com/adedov/libiqxmlrpc/wiki/Writing-Server)
- [Value Manipulations](https://github.com/adedov/libiqxmlrpc/wiki/Value-Manipulations)

**🔬 Advanced Topics** (Fork-Specific)
- [Performance Guide](/docs/PERFORMANCE_GUIDE.md)
- [Security Checklist](/docs/SECURITY_CHECKLIST.md)
- [Development Workflow](/docs/PROJECT_RULES.md)

## Community

- **Issues**: Report bugs or request features on GitHub
- **Pull Requests**: Contributions welcome - see [Project Rules](/docs/PROJECT_RULES.md)

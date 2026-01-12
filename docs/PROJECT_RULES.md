# Project Rules

## Overview

Libiqxmlrpc is a C++17 XML-RPC library (client + server) with HTTP/HTTPS support.
Dependencies: Boost (thread), libxml2, OpenSSL.

## Build Commands

```bash
mkdir build && cd build && cmake .. && make -j4   # Build
make check                                         # Build + run tests
make perf-test                                     # Run benchmarks
./tests/integration-test --run_test=<suite>        # Run specific test
```

## Git Workflow

- Create feature branches for all changes
- Write descriptive commit messages explaining "why" not just "what"
- Create PRs and merge after CI passes
- Never force push to main/master
- Check CI: `gh pr view <PR> --json statusCheckRollup`

## CI Checks

PRs must pass: ubuntu-24.04, ubi8, macos builds | ASan/UBSan | TSan | coverage | cppcheck | CodeQL

## Architecture

| Component | Files | Purpose |
|-----------|-------|---------|
| Value System | `value.h`, `value_type.h` | XML-RPC types (int, string, array, struct, etc.) |
| Server | `server.h`, `http_server.h`, `https_server.h` | Method dispatch, connection handling |
| Client | `client.h`, `http_client.h`, `https_client.h` | RPC calls via `execute()` |
| Network | `acceptor.h`, `connection.h`, `reactor.h` | Reactor pattern (poll/select) |
| HTTP | `http.h`, `http_errors.h` | Headers, packets, parsing |

### Key Classes
- `Value` — proxy for all XML-RPC types; uses `ValueTypeTag` for O(1) type checks
- `Method` — base class for RPC methods; register via `register_method()`
- `Executor_factory_base` — threading model (Serial or Pool)

## Reference Docs

| Topic | File |
|-------|------|
| C++ style & patterns | `docs/CPP_STYLE.md` |
| Coding standards | `docs/CODING_STANDARDS.md` |
| Testing & coverage | `docs/COVERAGE_GUIDE.md` |
| Performance rules | `docs/PERFORMANCE_GUIDE.md` |
| SSL/HTTPS testing | `docs/SSL_TESTING.md` |
| Debugging tips | `docs/DEBUGGING.md` |

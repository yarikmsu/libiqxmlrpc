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
- Commit to feature branches, not master
- Create PRs and merge after CI passes
- Check CI: `gh pr view <PR> --json statusCheckRollup`

## CI Checks
PRs must pass: ubuntu-24.04, ubi8, macos builds | ASan/UBSan | coverage | cppcheck | CodeQL

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

## Testing

- **Framework**: Boost.Test
- **Unit tests**: `tests/test_*.cc`
- **Coverage targets**: 95% lines, 60% branches
- **Detailed guide**: `docs/COVERAGE_GUIDE.md`

## Performance

- **Benchmarks**: `make perf-test` (results in `tests/performance_baseline.txt`)
- **Full review**: `PERFORMANCE_REVIEW.md`

### Hot Path Guidelines
Avoid: `boost::lexical_cast`, `dynamic_cast` for type checks, `std::locale` construction
Prefer: `num_conv::to_string/from_string`, `value->type_tag()`, `strftime`

## Reference Docs

| Topic | File |
|-------|------|
| Coverage patterns & pitfalls | `docs/COVERAGE_GUIDE.md` |
| SSL/HTTPS testing | `docs/SSL_TESTING.md` |
| Debugging tips | `docs/DEBUGGING.md` |
| Performance history | `PERFORMANCE_REVIEW.md` |
| Wire compatibility | `COMPATIBILITY_TESTING_PLAN.md` |

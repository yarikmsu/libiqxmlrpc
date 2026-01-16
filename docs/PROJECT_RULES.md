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

PRs must pass: ubuntu-24.04, ubi8, macos | ASan/UBSan | TSan | Valgrind | Fuzz | coverage | cppcheck | clang-tidy | CodeQL

Additional (non-blocking): Coverity Scan - weekly deep static analysis, results reviewed periodically

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
| Coverity Scan setup | `docs/COVERITY_SETUP.md` |
| Security checklist | `docs/SECURITY_CHECKLIST.md` |
| Code review checklist | `docs/CODE_REVIEW_CHECKLIST.md` |
| Code simplification | `docs/CODE_SIMPLIFICATION.md` |
| CI troubleshooting | `docs/CI_TROUBLESHOOTING.md` |

## Development Workflow

### Pre-Commit Checklist

Before EVERY commit, verify:

1. **Tests pass locally**: `make check` succeeds
2. **No untracked files forgotten**: `git status` (check for files that should be included)
3. **No debug code**: No `cout`, `printf`, `TODO` in new code
4. **Build clean**: No new compiler warnings

### Task Completion Workflow

When completing ANY code task:

#### Phase 1: Implement
1. Make code changes
2. Run `make check` - fix any failures
3. Check `git status` for untracked files

#### Phase 2: Quality Gates
Run quality checks against relevant checklists:
- **Code Review**: `docs/CODE_REVIEW_CHECKLIST.md`
- **Security**: `docs/SECURITY_CHECKLIST.md`
- **Simplicity**: `docs/CODE_SIMPLIFICATION.md`

#### Phase 3: Commit & CI
1. Stage all relevant files
2. Commit with descriptive message
3. Push and verify CI passes
4. If CI fails: fix locally, push again

#### Phase 4: Done When
- [ ] All local tests pass
- [ ] Code review checklist passes
- [ ] Security checklist passes
- [ ] All CI checks pass

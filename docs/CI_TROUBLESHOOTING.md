# CI Troubleshooting

Common CI failures and how to fix them.

## Commands

```bash
# Check PR status
gh pr view <PR_NUMBER> --json statusCheckRollup

# Get failed run details
gh run view <RUN_ID> --log-failed

# Re-run failed jobs
gh run rerun <RUN_ID> --failed
```

## Common Failures

| Job | Typical Cause | Fix |
|-----|---------------|-----|
| ASan/UBSan | Memory leak, use-after-free | Add cleanup, fix ownership, use smart pointers |
| TSan | Data race | Use `std::atomic`, add mutex, check shared state |
| ubi8 | Boost 1.69 incompatibility | Avoid newer Boost APIs, check compatibility |
| macos | OpenSSL path issues | Check include paths in cmake |
| cppcheck | Style/portability warnings | Fix or add inline suppression if false positive |
| CodeQL | Security issues | Review and fix security patterns |
| coverage | Coverage drop | Add tests for new code paths |

## Platform-Specific Notes

### ubi8 (RHEL 8)
- Uses Boost 1.69 (older)
- Uses OpenSSL 1.1.1
- Avoid C++20 features

### macOS
- Uses Homebrew OpenSSL
- Path: `/usr/local/opt/openssl` or `/opt/homebrew/opt/openssl`

### Ubuntu 24.04
- Latest Boost and OpenSSL
- Primary development target

## Local Debugging

### ASan/UBSan Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
make -j4
./tests/integration-test
```

### TSan Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
make -j4
./tests/integration-test
```

### Coverage Build
```bash
cmake .. -DENABLE_COVERAGE=ON
make check
lcov --summary coverage.info
```

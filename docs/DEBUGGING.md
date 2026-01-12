# Debugging Guide

Quick reference for debugging. For detailed guides, see related docs.

## Memory Debugging

### AddressSanitizer (ASan/UBSan)
```bash
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
make check
```

### ThreadSanitizer (TSan)
```bash
cmake .. -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
make check
```

### Valgrind
```bash
valgrind --leak-check=full ./tests/integration-test
```

## Performance Profiling

See `@docs/PERFORMANCE_GUIDE.md` for benchmark workflows.

### Linux perf
```bash
perf record ./tests/test_performance
perf report
```

## Coverage & Testing

See `@docs/COVERAGE_GUIDE.md` for coverage commands and test patterns.

## Common Issues

See `@.claude/rules/common-pitfalls.md` for:
- Test port conflicts
- SSL handshake issues
- Memory leaks in tests
- Thread safety pitfalls

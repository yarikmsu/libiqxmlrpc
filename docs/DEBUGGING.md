# Debugging Guide

## Running Tests

### Single Test with Verbose Output
```bash
./tests/integration-test --run_test=network_error_tests/client_connection_timeout --log_level=all
```

### Run Specific Test Suite
```bash
./tests/integration-test --run_test=network_error_tests
```

## Memory Debugging

### AddressSanitizer (Local)
```bash
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
make integration-test
./tests/integration-test
```

### Valgrind
```bash
valgrind --leak-check=full ./tests/integration-test
```

## Coverage Debugging

### Find Uncovered Lines
```bash
gcov -b libiqxmlrpc/CMakeFiles/iqxmlrpc.dir/connector.cc.gcno
grep -n "#####" connector.cc.gcov
```

## Performance Debugging

### Compare Before/After Changes
```bash
make perf-test
cp tests/performance_baseline.txt tests/baseline_before.txt
# ... make changes ...
make perf-test
diff tests/baseline_before.txt tests/performance_baseline.txt
```

### Profile with perf (Linux)
```bash
perf record ./tests/test_performance
perf report
```

## Common Issues

### Test Port Conflicts
Use unique port offsets for each test to avoid conflicts:
```cpp
start_server(1, 120);  // Uses port TEST_PORT + 120
```

### SSL Handshake Timeouts
- Ensure certificates are valid
- Check that SSL context is properly initialized
- Verify server is listening before client connects

### Memory Leaks in Tests
- Always call `stop_server()` in test cleanup
- Don't use 0-second timeouts that disconnect before server cleanup
- Let connections complete gracefully

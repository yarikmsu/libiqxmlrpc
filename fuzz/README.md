# Fuzz Testing for libiqxmlrpc

This directory contains fuzz targets for testing libiqxmlrpc with various fuzzing engines.

## Fuzz Targets

- **fuzz_request**: Tests XML-RPC request parsing (`parse_request`)
- **fuzz_response**: Tests XML-RPC response parsing (`parse_response`)
- **fuzz_http**: Tests HTTP header parsing

## Running Locally

### Prerequisites

- Clang with libFuzzer support (on Linux) or Homebrew LLVM (on macOS)
- Boost libraries
- libxml2
- OpenSSL

### Quick Test with Test Harness

You can verify the fuzz targets work without libFuzzer using the test harness:

```bash
# Build the library first
mkdir build && cd build
cmake .. -DCMAKE_CXX_FLAGS="-DBOOST_TIMER_ENABLE_DEPRECATED"
make -j$(nproc)
cd ..

# Compile a fuzz target with test harness
clang++ -std=c++11 -DBOOST_TIMER_ENABLE_DEPRECATED \
    -I. \
    fuzz/fuzz_request.cc fuzz/test_harness.cc \
    -o test_fuzz_request \
    -Lbuild/libiqxmlrpc -liqxmlrpc \
    -lxml2 -lboost_date_time -lboost_thread -lpthread

# Test with sample input
echo '<?xml version="1.0"?><methodCall><methodName>test</methodName></methodCall>' | \
    LD_LIBRARY_PATH=build/libiqxmlrpc ./test_fuzz_request
```

### Build with libFuzzer (Linux)

```bash
mkdir build-fuzz && cd build-fuzz
cmake .. \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer-no-link,address -DBOOST_TIMER_ENABLE_DEPRECATED" \
    -DLIB_FUZZING_ENGINE="-fsanitize=fuzzer" \
    -Dbuild_fuzzers=ON

make -j$(nproc)

# Run a fuzzer
./fuzz/fuzz_request corpus_dir/
```

### Build with libFuzzer (macOS with Homebrew LLVM)

```bash
brew install llvm

mkdir build-fuzz && cd build-fuzz
cmake .. \
    -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
    -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
    -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer-no-link,address -DBOOST_TIMER_ENABLE_DEPRECATED" \
    -DLIB_FUZZING_ENGINE="-fsanitize=fuzzer" \
    -Dbuild_fuzzers=ON

make -j$(nproc)
```

### Creating a Seed Corpus

Create a directory with sample XML-RPC messages:

```bash
mkdir corpus_request
echo '<?xml version="1.0"?><methodCall><methodName>test</methodName></methodCall>' > corpus_request/sample1.xml
./fuzz/fuzz_request corpus_request/
```

## OSS-Fuzz Integration

This project is integrated with [OSS-Fuzz](https://github.com/google/oss-fuzz).

The following files are used for OSS-Fuzz integration:
- `Dockerfile`: Build environment for OSS-Fuzz
- `build.sh`: Build script for compiling fuzz targets
- `project.yaml`: Project metadata for OSS-Fuzz

To test the OSS-Fuzz build locally:

```bash
# Clone OSS-Fuzz
git clone https://github.com/google/oss-fuzz.git
cd oss-fuzz

# Build the project
python infra/helper.py build_image libiqxmlrpc
python infra/helper.py build_fuzzers libiqxmlrpc

# Run a fuzzer
python infra/helper.py run_fuzzer libiqxmlrpc fuzz_request
```

# Fuzz Testing for libiqxmlrpc

This directory contains fuzz targets for testing libiqxmlrpc with various fuzzing engines.

## Fuzz Targets

- **fuzz_request**: Tests XML-RPC request parsing (`parse_request`)
- **fuzz_response**: Tests XML-RPC response parsing (`parse_response`)
- **fuzz_http**: Tests HTTP header parsing

## Running Locally with libFuzzer

### Prerequisites

- Clang with libFuzzer support
- Boost libraries
- libxml2
- OpenSSL

### Build and Run

```bash
# Build with fuzzing instrumentation
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

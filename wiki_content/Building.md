# Building from Source

## Requirements

### Compiler
- **GCC 7+** or **Clang 5+** (C++17 support required)

### Dependencies
- **CMake 3.10+** (3.21+ recommended for presets)
- **Boost 1.58+** (headers + `boost::lockfree::queue`)
- **libxml2 2.9+**
- **OpenSSL 1.1.0+** (1.0.2 no longer supported)

### Optional
- **libFuzzer** or **AFL++** (for fuzz testing)
- **lcov/gcovr** (for coverage reports)

### Verify Your System

Check that your system meets the requirements:

```bash
# Check compiler version (need GCC 7+ or Clang 5+)
g++ --version       # Should show 7.0.0 or higher
# OR
clang++ --version   # Should show 5.0.0 or higher

# Check CMake version (need 3.10+, recommend 3.21+)
cmake --version     # Should show 3.10.0 or higher

# Check OpenSSL version (need 1.1.0+)
openssl version     # Should show 1.1.0 or higher

# Check for Boost (headers should be available)
ls /usr/include/boost/version.hpp || ls /usr/local/include/boost/version.hpp

# Check for libxml2
xml2-config --version   # Should show 2.9.0 or higher
```

## Standard Build

```bash
git clone https://github.com/yarikmsu/libiqxmlrpc.git
cd libiqxmlrpc

# Configure
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local

# Build
cmake --build build -j$(nproc)

# Test
cd build && ctest --output-on-failure

# Install
sudo cmake --install build
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Debug` | `Release`, `Debug`, `RelWithDebInfo` |
| `BUILD_SHARED_LIBS` | `ON` | Build shared library |
| `ENABLE_COVERAGE` | `OFF` | Enable code coverage (requires gcov) |
| `LIB_FUZZING_ENGINE` | - | Path to libFuzzer for fuzz tests |

## CMake Presets (CMake 3.21+)

```bash
# List available presets
cmake --list-presets

# Use a preset
cmake --preset=fuzz-asan
cmake --build --preset=fuzz-asan
```

**Available Presets**:
- `fuzz-asan` - Address Sanitizer fuzzing
- `fuzz-ubsan` - Undefined Behavior Sanitizer fuzzing
- `fuzz-coverage` - Coverage-guided fuzzing

## Platform-Specific Notes

### Linux
All distributions with GCC 7+ work out of the box.

### macOS
Requires Xcode Command Line Tools:
```bash
xcode-select --install
```

### Windows (Cygwin/MSYS2)
See [upstream Windows notes](https://github.com/adedov/libiqxmlrpc/wiki/Windows-Notes) for WinSock initialization.

## Verify Installation

After installation, verify that the library is properly installed:

```bash
# Check that library files are installed
ls /usr/local/lib/libiqxmlrpc*

# Expected output (exact filenames vary by platform):
# /usr/local/lib/libiqxmlrpc.so       (Linux)
# /usr/local/lib/libiqxmlrpc.dylib    (macOS)
# /usr/local/lib/libiqxmlrpc.a        (static)

# Check that headers are installed
ls /usr/local/include/iqxmlrpc/

# Try pkg-config (if available)
pkg-config --modversion libiqxmlrpc
pkg-config --cflags --libs libiqxmlrpc
```

**Quick compile test**:
```bash
# Create a minimal test file
cat > test.cpp << 'EOF'
#include <iqxmlrpc/value.h>
int main() {
  iqxmlrpc::Value v(42);
  return v.get_int() == 42 ? 0 : 1;
}
EOF

# Compile and run
g++ -std=c++17 test.cpp -liqxmlrpc -o test && ./test
echo $?  # Should print 0 (success)

# Cleanup
rm test.cpp test
```

## Troubleshooting

- **OpenSSL not found**: Set `OPENSSL_ROOT_DIR` CMake variable
- **Boost not found**: Set `BOOST_ROOT` environment variable
- **Tests fail**: Check [CI Troubleshooting](/docs/CI_TROUBLESHOOTING.md)

## Next Steps

- [Quick Start](Quick-Start) - Run your first client/server
- [Development Workflow](/docs/PROJECT_RULES.md) - Contributing to the fork

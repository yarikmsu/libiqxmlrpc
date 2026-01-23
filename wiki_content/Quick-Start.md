# Quick Start Guide

Get started with libiqxmlrpc in 5 minutes.

## Installation

### Ubuntu/Debian
```bash
sudo apt-get install libboost-dev libboost-date-time-dev \
                     libxml2-dev libssl-dev cmake
git clone https://github.com/yarikmsu/libiqxmlrpc.git
cd libiqxmlrpc
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### macOS (Homebrew)
```bash
brew install boost libxml2 openssl cmake
git clone https://github.com/yarikmsu/libiqxmlrpc.git
cd libiqxmlrpc
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

## Your First Client

```cpp
#include <iqxmlrpc/http_client.h>
#include <iostream>

int main() {
  iqxmlrpc::Client<iqxmlrpc::Http_client_connection> client(
    iqnet::Inet_addr("localhost", 8080)
  );

  iqxmlrpc::Response resp = client.execute("hello", iqxmlrpc::Param_list());
  std::cout << resp.value().get_string() << std::endl;

  return 0;
}
```

## Your First Server

```cpp
#include <iqxmlrpc/http_server.h>
#include <iqxmlrpc/method.h>

class Hello : public iqxmlrpc::Method {
public:
  void execute(const iqxmlrpc::Param_list&, iqxmlrpc::Value& result) {
    result = iqxmlrpc::Value("Hello, world!");
  }
};

int main() {
  iqnet::Inet_addr addr(8080);
  iqxmlrpc::Http_server server(addr);
  server.register_method("hello", new Hello);
  server.work();
}
```

## Compile & Run

```bash
# Compile server
g++ -std=c++17 -o server server.cpp -liqxmlrpc

# Compile client
g++ -std=c++17 -o client client.cpp -liqxmlrpc

# Run server (terminal 1)
./server

# Run client (terminal 2)
./client
```

**Expected output**:
```
Hello, world!
```

**Note**: On some systems, you may need to specify library paths:
```bash
# If you get "cannot find -liqxmlrpc" errors:
g++ -std=c++17 -o client client.cpp -L/usr/local/lib -liqxmlrpc -I/usr/local/include

# Or set LD_LIBRARY_PATH (Linux) / DYLD_LIBRARY_PATH (macOS):
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

## Troubleshooting

### Client cannot connect
**Symptom**: `Connection refused` or timeout errors

**Solution**: Ensure the server is running first (in terminal 1) before running the client

### Link errors during compilation
**Symptom**: `undefined reference to iqxmlrpc::...` or `cannot find -liqxmlrpc`

**Solution**:
1. Verify library is installed: `ls /usr/local/lib/libiqxmlrpc*`
2. If missing, run: `sudo cmake --install build` from the build directory
3. Add library path: `g++ ... -L/usr/local/lib -liqxmlrpc`

### Runtime library not found
**Symptom**: `error while loading shared libraries: libiqxmlrpc.so`

**Solution** (Linux):
```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
# Or add permanently to ~/.bashrc
```

**Solution** (macOS):
```bash
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
```

### Compilation errors with C++17
**Symptom**: `error: 'string_view' is not a member of 'std'`

**Solution**: Ensure your compiler supports C++17:
- GCC 7+ or Clang 5+ required
- Check version: `g++ --version` or `clang++ --version`

## Next Steps

- [Building Guide](Building) - Detailed build options
- [Writing Clients](https://github.com/adedov/libiqxmlrpc/wiki/Writing-Client) - Advanced client patterns
- [Writing Servers](https://github.com/adedov/libiqxmlrpc/wiki/Writing-Server) - Advanced server patterns
- [Performance Tips](Performance-Results) - Get the most speed from your app

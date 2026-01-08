// Simple test harness to verify fuzz targets work
// Reads input from file or stdin

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdint>

// Forward declare the fuzz function
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main(int argc, char** argv) {
  std::string input;

  if (argc > 1) {
    std::ifstream file(argv[1]);
    if (!file) {
      std::cerr << "Cannot open file: " << argv[1] << std::endl;
      return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    input = buffer.str();
  } else {
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    input = buffer.str();
  }

  std::cout << "Testing with " << input.size() << " bytes of input..." << std::endl;

  int result = LLVMFuzzerTestOneInput(
    reinterpret_cast<const uint8_t*>(input.data()),
    input.size()
  );

  std::cout << "Test completed with result: " << result << std::endl;
  return result;
}

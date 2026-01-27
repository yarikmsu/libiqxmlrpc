// Base64 decode performance benchmark
// Measures decode throughput for various data sizes

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <random>
#include "libiqxmlrpc/value.h"

using namespace iqxmlrpc;

// Prevent compiler from optimizing away results
template<typename T>
void do_not_optimize(T&& val) {
    asm volatile("" : : "g"(val) : "memory");
}

// Generate random binary data
std::string generate_random_data(size_t size) {
    std::string data;
    data.reserve(size);
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<> dist(0, 255);
    for (size_t i = 0; i < size; ++i) {
        data.push_back(static_cast<char>(dist(gen)));
    }
    return data;
}

// Benchmark a single decode operation
struct BenchResult {
    double total_ms;
    double ns_per_op;
    double throughput_mbps;
    size_t iterations;
};

BenchResult benchmark_decode(const std::string& base64_data, size_t raw_size, int iterations) {
    // Warmup
    for (int i = 0; i < iterations / 10; ++i) {
        std::unique_ptr<Binary_data> bin(Binary_data::from_base64(base64_data));
        do_not_optimize(bin->get_data());
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::unique_ptr<Binary_data> bin(Binary_data::from_base64(base64_data));
        do_not_optimize(bin->get_data());
    }
    auto end = std::chrono::high_resolution_clock::now();

    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double ns_per_op = (total_ms * 1000000.0) / iterations;
    double bytes_per_second = (raw_size * iterations) / (total_ms / 1000.0);
    double throughput_mbps = bytes_per_second / (1024 * 1024);

    return {total_ms, ns_per_op, throughput_mbps, static_cast<size_t>(iterations)};
}

int main() {
    std::cout << "============================================================\n";
    std::cout << "Base64 Decode Performance Benchmark\n";
    std::cout << "============================================================\n\n";

    // Test sizes: 256B, 1KB, 4KB, 16KB, 64KB, 256KB
    std::vector<size_t> sizes = {256, 1024, 4096, 16384, 65536, 262144};
    std::vector<int> iterations = {100000, 100000, 50000, 10000, 5000, 1000};

    std::cout << std::left << std::setw(12) << "Size"
              << std::right << std::setw(12) << "Iterations"
              << std::setw(14) << "Total (ms)"
              << std::setw(14) << "ns/op"
              << std::setw(16) << "Throughput"
              << "\n";
    std::cout << std::string(68, '-') << "\n";

    for (size_t i = 0; i < sizes.size(); ++i) {
        size_t size = sizes[i];
        int iters = iterations[i];

        // Generate raw data and encode it
        std::string raw_data = generate_random_data(size);
        std::unique_ptr<Binary_data> encoder(Binary_data::from_data(raw_data));
        std::string base64_encoded = encoder->get_base64();

        // Benchmark decode
        BenchResult result = benchmark_decode(base64_encoded, size, iters);

        // Format size string
        std::string size_str;
        if (size >= 1024) {
            size_str = std::to_string(size / 1024) + " KB";
        } else {
            size_str = std::to_string(size) + " B";
        }

        std::cout << std::left << std::setw(12) << size_str
                  << std::right << std::setw(12) << result.iterations
                  << std::setw(14) << std::fixed << std::setprecision(2) << result.total_ms
                  << std::setw(14) << std::setprecision(0) << result.ns_per_op
                  << std::setw(12) << std::setprecision(1) << result.throughput_mbps << " MB/s"
                  << "\n";
    }

    std::cout << "\n============================================================\n";

    // Also test with whitespace (realistic scenario)
    std::cout << "\nWith whitespace (line breaks every 76 chars):\n";
    std::cout << std::string(68, '-') << "\n";

    for (size_t i = 0; i < 3; ++i) {  // Just test 3 sizes with whitespace
        size_t size = sizes[i + 2];  // 4KB, 16KB, 64KB
        int iters = iterations[i + 2];

        std::string raw_data = generate_random_data(size);
        std::unique_ptr<Binary_data> encoder(Binary_data::from_data(raw_data));
        std::string base64_encoded = encoder->get_base64();

        // Add line breaks every 76 characters (MIME style)
        std::string with_breaks;
        for (size_t j = 0; j < base64_encoded.size(); j += 76) {
            with_breaks += base64_encoded.substr(j, 76);
            if (j + 76 < base64_encoded.size()) {
                with_breaks += "\r\n";
            }
        }

        BenchResult result = benchmark_decode(with_breaks, size, iters);

        std::string size_str = std::to_string(size / 1024) + " KB";

        std::cout << std::left << std::setw(12) << size_str
                  << std::right << std::setw(12) << result.iterations
                  << std::setw(14) << std::fixed << std::setprecision(2) << result.total_ms
                  << std::setw(14) << std::setprecision(0) << result.ns_per_op
                  << std::setw(12) << std::setprecision(1) << result.throughput_mbps << " MB/s"
                  << "\n";
    }

    std::cout << "\n============================================================\n";

    return 0;
}

// Fuzz target for Method dispatcher and introspection
// Copyright (C) 2026 libiqxmlrpc contributors
//
// This fuzzer tests the Method_dispatcher_manager which handles:
// - Method registration and lookup
// - Introspection (system.listMethods)
// - Method creation from parsed requests
//
// Security relevance: Method dispatchers are the entry point for RPC calls
// and must safely handle arbitrary method names and parameters.

#include "fuzz_common.h"
#include "libiqxmlrpc/dispatcher_manager.h"
#include "libiqxmlrpc/method.h"
#include "libiqxmlrpc/request.h"
#include "libiqxmlrpc/response.h"
#include "libiqxmlrpc/value.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>

namespace {

// Simple test method for fuzzing
class TestMethod : public iqxmlrpc::Method {
public:
  void execute(const iqxmlrpc::Param_list& params, iqxmlrpc::Value& response) override {
    // Echo back the number of parameters
    response = static_cast<int>(params.size());
  }
};

// Factory for test methods
class TestMethodFactory : public iqxmlrpc::Method_factory_base {
public:
  iqxmlrpc::Method* create() override {
    return new TestMethod();
  }
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Limit input size to prevent slow units
  if (size > fuzz::MAX_INPUT_SIZE) return 0;

  std::string input(reinterpret_cast<const char*>(data), size);

  // Test 1: Parse request and attempt method creation
  try {
    std::unique_ptr<iqxmlrpc::Request> req(iqxmlrpc::parse_request(input));
    if (req) {
      // Create a dispatcher manager with introspection enabled
      iqxmlrpc::Method_dispatcher_manager disp_manager;
      disp_manager.enable_introspection();

      // Register a test method
      disp_manager.register_method("test.echo", new TestMethodFactory());

      // Try to create the method from the parsed request
      iqxmlrpc::Method::Data method_data;
      method_data.method_name = req->get_name();

      iqxmlrpc::Method* method = disp_manager.create_method(method_data);
      if (method) {
        // Execute the method with parsed parameters
        try {
          iqxmlrpc::Value response{iqxmlrpc::Nil{}};
          const iqxmlrpc::Param_list& params = req->get_params();
          method->process_execution(nullptr, params, response);
          fuzz::exercise_value(response);
        } catch (...) {
          // Method execution may throw
        }
        delete method;
      }

      // Exercise get_methods_list (introspection)
      iqxmlrpc::Array methods_list;
      disp_manager.get_methods_list(methods_list);
      (void)methods_list.size();
    }
  } catch (...) {
    // Parsing and dispatch may throw
  }

  // Test 2: Register methods with fuzzed names
  if (size > 1) {
    try {
      iqxmlrpc::Method_dispatcher_manager disp_manager;

      // Extract method name from input
      size_t name_len = data[0] % 64;
      if (name_len > size - 1) name_len = size - 1;
      std::string method_name(reinterpret_cast<const char*>(data + 1), name_len);

      // Register method with fuzzed name
      disp_manager.register_method(method_name, new TestMethodFactory());

      // Try to create the registered method
      iqxmlrpc::Method::Data method_data;
      method_data.method_name = method_name;
      iqxmlrpc::Method* method = disp_manager.create_method(method_data);
      if (method) {
        (void)method->name();
        delete method;
      }

      // Get methods list
      iqxmlrpc::Array methods_list;
      disp_manager.get_methods_list(methods_list);
    } catch (...) {
      // Expected for invalid method names
    }
  }

  // Test 3: Test introspection method names
  {
    static const char* introspection_methods[] = {
      "system.listMethods",
      "system.methodHelp",
      "system.methodSignature",
      "system.getCapabilities",
      "system.multicall",
    };

    if (size >= 1) {
      try {
        iqxmlrpc::Method_dispatcher_manager disp_manager;
        disp_manager.enable_introspection();
        disp_manager.register_method("test.method", new TestMethodFactory());

        size_t idx = data[0] % (sizeof(introspection_methods) / sizeof(introspection_methods[0]));

        iqxmlrpc::Method::Data method_data;
        method_data.method_name = introspection_methods[idx];

        iqxmlrpc::Method* method = disp_manager.create_method(method_data);
        if (method) {
          // Execute introspection method
          iqxmlrpc::Value response{iqxmlrpc::Nil{}};
          iqxmlrpc::Param_list empty_params;
          try {
            method->process_execution(nullptr, empty_params, response);
            fuzz::exercise_value(response);
          } catch (...) {
            // Some introspection methods may require parameters
          }
          delete method;
        }
      } catch (...) {
        // Expected
      }
    }
  }

  // Test 4: Test method lookup with non-existent methods
  if (size >= 1) {
    try {
      iqxmlrpc::Method_dispatcher_manager disp_manager;
      disp_manager.register_method("existing.method", new TestMethodFactory());

      // Try to create a non-existent method
      iqxmlrpc::Method::Data method_data;
      method_data.method_name = input;  // Use raw fuzz input as method name

      iqxmlrpc::Method* method = disp_manager.create_method(method_data);
      if (method) {
        delete method;
      }
      // Null result expected for unknown methods
    } catch (...) {
      // Expected
    }
  }

  return 0;
}

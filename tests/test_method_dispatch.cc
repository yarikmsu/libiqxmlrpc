#define BOOST_TEST_MODULE method_dispatch_test
#include <memory>
#include <vector>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/method.h"
#include "libiqxmlrpc/dispatcher_manager.h"

using namespace boost::unit_test;
using namespace iqxmlrpc;

// Test method that returns a simple value
class TestMethod : public Method {
public:
    int call_count = 0;
    Param_list last_params;

    TestMethod(): Method(), call_count(0), last_params() {}

    void execute(const Param_list& params, Value& result) override {
        call_count++;
        last_params = params;
        result = 42;
    }
};

// Test method that throws a Fault
class FaultMethod : public Method {
public:
    void execute(const Param_list&, Value&) override {
        throw Fault(100, "Test fault");
    }
};

// Test method that returns its parameters
class EchoMethod : public Method {
public:
    void execute(const Param_list& params, Value& result) override {
        Array arr;
        for (const auto& p : params) {
            arr.push_back(p);
        }
        result = arr;
    }
};

// Test interceptor that tracks calls
class TrackingInterceptor : public Interceptor {
public:
    int process_count = 0;
    bool should_yield = true;
    std::string method_name;

    TrackingInterceptor(): Interceptor(), process_count(0), should_yield(true), method_name() {}

    void process(Method* m, const Param_list& params, Value& result) override {
        process_count++;
        method_name = m->name();
        if (should_yield) {
            yield(m, params, result);
        }
    }
};

// Test interceptor that modifies the result
class ModifyingInterceptor : public Interceptor {
public:
    int multiplier = 2;

    void process(Method* m, const Param_list& params, Value& result) override {
        yield(m, params, result);
        // Modify the result after method execution
        if (result.is_int()) {
            result = result.get_int() * multiplier;
        }
    }
};

// Free function for Method_function_adapter test
void test_function(Method* /*m*/, const Param_list& params, Value& result) {
    result = static_cast<int>(params.size());
}

BOOST_AUTO_TEST_SUITE(method_tests)

BOOST_AUTO_TEST_CASE(method_execute_basic)
{
    TestMethod method;
    Value result = Nil();
    Param_list params;

    method.process_execution(nullptr, params, result);

    BOOST_CHECK_EQUAL(method.call_count, 1);
    BOOST_CHECK(result.is_int());
    BOOST_CHECK_EQUAL(result.get_int(), 42);
}

BOOST_AUTO_TEST_CASE(method_execute_with_params)
{
    TestMethod method;
    Value result = Nil();
    Param_list params;
    params.push_back(Value(1));
    params.push_back(Value("test"));
    params.push_back(Value(3.14));

    method.process_execution(nullptr, params, result);

    BOOST_CHECK_EQUAL(method.last_params.size(), 3u);
    BOOST_CHECK_EQUAL(method.last_params[0].get_int(), 1);
    BOOST_CHECK_EQUAL(method.last_params[1].get_string(), "test");
}

BOOST_AUTO_TEST_CASE(method_echo_params)
{
    EchoMethod method;
    Value result = Nil();
    Param_list params;
    params.push_back(Value(100));
    params.push_back(Value("hello"));

    method.process_execution(nullptr, params, result);

    BOOST_CHECK(result.is_array());
    BOOST_CHECK_EQUAL(result.size(), 2u);
    BOOST_CHECK_EQUAL(result[0].get_int(), 100);
    BOOST_CHECK_EQUAL(result[1].get_string(), "hello");
}

BOOST_AUTO_TEST_CASE(method_throws_fault)
{
    FaultMethod method;
    Value result = Nil();
    Param_list params;

    BOOST_CHECK_THROW(method.process_execution(nullptr, params, result), Fault);

    try {
        method.process_execution(nullptr, params, result);
    } catch (const Fault& f) {
        BOOST_CHECK_EQUAL(f.code(), 100);
        BOOST_CHECK_EQUAL(f.what(), std::string("Test fault"));
    }
}

BOOST_AUTO_TEST_CASE(method_authentication)
{
    TestMethod method;

    BOOST_CHECK(!method.authenticated());
    BOOST_CHECK(method.authname().empty());

    method.authname("testuser");

    BOOST_CHECK(method.authenticated());
    BOOST_CHECK_EQUAL(method.authname(), "testuser");
}

BOOST_AUTO_TEST_CASE(method_xheaders)
{
    TestMethod method;
    XHeaders& headers = method.xheaders();

    headers["x-custom-header"] = "custom-value";
    headers["x-another"] = "another-value";

    BOOST_CHECK_EQUAL(method.xheaders()["x-custom-header"], "custom-value");
    BOOST_CHECK_EQUAL(method.xheaders()["x-another"], "another-value");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(interceptor_tests)

BOOST_AUTO_TEST_CASE(single_interceptor)
{
    TestMethod method;
    TrackingInterceptor interceptor;
    Value result = Nil();
    Param_list params;

    method.process_execution(&interceptor, params, result);

    BOOST_CHECK_EQUAL(interceptor.process_count, 1);
    BOOST_CHECK_EQUAL(method.call_count, 1);
    BOOST_CHECK_EQUAL(result.get_int(), 42);
}

BOOST_AUTO_TEST_CASE(interceptor_no_yield)
{
    TestMethod method;
    TrackingInterceptor interceptor;
    interceptor.should_yield = false;
    Value result = Nil();
    Param_list params;

    method.process_execution(&interceptor, params, result);

    BOOST_CHECK_EQUAL(interceptor.process_count, 1);
    BOOST_CHECK_EQUAL(method.call_count, 0);  // Method not called
}

BOOST_AUTO_TEST_CASE(nested_interceptors)
{
    TestMethod method;
    auto* outer = new TrackingInterceptor();
    auto* inner = new TrackingInterceptor();
    outer->nest(inner);
    Value result = Nil();
    Param_list params;

    method.process_execution(outer, params, result);

    BOOST_CHECK_EQUAL(outer->process_count, 1);
    BOOST_CHECK_EQUAL(inner->process_count, 1);
    BOOST_CHECK_EQUAL(method.call_count, 1);

    delete outer;  // Also deletes inner via unique_ptr
}

BOOST_AUTO_TEST_CASE(modifying_interceptor)
{
    TestMethod method;  // Returns 42
    ModifyingInterceptor interceptor;
    interceptor.multiplier = 3;
    Value result = Nil();
    Param_list params;

    method.process_execution(&interceptor, params, result);

    BOOST_CHECK_EQUAL(result.get_int(), 126);  // 42 * 3
}

BOOST_AUTO_TEST_CASE(chained_modifying_interceptors)
{
    TestMethod method;  // Returns 42
    auto* outer = new ModifyingInterceptor();
    auto* inner = new ModifyingInterceptor();
    outer->multiplier = 2;
    inner->multiplier = 3;
    outer->nest(inner);
    Value result = Nil();
    Param_list params;

    method.process_execution(outer, params, result);

    // Inner executes first (after method), then outer
    // method returns 42
    // inner multiplies by 3: 42 * 3 = 126
    // outer multiplies by 2: 126 * 2 = 252
    BOOST_CHECK_EQUAL(result.get_int(), 252);

    delete outer;
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(method_factory_tests)

BOOST_AUTO_TEST_CASE(simple_factory)
{
    Method_factory<TestMethod> factory;
    std::unique_ptr<Method> method(factory.create());

    BOOST_CHECK(method != nullptr);

    Value result = Nil();
    Param_list params;
    method->process_execution(nullptr, params, result);

    BOOST_CHECK_EQUAL(result.get_int(), 42);
}

BOOST_AUTO_TEST_CASE(function_adapter_factory)
{
    Method_factory<Method_function_adapter> factory(test_function);
    std::unique_ptr<Method> method(factory.create());

    BOOST_CHECK(method != nullptr);

    Value result = Nil();
    Param_list params;
    params.push_back(Value(1));
    params.push_back(Value(2));
    params.push_back(Value(3));
    method->process_execution(nullptr, params, result);

    BOOST_CHECK_EQUAL(result.get_int(), 3);  // params.size()
}

BOOST_AUTO_TEST_CASE(multiple_factory_creates)
{
    Method_factory<TestMethod> factory;

    std::unique_ptr<Method> m1(factory.create());
    std::unique_ptr<Method> m2(factory.create());
    std::unique_ptr<Method> m3(factory.create());

    BOOST_CHECK(m1.get() != m2.get());
    BOOST_CHECK(m2.get() != m3.get());
    BOOST_CHECK(m1.get() != m3.get());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(dispatcher_manager_tests)

BOOST_AUTO_TEST_CASE(register_and_create_method)
{
    Method_dispatcher_manager manager;
    manager.register_method("test.method", new Method_factory<TestMethod>());

    Method::Data data;
    data.method_name = "test.method";

    std::unique_ptr<Method> method(manager.create_method(data));

    BOOST_REQUIRE(method != nullptr);
    BOOST_CHECK_EQUAL(method->name(), "test.method");

    Value result = Nil();
    Param_list params;
    method->process_execution(nullptr, params, result);
    BOOST_CHECK_EQUAL(result.get_int(), 42);
}

BOOST_AUTO_TEST_CASE(register_multiple_methods)
{
    Method_dispatcher_manager manager;
    manager.register_method("method.one", new Method_factory<TestMethod>());
    manager.register_method("method.two", new Method_factory<EchoMethod>());

    Method::Data data1;
    data1.method_name = "method.one";
    std::unique_ptr<Method> m1(manager.create_method(data1));

    Method::Data data2;
    data2.method_name = "method.two";
    std::unique_ptr<Method> m2(manager.create_method(data2));

    BOOST_REQUIRE(m1 != nullptr);
    BOOST_REQUIRE(m2 != nullptr);
    BOOST_CHECK_EQUAL(m1->name(), "method.one");
    BOOST_CHECK_EQUAL(m2->name(), "method.two");
}

BOOST_AUTO_TEST_CASE(create_unknown_method_throws)
{
    Method_dispatcher_manager manager;
    manager.register_method("known.method", new Method_factory<TestMethod>());

    Method::Data data;
    data.method_name = "unknown.method";

    BOOST_CHECK_THROW(manager.create_method(data), Unknown_method);
}

BOOST_AUTO_TEST_CASE(get_methods_list_empty)
{
    Method_dispatcher_manager manager;
    Array methods;

    manager.get_methods_list(methods);

    BOOST_CHECK_EQUAL(methods.size(), 0u);
}

BOOST_AUTO_TEST_CASE(get_methods_list_with_methods)
{
    Method_dispatcher_manager manager;
    manager.register_method("alpha", new Method_factory<TestMethod>());
    manager.register_method("beta", new Method_factory<TestMethod>());
    manager.register_method("gamma", new Method_factory<TestMethod>());

    Array methods;
    manager.get_methods_list(methods);

    BOOST_CHECK_EQUAL(methods.size(), 3u);

    // Check that all method names are in the list
    std::vector<std::string> names;
    for (size_t i = 0; i < methods.size(); ++i) {
        names.push_back(methods[i].get_string());
    }

    BOOST_CHECK(std::find(names.begin(), names.end(), "alpha") != names.end());
    BOOST_CHECK(std::find(names.begin(), names.end(), "beta") != names.end());
    BOOST_CHECK(std::find(names.begin(), names.end(), "gamma") != names.end());
}

BOOST_AUTO_TEST_CASE(enable_introspection)
{
    Method_dispatcher_manager manager;
    manager.register_method("test.method", new Method_factory<TestMethod>());
    manager.enable_introspection();

    Method::Data data;
    data.method_name = "system.listMethods";

    std::unique_ptr<Method> list_method(manager.create_method(data));
    BOOST_REQUIRE(list_method != nullptr);

    Value result = Nil();
    Param_list params;
    list_method->process_execution(nullptr, params, result);

    BOOST_CHECK(result.is_array());
    BOOST_CHECK(result.size() >= 2u);  // At least test.method and system.listMethods
}

BOOST_AUTO_TEST_CASE(method_data_propagated)
{
    Method_dispatcher_manager manager;
    manager.register_method("test.method", new Method_factory<TestMethod>());

    Method::Data data;
    data.method_name = "test.method";
    data.peer_addr = iqnet::Inet_addr("192.168.1.1", 8080);

    std::unique_ptr<Method> method(manager.create_method(data));

    BOOST_REQUIRE(method != nullptr);
    BOOST_CHECK_EQUAL(method->name(), "test.method");
    BOOST_CHECK_EQUAL(method->peer_addr().get_host_name(), "192.168.1.1");
    BOOST_CHECK_EQUAL(method->peer_addr().get_port(), 8080);
}

BOOST_AUTO_TEST_CASE(overwrite_method_registration)
{
    Method_dispatcher_manager manager;
    manager.register_method("test.method", new Method_factory<TestMethod>());
    manager.register_method("test.method", new Method_factory<EchoMethod>());

    Method::Data data;
    data.method_name = "test.method";

    std::unique_ptr<Method> method(manager.create_method(data));
    BOOST_REQUIRE(method != nullptr);

    // Should be EchoMethod (the second registration)
    Value result = Nil();
    Param_list params;
    params.push_back(Value("test"));
    method->process_execution(nullptr, params, result);

    BOOST_CHECK(result.is_array());  // EchoMethod returns Array
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(function_adapter_tests)

BOOST_AUTO_TEST_CASE(function_adapter_basic)
{
    Method_function_adapter adapter(test_function);
    Value result = Nil();
    Param_list params;
    params.push_back(Value(1));
    params.push_back(Value(2));

    adapter.process_execution(nullptr, params, result);

    BOOST_CHECK_EQUAL(result.get_int(), 2);  // params.size()
}

BOOST_AUTO_TEST_CASE(function_adapter_empty_params)
{
    Method_function_adapter adapter(test_function);
    Value result = Nil();
    Param_list params;

    adapter.process_execution(nullptr, params, result);

    BOOST_CHECK_EQUAL(result.get_int(), 0);  // empty params
}

BOOST_AUTO_TEST_SUITE_END()

// vim:ts=2:sw=2:et

find_package(Boost 1.53.0 COMPONENTS unit_test_framework program_options)

include_directories(${OPENSSL_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

if (NOT Boost_USE_STATIC_LIBS)
	add_definitions(-DBOOST_TEST_DYN_LINK)
endif (NOT Boost_USE_STATIC_LIBS)

# List to collect all unit test targets
set(UNIT_TEST_TARGETS "" CACHE INTERNAL "")

# Macro for unit tests (added to ctest)
macro (iqxmlrpc_test test_name)
	message("Add test ${test_name}")
	add_executable(${test_name} ${ARGN})
	target_link_libraries(${test_name} iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES} ${OPENSSL_LIBRARIES})
	add_test(NAME ${test_name} COMMAND ${test_name})
	set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} ${test_name} CACHE INTERNAL "")
endmacro (iqxmlrpc_test)

# Macro for integration tests (not added to ctest - require server/client setup)
macro (iqxmlrpc_integration_test test_name)
	message("Add integration test ${test_name}")
	add_executable(${test_name} ${ARGN})
	target_link_libraries(${test_name} iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES} ${OPENSSL_LIBRARIES})
endmacro (iqxmlrpc_integration_test)

iqxmlrpc_test(value-test test_value_usage.cc)
iqxmlrpc_integration_test(server-test test_server.cc server_config.h server_config.cc methods.h methods.cc)
set(CLIENT_COMMON_SRC client_common.h client_common.cc client_opts.h client_opts.cc)
iqxmlrpc_integration_test(client-test ${CLIENT_COMMON_SRC} client.cc)
iqxmlrpc_integration_test(client-stress-test ${CLIENT_COMMON_SRC} client_stress.cc)
iqxmlrpc_test(xheaders-test test_xheaders.cc)

if (NOT WIN32)
	iqxmlrpc_test(parser-test parser2.cc)
endif (NOT WIN32)

# New unit tests for coverage
iqxmlrpc_test(http-test test_http.cc)
iqxmlrpc_test(request-response-test test_request_response.cc)
iqxmlrpc_test(inet-addr-test test_inet_addr.cc)
iqxmlrpc_test(value-types-extended-test test_value_types_extended.cc)
iqxmlrpc_test(method-dispatch-test test_method_dispatch.cc)
iqxmlrpc_test(exceptions-test test_exceptions.cc)
iqxmlrpc_test(socket-test test_socket.cc)
iqxmlrpc_test(safe-math-test test_safe_math.cc)
iqxmlrpc_test(perf-utils-test test_perf_utils.cc)
iqxmlrpc_test(security-test test_security.cc)
iqxmlrpc_test(crlf-injection-test test_crlf_injection.cc)
iqxmlrpc_test(e1-e3-optimization-test test_e1_e3_optimization.cc)
iqxmlrpc_test(num-conv-test test_num_conv.cc)
iqxmlrpc_test(security-edge-cases-test test_security_edge_cases.cc)

# Thread safety tests
# NOTE: These tests stress-test Boost.Lockfree which produces TSan false positives.
# The tests validate correctness via data integrity checks, not TSan cleanliness.
# Use label "lockfree_stress" to exclude from TSan CI runs via: ctest -LE lockfree_stress
iqxmlrpc_test(thread-safety-test test_thread_safety.cc methods.cc)
set_tests_properties(thread-safety-test PROPERTIES
    TIMEOUT 180
    LABELS "lockfree_stress")

# Integration test (in-process server/client testing)
# Split into multiple files for faster incremental compilation
set(INTEGRATION_TEST_SOURCES
    test_integration.cc
    test_integration_security.cc
    test_integration_server.cc
    test_integration_client.cc
    test_integration_connection.cc
    test_integration_ssl.cc
    test_integration_errors.cc
    test_integration_reactor.cc
    test_integration_advanced.cc
    test_coverage_gaps.cc
    methods.cc
)
iqxmlrpc_test(integration-test ${INTEGRATION_TEST_SOURCES})
set_tests_properties(integration-test PROPERTIES TIMEOUT 120)

# Run tests with: make check (or cmake --build . --target check)
# Note: Not ALL - tests run explicitly via CI Test step or make check
add_custom_target(check
	COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
	DEPENDS ${UNIT_TEST_TARGETS}
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	COMMENT "Running unit tests..."
)

# TODO: server-stop-test

# Performance benchmark (separate from unit tests - run with make perf-test)
add_executable(performance-test test_performance.cc)
target_link_libraries(performance-test iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES})

add_custom_target(perf-test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/performance-test
    DEPENDS performance-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running performance benchmarks..."
)

# M1-M4 optimization baseline benchmarks (separate - run with make perf-m1-m4)
add_executable(performance-m1-m4-test test_performance_m1_m4.cc)
target_link_libraries(performance-m1-m4-test iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES})

add_custom_target(perf-m1-m4
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/performance-m1-m4-test
    DEPENDS performance-m1-m4-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running M1-M4 optimization baseline benchmarks..."
)

# E1-E3 optimization benchmarks (separate - run with make perf-e1-e3)
add_executable(performance-e1-e3-test test_performance_e1_e3.cc)
target_link_libraries(performance-e1-e3-test iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES})

add_custom_target(perf-e1-e3
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/performance-e1-e3-test
    DEPENDS performance-e1-e3-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running E1-E3 ostringstream optimization benchmarks..."
)

# Base64 decode optimization benchmark (separate - run with make perf-base64)
add_executable(base64-benchmark-test test_base64_benchmark.cc)
target_link_libraries(base64-benchmark-test iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES})

add_custom_target(perf-base64
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/base64-benchmark-test
    DEPENDS base64-benchmark-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running Base64 decode optimization benchmarks..."
)

# XML parsing optimization benchmark (separate - run with make perf-xml)
add_executable(xml-parsing-benchmark-test test_xml_parsing_benchmark.cc)
target_link_libraries(xml-parsing-benchmark-test iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES})

add_custom_target(perf-xml
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/xml-parsing-benchmark-test
    DEPENDS xml-parsing-benchmark-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running XML parsing optimization benchmarks..."
)

# M6 HTTP tokenization benchmarks (separate - run with make perf-http-tokenize)
add_executable(http-tokenize-benchmark-test test_http_tokenize_benchmark.cc)
target_link_libraries(http-tokenize-benchmark-test iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES})

add_custom_target(perf-http-tokenize
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/http-tokenize-benchmark-test
    DEPENDS http-tokenize-benchmark-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running M6 HTTP tokenization benchmarks..."
)

# M5 State machine lookup benchmarks (separate - run with make perf-state-machine)
add_executable(state-machine-benchmark-test test_state_machine_benchmark.cc)
target_link_libraries(state-machine-benchmark-test iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES})

add_custom_target(perf-state-machine
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/state-machine-benchmark-test
    DEPENDS state-machine-benchmark-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running M5 state machine lookup benchmarks..."
)

# Wire protocol compatibility test (requires external Python XML-RPC server)
add_executable(wire-compatibility-test test_wire_compatibility.cc)
target_link_libraries(wire-compatibility-test iqxmlrpc ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES} ${OPENSSL_LIBRARIES})

# Copy interop test scripts to build directory
file(COPY interop DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Interoperability test target (run with: make interop-test)
# Starts Python XML-RPC server, runs wire-compatibility-test, reports results
add_custom_target(interop-test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/interop/run_interop_tests.sh ${CMAKE_BINARY_DIR}
    DEPENDS wire-compatibility-test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running interoperability tests against Python XML-RPC server..."
)

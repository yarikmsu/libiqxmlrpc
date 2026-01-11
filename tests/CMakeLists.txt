find_package(Boost 1.41.0 COMPONENTS unit_test_framework program_options thread chrono)

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

# Integration test (in-process server/client testing)
iqxmlrpc_test(integration-test test_integration.cc methods.cc)
set_tests_properties(integration-test PROPERTIES TIMEOUT 120)

# Run tests automatically after build
add_custom_target(check ALL
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

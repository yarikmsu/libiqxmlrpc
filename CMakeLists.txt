cmake_minimum_required(VERSION 3.10)
project(Libiqxmlrpc)
set(Libiqxmlrpc_VERSION 0.14.1)

# Require C++17 standard for entire project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable strict compiler warnings by default
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Weffc++ -Werror)
endif()

# Code coverage option
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()

# Test hooks option - must be set before library compilation
option(build_tests "Build tests?" ON)
option(ENABLE_TESTING_HOOKS "Enable test hooks for SSL factory injection (test builds only)" OFF)
if(ENABLE_TESTING_HOOKS)
    if(NOT build_tests)
        message(WARNING "ENABLE_TESTING_HOOKS is enabled but build_tests is OFF. "
                        "Testing hooks are only useful when tests are built.")
    endif()
    add_compile_definitions(IQXMLRPC_TESTING)
endif()

add_subdirectory(libiqxmlrpc)

if (build_tests)
	enable_testing()
	add_subdirectory(tests)
endif (build_tests)

option(build_fuzzers "Build fuzz targets?" OFF)
if (build_fuzzers)
	add_subdirectory(fuzz)
endif (build_fuzzers)

option(build_docs "Build doxygen docs?" ON)
if (build_docs)
	add_subdirectory(doc)
endif (build_docs)

add_custom_target(clog
	COMMAND git log > ChangeLog)

# clang-tidy target for parallel static analysis
# Requires: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
# Usage: make clang-tidy
# Search common locations including Homebrew LLVM on macOS
set(LLVM_SEARCH_PATHS
    /opt/homebrew/opt/llvm/bin
    /usr/local/opt/llvm/bin
    /usr/lib/llvm-18/bin
    /usr/lib/llvm-17/bin
)
find_program(RUN_CLANG_TIDY NAMES run-clang-tidy run-clang-tidy.py
    HINTS ${LLVM_SEARCH_PATHS})
find_program(CLANG_TIDY_BIN NAMES clang-tidy
    HINTS ${LLVM_SEARCH_PATHS})

# On macOS with Homebrew LLVM, we need to specify the SDK path
set(CLANG_TIDY_EXTRA_ARGS "")
if(APPLE)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE MACOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE XCRUN_RESULT)
    if(XCRUN_RESULT EQUAL 0 AND MACOS_SDK_PATH)
        set(CLANG_TIDY_EXTRA_ARGS "-extra-arg=-isysroot${MACOS_SDK_PATH}")
    endif()
endif()

if(RUN_CLANG_TIDY AND CLANG_TIDY_BIN)
    add_custom_target(clang-tidy
        COMMAND ${RUN_CLANG_TIDY}
            -clang-tidy-binary ${CLANG_TIDY_BIN}
            -p ${CMAKE_BINARY_DIR}
            -header-filter=libiqxmlrpc/
            ${CLANG_TIDY_EXTRA_ARGS}
            "libiqxmlrpc/.*\\.cc"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy in parallel..."
        VERBATIM)
elseif(CLANG_TIDY_BIN)
    # Fallback: use find/xargs if run-clang-tidy not available
    add_custom_target(clang-tidy
        COMMAND find ${CMAKE_SOURCE_DIR}/libiqxmlrpc -name "*.cc" -print0 |
            xargs -0 -P $$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) -n 1
            ${CLANG_TIDY_BIN} -p ${CMAKE_BINARY_DIR} -header-filter=libiqxmlrpc/ ${CLANG_TIDY_EXTRA_ARGS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy (parallel fallback)..."
        VERBATIM)
else()
    message(STATUS "clang-tidy not found, 'make clang-tidy' target disabled")
endif()
if(CLANG_TIDY_BIN AND NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    message(STATUS "Note: 'make clang-tidy' requires CMAKE_EXPORT_COMPILE_COMMANDS=ON")
endif()

add_custom_target(dist
	COMMAND git archive --prefix=libiqxmlrpc-${Libiqxmlrpc_VERSION}/ --format=tar HEAD | bzip2 > libiqxmlrpc-${Libiqxmlrpc_VERSION}.tar.bz2
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

include(LibiqxmlrpcCPack.txt)

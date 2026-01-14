cmake_minimum_required(VERSION 3.10)
project(Libiqxmlrpc)
set(Libiqxmlrpc_VERSION 0.13.6)

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

add_custom_target(dist
	COMMAND git archive --prefix=libiqxmlrpc-${Libiqxmlrpc_VERSION}/ --format=tar HEAD | bzip2 > libiqxmlrpc-${Libiqxmlrpc_VERSION}.tar.bz2
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

include(LibiqxmlrpcCPack.txt)

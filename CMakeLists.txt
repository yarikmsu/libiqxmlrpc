cmake_minimum_required(VERSION 3.10)
project(Libiqxmlrpc)
set(Libiqxmlrpc_VERSION 0.13.6)

# Require C++17 standard for entire project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Code coverage option
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()

add_subdirectory(libiqxmlrpc)

option(build_tests "Build tests?" ON)
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

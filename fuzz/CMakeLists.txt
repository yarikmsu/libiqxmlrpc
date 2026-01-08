cmake_minimum_required(VERSION 3.10)

# Fuzz targets for libiqxmlrpc
# These are built when FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is defined

set(FUZZ_TARGETS
  fuzz_request
  fuzz_response
  fuzz_http
)

foreach(target ${FUZZ_TARGETS})
  add_executable(${target} ${target}.cc)
  target_link_libraries(${target} iqxmlrpc ${LIB_FUZZING_ENGINE})
  target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR})
endforeach()

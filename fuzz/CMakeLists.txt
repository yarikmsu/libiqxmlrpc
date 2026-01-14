cmake_minimum_required(VERSION 3.10)

# Fuzz targets for libiqxmlrpc
#
# Build requirements:
#   - LIB_FUZZING_ENGINE must be set (e.g., -fsanitize=fuzzer for libFuzzer)
#   - Recommend building with ASan/UBSan for maximum bug detection:
#     cmake -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer-no-link,address,undefined" \
#           -DLIB_FUZZING_ENGINE="-fsanitize=fuzzer" ..
#
# Running fuzzers:
#   ./fuzz_request -dict=../fuzz/xml.dict corpus/request/
#   ./fuzz_response -dict=../fuzz/xml.dict corpus/response/
#   ./fuzz_http -dict=../fuzz/http.dict corpus/http/
#   ./fuzz_value -dict=../fuzz/xml.dict corpus/value/
#
# Dictionaries:
#   xml.dict  - XML-RPC tokens for fuzz_request, fuzz_response, fuzz_value
#   http.dict - HTTP tokens for fuzz_http

# Validate that LIB_FUZZING_ENGINE is set
if(NOT DEFINED LIB_FUZZING_ENGINE OR "${LIB_FUZZING_ENGINE}" STREQUAL "")
  message(WARNING "LIB_FUZZING_ENGINE not set. Fuzz targets will not link correctly. "
                  "Set -DLIB_FUZZING_ENGINE=\"-fsanitize=fuzzer\" for libFuzzer.")
endif()

set(FUZZ_TARGETS
  fuzz_request
  fuzz_response
  fuzz_http
  fuzz_value
)

foreach(target ${FUZZ_TARGETS})
  # EXCLUDE_FROM_ALL prevents building fuzz targets in normal builds
  # Build explicitly with: make fuzz_request fuzz_response fuzz_http fuzz_value
  add_executable(${target} EXCLUDE_FROM_ALL ${target}.cc)
  target_link_libraries(${target} iqxmlrpc ${LIB_FUZZING_ENGINE})
  target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR})
endforeach()

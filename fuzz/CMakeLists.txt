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
#   ./fuzz_packet -dict=../fuzz/http.dict corpus/packet/
#   ./fuzz_base64 corpus/base64/
#   ./fuzz_serialize -dict=../fuzz/xml.dict corpus/serialize/
#   ./fuzz_num_conv corpus/num_conv/
#   ./fuzz_xheaders corpus/xheaders/
#
# Dictionaries:
#   xml.dict  - XML-RPC tokens for fuzz_request, fuzz_response, fuzz_value, fuzz_serialize
#   http.dict - HTTP tokens for fuzz_http, fuzz_packet
#
# Seed corpus:
#   fuzz/corpus/ contains initial seed inputs for each fuzzer

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
  fuzz_packet
  fuzz_base64
  fuzz_serialize
  fuzz_num_conv
  fuzz_xheaders
  fuzz_inet_addr
  fuzz_dispatcher
)

foreach(target ${FUZZ_TARGETS})
  # EXCLUDE_FROM_ALL prevents building fuzz targets in normal builds
  # Build explicitly with: make fuzz_request fuzz_response ...
  add_executable(${target} EXCLUDE_FROM_ALL ${target}.cc)
  target_link_libraries(${target} iqxmlrpc ${LIB_FUZZING_ENGINE})
  target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR})
endforeach()

# Copy dictionaries with OSS-Fuzz naming convention
# OSS-Fuzz expects <fuzzer_name>.dict alongside the fuzzer binary
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xml.dict
               ${CMAKE_CURRENT_BINARY_DIR}/fuzz_request.dict COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xml.dict
               ${CMAKE_CURRENT_BINARY_DIR}/fuzz_response.dict COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xml.dict
               ${CMAKE_CURRENT_BINARY_DIR}/fuzz_value.dict COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xml.dict
               ${CMAKE_CURRENT_BINARY_DIR}/fuzz_serialize.dict COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/http.dict
               ${CMAKE_CURRENT_BINARY_DIR}/fuzz_http.dict COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/http.dict
               ${CMAKE_CURRENT_BINARY_DIR}/fuzz_packet.dict COPYONLY)

# Copy seed corpus directories for local testing
# OSS-Fuzz uses _seed_corpus.zip files created by build.sh
file(GLOB CORPUS_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/corpus/*")
foreach(corpus_dir ${CORPUS_DIRS})
  get_filename_component(corpus_name ${corpus_dir} NAME)
  file(COPY ${corpus_dir} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/corpus/)
endforeach()

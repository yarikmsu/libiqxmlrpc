#!/bin/bash -eu
# OSS-Fuzz build script for libiqxmlrpc
# Copyright (C) 2024 libiqxmlrpc contributors

# Build libiqxmlrpc with fuzzing instrumentation
mkdir -p build
cd build
cmake .. \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DCMAKE_CXX_FLAGS="$CXXFLAGS -DBOOST_TIMER_ENABLE_DEPRECATED" \
    -DCMAKE_EXE_LINKER_FLAGS="$LIB_FUZZING_ENGINE" \
    -DCMAKE_SHARED_LINKER_FLAGS="$LIB_FUZZING_ENGINE" \
    -Dbuild_tests=OFF

make -j$(nproc)
cd ..

# Build fuzz targets
for fuzzer in fuzz/fuzz_*.cc; do
    name=$(basename "$fuzzer" .cc)
    $CXX $CXXFLAGS -DBOOST_TIMER_ENABLE_DEPRECATED \
        -I. \
        "$fuzzer" \
        -o "$OUT/$name" \
        build/libiqxmlrpc/libiqxmlrpc.a \
        $LIB_FUZZING_ENGINE \
        -lxml2 -lboost_date_time -lboost_thread -lpthread
done

# Copy seed corpus
mkdir -p "$OUT/fuzz_request_seed_corpus"
mkdir -p "$OUT/fuzz_response_seed_corpus"

# Create seed corpus for request fuzzer
cat > "$OUT/fuzz_request_seed_corpus/sample1.xml" << 'EOF'
<?xml version="1.0"?>
<methodCall>
  <methodName>examples.getStateName</methodName>
  <params>
    <param><value><i4>41</i4></value></param>
  </params>
</methodCall>
EOF

cat > "$OUT/fuzz_request_seed_corpus/sample2.xml" << 'EOF'
<?xml version="1.0"?>
<methodCall>
  <methodName>sample.sumAndDifference</methodName>
  <params>
    <param><value><int>5</int></value></param>
    <param><value><int>3</int></value></param>
  </params>
</methodCall>
EOF

# Create seed corpus for response fuzzer
cat > "$OUT/fuzz_response_seed_corpus/sample1.xml" << 'EOF'
<?xml version="1.0"?>
<methodResponse>
  <params>
    <param><value><string>South Dakota</string></value></param>
  </params>
</methodResponse>
EOF

cat > "$OUT/fuzz_response_seed_corpus/sample2.xml" << 'EOF'
<?xml version="1.0"?>
<methodResponse>
  <fault>
    <value>
      <struct>
        <member>
          <name>faultCode</name>
          <value><int>4</int></value>
        </member>
        <member>
          <name>faultString</name>
          <value><string>Too many parameters.</string></value>
        </member>
      </struct>
    </value>
  </fault>
</methodResponse>
EOF

# Zip seed corpora
cd "$OUT"
zip -q fuzz_request_seed_corpus.zip fuzz_request_seed_corpus/*
zip -q fuzz_response_seed_corpus.zip fuzz_response_seed_corpus/*
rm -rf fuzz_request_seed_corpus fuzz_response_seed_corpus

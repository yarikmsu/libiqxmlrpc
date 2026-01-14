#!/bin/bash -eu
# OSS-Fuzz build script for libiqxmlrpc
# Copyright (C) 2024 libiqxmlrpc contributors

# Build libxml2 as static library
cd $SRC
if [ ! -d libxml2 ]; then
    git clone --depth 1 https://gitlab.gnome.org/GNOME/libxml2.git
fi
cd libxml2
mkdir -p build && cd build
cmake .. \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DBUILD_SHARED_LIBS=OFF \
    -DLIBXML2_WITH_PYTHON=OFF \
    -DLIBXML2_WITH_LZMA=OFF \
    -DLIBXML2_WITH_ZLIB=OFF \
    -DLIBXML2_WITH_ICU=OFF
make -j$(nproc)
LIBXML2_DIR="$SRC/libxml2"
LIBXML2_LIB="$LIBXML2_DIR/build/libxml2.a"
LIBXML2_INCLUDE_SRC="$LIBXML2_DIR/include"
LIBXML2_INCLUDE_BUILD="$LIBXML2_DIR/build"

# Build libiqxmlrpc with fuzzing instrumentation as static library
cd $SRC/libiqxmlrpc
mkdir -p build
cd build
cmake .. \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DCMAKE_CXX_FLAGS="$CXXFLAGS -std=c++17 -DBOOST_TIMER_ENABLE_DEPRECATED -I$LIBXML2_INCLUDE_SRC -I$LIBXML2_INCLUDE_BUILD" \
    -DBUILD_SHARED_LIBS=OFF \
    -Dbuild_tests=OFF \
    -DLIBXML2_INCLUDE_DIR="$LIBXML2_INCLUDE_SRC;$LIBXML2_INCLUDE_BUILD" \
    -DLIBXML2_LIBRARY="$LIBXML2_LIB"

make -j$(nproc)
cd ..

# Build fuzz targets with static linking
for fuzzer in fuzz/fuzz_*.cc; do
    name=$(basename "$fuzzer" .cc)
    $CXX $CXXFLAGS -std=c++17 -DBOOST_TIMER_ENABLE_DEPRECATED \
        -I. -I"$LIBXML2_INCLUDE_SRC" -I"$LIBXML2_INCLUDE_BUILD" \
        "$fuzzer" \
        -o "$OUT/$name" \
        build/libiqxmlrpc/libiqxmlrpc.a \
        "$LIBXML2_LIB" \
        $LIB_FUZZING_ENGINE \
        -Wl,-Bstatic -lboost_date_time -lboost_thread -lboost_system \
        -Wl,-Bdynamic -lpthread
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

# Create seed corpus for fuzz_http
mkdir -p "$OUT/fuzz_http_seed_corpus"
cat > "$OUT/fuzz_http_seed_corpus/request.http" << 'EOF'
POST /RPC2 HTTP/1.1
Host: localhost:8080
Content-Type: text/xml
Content-Length: 100

EOF

cat > "$OUT/fuzz_http_seed_corpus/response.http" << 'EOF'
HTTP/1.1 200 OK
Content-Type: text/xml
Content-Length: 100
Connection: keep-alive

EOF

# Create seed corpus for fuzz_value
mkdir -p "$OUT/fuzz_value_seed_corpus"
cat > "$OUT/fuzz_value_seed_corpus/int.xml" << 'EOF'
<int>42</int>
EOF

cat > "$OUT/fuzz_value_seed_corpus/string.xml" << 'EOF'
<string>Hello World</string>
EOF

cat > "$OUT/fuzz_value_seed_corpus/struct.xml" << 'EOF'
<struct><member><name>key</name><value><string>value</string></value></member></struct>
EOF

cat > "$OUT/fuzz_value_seed_corpus/array.xml" << 'EOF'
<array><data><value><i4>1</i4></value><value><i4>2</i4></value></data></array>
EOF

# Create seed corpus for fuzz_packet
mkdir -p "$OUT/fuzz_packet_seed_corpus"
cat > "$OUT/fuzz_packet_seed_corpus/full_request.http" << 'EOF'
POST /RPC2 HTTP/1.1
Host: localhost:8080
Content-Type: text/xml
Content-Length: 39

<?xml version="1.0"?><methodCall/>
EOF

# Create seed corpus for fuzz_base64
mkdir -p "$OUT/fuzz_base64_seed_corpus"
echo -n "SGVsbG8gV29ybGQ=" > "$OUT/fuzz_base64_seed_corpus/hello.b64"
echo -n "dGVzdA==" > "$OUT/fuzz_base64_seed_corpus/test.b64"
echo -n "YQ==" > "$OUT/fuzz_base64_seed_corpus/single.b64"

# Create seed corpus for fuzz_serialize
mkdir -p "$OUT/fuzz_serialize_seed_corpus"
cat > "$OUT/fuzz_serialize_seed_corpus/request.xml" << 'EOF'
<?xml version="1.0"?>
<methodCall>
  <methodName>test.method</methodName>
  <params>
    <param><value><string>test</string></value></param>
  </params>
</methodCall>
EOF

cat > "$OUT/fuzz_serialize_seed_corpus/response.xml" << 'EOF'
<?xml version="1.0"?>
<methodResponse>
  <params>
    <param><value><string>result</string></value></param>
  </params>
</methodResponse>
EOF

# Create seed corpus for fuzz_num_conv
mkdir -p "$OUT/fuzz_num_conv_seed_corpus"
echo -n "42" > "$OUT/fuzz_num_conv_seed_corpus/int.txt"
echo -n "-123456789" > "$OUT/fuzz_num_conv_seed_corpus/negative.txt"
echo -n "3.14159265358979" > "$OUT/fuzz_num_conv_seed_corpus/double.txt"
echo -n "9223372036854775807" > "$OUT/fuzz_num_conv_seed_corpus/int64_max.txt"

# Create seed corpus for fuzz_xheaders
mkdir -p "$OUT/fuzz_xheaders_seed_corpus"
echo -n "X-Custom-Header-Value" > "$OUT/fuzz_xheaders_seed_corpus/valid.txt"
echo -n "value with spaces" > "$OUT/fuzz_xheaders_seed_corpus/spaces.txt"

# Create seed corpus for fuzz_inet_addr
mkdir -p "$OUT/fuzz_inet_addr_seed_corpus"
echo -n "127.0.0.1" > "$OUT/fuzz_inet_addr_seed_corpus/localhost.txt"
echo -n "192.168.1.1" > "$OUT/fuzz_inet_addr_seed_corpus/private.txt"
echo -n "10.0.0.1" > "$OUT/fuzz_inet_addr_seed_corpus/class_a.txt"
printf "host\nname" > "$OUT/fuzz_inet_addr_seed_corpus/crlf.txt"

# Create seed corpus for fuzz_dispatcher
mkdir -p "$OUT/fuzz_dispatcher_seed_corpus"
cat > "$OUT/fuzz_dispatcher_seed_corpus/listmethods.xml" << 'EOF'
<?xml version="1.0"?>
<methodCall>
<methodName>system.listMethods</methodName>
<params></params>
</methodCall>
EOF

cat > "$OUT/fuzz_dispatcher_seed_corpus/echo.xml" << 'EOF'
<?xml version="1.0"?>
<methodCall>
<methodName>test.echo</methodName>
<params><param><value><string>hello</string></value></param></params>
</methodCall>
EOF

# Zip all seed corpora
cd "$OUT"
for corpus_dir in fuzz_*_seed_corpus; do
    if [ -d "$corpus_dir" ]; then
        zip -q "${corpus_dir}.zip" "$corpus_dir"/*
        rm -rf "$corpus_dir"
    fi
done

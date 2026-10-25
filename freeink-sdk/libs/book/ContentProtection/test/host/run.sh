#!/bin/sh
# Builds and runs the ContentProtection host tests.
# No device or PlatformIO needed.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/contentprotection-tests"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

c++ -std=c++17 -fno-exceptions -Wall -Wextra -Werror -I../../include \
  ../../src/Zip.cpp test_zip.cpp -o "$BUILD_DIR/test_zip"
"$BUILD_DIR/test_zip"

c++ -std=c++17 -fno-exceptions -Wall -Wextra -Werror -I../../include \
  -I../../../../network/JsonSax/include \
  ../../src/LcpLicense.cpp ../../../../network/JsonSax/src/StreamingJsonParser.cpp \
  test_lcp.cpp -o "$BUILD_DIR/test_lcp"
"$BUILD_DIR/test_lcp"

cc -w -I../../include -I../../third_party/miniz -c ../../src/vendor/miniz_impl.c -o "$BUILD_DIR/miniz.o"
c++ -std=c++17 -fno-exceptions -Wall -Wextra -Werror -I../../include -I../../third_party/miniz \
  ../../src/ProtectedBook.cpp ../../src/Zip.cpp "$BUILD_DIR/miniz.o" test_protected.cpp \
  -o "$BUILD_DIR/test_protected"
"$BUILD_DIR/test_protected"

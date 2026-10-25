#!/bin/sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-crossnext-ota-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -DFREEINK_DEVICE_X4PRO=1 \
  -I../../lib/JsonParser -I../../freeink-sdk/libs/network/JsonSax/include \
  ../../freeink-sdk/libs/network/JsonSax/src/StreamingJsonParser.cpp ../../lib/JsonParser/ReleaseJsonParser.cpp \
  test.cpp -o "$BUILD_DIR/test_crossnext_ota"
"$BUILD_DIR/test_crossnext_ota"

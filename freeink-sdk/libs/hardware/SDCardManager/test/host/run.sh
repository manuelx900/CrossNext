#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/freeink-sd-stream.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror ${CXXFLAGS:-} \
  -Istubs -I../../include test_stream.cpp ../../src/SDCardManager.cpp \
  -o "$BUILD_DIR/test_stream"
"$BUILD_DIR/test_stream"

#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/xtc-reading-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-parentheses -O2 \
  -Istubs -I../reading-suite/stubs -I../../lib/Memory -I../../lib/Xtc \
  test.cpp ../../lib/Xtc/Xtc/XtcParser.cpp ../../src/activities/reader/ReaderSession.cpp \
  ../../src/apps_local/reading/ReadingStatsCore.cpp ../../src/apps_local/reading/ReadingStatsStore.cpp \
  -o "$BUILD_DIR/test"
"$BUILD_DIR/test"

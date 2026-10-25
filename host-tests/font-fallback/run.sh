#!/bin/sh
set -eu
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/crossplay-font-fallback-tests"
mkdir -p "$OUT"
"${CXX:-c++}" -std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -I../../lib/EpdFont -I../../lib/Utf8 -I../../lib/GfxRenderer -I../../src/apps_local/ui \
  test_fallback.cpp ../../lib/EpdFont/EpdFont.cpp ../../lib/EpdFont/EpdFontFamily.cpp \
  ../../lib/Utf8/Utf8.cpp -o "$OUT/test"
"$OUT/test"

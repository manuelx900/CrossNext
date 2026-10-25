#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="qa-artifacts/reader-speed-2026-10-20/native-bitmap"
mkdir -p "$out"
"${CXX:-c++}" -std=c++20 -O2 -Wno-comment -Ihost-tests/flashcards-stress/stubs -Ihost-tests/study-suite/stubs -Ilib/GfxRenderer \
 host-tests/bitmap-read-ahead/test.cpp lib/GfxRenderer/Bitmap.cpp lib/GfxRenderer/BitmapHelpers.cpp -o "$out/test"
"$out/test"

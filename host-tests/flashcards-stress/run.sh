#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="qa-artifacts/flashcards-stress-2026-10-19/native"
mkdir -p "$out"
"${CXX:-c++}" -std=c++20 -O2 -Wno-comment -Wno-unused-function -Wno-unused-variable -D__LINUX__ -DNO_SIMD -DFLASH_STRESS_NATIVE_LOG=1 \
 -Ihost-tests/flashcards-stress/stubs -Ihost-tests/study-suite/stubs -Ilib/I18n -Ilib/Epub -Ilib/Memory -Ilib/zstd/src \
 -Ilib/ZipFile -Ilib/miniz/src -Ilib/GfxRenderer -Ilib/PngToBmpConverter -Ilib/JpegToBmpConverter \
 -I.pio/libdeps/gh_release_x4pro/JPEGDEC/src \
 host-tests/flashcards-stress/test.cpp src/apps_local/flashcards/FlashImport.cpp src/apps_local/flashcards/FlashMedia.cpp \
 src/apps_local/flashcards/FlashText.cpp src/apps_local/flashcards/AnkiSqlite.cpp lib/Epub/Epub/htmlEntities.cpp \
 src/apps_local/study/StudyDeck.cpp src/apps_local/study/StudyImages.cpp src/apps_local/study/StudyFsrs.cpp \
 lib/ZipFile/ZipFile.cpp lib/miniz/src/InflateStream.cpp lib/PngToBmpConverter/PngToBmpConverter.cpp \
 lib/JpegToBmpConverter/JpegToBmpConverter.cpp lib/GfxRenderer/BitmapHelpers.cpp \
 .pio/libdeps/gh_release_x4pro/JPEGDEC/src/JPEGDEC.cpp \
 -x c++ lib/miniz/src/miniz_impl.c lib/zstd/src/zstddeclib.c -o "$out/tests"
"$out/tests" "assets_local/simulator/stress-corpus" "$out"

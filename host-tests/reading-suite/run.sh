#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/reading-suite-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 -Istubs -I../../lib/Memory test_core.cpp \
  ../../src/apps_local/home/HomeMenuCore.cpp ../../src/apps_local/home/HomeMenuStore.cpp ../../src/apps_local/game2048/Game2048Core.cpp \
  ../../src/apps_local/game2048/Game2048Codec.cpp ../../src/apps_local/game2048/Game2048Store.cpp \
  ../../src/apps_local/reading/ReadingStatsCore.cpp ../../src/apps_local/reading/ReadingStatsStore.cpp \
  ../../src/activities/reader/ReaderSession.cpp -o "$BUILD_DIR/test_core"
"$BUILD_DIR/test_core"
./run-ui.sh "$@"

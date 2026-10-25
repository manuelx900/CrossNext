#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/chess-puzzles-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror -I../reading-suite/stubs test_puzzles.cpp \
  ../../src/apps_local/chess/ChessCore.cpp ../../src/apps_local/chess/ChessEngine.cpp ../../src/apps_local/chess/puzzles/ChessPuzzleCore.cpp \
  ../../src/apps_local/chess/puzzles/ChessPuzzleData.cpp ../../src/apps_local/chess/puzzles/ChessPuzzleStore.cpp -o "$BUILD_DIR/test_puzzles"
"$BUILD_DIR/test_puzzles"

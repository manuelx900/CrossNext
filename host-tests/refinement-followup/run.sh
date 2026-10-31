#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/crossnext-refinement-followup"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror host-tests/refinement-followup/test.cpp src/apps_local/flashcards/FlashHistory.cpp src/apps_local/game2048/Game2048Core.cpp -o "$out/tests"
"$out/tests"

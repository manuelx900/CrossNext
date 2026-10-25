#!/bin/sh
set -eu
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/crossplay-refinement-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$OUT"
"${CXX:-c++}" -std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -Wno-misleading-indentation test_core.cpp ../../src/apps_local/quest/QuestCore.cpp ../../src/apps_local/reading/ReadingStatsCore.cpp ../../src/apps_local/wordle/WordleCore.cpp ../../src/apps_local/wordle/LocalWordle.cpp ../../src/apps_local/picross/PicrossCore.cpp ../../src/apps_local/habits/HabitsCore.cpp -o "$OUT/test"
"$OUT/test"

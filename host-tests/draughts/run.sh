#!/bin/sh
set -eu
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/crossplay-draughts-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$OUT"
"${CXX:-c++}" -std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror test_core.cpp ../../src/apps_local/checkers/DraughtsCore.cpp -o "$OUT/test"
"$OUT/test"

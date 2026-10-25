#!/bin/sh
set -eu
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/crossplay-picross-store-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$OUT"
"${PYTHON:-python3}" extract.py "$OUT/production.inc"
"${CXX:-c++}" -std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -I"$OUT" test_store.cpp ../../src/apps_local/picross/PicrossCore.cpp -o "$OUT/test"
"$OUT/test"

#!/bin/sh
set -eu
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/crossplay-localwordle-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$OUT"
"${PYTHON:-python3}" ../../tools_local/wordle/build_words.py
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror -I../reading-suite/stubs test_core.cpp ../../src/apps_local/wordle/WordleCore.cpp ../../src/apps_local/wordle/LocalWordle.cpp ../../src/apps_local/wordle/LocalWordleStore.cpp -o "$OUT/test"
"$OUT/test"

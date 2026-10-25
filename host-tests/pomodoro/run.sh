#!/bin/sh
set -eu
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/crossplay-pomodoro-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$OUT"
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror -I../reading-suite/stubs test_core.cpp ../../src/apps_local/pomodoro/PomodoroCore.cpp ../../src/apps_local/pomodoro/PomodoroStore.cpp -o "$OUT/test"
"$OUT/test"

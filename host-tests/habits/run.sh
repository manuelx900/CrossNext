#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/habits-tests-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 test_habits.cpp \
  ../../src/apps_local/habits/HabitsCore.cpp -o "$BUILD_DIR/test_habits"
"$BUILD_DIR/test_habits"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -Istubs test_store.cpp \
  ../../src/apps_local/habits/HabitsCore.cpp ../../src/apps_local/habits/HabitsStore.cpp \
  -o "$BUILD_DIR/test_store"
"$BUILD_DIR/test_store"
./run-ui.sh

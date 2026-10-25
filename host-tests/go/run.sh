#!/bin/sh
# Builds and runs the Go rules tests.
#
# Two languages in one binary. GoCore/GoEngine/GoSave are freestanding C++17;
# michi-c2 is vendored C that does not compile as C++ (it does arithmetic on
# enums and returns string literals as char*), so its files are built as C with
# their own flags and linked in. That is the same boundary the device build
# draws, and building it here is what keeps it honest -- the michi bridge is
# exercised by the suite rather than only by the firmware.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-go-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/go
MICHI=$SRC/michi

# Somebody else's engine, built on its own terms. -Wall -Wextra -Werror here
# would be a demand that upstream write to this fork's standards, and the answer
# to a warning in vendored code is a patch nobody wants to carry across a sync.
for unit in board board_util michi patterns params control MichiShim MichiBridge; do
  # -DMICHI_ASSERTS: upstream's internal consistency checks are OFF on the device
  # (their failure path is exit(), which there is a reboot, and they cost 4.4KB
  # of the search task's stack). Here they are free and readable, and one of
  # them caught an uninitialised board, so the suite keeps them.
  "${CC:-cc}" -std=c99 -O2 -w -DMICHI_ASSERTS -I$MICHI -c "$MICHI/$unit.c" -o "$BUILD_DIR/$unit.o"
done

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC -I../../lib/I18n test_go.cpp \
  $SRC/GoCore.cpp $SRC/GoEngine.cpp $SRC/GoSave.cpp $SRC/GoMichi.cpp ../../lib/I18n/I18n.cpp ../../lib/I18n/I18nStrings.cpp \
  "$BUILD_DIR"/board.o "$BUILD_DIR"/board_util.o "$BUILD_DIR"/michi.o \
  "$BUILD_DIR"/patterns.o "$BUILD_DIR"/params.o "$BUILD_DIR"/control.o \
  "$BUILD_DIR"/MichiShim.o "$BUILD_DIR"/MichiBridge.o \
  -o "$BUILD_DIR/test_go"
"$BUILD_DIR/test_go"

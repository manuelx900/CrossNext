#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/habits-ui-tests-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
SDK="${FREEINK_SDK_ROOT:-../../freeink-sdk}"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment \
  -I"$SDK/libs/ui/FreeInkUI/include" -I"$SDK/libs/assets/Icons/include" -I../../lib/EpdFont -I../../lib/I18n \
  test_screens.cpp ../../src/apps_local/habits/HabitsScreens.cpp ../../src/apps_local/habits/HabitsCore.cpp \
  ../../src/apps_local/ui/PixelMenu.cpp ../../lib/I18n/I18n.cpp ../../lib/I18n/I18nStrings.cpp \
  "$SDK/libs/ui/FreeInkUI/src/FreeInkUI.cpp" \
  -o "$BUILD_DIR/test_screens"
"$BUILD_DIR/test_screens" "$@"

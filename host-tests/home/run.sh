#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/home-ui-tests-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
SDK="${FREEINK_SDK_ROOT:-../../freeink-sdk}"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" ${EXTRA_CXXFLAGS:-} -I../../lib/I18n -std=c++17 -Wall -Wextra -Werror -Wno-comment \
  -I"$SDK/libs/ui/FreeInkUI/include" -I"$SDK/libs/assets/Icons/include" -I../../lib/EpdFont \
  ../../lib/I18n/I18n.cpp ../../lib/I18n/I18nStrings.cpp test_home.cpp ../../src/apps_local/home/HomeScreen.cpp \
  ../../src/apps_local/ui/PixelMenu.cpp \
  "$SDK/libs/ui/FreeInkUI/src/FreeInkUI.cpp" \
  -o "$BUILD_DIR/test_home"
"$BUILD_DIR/test_home" "$@"

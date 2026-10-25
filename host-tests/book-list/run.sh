#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/book-list-tests-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
SDK=../../freeink-sdk/libs/ui/FreeInkUI
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -I../../lib/I18n -std=c++17 -Wall -Wextra -Werror -Wno-comment \
  -I"$SDK/include" -I../../freeink-sdk/libs/assets/Icons/include -I../../lib/EpdFont \
  ../../lib/I18n/I18n.cpp ../../lib/I18n/I18nStrings.cpp test.cpp ../../src/apps_local/ui/PixelMenu.cpp "$SDK/src/FreeInkUI.cpp" \
  -o "$BUILD_DIR/test_book_list"
"$BUILD_DIR/test_book_list" "$@"

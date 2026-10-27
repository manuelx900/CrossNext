#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/crossnext-popup-contrast-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$OUT"
SDK=freeink-sdk/libs/ui/FreeInkUI
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-comment -I"$SDK/include" -Ilib/EpdFont -Ilib/I18n host-tests/popup-contrast/test.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp "$SDK/src/FreeInkUI.cpp" -o "$OUT/test"
"$OUT/test"

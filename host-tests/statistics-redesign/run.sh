#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/statistics-redesign-tests"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -Wno-missing-field-initializers -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/statistics-redesign/test_screens.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/reading/ReadingStatsScreens.cpp src/apps_local/reading/ReadingStatsCore.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

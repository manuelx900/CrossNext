#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/words-tools-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/words-tools-redesign/test_screens.cpp src/apps_local/tools/ToolsScreens.cpp src/apps_local/tools/ToolState.cpp src/apps_local/words/WordsScreens.cpp src/apps_local/wallpapers/WallpapersScreens.cpp src/apps_local/wallpapers/WallpapersCore.cpp src/apps_local/reading/ReadingStatsCore.cpp src/apps_local/ui/PixelMenu.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

#!/bin/sh
set -e
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/connect-sudoku-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/connect-sudoku-redesign/test_screens.cpp src/apps_local/connectfour/ConnectFourScreens.cpp src/apps_local/sudoku/SudokuScreens.cpp src/apps_local/sudoku/SudokuCore.cpp src/apps_local/ui/GameFrontDoor.cpp src/apps_local/ui/PixelMenu.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

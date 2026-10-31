#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/theme-cards-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/theme-cards/test.cpp src/apps_local/game2048/Game2048Screens.cpp src/apps_local/game2048/Game2048Core.cpp src/apps_local/go/GoScreens.cpp src/apps_local/go/GoCore.cpp src/apps_local/minesweeper/MinesweeperScreens.cpp src/apps_local/murdle/MurdleScreens.cpp src/apps_local/murdle/MurdleCore.cpp src/apps_local/murdle/MurdleCast.cpp src/apps_local/murdle/MurdleText.cpp src/apps_local/sudoku/SudokuScreens.cpp src/apps_local/sudoku/SudokuCore.cpp src/apps_local/reading/ReadingStatsScreens.cpp src/apps_local/reading/ReadingGoalScreens.cpp src/apps_local/reading/ReadingStatsCore.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests"

#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/reading-suite-ui-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
SDK=../../freeink-sdk
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" ${EXTRA_CXXFLAGS:-} -std=c++17 -Wall -Wextra -Werror -Wno-comment -I"$SDK/libs/ui/FreeInkUI/include" -I"$SDK/libs/assets/Icons/include" -I../../lib/EpdFont -I../../lib/I18n test_screens.cpp ../../src/apps_local/pomodoro/PomodoroCore.cpp ../../src/apps_local/pomodoro/PomodoroScreens.cpp ../../src/apps_local/chess/puzzles/ChessPuzzleScreens.cpp ../../src/apps_local/chess/puzzles/ChessPuzzleCore.cpp ../../src/apps_local/chess/puzzles/ChessPuzzleData.cpp ../../src/apps_local/chess/ChessCore.cpp ../../src/apps_local/chess/ChessEngine.cpp ../../src/apps_local/pocketgames/PocketGamesCore.cpp ../../src/apps_local/pocketgames/PocketGamesScreens.cpp ../../src/apps_local/pocketgames/OthelloScreens.cpp ../../src/apps_local/reading/ReadingStatsScreens.cpp ../../src/apps_local/reading/ReadingStatsCore.cpp ../../src/apps_local/game2048/Game2048Screens.cpp ../../src/apps_local/game2048/Game2048Core.cpp ../../src/apps_local/ui/PixelMenu.cpp ../../src/apps_local/ui/GameFrontDoor.cpp ../../lib/I18n/I18n.cpp ../../lib/I18n/I18nStrings.cpp "$SDK/libs/ui/FreeInkUI/src/FreeInkUI.cpp" -o "$BUILD_DIR/test_screens"
"$BUILD_DIR/test_screens" "$@"

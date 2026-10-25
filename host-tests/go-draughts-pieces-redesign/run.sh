#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/go-draughts-pieces-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/go-draughts-pieces-redesign/test_screens.cpp src/apps_local/go/GoScreens.cpp src/apps_local/go/GoCore.cpp src/apps_local/checkers/DraughtsScreens.cpp src/apps_local/checkers/DraughtsCore.cpp src/apps_local/chess/ChessScreens.cpp src/apps_local/chess/ChessCore.cpp src/apps_local/chess/ChessEngine.cpp src/apps_local/chess/puzzles/ChessPuzzleScreens.cpp src/apps_local/chess/puzzles/ChessPuzzleCore.cpp src/apps_local/chess/puzzles/ChessPuzzleData.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

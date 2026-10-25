#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/frontdoor-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-unused-function -Wno-sign-compare -Wno-missing-field-initializers -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/frontdoors/test_frontdoors.cpp src/apps_local/player/PlayerAvatar.cpp src/apps_local/player/PlayerName.cpp src/apps_local/link/LinkScreens.cpp src/apps_local/ui/GameFrontDoor.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/sudoku/SudokuScreens.cpp src/apps_local/sudoku/SudokuCore.cpp src/apps_local/hearts/HeartsCore.cpp src/apps_local/hearts/HeartsScreens.cpp src/apps_local/solitaire/SolitaireCore.cpp src/apps_local/solitaire/SolitaireScreens.cpp src/apps_local/cards/CardArt.cpp src/apps_local/connectfour/ConnectFourScreens.cpp src/apps_local/battleship/BattleshipScreens.cpp src/apps_local/battleship/BattleshipCore.cpp src/apps_local/go/GoScreens.cpp src/apps_local/go/GoCore.cpp src/apps_local/chess/ChessScreens.cpp src/apps_local/checkers/DraughtsScreens.cpp src/apps_local/checkers/DraughtsCore.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/screens"
"$out/screens" "$@"

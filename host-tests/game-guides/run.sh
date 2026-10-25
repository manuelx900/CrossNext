#!/bin/sh
set -e
cd "$(dirname "$0")/../.."
BUILD_DIR="${TMPDIR:-/tmp}/game-guides-$(pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -Wno-unused-const-variable \
  -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n \
  host-tests/game-guides/test_screens.cpp \
  src/apps_local/go/GoScreens.cpp src/apps_local/go/GoCore.cpp \
  src/apps_local/solitaire/SolitaireScreens.cpp src/apps_local/solitaire/SolitaireCore.cpp \
  src/apps_local/hearts/HeartsScreens.cpp src/apps_local/hearts/HeartsCore.cpp \
  src/apps_local/wordle/WordleScreens.cpp src/apps_local/wordle/WordleCore.cpp \
  src/apps_local/carnivoro/CarnivoroScreens.cpp src/apps_local/carnivoro/CarnivoroCore.cpp \
  src/apps_local/sudoku/SudokuScreens.cpp src/apps_local/sudoku/SudokuCore.cpp \
  src/apps_local/cards/CardArt.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp \
  lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp \
  -o "$BUILD_DIR/test_guides"
"$BUILD_DIR/test_guides" "$BUILD_DIR"

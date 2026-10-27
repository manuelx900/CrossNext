#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/landscape-layouts-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-comment -Wno-missing-field-initializers -Wno-unused-const-variable \
 -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n \
 host-tests/landscape-layouts/test.cpp src/apps_local/home/HomeScreen.cpp \
 src/apps_local/chess/ChessScreens.cpp src/apps_local/chess/ChessCore.cpp \
 src/apps_local/connectfour/ConnectFourScreens.cpp src/apps_local/pocketgames/PocketGamesScreens.cpp \
 src/apps_local/pocketgames/OthelloScreens.cpp src/apps_local/pocketgames/PocketGamesCore.cpp \
 src/apps_local/cards/CardArt.cpp src/apps_local/pomodoro/PomodoroScreens.cpp src/apps_local/pomodoro/PomodoroCore.cpp \
 src/apps_local/words/WordsScreens.cpp src/apps_local/wallpapers/WallpapersScreens.cpp \
 src/apps_local/reading/ReadingStatsScreens.cpp src/apps_local/reading/ReadingStatsCore.cpp \
 src/apps_local/picross/PicrossScreens.cpp src/apps_local/picross/PicrossCore.cpp \
 src/apps_local/go/GoScreens.cpp src/apps_local/go/GoCore.cpp \
 src/apps_local/battleship/BattleshipScreens.cpp src/apps_local/battleship/BattleshipCore.cpp \
 src/apps_local/carnivoro/CarnivoroScreens.cpp src/apps_local/carnivoro/CarnivoroCore.cpp \
 src/apps_local/habits/HabitsScreens.cpp src/apps_local/habits/HabitsCore.cpp \
 src/apps_local/flashcards/FlashScreens.cpp src/apps_local/flashcards/FlashHistory.cpp \
 src/apps_local/tools/ToolsScreens.cpp src/apps_local/wordle/WordleScreens.cpp src/apps_local/wordle/WordleCore.cpp \
 src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp \
 lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$out"

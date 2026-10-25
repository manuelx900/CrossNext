#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/crossnext-coherence-ui"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers -Wno-misleading-indentation -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/coherence/test_screens.cpp src/apps_local/words/WordsScreens.cpp src/apps_local/study/StudyScreens.cpp src/apps_local/flashcards/FlashScreens.cpp src/apps_local/habits/HabitsScreens.cpp src/apps_local/habits/HabitsCore.cpp src/apps_local/notes/NotesScreens.cpp src/apps_local/home/HomeScreen.cpp src/apps_local/murdle/MurdleScreens.cpp src/apps_local/murdle/MurdleCast.cpp src/apps_local/murdle/MurdleCore.cpp src/apps_local/murdle/MurdleText.cpp src/apps_local/wallpapers/WallpapersScreens.cpp src/apps_local/wallpapers/WallpapersCore.cpp src/apps_local/chess/puzzles/ChessPuzzleScreens.cpp src/apps_local/chess/puzzles/ChessPuzzleCore.cpp src/apps_local/chess/puzzles/ChessPuzzleData.cpp src/apps_local/chess/ChessCore.cpp src/apps_local/chess/ChessEngine.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/test_screens"
"$out/test_screens" "$@"

#!/bin/sh
set -e
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/othello-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/othello-redesign/test_screens.cpp src/apps_local/pocketgames/PocketGamesScreens.cpp src/apps_local/pocketgames/OthelloScreens.cpp src/apps_local/pocketgames/PocketGamesCore.cpp src/apps_local/ui/GameFrontDoor.cpp src/apps_local/ui/PixelMenu.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

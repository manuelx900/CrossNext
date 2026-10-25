#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/flashcards-redesign-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/flashcards-redesign/test.cpp src/apps_local/flashcards/FlashScreens.cpp src/apps_local/flashcards/FlashSettingsScreens.cpp src/apps_local/flashcards/FlashHistory.cpp src/apps_local/study/StudyScreens.cpp src/apps_local/study/StudyDeck.cpp src/apps_local/study/StudyStats.cpp src/apps_local/study/StudyFsrs.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

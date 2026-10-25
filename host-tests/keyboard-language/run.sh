#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/crossplay-keyboard-language"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wno-comment -Ihost-tests/keyboard-language/stubs -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/I18n host-tests/keyboard-language/test.cpp src/activities/util/KeyboardLayoutSet.cpp src/apps_local/wordle/WordleScreens.cpp src/apps_local/wordle/WordleCore.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests"

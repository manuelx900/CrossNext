#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/murdle-redesign-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" ${EXTRA_CXXFLAGS:-} -std=c++17 -Wall -Wextra -Werror -Wno-comment -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/murdle-redesign/test.cpp src/apps_local/murdle/MurdleScreens.cpp src/apps_local/murdle/MurdleCore.cpp src/apps_local/murdle/MurdleCast.cpp src/apps_local/murdle/MurdleText.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

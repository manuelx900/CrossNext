#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/crossnext-header-clock-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$OUT"
SDK=freeink-sdk/libs/ui/FreeInkUI
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-comment -I"$SDK/include" -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/header-clock/test.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/cards/CardArt.cpp src/apps_local/solitaire/SolitaireCore.cpp src/apps_local/solitaire/SolitaireScreens.cpp src/apps_local/hearts/HeartsCore.cpp src/apps_local/hearts/HeartsScreens.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp "$SDK/src/FreeInkUI.cpp" -o "$OUT/test"
"$OUT/test"

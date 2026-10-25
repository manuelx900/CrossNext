#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/mystery-naval-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-comment \
  -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n -Ihost-tests/sdart/stubs \
  host-tests/mystery-naval-redesign/test_screens.cpp \
  src/apps_local/intercept/InterceptScreens.cpp src/apps_local/intercept/InterceptCore.cpp \
  src/apps_local/intercept/InterceptData.cpp src/apps_local/reading/ReadingStatsCore.cpp \
  src/apps_local/carnivoro/CarnivoroScreens.cpp src/apps_local/carnivoro/CarnivoroCore.cpp \
  src/apps_local/battleship/BattleshipScreens.cpp src/apps_local/battleship/BattleshipCore.cpp \
  src/apps_local/ui/SdBitmapPack.cpp src/apps_local/ui/PixelMenu.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp \
  freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/screens"
"$out/screens" "$@"

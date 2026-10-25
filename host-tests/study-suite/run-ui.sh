#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/study-suite-ui-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
SDK=../../freeink-sdk
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" ${EXTRA_CXXFLAGS:-} -std=c++17 -Wall -Wextra -Werror -Wno-comment -I"$SDK/libs/ui/FreeInkUI/include" -I"$SDK/libs/assets/Icons/include" -I../../lib/EpdFont -I../../lib/I18n -I../sdart/stubs -I../../src/apps_local/quest test_screens.cpp ../../src/apps_local/tools/ToolsScreens.cpp ../../src/apps_local/quest/QuestScreens.cpp ../../src/apps_local/quest/QuestCore.cpp ../../src/apps_local/reading/ReadingStatsCore.cpp ../../src/apps_local/intercept/InterceptScreens.cpp ../../src/apps_local/intercept/InterceptCore.cpp ../../src/apps_local/intercept/InterceptData.cpp ../../src/apps_local/ui/PixelMenu.cpp ../../src/apps_local/ui/SdBitmapPack.cpp ../../lib/I18n/I18n.cpp ../../lib/I18n/I18nStrings.cpp "$SDK/libs/ui/FreeInkUI/src/FreeInkUI.cpp" -o "$BUILD_DIR/test_screens"
"$BUILD_DIR/test_screens" "$@"

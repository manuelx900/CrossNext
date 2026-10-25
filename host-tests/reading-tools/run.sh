#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/reading-tools-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n host-tests/reading-tools/test.cpp src/apps_local/quotes/QuotesScreens.cpp src/apps_local/notes/NotesScreens.cpp src/apps_local/home/HomeScreen.cpp src/apps_local/ui/PixelMenu.cpp src/apps_local/ui/GameFrontDoor.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Istubs -Ihost-tests/reading-tools/stubs host-tests/reading-tools/store.cpp src/apps_local/quotes/QuotesStore.cpp -o "$out/store-tests"
"$out/store-tests"

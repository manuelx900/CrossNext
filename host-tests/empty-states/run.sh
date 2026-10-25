#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/empty-states-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment \
  -I freeink-sdk/libs/ui/FreeInkUI/include -I freeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n \
  host-tests/empty-states/test.cpp src/apps_local/notes/NotesScreens.cpp src/apps_local/quotes/QuotesScreens.cpp \
  src/apps_local/words/WordsScreens.cpp src/apps_local/ui/PixelMenu.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp \
  freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/tests"
"$out/tests" "$@"

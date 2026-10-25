#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/quick-cover-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
 -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include -Ilib/EpdFont -Ilib/I18n \
 host-tests/quick-cover/test.cpp src/apps_local/ui/PixelMenu.cpp lib/I18n/I18n.cpp lib/I18n/I18nStrings.cpp \
 freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$out/test"
"$out/test" "$@"

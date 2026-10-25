#!/bin/sh
set -eu
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/crossplay-bilingual-tests"
mkdir -p "$OUT"
"${CXX:-c++}" -std=c++17 -UNDEBUG -Wall -Wextra -Werror -I../../lib/I18n \
  test_i18n.cpp ../../lib/I18n/I18n.cpp ../../lib/I18n/I18nStrings.cpp -o "$OUT/test"
"$OUT/test"

#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
output="${TMPDIR:-/tmp}/crossplay-button-navigation-$(pwd | cksum | cut -d' ' -f1)"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Ifreeink-sdk/libs/ui/FreeInkUI/include \
  freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp host-tests/button-navigation/test.cpp -o "$output"
"$output"

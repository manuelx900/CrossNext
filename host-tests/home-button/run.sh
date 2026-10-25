#!/usr/bin/env bash
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -include initializer_list -I"$ROOT/src" \
  -I"$ROOT/freeink-sdk/libs/ui/FreeInkUI/include" \
  "$ROOT/host-tests/home-button/test.cpp" -o "$BUILD/test-home-button"
"$BUILD/test-home-button"

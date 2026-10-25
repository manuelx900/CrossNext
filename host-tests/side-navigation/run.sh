#!/usr/bin/env bash
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -I"$ROOT/freeink-sdk/libs/ui/FreeInkUI/include" "$ROOT/host-tests/side-navigation/test.cpp" -o "$BUILD/test"
"$BUILD/test"

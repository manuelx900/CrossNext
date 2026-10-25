#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/crossnext-cover-sampling"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
 -Ifreeink-sdk/libs/ui/FreeInkUI/include -Ifreeink-sdk/libs/assets/Icons/include \
 host-tests/cover-sampling/test.cpp -o "$out/test"
"$out/test"

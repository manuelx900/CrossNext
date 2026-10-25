#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/crossnext-interface-controls"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Isrc host-tests/interface-controls/test.cpp -o "$out"
"$out"

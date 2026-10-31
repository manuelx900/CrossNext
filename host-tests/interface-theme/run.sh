#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/interface-theme-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Isrc host-tests/interface-theme/test.cpp -o "$out/tests"
"$out/tests"

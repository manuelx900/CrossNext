#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/../.."
out="$(mktemp -d "${TMPDIR:-/tmp}/power-wake.XXXXXXXX")"
trap 'rm -rf "$out"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Isrc host-tests/power-wake/test.cpp -o "$out/test"
"$out/test"

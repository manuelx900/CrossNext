#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/timezone-followup-tests"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Ihost-tests/timezones-followup/stubs host-tests/timezones-followup/test_timezones.cpp src/util/Timezones.cpp -o "$out/tests"
"$out/tests"

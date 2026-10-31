#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/cover-scene-tests"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Ihost-tests/cover-scene/stubs -Ihost-tests/epub-event-cache/stubs -Ifreeink-sdk/libs/ui/FreeInkUI/include host-tests/cover-scene/test.cpp src/apps_local/home/CoverSceneCache.cpp -o "$out/tests"
"$out/tests"
"$out/tests" oom

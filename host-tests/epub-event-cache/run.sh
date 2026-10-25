#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-unused-parameter \
  -Ihost-tests/epub-event-cache/stubs -Ilib/Epub/Epub/parsers \
  host-tests/epub-event-cache/test.cpp lib/Epub/Epub/parsers/ChapterEventCache.cpp -o "$out/test"
"$out/test"

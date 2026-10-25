#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror \
  -Ihost-tests/page-write-batching/stubs -Ilib/Epub/Epub \
  host-tests/page-write-batching/test.cpp lib/Epub/Epub/PageCacheWriter.cpp -o "$out/test"
"$out/test"

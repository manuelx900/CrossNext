#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/dictionary-readable-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror -Wno-sign-compare -Wno-unused-variable \
  -Ihost-tests/dictionary-readable/stubs -Ilib/Memory \
  host-tests/dictionary-readable/test.cpp src/util/Dictionary.cpp src/apps_local/words/DictionaryHtml.cpp \
  -o "$out/tests"
"$out/tests"

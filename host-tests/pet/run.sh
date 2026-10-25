#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/pet-tests-$(pwd | cksum | cut -d' ' -f1)"
# run.py compiles test_state.cpp, test_storage.cpp and test_screens.cpp and runs test_resources.py.
exec "${PYTHON:-python3}" host-tests/pet/run.py --output "$out" "$@"

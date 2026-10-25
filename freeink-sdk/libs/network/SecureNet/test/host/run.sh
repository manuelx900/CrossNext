#!/bin/sh
# Builds and runs the fetchResumable host test against a scripted fake
# WiFiClient (stubs/); https is never exercised.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/freeink-securenet-host-tests"
mkdir -p "$BUILD_DIR"
c++ -std=c++17 -Wall -Wextra -Werror -Istubs -I../../include test_resumable_fetch.cpp -o "$BUILD_DIR/test_resumable_fetch"
"$BUILD_DIR/test_resumable_fetch"

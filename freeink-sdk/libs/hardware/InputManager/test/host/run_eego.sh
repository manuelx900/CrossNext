#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/freeink-eego.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter \
  -DFREEINK_DEVICE_EEGO_A4=1 -DARDUINO_USB_CDC_ON_BOOT=1 \
  -Ieego_stubs -Imetalio_stubs -I../../include -I../../../BoardConfig/include \
  test_eego.cpp ../../src/InputManager.cpp -o "$BUILD_DIR/test_eego"
"$BUILD_DIR/test_eego"

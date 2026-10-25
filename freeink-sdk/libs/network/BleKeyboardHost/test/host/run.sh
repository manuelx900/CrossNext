#!/bin/sh
# Builds and runs the BLE HID host tests: src/BleKeyboardHost.cpp compiled with
# FREEINK_CAP_BLE_HID_HOST=1 against the fake NimBLE/FreeRTOS stack in stubs/.
# -Wno-unused-variable: with scan debug logging off, startScan() keeps a result
# it only prints in debug builds.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/freeink-ble-hid-host-tests"
mkdir -p "$BUILD_DIR"
c++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-variable -pthread -Istubs -I../../include -I../../src \
  test_ble_keyboard_host.cpp fake_ble.cpp ../../src/BleKeyboardHost.cpp ../../src/HidKeymap.cpp \
  -o "$BUILD_DIR/test_ble_keyboard_host"
"$BUILD_DIR/test_ble_keyboard_host"

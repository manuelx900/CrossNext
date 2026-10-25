#!/bin/sh
# Builds and runs the BQ27220 Design Capacity load against a model gauge.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/freeink-battery-tests"
mkdir -p "$BUILD_DIR"
c++ -std=c++17 -Wall -Wextra -Werror \
  -DFREEINK_DEVICE_X3=1 -DFREEINK_DEVICE_X4=1 -DFREEINK_BATTERY_I2C_GAUGE=1 \
  -Istubs -I../../include -I../../../BoardConfig/include -I../../../XteinkDetect/test/host/stubs \
  test_bq27220_capacity.cpp -o "$BUILD_DIR/test_bq27220_capacity"
"$BUILD_DIR/test_bq27220_capacity"

#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/crossplay-idle.XXXXXXXX")"
trap 'rm -rf "$OUT"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror host-tests/idle-wait/test.cpp -o "$OUT/deadlines"
"$OUT/deadlines"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -fno-access-control -DFREEINK_DEVICE_X4PRO=1 \
  -Ihost-tests/idle-wait/stubs -Ilib/hal host-tests/idle-wait/test_wake.cpp lib/hal/HalGPIOWake.cpp -o "$OUT/wake"
"$OUT/wake"
SDK=freeink-sdk/libs/hardware/InputManager
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-unused-parameter \
  -DFREEINK_DEVICE_METALIO_EINK4=1 -DARDUINO_USB_CDC_ON_BOOT=1 \
  -I"$SDK/test/host/metalio_stubs" -I"$SDK/include" -Ifreeink-sdk/libs/hardware/BoardConfig/include \
  host-tests/idle-wait/test_input.cpp "$SDK/src/InputManager.cpp" -o "$OUT/input"
"$OUT/input"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-unused-parameter \
  -DFREEINK_DEVICE_METALIO_EINK4=1 -DARDUINO_USB_CDC_ON_BOOT=1 \
  -Ihost-tests/idle-wait/gt911_stubs -I"$SDK/test/host/metalio_stubs" -I"$SDK/include" \
  -Ifreeink-sdk/libs/hardware/BoardConfig/include -Isrc \
  host-tests/idle-wait/test_home_gt911.cpp "$SDK/src/InputManager.cpp" -o "$OUT/home-gt911"
"$OUT/home-gt911"

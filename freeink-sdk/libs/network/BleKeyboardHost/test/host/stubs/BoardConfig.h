#pragma once

// Host-test stand-in for <BoardConfig.h>. Only the BLE HID host capability and
// its policy macros are needed; the capability is ON so the real NimBLE path in
// src/BleKeyboardHost.cpp is compiled against the fakes in this directory.

#define FREEINK_CAP_BLE_HID_HOST 1
#define FREEINK_BLE_HID_SHOW_UNNAMED_DEVICES 0
#define FREEINK_BLE_HID_REQUIRE_MITM 0
#define CONFIG_BT_NIMBLE_EXT_ADV 0

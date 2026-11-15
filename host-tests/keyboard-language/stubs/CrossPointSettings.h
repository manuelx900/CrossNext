#pragma once
#include <cstdint>
struct KeyboardSettings {
  uint16_t keyboardLayouts = 0;
  uint64_t keyboardLanguageLayouts = 0;
};
inline KeyboardSettings keyboardSettings;
#define SETTINGS keyboardSettings

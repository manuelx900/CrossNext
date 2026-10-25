#pragma once
#include <cstdint>
namespace quickui {
enum class Shortcut : uint8_t { Dark, Refresh, Rotate, Light, Wifi, Lock, Sleep, Off, Restart, Warm, Count };
inline constexpr int kSlots = 4;
}  // namespace quickui

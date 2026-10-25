#pragma once

#include <cstdint>

namespace powermenu {

// Persisted indices match the wake-mode choices in SettingsList.
enum class WakeMode : uint8_t { ShortPress, LongPress };

constexpr bool acceptsWake(const WakeMode mode, const bool holdVerified) {
  switch (mode) {
    case WakeMode::ShortPress:
      return true;
    case WakeMode::LongPress:
      return holdVerified;
  }
  return true;
}

}  // namespace powermenu

#pragma once

#include <cstdint>

namespace quotes {
enum class FontFamily : uint8_t { NotoSerif, NotoSans, InstrumentSerif };
inline constexpr int kBuiltinFamilies = int(FontFamily::InstrumentSerif) + 1;
inline constexpr uint8_t kBuiltinPoints[kBuiltinFamilies][3] = {{11, 14, 0}, {10, 14, 0}, {10, 13, 24}};
}  // namespace quotes

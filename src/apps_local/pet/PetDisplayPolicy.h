#pragma once

#include <cstdint>

namespace petdisplay {
// Keep one fast B/W panel mode throughout PET; gray source ink uses the renderer's dither.
#ifdef SIMULATOR
inline constexpr bool AMBIENT_MOTION = true;
inline constexpr bool NATIVE_GRAYSCALE = true;
#else
inline constexpr bool AMBIENT_MOTION = false;
inline constexpr bool NATIVE_GRAYSCALE = false;
#endif
inline constexpr uint32_t hold(uint32_t preview, uint32_t paper) { return NATIVE_GRAYSCALE ? preview : paper; }
}  // namespace petdisplay

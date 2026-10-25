#pragma once

#include "../ui/SdBitmapPack.h"

namespace questassets {
inline constexpr char PATH[] = "/apps/reading-adventure/graphics.bin";
inline constexpr uint32_t COUNT = 900;
// Stable sprite IDs are independent of equipment/save IDs.
inline constexpr uint32_t FIGURES = 0, FIGURE_MASKS = 200, PETS = 400, PET_MASKS = 450, PET_ICONS = 500,
                          PET_ICON_MASKS = 550, AURAS = 600, AURA_MASKS = 650, OUTFITS = 700, OUTFIT_MASKS = 800;
}  // namespace questassets

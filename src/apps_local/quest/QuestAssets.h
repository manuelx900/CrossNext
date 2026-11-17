#pragma once

#include "../ui/SdBitmapPack.h"

namespace questassets {
inline constexpr const char* CELEBRATIONS_PATH = "/apps/reading-adventure-v2/celebrations.bin";
inline constexpr unsigned CELEBRATIONS_COUNT = 6;
inline constexpr char PATH[] = "/apps/reading-adventure-v2/graphics.bin";
inline constexpr char STORY_PATH[] = "/apps/reading-adventure-v2/story.bin";
inline constexpr uint32_t STORY_SCENES = 120, STORY_COUNT = STORY_SCENES * 2;
inline constexpr char REWARDS_PATH[] = "/apps/reading-adventure-v2/rewards.bin";
inline constexpr char GUARDIANS_PATH[] = "/apps/reading-adventure-v2/guardians.bin";
inline constexpr char BESTIARY_OUTFITS_PATH[] = "/apps/reading-adventure-v2/bestiary-outfits.bin";
inline constexpr uint32_t BESTIARY_OUTFITS_COUNT = 224, BESTIARY_OUTFIT_FIGURES = 0, BESTIARY_OUTFIT_MASKS = 112;
inline constexpr uint32_t REWARDS_COUNT = 593, GUARDIANS_COUNT = 360;
inline constexpr uint32_t EXTRA_DECOR = 0, EXTRA_DECOR_MASKS = 101, KING_FIGURES = 202, KING_FIGURE_MASKS = 204;
inline constexpr uint32_t KING_PET = 206, KING_PET_MASK = 207, KING_BACKGROUND = 208, VICTORY_SCENES = 210,
                          KING_BACKGROUND_ICON = 214;
inline constexpr uint32_t HUNT_BEASTS = 215, HUNT_BEAST_MASKS = 227, HUNT_FIGURES = 239, HUNT_FIGURE_MASKS = 247,
                          INK_RELIC = 255, INK_RELIC_MASK = 256;
inline constexpr uint32_t BESTIARY_BEASTS = 257, BESTIARY_MASKS = 425;
inline constexpr uint32_t COUNT = 1103;
inline constexpr uint32_t DECORATIONS = 1071, DECORATION_MASKS = 1087;
inline constexpr uint32_t SWORD_INK = 1065, SWORD_MASKS = 1068;
inline constexpr uint32_t SAGA_SCENES = 1041;
inline constexpr uint32_t KING_PROLOGUE = 1037;
inline constexpr int COUNTRY_PATH_BACKGROUND = 2;
inline constexpr uint32_t ENEMIES = 951, ENEMY_MASKS = 990, ENEMY_STATES = 3, ENEMY_FAMILIES = 13;
inline constexpr uint32_t BACKGROUND_ICONS = 901;
// Stable sprite IDs are independent of equipment/save IDs.
inline constexpr uint32_t FIGURES = 0, FIGURE_MASKS = 200, PETS = 400, PET_MASKS = 450, PET_ICONS = 500,
                          PET_ICON_MASKS = 550, BACKGROUND_LEFT = 600, BACKGROUND_RIGHT = 650, OUTFITS = 700,
                          OUTFIT_MASKS = 800;
}  // namespace questassets

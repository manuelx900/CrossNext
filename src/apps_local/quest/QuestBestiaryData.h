#pragma once
#include "QuestCore.h"
namespace quest {
struct BestiarySpec {
  uint16_t minutes;
  uint8_t paths, chapter;
  HuntRank rank;
};
inline constexpr BestiarySpec BESTIARY[] = {
    {90, 1, 0, HuntRank::Elite},          {105, 2, 0, HuntRank::Elite},         {120, 3, 0, HuntRank::Elite},
    {135, 4, 0, HuntRank::Elite},         {150, 5, 0, HuntRank::Elite},         {165, 6, 0, HuntRank::Elite},
    {240, 7, 0, HuntRank::Legendary},     {300, 8, 0, HuntRank::Legendary},     {105, 21, 20, HuntRank::Elite},
    {120, 22, 20, HuntRank::Elite},       {135, 23, 20, HuntRank::Elite},       {150, 24, 20, HuntRank::Elite},
    {165, 25, 20, HuntRank::Elite},       {180, 26, 20, HuntRank::Elite},       {270, 27, 20, HuntRank::Legendary},
    {330, 28, 20, HuntRank::Legendary},   {120, 41, 40, HuntRank::Elite},       {135, 42, 40, HuntRank::Elite},
    {150, 43, 40, HuntRank::Elite},       {165, 44, 40, HuntRank::Elite},       {180, 45, 40, HuntRank::Elite},
    {195, 46, 40, HuntRank::Elite},       {300, 47, 40, HuntRank::Legendary},   {360, 48, 40, HuntRank::Legendary},
    {135, 51, 50, HuntRank::Elite},       {150, 52, 50, HuntRank::Elite},       {165, 53, 50, HuntRank::Elite},
    {180, 54, 50, HuntRank::Elite},       {195, 55, 50, HuntRank::Elite},       {210, 56, 50, HuntRank::Elite},
    {330, 57, 50, HuntRank::Legendary},   {390, 58, 50, HuntRank::Legendary},   {150, 61, 60, HuntRank::Elite},
    {165, 62, 60, HuntRank::Elite},       {180, 63, 60, HuntRank::Elite},       {195, 64, 60, HuntRank::Elite},
    {210, 65, 60, HuntRank::Elite},       {225, 66, 60, HuntRank::Elite},       {360, 67, 60, HuntRank::Legendary},
    {420, 68, 60, HuntRank::Legendary},   {165, 71, 70, HuntRank::Elite},       {180, 72, 70, HuntRank::Elite},
    {195, 73, 70, HuntRank::Elite},       {210, 74, 70, HuntRank::Elite},       {225, 75, 70, HuntRank::Elite},
    {240, 76, 70, HuntRank::Elite},       {390, 77, 70, HuntRank::Legendary},   {450, 78, 70, HuntRank::Legendary},
    {180, 101, 100, HuntRank::Elite},     {195, 102, 100, HuntRank::Elite},     {210, 103, 100, HuntRank::Elite},
    {225, 104, 100, HuntRank::Elite},     {240, 105, 100, HuntRank::Elite},     {255, 106, 100, HuntRank::Elite},
    {420, 107, 100, HuntRank::Legendary}, {480, 108, 100, HuntRank::Legendary},
};
static_assert(sizeof(BESTIARY) / sizeof(BESTIARY[0]) == kBestiaryCount);
}  // namespace quest

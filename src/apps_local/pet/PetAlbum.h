#pragma once

#include "PetScreens.h"

namespace petui {
enum class DiscoveryMetric : uint8_t { ReadingHours, Games, Days, Streak };
struct Discovery {
  DiscoveryMetric metric;
  uint16_t threshold;
  StrId title;
};
inline constexpr Discovery DISCOVERIES[] = {
    {DiscoveryMetric::ReadingHours, 2, StrId::STR_PET_DISCOVERY_READ_2},
    {DiscoveryMetric::ReadingHours, 3, StrId::STR_PET_DISCOVERY_READ_3},
    {DiscoveryMetric::ReadingHours, 4, StrId::STR_PET_DISCOVERY_READ_4},
    {DiscoveryMetric::ReadingHours, 6, StrId::STR_PET_DISCOVERY_READ_6},
    {DiscoveryMetric::ReadingHours, 8, StrId::STR_PET_DISCOVERY_READ_8},
    {DiscoveryMetric::ReadingHours, 12, StrId::STR_PET_DISCOVERY_READ_12},
    {DiscoveryMetric::ReadingHours, 15, StrId::STR_PET_DISCOVERY_READ_15},
    {DiscoveryMetric::ReadingHours, 20, StrId::STR_PET_DISCOVERY_READ_20},
    {DiscoveryMetric::ReadingHours, 30, StrId::STR_PET_DISCOVERY_READ_30},
    {DiscoveryMetric::ReadingHours, 40, StrId::STR_PET_DISCOVERY_READ_40},
    {DiscoveryMetric::ReadingHours, 60, StrId::STR_PET_DISCOVERY_READ_60},
    {DiscoveryMetric::ReadingHours, 75, StrId::STR_PET_DISCOVERY_READ_75},
    {DiscoveryMetric::ReadingHours, 90, StrId::STR_PET_DISCOVERY_READ_90},
    {DiscoveryMetric::Games, 2, StrId::STR_PET_DISCOVERY_PLAY_2},
    {DiscoveryMetric::Games, 10, StrId::STR_PET_DISCOVERY_PLAY_10},
    {DiscoveryMetric::Games, 20, StrId::STR_PET_DISCOVERY_PLAY_20},
    {DiscoveryMetric::Games, 50, StrId::STR_PET_DISCOVERY_PLAY_50},
    {DiscoveryMetric::Days, 2, StrId::STR_PET_DISCOVERY_DAYS_2},
    {DiscoveryMetric::Days, 5, StrId::STR_PET_DISCOVERY_DAYS_5},
    {DiscoveryMetric::Days, 7, StrId::STR_PET_DISCOVERY_DAYS_7},
    {DiscoveryMetric::Days, 14, StrId::STR_PET_DISCOVERY_DAYS_14},
    {DiscoveryMetric::Days, 30, StrId::STR_PET_DISCOVERY_DAYS_30},
    {DiscoveryMetric::Days, 60, StrId::STR_PET_DISCOVERY_DAYS_60},
    {DiscoveryMetric::Streak, 3, StrId::STR_PET_DISCOVERY_STREAK_3},
    {DiscoveryMetric::Streak, 7, StrId::STR_PET_DISCOVERY_STREAK_7},
    {DiscoveryMetric::Streak, 14, StrId::STR_PET_DISCOVERY_STREAK_14},
    {DiscoveryMetric::Streak, 30, StrId::STR_PET_DISCOVERY_STREAK_30},
};
inline constexpr unsigned DISCOVERY_COUNT = sizeof(DISCOVERIES) / sizeof(DISCOVERIES[0]);
inline constexpr unsigned MEMORY_COUNT = pet::MOMENT_COUNT + DISCOVERY_COUNT;
static_assert(MEMORY_COUNT == 50);
inline bool memoryCaptured(const pet::Profile& p, unsigned id) {
  return id < pet::MOMENT_COUNT && (p.momentMask & (1u << id));
}
inline uint64_t discoveryValue(const pet::Profile& p, DiscoveryMetric metric) {
  switch (metric) {
    case DiscoveryMetric::ReadingHours:
      return p.readingSeconds / 3600;
    case DiscoveryMetric::Games:
      return p.gamesWon;
    case DiscoveryMetric::Days:
      return p.sharedDays;
    case DiscoveryMetric::Streak:
      return p.streak;
  }
  return 0;
}
inline bool memoryUnrecorded(const pet::Profile& p, unsigned id) {
  if (id < unsigned(pet::Moment::Reading1h) || id > unsigned(pet::Moment::Reading100h) || memoryCaptured(p, id))
    return false;
  static constexpr unsigned hours[] = {1, 5, 10, 25, 50, 100};
  return p.readingSeconds >= uint64_t(hours[id - unsigned(pet::Moment::Reading1h)]) * 3600;
}
inline pet::Stage memoryStage(const pet::Profile& p, unsigned id) {
  if (memoryCaptured(p, id)) return pet::momentStage(p.moments[id]);
  if (id >= pet::MOMENT_COUNT && id < MEMORY_COUNT &&
      DISCOVERIES[id - pet::MOMENT_COUNT].metric == DiscoveryMetric::ReadingHours)
    return pet::stage(uint64_t(DISCOVERIES[id - pet::MOMENT_COUNT].threshold) * 3600);
  if (id == 2) return pet::Stage::Egg;
  if (id == 0) return pet::Stage::Newborn;
  if (id >= 6 && id <= 8) return pet::Stage(id - 4);
  return pet::stage(p.readingSeconds);
}
inline bool memoryOwned(const pet::Profile& p, unsigned id) {
  if (!p.created || id >= MEMORY_COUNT) return false;
  if (id >= pet::MOMENT_COUNT) {
    const auto& discovery = DISCOVERIES[id - pet::MOMENT_COUNT];
    return discoveryValue(p, discovery.metric) >= discovery.threshold;
  }
  if (memoryCaptured(p, id)) return true;
  if (id >= pet::LEGACY_MOMENT_COUNT) return false;
  if (id == 2) return true;
  if (id == 0 || (id >= 6 && id <= 8)) return unsigned(pet::stage(p.readingSeconds)) >= unsigned(memoryStage(p, id));
  if (id == 10) return p.gameDay > 0;
  static constexpr uint16_t flags[] = {1, 2, 0, 4, 8, 16, 0, 0, 0, 0, 0};
  return (p.memories & flags[id]) != 0;
}
inline unsigned memoryOwnedCount(const pet::Profile& p) {
  unsigned count = 0;
  for (unsigned id = 0; id < MEMORY_COUNT; ++id) count += memoryOwned(p, id);
  return count;
}
inline const char* memoryTitle(unsigned id) {
  if (id >= pet::MOMENT_COUNT && id < MEMORY_COUNT) return I18N.get(DISCOVERIES[id - pet::MOMENT_COUNT].title);
  static constexpr StrId keys[] = {
      StrId::STR_PET_FIRST_HATCH,     StrId::STR_PET_MEMORY_HOME,      StrId::STR_PET_FIRST_DAY,
      StrId::STR_PET_FIRST_FOOD,      StrId::STR_PET_FIRST_STROKE,     StrId::STR_PET_FIRST_HEALTH,
      StrId::STR_PET_MEMORY_BABY,     StrId::STR_PET_MEMORY_JUVENILE,  StrId::STR_PET_MEMORY_ADULT,
      StrId::STR_PET_FIRST_REST,      StrId::STR_PET_FIRST_GAME,       StrId::STR_PET_MEMORY_READ_1,
      StrId::STR_PET_MEMORY_READ_5,   StrId::STR_PET_MEMORY_READ_10,   StrId::STR_PET_MEMORY_READ_25,
      StrId::STR_PET_MEMORY_READ_50,  StrId::STR_PET_MEMORY_READ_100,  StrId::STR_PET_MEMORY_GAMES_5,
      StrId::STR_PET_MEMORY_GAMES_25, StrId::STR_PET_MEMORY_GAMES_100, StrId::STR_PET_MEMORY_TREATMENT,
      StrId::STR_PET_MEMORY_CURES_5,  StrId::STR_PET_MEMORY_CURES_20};
  return I18N.get(keys[id < pet::MOMENT_COUNT ? id : 2]);
}
inline const char* memoryText(unsigned id) {
  if (id >= pet::MOMENT_COUNT && id < MEMORY_COUNT) {
    switch (DISCOVERIES[id - pet::MOMENT_COUNT].metric) {
      case DiscoveryMetric::ReadingHours:
        return tr(STR_PET_DISCOVERY_READING_TEXT);
      case DiscoveryMetric::Games:
        return tr(STR_PET_DISCOVERY_GAMES_TEXT);
      case DiscoveryMetric::Days:
        return tr(STR_PET_DISCOVERY_DAYS_TEXT);
      case DiscoveryMetric::Streak:
        return tr(STR_PET_DISCOVERY_STREAK_TEXT);
    }
  }
  if (id >= unsigned(pet::Moment::Treatment) && id < pet::MOMENT_COUNT) return tr(STR_PET_MEMORY_CURES_TEXT);
  if (id >= unsigned(pet::Moment::Games5) && id < pet::MOMENT_COUNT) return tr(STR_PET_MEMORY_GAMES_TEXT);
  if (id >= pet::LEGACY_MOMENT_COUNT && id < pet::MOMENT_COUNT) return tr(STR_PET_MEMORY_READING_TEXT);
  static constexpr StrId keys[] = {
      StrId::STR_PET_FIRST_HATCH_TEXT, StrId::STR_PET_MEMORY_HOME_TEXT,     StrId::STR_PET_FIRST_DAY_TEXT,
      StrId::STR_PET_FIRST_FOOD_TEXT,  StrId::STR_PET_FIRST_STROKE_TEXT,    StrId::STR_PET_FIRST_HEALTH_TEXT,
      StrId::STR_PET_MEMORY_BABY_TEXT, StrId::STR_PET_MEMORY_JUVENILE_TEXT, StrId::STR_PET_MEMORY_ADULT_TEXT,
      StrId::STR_PET_FIRST_REST_TEXT,  StrId::STR_PET_FIRST_GAME_TEXT};
  return I18N.get(keys[id < pet::MOMENT_COUNT ? id : 2]);
}
}  // namespace petui

#pragma once

#include <array>

#include "QuestBestiaryData.h"
#include "QuestCampaign.h"
#include "QuestExpansionPolicy.h"

namespace quest {
inline constexpr int MAX_AVAILABLE_HUNTS = 4;
inline constexpr uint64_t REQUEST_METRIC_MAX = (uint64_t(1) << 54) - 1;
inline constexpr bool validRequest(int id) { return id >= 0 && id < kRequestCount; }
inline constexpr bool specialHunt(int id) { return id >= kCampaignHuntCount && validRequest(id); }
inline constexpr bool bestiaryHunt(int id) { return id >= kBestiaryHuntStart && validRequest(id); }
inline constexpr HuntRank huntRank(int id) {
  if (bestiaryHunt(id)) {
    const auto rank = BESTIARY[id - kBestiaryHuntStart].rank;
    return rank == HuntRank::Basic ? HuntRank::Elite : rank;
  }
  if (specialHunt(id)) {
    constexpr HuntRank ranks[] = {HuntRank::Elite, HuntRank::Legendary, HuntRank::Legendary, HuntRank::Secret};
    return ranks[id - kCampaignHuntCount];
  }
  if (id == kShadowHunt || id == kKingHunt) return HuntRank::Secret;
  return id >= 0 && id < kLegacyRequestCount && id / kRequestsPerRegion == int(CAMPAIGN_BOOKS) - 1 ? HuntRank::Elite
                                                                                                   : HuntRank::Basic;
}
inline constexpr bool huntOutfit(int item) {
  return (item >= kHuntOutfitStart && item < kInkRelic) ||
         (item >= kBestiaryOutfitStart && item < kBestiaryOutfitStart + kBestiaryCount);
}
inline constexpr int huntOutfitIndex(int item) { return item - kHuntOutfitStart - (item >= kBestiaryOutfitStart); }
inline constexpr int huntOutfitAt(int index) {
  return kHuntOutfitStart + index + (index >= kInkRelic - kHuntOutfitStart);
}
inline constexpr unsigned requestChapter(int id) {
  if (id >= kShadowHunt) return CAMPAIGN_CHAPTERS - 2 + (id == kKingHunt);
  return id / kRequestsPerRegion * CHAPTERS_PER_BOOK + id % kRequestsPerRegion * CHAPTERS_PER_BOOK / kRequestsPerRegion;
}
inline constexpr unsigned huntEncounter(int id) {
  if (bestiaryHunt(id)) return battleAnchor(BESTIARY[id - kBestiaryHuntStart].chapter);
  if (specialHunt(id)) {
    constexpr unsigned settings[] = {64, 43, 97, 118};
    return battleAnchor(settings[id - kCampaignHuntCount]);
  }
  const auto chapter = requestChapter(id);
  // The final variants in older profiles fought the chapter 118 guardian.
  return battleAnchor(id < kLegacyRequestCount && chapter >= 118 ? 117 : chapter);
}
inline constexpr int huntForPath(unsigned chapter) {
  if (chapter >= 118) return chapter == 118 ? kShadowHunt : kKingHunt;
  const auto first = chapter / CHAPTERS_PER_BOOK * CHAPTERS_PER_BOOK;
  const auto key = guardian(battleAnchor(chapter));
  unsigned path = first;
  while (guardian(battleAnchor(path)) != key) ++path;
  const int start = first / CHAPTERS_PER_BOOK * kRequestsPerRegion +
                    ((path - first) * kRequestsPerRegion + CHAPTERS_PER_BOOK - 1) / CHAPTERS_PER_BOOK;
  for (int id = start; id < kLegacyRequestCount && requestChapter(id) == path; ++id)
    if (questexp::REQUEST_REWARDS[id] != kGearCount) return id;
  return start;
}
inline constexpr unsigned legacyRequestMinutes(int id) {
  if (bestiaryHunt(id)) return BESTIARY[id - kBestiaryHuntStart].minutes;
  constexpr uint8_t minutes[] = {10, 15, 20, 15, 25, 20, 30, 15, 20, 30, 25, 15, 35,
                                 20, 30, 25, 40, 20, 30, 25, 45, 30, 40, 35, 60};
  if (id == kShadowHunt) return 35;
  if (id == kKingHunt) return 60;
  if (specialHunt(id)) {
    constexpr unsigned special[] = {105, 180, 300, 1800};
    return special[id - kCampaignHuntCount];
  }
  return validRequest(id) ? minutes[id % kRequestsPerRegion] : 0;
}
inline constexpr unsigned requestMinutes(int id) {
  if (bestiaryHunt(id)) {
    const unsigned minutes = BESTIARY[id - kBestiaryHuntStart].minutes;
    return huntRank(id) == HuntRank::Elite ? 180 + (minutes - 90) * 2 / 3 : 360 + (minutes - 240) / 2;
  }
  constexpr uint8_t MINUTES[] = {10, 15, 20, 15, 25, 20, 30, 15, 20, 30, 25, 15, 35,
                                 20, 30, 25, 40, 20, 30, 25, 45, 30, 40, 35, 60};
  if (id == kShadowHunt) return 1500;
  if (id == kKingHunt) return 1800;
  if (specialHunt(id)) {
    constexpr unsigned minutes[] = {180, 300, 420, 1800};
    return minutes[id - kCampaignHuntCount];
  }
  return !validRequest(id)                 ? 0
         : huntRank(id) == HuntRank::Elite ? 180 + MINUTES[id % kRequestsPerRegion] * 2
                                           : 25 + id / kRequestsPerRegion * 5 + MINUTES[id % kRequestsPerRegion] / 2;
}
inline constexpr unsigned requestXp(int id) { return questbalance::victoryXp(requestMinutes(id)); }
inline constexpr unsigned requestCoins(int id) { return requestMinutes(id) / 4 + 8; }
inline unsigned requestCoins(const Profile& p, int id) {
  return (requestCoins(id) * (100 + bonuses(p).fortune) + 50) / 100;
}
inline constexpr unsigned INK_HUNT_SECONDS = requestMinutes(kInkHunt) * 60;
inline constexpr int huntOutfitItem(int id) {
  return !validRequest(id)  ? kGearCount
         : bestiaryHunt(id) ? kBestiaryOutfitStart + id - kBestiaryHuntStart
         : specialHunt(id)  ? kHuntOutfitStart + kCampaignHuntFamilies + id - kCampaignHuntCount
                            : kHuntOutfitStart + int(guardian(huntEncounter(id)));
}
inline const HuntVictory& huntVictory(const Profile& p, int id) {
  static constexpr HuntVictory empty;
  return validRequest(id) ? p.victories[huntOutfitIndex(huntOutfitItem(id))] : empty;
}
inline constexpr auto huntOutfitSources() {
  std::array<int, kHuntOutfitCount> sources{};
  for (unsigned path = 0; path < CAMPAIGN_CHAPTERS; ++path) {
    const int id = huntForPath(path);
    sources[huntOutfitIndex(huntOutfitItem(id))] = id;
  }
  for (int id = kCampaignHuntCount; id < kRequestCount; ++id) sources[huntOutfitIndex(huntOutfitItem(id))] = id;
  return sources;
}
inline constexpr auto HUNT_OUTFIT_SOURCES = huntOutfitSources();
inline constexpr int huntOutfitSource(int item) {
  return huntOutfit(item) ? HUNT_OUTFIT_SOURCES[huntOutfitIndex(item)] : -1;
}
inline constexpr int huntOutfitBase(int item) {
  if (item >= kBestiaryOutfitStart && huntOutfit(item)) return kGearCount;
  constexpr uint8_t bodies[] = {
      34, 50, 4,  18, 28, 52, 3,  7,  10, 5,  12, 51, 11, 37, 42, 54, 29, 53, 27, 38, 65, 19, 30, 57, 48,
      59, 47, 49, 44, 64, 8,  23, 61, 63, 55, 67, 32, 43, 45, 62, 66, 73, 35, 60, 70, 75, 80, 82, 25, 26,
      69, 71, 76, 86, 36, 41, 58, 72, 74, 87, 16, 17, 77, 81, 83, 90, 78, 79, 89, 91, 93, 96, 33, 98,
  };
  static_assert(sizeof(bodies) == kCampaignHuntFamilies);
  const unsigned index = unsigned(item - kHuntOutfitStart);
  return index < kCampaignHuntFamilies ? bodies[index] : 0;
}
inline constexpr int huntExtraItem(int id, unsigned index) {
  if (!specialHunt(id) || bestiaryHunt(id) || index >= 2) return kGearCount;
  constexpr int extras[][2] = {{157, kGearCount}, {163, 121}, {190, 129}, {kInkRelic, kGearCount}};
  return extras[id - kCampaignHuntCount][index];
}
inline constexpr int huntExtraSource(int item) {
  if (item == kGearCount) return -1;
  for (int id = kCampaignHuntCount; id < kRequestCount; ++id)
    for (unsigned n = 0; n < 2; ++n)
      if (huntExtraItem(id, n) == item) return id;
  return -1;
}
inline bool hasHuntClaim(const Profile& p, int id) {
  return validRequest(id) && (p.requestClaims[id / 8] & (1u << (id % 8)));
}
inline bool earnedHuntOutfit(const Profile& p, int item) {
  const int id = huntOutfitSource(item);
  if (hasHuntClaim(p, id)) return true;
  if (id < 0 || id >= kLegacyRequestCount) return false;
  const int first = id / kRequestsPerRegion * kRequestsPerRegion;
  for (int old = first; old < first + kRequestsPerRegion; ++old)
    if (hasHuntClaim(p, old) && huntOutfitItem(old) == item) return true;
  return false;
}
inline int replayHuntId(int id) { return validRequest(id) ? huntOutfitSource(huntOutfitItem(id)) : -1; }
inline uint32_t huntWinCount(const Profile& p, int id) {
  if (!validRequest(id)) return 0;
  const auto recorded = p.huntWins[huntOutfitIndex(huntOutfitItem(id))];
  return recorded ? recorded : unsigned(earnedHuntOutfit(p, huntOutfitItem(id)));
}
inline bool huntReached(const Profile& p, int id) {
  if (bestiaryHunt(id)) return completedPaths(p) >= BESTIARY[id - kBestiaryHuntStart].paths;
  if (specialHunt(id)) {
    switch (id) {
      case kStormHunt:
        return completedPaths(p) >= 5;
      case kCrystalHunt:
        return completedPaths(p) >= 10;
      case kPhoenixHunt:
        return completedPaths(p) >= 25;
      case kInkHunt:
        return p.inkHuntDiscovered;
    }
  }
  return completedPaths(p) > requestChapter(id);
}
inline RequestGoal requestGoal(const Profile& p, int id) {
  return p.activeRequest == id ? p.requestGoal : RequestGoal::Minutes;
}
inline unsigned requestTarget(const Profile& p, int id) {
  if (!validRequest(id)) return 0;
  switch (requestGoal(p, id)) {
    case RequestGoal::Minutes:
      return p.activeRequest == id && p.requestTargetMinutes ? p.requestTargetMinutes
             : id == kInkHunt && p.pausedInkTargetMinutes    ? p.pausedInkTargetMinutes
                                                             : huntMinutes(p, id);
    case RequestGoal::Pages:
      return 10 + (id / 5 % 4) * 5;
    case RequestGoal::Sessions:
      return 2 + id / 25 % 2;
    case RequestGoal::Books:
      return 1;
    case RequestGoal::Days:
      return 2 + id / 25 % 2;
  }
  return 0;
}
inline unsigned requestXp(const Profile& p, int id) {
  return questbalance::victoryXp(requestGoal(p, id) == RequestGoal::Minutes ? requestTarget(p, id) : huntMinutes(p, id),
                                 bonuses(p).experience);
}
uint64_t requestMetric(const Profile&, RequestGoal, const readingstats::Summary*);
bool requestClaimed(const Profile&, int id);
bool requestUnlocked(const Profile&, int id);
unsigned requestValue(const Profile&, int id, const readingstats::Summary* = nullptr);
bool acceptRequest(Profile&, int id);
bool cancelRequest(Profile&, int id, const readingstats::Summary* = nullptr);
bool replaceRequest(Profile&, int id, const readingstats::Summary* = nullptr);
bool collectRequest(Profile&, int id, const readingstats::Summary* = nullptr, int day = 0);
int availableHunts(const Profile&, int (&ids)[MAX_AVAILABLE_HUNTS]);
int requestCount(const Profile&, bool collection);
int requestAt(const Profile&, bool collection, int index);
}  // namespace quest

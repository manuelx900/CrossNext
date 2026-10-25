#include "QuestCore.h"

#include <algorithm>
#include <cstring>
namespace quest {
namespace {
void put(uint8_t* p, uint64_t v, int n) {
  for (int k = 0; k < n; ++k) p[k] = v >> (8 * k);
}
uint64_t get(const uint8_t* p, int n) {
  uint64_t v = 0;
  for (int k = 0; k < n; ++k) v |= uint64_t(p[k]) << (8 * k);
  return v;
}
bool bit(const uint8_t* b, int n) { return b[n / 8] & (1u << (n % 8)); }
void set(uint8_t* b, int n) { b[n / 8] |= 1u << (n % 8); }
bool reward(Profile& p, uint32_t amount) {
  if (p.bonus > UINT32_MAX - amount) return false;
  p.bonus += amount;
  p.xp = p.xp > UINT64_MAX - amount ? UINT64_MAX : p.xp + amount;
  return true;
}
struct StreakReward {
  uint16_t days, xp, coins;
};
constexpr StreakReward STREAK_REWARDS[] = {
    {3, 40, 10},    {7, 80, 25},    {14, 120, 50},   {30, 200, 100},
    {60, 300, 150}, {90, 400, 200}, {180, 600, 300}, {250, 800, 400},
};
const StreakReward* streakReward(int q) {
  if (q < 0 || q >= kQuestCount || q % 6 != 5) return nullptr;
  for (const auto& reward : STREAK_REWARDS)
    if (q / 6 + 1 == reward.days) return &reward;
  return nullptr;
}
uint32_t accumulatedLevelCoins(uint32_t n) { return 20 * (n - 1) + n * (n + 1) - 2; }
uint64_t earnedCoins(uint64_t xp, const uint8_t* claimed) {
  uint64_t total = xp / 10 + accumulatedLevelCoins(level(xp));
  for (const auto& reward : STREAK_REWARDS)
    if (bit(claimed, (reward.days - 1) * 6 + 5)) total += reward.coins;
  return total;
}
}  // namespace
uint64_t threshold(uint32_t n) {
  if (n < 2) return 0;
  const uint64_t steps = n - 1;
  return steps * 80 + steps * steps * 4;
}
uint32_t level(uint64_t xp) {
  uint32_t n = 1;
  while (n < MAX_LEVEL && xp >= threshold(n + 1)) ++n;
  return n;
}
uint64_t coins(const Profile& p) {
  const auto earned = earnedCoins(p.xp, p.claimed);
  return earned >= p.spent ? earned - p.spent : 0;
}
uint32_t levelRewardCoins(uint32_t n) { return n >= 2 && n <= MAX_LEVEL ? 20 + 2 * n : 0; }
bool hasLevelUp(const Profile& p) { return p.noticedLevel < level(p.xp); }
uint32_t pendingLevelCoins(const Profile& p) {
  return hasLevelUp(p) ? accumulatedLevelCoins(level(p.xp)) - accumulatedLevelCoins(p.noticedLevel) : 0;
}
bool acknowledgeLevel(Profile& p) {
  if (!hasLevelUp(p)) return false;
  p.noticedLevel = level(p.xp);
  return true;
}
void refresh(Profile& p, const readingstats::Summary& s) {
  const uint64_t minutes = s.seconds / 60, base = minutes > UINT64_MAX / 3 ? UINT64_MAX : minutes * 3;
  const uint64_t other = uint64_t(s.completed) * 120 + uint64_t(s.totalDays) * 15 + p.bonus;
  p.xp = std::max(p.xp, base > UINT64_MAX - other ? UINT64_MAX : base + other);
}
bool claim(Profile& p, int today, uint32_t seconds) {
  if (today < 18262 || today > 84005 || today <= p.claimedDay || seconds < DAILY_MINUTES * 60 || !reward(p, 60))
    return false;
  p.claimedDay = today;
  return true;
}
uint32_t questTarget(int q) {
  if (q < 0 || q >= kQuestCount) return 0;
  static constexpr uint32_t steps[] = {30, 25, 5, 2, 1, 1};
  return (q / 6 + 1) * steps[q % 6];
}
uint64_t questValue(int q, const readingstats::Summary& s) {
  switch (q % 6) {
    case 0:
      return s.seconds / 60;
    case 1:
      return s.pages;
    case 2:
      return s.sessions;
    case 3:
      return s.totalDays;
    case 4:
      return s.completed;
    case 5:
      return s.longest;
  }
  return 0;
}
bool isClaimed(const Profile& p, int q) { return q >= 0 && q < kQuestCount && bit(p.claimed, q); }
int orderedQuest(const Profile& p, const readingstats::Summary& stats, int index) {
  if (index < 0 || index >= kQuestCount) return -1;
  for (int group = 0; group < 3; ++group) {
    for (int q = 0; q < kQuestCount; ++q) {
      const int rank = isClaimed(p, q) ? 2 : questValue(q, stats) >= questTarget(q) ? 0 : 1;
      if (rank == group && index-- == 0) return q;
    }
  }
  return -1;
}
uint32_t questReward(int q) {
  if (q < 0 || q >= kQuestCount) return 0;
  if (const auto* reward = streakReward(q)) return reward->xp;
  static constexpr uint32_t rewards[] = {10, 10, 20, 20, 60, 10};
  return rewards[q % 6] + (q / 6) / 100 * 10;
}
uint32_t questCoinReward(int q) {
  const auto* reward = streakReward(q);
  return questReward(q) / 10 + (reward ? reward->coins : 0);
}
bool claimMilestone(Profile& p, int q, const readingstats::Summary& s) {
  if (q < 0 || q >= kQuestCount || isClaimed(p, q) || questValue(q, s) < questTarget(q) || !reward(p, questReward(q)))
    return false;
  set(p.claimed, q);
  return true;
}
Slot slot(int n) { return n < kOutfitCount ? Slot::Body : n < kOutfitCount + kAuraCount ? Slot::Aura : Slot::Pet; }
int first(Slot s) {
  switch (s) {
    case Slot::Body:
      return 0;
    case Slot::Aura:
      return kOutfitCount;
    case Slot::Pet:
      return kOutfitCount + kAuraCount;
  }
  return kGearCount;
}
int count(Slot s) {
  switch (s) {
    case Slot::Body:
      return kOutfitCount;
    case Slot::Aura:
      return kAuraCount;
    case Slot::Pet:
      return kPetCount;
  }
  return 0;
}
int catalogItem(Slot s, int index) { return index >= 0 && index < count(s) ? first(s) + index : kGearCount; }
bool visibleItem(int item) { return item >= 0 && item < kGearCount; }
int ownedCount(const Profile& p, Slot s) {
  int total = 0;
  for (int n = 0; n < count(s); ++n)
    if (owns(p, first(s) + n)) ++total;
  return total;
}
int ownedItem(const Profile& p, Slot s, int index) {
  if (index < 0) return kGearCount;
  for (int n = 0; n < count(s); ++n)
    if (owns(p, first(s) + n) && index-- == 0) return first(s) + n;
  return kGearCount;
}
int ownedAuraCount(const Profile& p) { return ownedCount(p, Slot::Aura); }
int shopCount(const Profile& p, Slot s) { return count(s) - ownedCount(p, s); }
int shopItem(const Profile& p, Slot s, int index) {
  if (index < 0) return kGearCount;
  for (int n = 0; n < count(s); ++n)
    if (!owns(p, first(s) + n) && index-- == 0) return first(s) + n;
  return kGearCount;
}
int ownedAuraItem(const Profile& p, int index) { return ownedItem(p, Slot::Aura, index); }
bool unequip(Profile& p, Slot s) {
  if (s != Slot::Aura && s != Slot::Pet) return false;
  if (p.equipped[int(s)] == kGearCount) return false;
  p.equipped[int(s)] = kGearCount;
  return true;
}
uint32_t requiredLevel(int n) {
  if (!visibleItem(n)) return 101;
  const int index = n - first(slot(n));
  return slot(n) == Slot::Body ? index + 1 : 1 + (index * 99 + 24) / 49;
}
uint32_t price(int n) {
  if (!visibleItem(n)) return UINT32_MAX;
  if (n == 0) return 0;
  const auto level = requiredLevel(n);
  const auto index = n - first(slot(n));
  return slot(n) == Slot::Body ? 15 + level * 9 / 10 + index / 6 : 22 + level + index / 8;
}
bool owns(const Profile& p, int n) { return visibleItem(n) && bit(p.owned, n); }
bool equip(Profile& p, int n) {
  if (!visibleItem(n)) return false;
  if (!owns(p, n)) {
    if (level(p.xp) < requiredLevel(n) || coins(p) < price(n)) return false;
    p.spent += price(n);
    set(p.owned, n);
  }
  p.equipped[int(slot(n))] = n;
  return true;
}
namespace {
void commonFields(const uint8_t* b, Profile& p) {
  p = {};
  p.sequence = get(b + 4, 4);
  p.xp = get(b + 8, 8);
  p.spent = get(b + 16, 8);
  p.bonus = get(b + 24, 4);
  p.claimedDay = get(b + 28, 4);
  p.noticedLevel = level(p.xp);
}
bool validCommon(const uint8_t* b, bool modern = false) {
  const uint64_t xp = get(b + 8, 8);
  return get(b + 28, 4) <= 84005 && get(b + 16, 8) <= (modern ? earnedCoins(xp, b + 64) : xp / 10);
}
int migrateItem(int old) {
  if (old < 0 || old >= 375) return kGearCount;
  if (old < 150) return (old % 50) * 2;
  if (old < 350) return first(Slot::Aura) + (old - 150) / 4;
  static constexpr uint8_t legacyPets[] = {3,  5,  4,  1,  0,  6,  7,  8,  2,  9,  10, 11, 12,
                                           13, 14, 32, 30, 39, 33, 40, 42, 41, 46, 47, 49};
  return first(Slot::Pet) + legacyPets[old - 350];
}
bool expandedLegacy(const uint8_t* b, Profile& p, bool v3) {
  const int checksumAt = v3 ? 316 : 252;
  if (std::memcmp(b, v3 ? "RQU3" : "RQU2", 4) || get(b + checksumAt, 4) != readingstats::checksum(b, checksumAt) ||
      !validCommon(b) || b[32] > 1 || (b[89] & 0x80) || (v3 && (b[277] & 0xF0)))
    return false;
  static constexpr int starts[] = {0, 50, 100, 150, 350};
  static constexpr int counts[] = {50, 50, 50, 200, 25};
  for (int n = 0; n < 5; ++n) {
    const int item = get(b + 33 + n * 2, 2);
    if (item != 375 && (item < starts[n] || item >= starts[n] + counts[n] || !bit(b + 43, item))) return false;
  }
  commonFields(b, p);
  p.gender = Gender(b[32]);
  // Retired head/foot purchases become the matching complete outfit. Currency is unchanged.
  for (int item = 0; item < 375; ++item)
    if (bit(b + 43, item)) set(p.owned, migrateItem(item));
  const int body = get(b + 35, 2), aura = get(b + 39, 2), pet = get(b + 41, 2);
  p.equipped[int(Slot::Body)] = body == 375 ? 0 : migrateItem(body);
  p.equipped[int(Slot::Aura)] = migrateItem(aura);
  p.equipped[int(Slot::Pet)] = migrateItem(pet);
  std::memcpy(p.claimed, b + 90, v3 ? sizeof(p.claimed) : 125);
  return true;
}
}  // namespace
void encode(const Profile& p, uint8_t* b) {
  std::memset(b, 0, kProfileBytes);
  std::memcpy(b, "RQU5", 4);
  put(b + 4, p.sequence, 4);
  put(b + 8, p.xp, 8);
  put(b + 16, p.spent, 8);
  put(b + 24, p.bonus, 4);
  put(b + 28, uint32_t(p.claimedDay), 4);
  b[32] = uint8_t(p.gender);
  for (int n = 0; n < kSlotCount; ++n) put(b + 33 + n * 2, p.equipped[n], 2);
  std::memcpy(b + 39, p.owned, sizeof(p.owned));
  std::memcpy(b + 64, p.claimed, sizeof(p.claimed));
  b[252] = p.noticedLevel;
  put(b + 316, readingstats::checksum(b, 316), 4);
}
namespace {
bool decodeCurrent(const uint8_t* b, Profile& p, bool v5) {
  if (std::memcmp(b, v5 ? "RQU5" : "RQU4", 4) || get(b + 316, 4) != readingstats::checksum(b, 316) ||
      !validCommon(b, v5) || (v5 && (b[252] < 1 || b[252] > level(get(b + 8, 8)))) || b[32] > 1 || (b[251] & 0xF0) ||
      !(b[39] & 1))
    return false;
  for (int n = 0; n < kSlotCount; ++n) {
    const int item = get(b + 33 + n * 2, 2);
    if (item == kGearCount) {
      if (n == int(Slot::Body)) return false;
    } else if (!visibleItem(item) || int(slot(item)) != n || !bit(b + 39, item))
      return false;
  }
  commonFields(b, p);
  p.gender = Gender(b[32]);
  for (int n = 0; n < kSlotCount; ++n) p.equipped[n] = get(b + 33 + n * 2, 2);
  std::memcpy(p.owned, b + 39, sizeof(p.owned));
  std::memcpy(p.claimed, b + 64, sizeof(p.claimed));
  if (v5) p.noticedLevel = b[252];
  return true;
}
}  // namespace
bool decode(const uint8_t* b, Profile& p) { return decodeCurrent(b, p, true); }
bool decodeV4(const uint8_t* b, Profile& p) { return decodeCurrent(b, p, false); }
bool decodeV3(const uint8_t* b, Profile& p) { return expandedLegacy(b, p, true); }
bool decodeV2(const uint8_t* b, Profile& p) { return expandedLegacy(b, p, false); }
bool decodeLegacy(const uint8_t* b, Profile& p) {
  if (std::memcmp(b, "RQU1", 4) || get(b + 60, 4) != readingstats::checksum(b, 60) || !(b[32] & 1) || b[32] > 7 ||
      b[33] > 2 || !(b[32] & (1 << b[33])) || !validCommon(b))
    return false;
  commonFields(b, p);
  for (int n = 0; n < 3; ++n)
    if (b[32] & (1 << n)) set(p.owned, n * 2);
  p.equipped[int(Slot::Body)] = b[33] * 2;
  return true;
}
}  // namespace quest

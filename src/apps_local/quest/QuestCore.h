#pragma once
#include <cstdint>

#include "../reading/ReadingStatsCore.h"
namespace quest {
constexpr int kProfileBytes = 320, kV2ProfileBytes = 256, kGearCount = 200, kQuestCount = 1500;
constexpr int kSlotCount = 3, kOutfitCount = 100, kAuraCount = 50, kPetCount = 50;
constexpr uint32_t MAX_LEVEL = 100, LEVELS_PER_TITLE = 5, TITLE_COUNT = MAX_LEVEL / LEVELS_PER_TITLE;
constexpr uint32_t DAILY_MINUTES = 15;
enum class Slot : uint8_t { Body, Aura, Pet };
enum class Gender : uint8_t { Masculine, Feminine };
struct Profile {
  uint32_t sequence = 0;
  uint64_t xp = 0, spent = 0;
  uint32_t bonus = 0;
  int claimedDay = 0;
  Gender gender = Gender::Masculine;
  uint8_t noticedLevel = 1;
  uint16_t equipped[kSlotCount] = {0, kGearCount, kGearCount};
  uint8_t owned[(kGearCount + 7) / 8] = {1}, claimed[(kQuestCount + 7) / 8]{};
};
// Activity-owned scratch keeps persistence and transaction copies off the task stack.
struct StoreWork {
  uint8_t bytes[kProfileBytes]{};
  Profile candidate;
};
uint32_t level(uint64_t xp);
uint64_t threshold(uint32_t level);
uint64_t coins(const Profile&);
uint32_t levelRewardCoins(uint32_t level);
uint32_t pendingLevelCoins(const Profile&);
bool hasLevelUp(const Profile&);
bool acknowledgeLevel(Profile&);
void refresh(Profile&, const readingstats::Summary&);
bool claim(Profile&, int today, uint32_t seconds);
bool claimMilestone(Profile&, int quest, const readingstats::Summary&);
uint64_t questValue(int quest, const readingstats::Summary&);
uint32_t questTarget(int quest);
uint32_t questReward(int quest);
uint32_t questCoinReward(int quest);
bool isClaimed(const Profile&, int quest);
// Ready rewards first, then unfinished quests, then previously claimed quests.
int orderedQuest(const Profile&, const readingstats::Summary&, int index);
Slot slot(int item);
int first(Slot);
int count(Slot);
int catalogItem(Slot, int index);
bool visibleItem(int item);
int ownedCount(const Profile&, Slot);
int ownedItem(const Profile&, Slot, int index);
int shopCount(const Profile&, Slot);
int shopItem(const Profile&, Slot, int index);
int ownedAuraCount(const Profile&);
int ownedAuraItem(const Profile&, int index);
bool unequip(Profile&, Slot);
uint32_t requiredLevel(int item);
uint32_t price(int item);
bool owns(const Profile&, int item);
bool equip(Profile&, int item);
void encode(const Profile&, uint8_t*);
bool decode(const uint8_t*, Profile&);
bool decodeV4(const uint8_t*, Profile&);
bool decodeV3(const uint8_t*, Profile&);
bool decodeV2(const uint8_t*, Profile&);
bool decodeLegacy(const uint8_t*, Profile&);
bool load(Profile&, bool& writable, StoreWork&);
bool save(Profile&, bool writable, StoreWork&);
}  // namespace quest

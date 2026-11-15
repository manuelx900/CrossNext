#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/quest/QuestExpansionPolicy.h"
#include "../../src/apps_local/quest/QuestScenePlan.h"
#include "../study-suite/stubs/HalStorage.h"

int main() {
  quest::Profile profile;
  bool seen[quest::kGearCount]{};
  for (int n = 0; n < quest::kDecorationCount; ++n) {
    const int item = quest::catalogItem(quest::Slot::Decoration, n);
    assert(!seen[item] && quest::availableItem(item));
    seen[item] = true;
    assert(quest::grantItem(profile, item) && quest::equip(profile, item));
    unsigned equipped = 0;
    for (const auto id : profile.decorations) equipped += quest::visibleItem(id);
    assert(equipped == 1 && quest::activeDecoration(profile) == item);
  }
  assert(quest::ownedCount(profile, quest::Slot::Decoration) == 60);
  unsigned reserved = 0;
  for (int item = 216; item <= quest::kKingDecoration; ++item) {
    if (quest::availableItem(item)) continue;
    ++reserved;
    assert(!quest::grantItem(profile, item) && !quest::equip(profile, item));
    const int index = item - quest::kExpandedItemStart;
    profile.expansionOwned[index / 8] |= 1u << (index % 8);
  }
  assert(reserved == 57);
  profile.decorations[0] = 200;
  profile.decorations[1] = 201;
  uint8_t bytes[quest::kProfileBytes];
  quest::encode(profile, bytes);
  quest::Profile copy;
  assert(quest::decode(bytes, copy));
  assert(copy.decorations[0] == quest::kGearCount && copy.decorations[1] == 201);
  assert(!std::memcmp(copy.expansionOwned, profile.expansionOwned, sizeof(profile.expansionOwned)));
  assert(quest::equip(copy, 200) && copy.decorations[1] == quest::kGearCount);
  const auto preview = quest::bonuses(copy, 217);
  assert(quest::equip(copy, 217));
  const auto actual = quest::bonuses(copy);
  assert(preview.power == actual.power && preview.experience == actual.experience && preview.fortune == actual.fortune);
  profile = {};
  profile.campaign.encounter = 23;
  profile.campaign.phase = quest::BattlePhase::Story;
  profile.campaign.introducedBooks = 1;
  profile.campaign.prologueSeen = true;
  profile.campaign.narratedEncounters = 24;
  profile.expansionOwned[0] |= 1u << 2;
  profile.campaign.pendingReward = 218;
  quest::encode(profile, bytes);
  assert(quest::decode(bytes, copy) && quest::owns(copy, 218));
  assert(copy.campaign.pendingReward == quest::kGearCount);
  for (unsigned n = 0; n < quest::CAMPAIGN_BOOKS; ++n) profile.sagaPositions[n] = ((n * 10) << 8) | (n % 3);
  quest::encode(profile, bytes);
  assert(quest::decode(bytes, copy));
  assert(!std::memcmp(profile.sagaPositions, copy.sagaPositions, sizeof(profile.sagaPositions)));
  bytes[475] = 0;
  const auto repairChecksum = [&]() {
    const auto checksum = readingstats::checksum(bytes, 508);
    for (int n = 0; n < 4; ++n) bytes[508 + n] = checksum >> (n * 8);
    const auto full = readingstats::checksum(bytes, quest::kProfileBytes - 4);
    for (int n = 0; n < 4; ++n) bytes[quest::kProfileBytes - 4 + n] = full >> (n * 8);
  };
  repairChecksum();
  assert(quest::decode(bytes, copy));
  for (auto position : copy.sagaPositions) assert(position == 0);
  bytes[475] = 1;
  bytes[477] = 121;
  repairChecksum();
  assert(!quest::decode(bytes, copy));
  testStorage = {};
  quest::StoreWork work;
  bool writable = true;
  assert(quest::save(profile, writable, work));
  copy = {};
  assert(quest::load(copy, writable, work));
  assert(!std::memcmp(profile.sagaPositions, copy.sagaPositions, sizeof(profile.sagaPositions)));
  profile.sagaPositions[3] = 120 << 8;
  testStorage.writeLimit = 32;
  assert(!quest::save(profile, writable, work));
  testStorage.writeLimit = -1;
  assert(quest::load(copy, writable, work));
  assert(copy.sagaPositions[3] == (30 << 8));
  std::puts(
      "Polish: 60 active/57 reserved decorations, single-equipment migration, archived ownership/pending gift, preview "
      "parity and bookmark persistence passed");
}

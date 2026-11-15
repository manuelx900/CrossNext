#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/quest/QuestRequests.h"

namespace {
static uint8_t bytes[quest::kProfileBytes];
void checksum() {
  for (int offset : {316, 508, quest::kProfileBytes - 4}) {
    const uint32_t value = readingstats::checksum(bytes, offset);
    for (int n = 0; n < 4; ++n) bytes[offset + n] = value >> (8 * n);
  }
}
void legacy(const char* name, bool active) {
  char path[160];
  std::snprintf(path, sizeof(path), "host-tests/quest-campaign/fixtures/%s.bin", name);
  auto* file = std::fopen(path, "rb");
  assert(file && std::fread(bytes, 1, quest::kV6ProfileBytes, file) == quest::kV6ProfileBytes);
  std::fclose(file);
  assert(bytes[401] == 7);
  quest::Profile restored;
  assert(quest::decode(bytes, restored));
  assert(restored.activeRequest == (active ? quest::kInkHunt : quest::kRequestCount));
  if (active) assert(restored.requestReported == 1800);
  quest::encode(restored, bytes);
  assert(bytes[401] == 8 && quest::decode(bytes, restored));
}
}  // namespace

int main() {
  legacy("hunt-v7-idle", false);
  legacy("hunt-v7-ready", true);
  static_assert(quest::kBestiaryCount > 50 && quest::kInkRelic == 399);
  quest::Profile hero, restored;
  auto& c = hero.campaign;
  c.initialized = c.prologueSeen = true;
  c.encounter = quest::CAMPAIGN_ENCOUNTERS - 1;
  c.phase = quest::BattlePhase::Complete;
  c.introducedBooks = 12;
  c.narratedEncounters = quest::CAMPAIGN_ENCOUNTERS;
  c.observedSeconds = 100000;
  hero.inkHuntDiscovered = true;
  hero.requestClaims[quest::kInkHunt / 8] |= 1u << (quest::kInkHunt % 8);
  assert(quest::equip(hero, quest::kInkRelic));
  for (int id = quest::kBestiaryHuntStart; id < quest::kRequestCount; ++id) {
    const int item = quest::huntOutfitItem(id);
    assert(quest::huntOutfit(item) && quest::slot(item) == quest::Slot::Body);
    assert(quest::huntOutfitSource(item) == id && !quest::owns(hero, item));
    assert(quest::acceptRequest(hero, id) && !quest::collectRequest(hero, id));
    c.observedSeconds += 60;
    assert(quest::requestValue(hero, id) == 1);
    quest::encode(hero, bytes);
    assert(quest::decode(bytes, restored) && restored.activeRequest == id);
    assert(quest::requestValue(restored, id) == 1 && quest::equipped(restored, quest::kInkRelic));
    c.observedSeconds += (quest::requestMinutes(id) - 1) * 60;
    const auto xp = hero.xp, coins = c.extraCoins;
    const auto prize = quest::requestXp(hero, id);
    assert(quest::collectRequest(hero, id));
    assert(hero.xp == xp + prize && c.extraCoins == coins + quest::requestCoins(hero, id));
    assert(quest::owns(hero, item) && quest::equip(hero, item));
    assert(!quest::collectRequest(hero, id) && quest::requestUnlocked(hero, id));
    quest::encode(hero, bytes);
    assert(quest::decode(bytes, restored) && quest::equipped(restored, item));
    for (int old = quest::kBestiaryHuntStart; old <= id; ++old)
      assert(quest::requestClaimed(restored, old) && quest::owns(restored, quest::huntOutfitItem(old)));
    assert(quest::requestCount(hero, false) <= 4);
  }
  bytes[293] |= 0x80;
  checksum();
  assert(!quest::decode(bytes, restored));
  quest::Profile discovery;
  discovery.campaign = c;
  discovery.campaign.extraCoins = 0;
  for (int id = quest::kBestiaryHuntStart; id < quest::kBestiaryHuntStart + 12; ++id) {
    assert(quest::acceptRequest(discovery, id));
    discovery.campaign.observedSeconds += quest::requestMinutes(id) * 60;
    assert(quest::collectRequest(discovery, id));
  }
  assert(discovery.inkHuntDiscovered);
  readingstats::Summary summary;
  quest::restartAdventure(hero, summary, 20000);
  for (int id = quest::kBestiaryHuntStart; id < quest::kRequestCount; ++id)
    assert(!quest::requestClaimed(hero, id) && !quest::owns(hero, quest::huntOutfitItem(id)));
  std::printf(
      "Bestiary: 56 rewards, partial progress, save/reload, once-only claims, legacy v7, sentinel and relic IDs "
      "passed; Profile=%zu\n",
      sizeof(hero));
}

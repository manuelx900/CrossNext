#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestRequests.h"

static quest::Profile hero, loaded;
static uint8_t bytes[quest::kProfileBytes];
static readingstats::Summary summary;
static void checksum() {
  const auto crc = readingstats::checksum(bytes, sizeof(bytes) - 4);
  for (unsigned n = 0; n < 4; ++n) bytes[sizeof(bytes) - 4 + n] = uint8_t(crc >> (n * 8));
}
static void read(unsigned seconds) {
  summary.seconds += seconds;
  quest::syncCampaign(hero, summary, 20500);
}
static void win(int id) {
  assert(quest::acceptRequest(hero, id));
  assert(quest::requestValue(hero, id, &summary) == 0);
  const auto before = hero.xp;
  read(quest::requestTarget(hero, id) * 60 - 1);
  assert(hero.xp == before && !quest::collectRequest(hero, id, &summary, 20500));
  read(1);
  assert(quest::collectRequest(hero, id, &summary, 20500));
  assert(hero.xp > before && !quest::collectRequest(hero, id, &summary, 20500));
}
int main() {
  hero.campaign.initialized = hero.campaign.prologueSeen = true;
  hero.campaign.introducedBooks = 12;
  hero.campaign.narratedEncounters = 720;
  hero.campaign.encounter = 719;
  hero.campaign.phase = quest::BattlePhase::Complete;
  hero.campaign.originSeconds = hero.campaign.observedSeconds = summary.seconds = 1000;
  for (int old = 0; old < quest::kRequestCount; ++old) {
    const int canonical = quest::replayHuntId(old);
    assert(quest::validRequest(canonical) && quest::huntOutfitItem(canonical) == quest::huntOutfitItem(old));
    assert(canonical == quest::kInkHunt || quest::requestUnlocked(hero, canonical));
  }
  assert(quest::replayHuntId(-1) == -1 && quest::replayHuntId(quest::kRequestCount) == -1);
  const int id = quest::huntForPath(0);
  win(id);
  assert(quest::huntWinCount(hero, id) == 1);
  const auto first = quest::huntVictory(hero, id).xp;
  const auto uniqueItems = hero.acquiredCount;
  quest::markItemSeen(hero, quest::huntOutfitItem(id));
  const auto coinsBefore = quest::coins(hero);
  assert(quest::acceptRequest(hero, id));
  const auto target = quest::requestTarget(hero, id);
  read(30);
  quest::encode(hero, bytes);
  assert(quest::decode(bytes, loaded));
  assert(loaded.activeRequest == id && quest::huntWinCount(loaded, id) == 1);
  assert(quest::requestValue(loaded, id, &summary) == 0 && quest::requestTarget(loaded, id) == target);
  hero = loaded;
  read(target * 60 - 30);
  assert(quest::collectRequest(hero, id, &summary, 20500));
  assert(quest::huntWinCount(hero, id) == 2 && hero.xp >= first * 2);
  assert(quest::coins(hero) > coinsBefore && hero.acquiredCount == uniqueItems);
  assert(!quest::isNewItem(hero, quest::huntOutfitItem(id)));
  win(id);
  assert(quest::huntWinCount(hero, id) == 3 && quest::requestCount(hero, true) == 1);
  quest::encode(hero, bytes);
  assert(quest::decode(bytes, loaded) && quest::huntWinCount(loaded, id) == 3);

  hero.inkHuntDiscovered = true;
  win(quest::kInkHunt);
  assert(quest::acceptRequest(hero, quest::kInkHunt));
  const auto inkTarget = quest::requestTarget(hero, quest::kInkHunt);
  read(120);
  assert(quest::cancelRequest(hero, quest::kInkHunt, &summary));
  quest::encode(hero, bytes);
  assert(quest::decode(bytes, loaded));
  hero = loaded;
  assert(quest::acceptRequest(hero, quest::kInkHunt));
  assert(quest::requestTarget(hero, quest::kInkHunt) == inkTarget);
  assert(quest::requestValue(hero, quest::kInkHunt, &summary) == 2);
  read((inkTarget - 2) * 60);
  assert(quest::collectRequest(hero, quest::kInkHunt, &summary, 20500));
  assert(quest::huntWinCount(hero, quest::kInkHunt) == 2);

  std::memset(hero.huntWins, 0, sizeof(hero.huntWins));
  quest::encode(hero, bytes);
  bytes[7110] = 4;
  checksum();
  assert(quest::decode(bytes, loaded) && quest::huntWinCount(loaded, id) == 1);
  hero = loaded;
  quest::encode(hero, bytes);
  const auto index = quest::huntOutfitIndex(quest::huntOutfitItem(quest::kCrystalHunt));
  bytes[7124 + index * 4] = 1;
  checksum();
  assert(!quest::decode(bytes, loaded));
  assert(quest::acceptRequest(hero, id));
  quest::encode(hero, bytes);
  bytes[7110] = 4;
  checksum();
  assert(!quest::decode(bytes, loaded));
  assert(quest::cancelRequest(hero, id));
  win(id);
  assert(quest::huntWinCount(hero, id) == 2);
  quest::encode(hero, bytes);
  bytes[7110] = quest::BALANCE_REVISION + 1;
  checksum();
  assert(quest::hasNewerBalance(bytes) && !quest::decode(bytes, loaded));

  hero.huntWins[quest::huntOutfitIndex(quest::huntOutfitItem(id))] = UINT32_MAX;
  win(id);
  assert(quest::huntWinCount(hero, id) == UINT32_MAX && quest::requestUnlocked(hero, id));
  for (int source : quest::HUNT_OUTFIT_SOURCES) hero.requestClaims[source / 8] |= 1u << (source % 8);
  assert(quest::requestCount(hero, true) == quest::kHuntOutfitCount);
  int offers[quest::MAX_AVAILABLE_HUNTS];
  assert(quest::availableHunts(hero, offers) == quest::MAX_AVAILABLE_HUNTS);
  for (int source : offers) assert(quest::requestUnlocked(hero, source));
  std::printf(
      "Repeat hunts: fresh reading, three wins, resume, unique keepsakes, Ink replay/pause, v4 migration, future "
      "protection and saturated counter passed; Profile=%zu StoreWork=%zu\n",
      sizeof(hero), sizeof(quest::StoreWork));
}

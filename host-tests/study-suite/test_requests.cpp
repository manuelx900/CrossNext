#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestReadingReturn.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "stubs/HalStorage.h"

static void checksum(uint8_t* b) {
  const auto crc = readingstats::checksum(b, 508);
  for (int n = 0; n < 4; ++n) b[508 + n] = crc >> (8 * n);
  const auto full = readingstats::checksum(b, quest::kProfileBytes - 4);
  for (int n = 0; n < 4; ++n) b[quest::kProfileBytes - 4 + n] = full >> (8 * n);
}
int main() {
  quest::Profile p;
  p.campaign.initialized = true;
  p.campaign.prologueSeen = true;
  p.campaign.introducedBooks = 1;
  p.campaign.encounter = 6;
  p.campaign.narratedEncounters = 7;
  p.campaign.originSeconds = 1000;
  p.campaign.observedSeconds = 1000;
  assert(quest::requestCount(p, false) == 2);
  assert(quest::requestCount(p, true) == 0);
  assert(!quest::acceptRequest(p, -1) && !quest::acceptRequest(p, quest::kRequestCount) && !quest::acceptRequest(p, 3));
  p.campaign.observedSeconds = 999;
  assert(!quest::acceptRequest(p, 0));
  p.campaign.observedSeconds = 1000;
  assert(quest::acceptRequest(p, 0));
  assert(!quest::acceptRequest(p, 1));
  assert(quest::requestValue(p, 0) == 0);
  assert(quest::requestAt(p, false, 0) == 0);
  uint8_t bytes[quest::kProfileBytes];
  quest::encode(p, bytes);
  quest::Profile restored;
  assert(quest::decode(bytes, restored));
  assert(restored.activeRequest == 0 && restored.requestStarted == 1000);
  const auto before = p;
  assert(questreturn::capture(before, p).kind == questreturn::Kind::None);
  p.campaign.observedSeconds += quest::requestTarget(before, 0) * 60 - 1;
  assert(!quest::collectRequest(p, 0));
  ++p.campaign.observedSeconds;
  assert(questreturn::capture(before, p).seconds == quest::requestTarget(before, 0) * 60);
  assert(quest::collectRequest(p, 0));
  assert(p.xp == quest::requestXp(before, 0) && p.campaign.extraCoins == quest::requestCoins(before, 0));
  assert(quest::requestClaimed(p, 0) && p.activeRequest == quest::kRequestCount);
  const auto earned = p.xp;
  assert(!quest::collectRequest(p, 0) && p.xp == earned);
  quest::encode(p, bytes);
  assert(quest::decode(bytes, restored));
  assert(quest::requestClaimed(restored, 0));
  // Existing schema-3 saves migrate without consuming a request or losing equipment.
  bytes[3] = '6';
  bytes[401] = 3;
  std::memset(bytes + 413, 0, 95);
  for (int at : {407, 409, 411}) {
    bytes[at] = 216;
    bytes[at + 1] = 0;
  }
  checksum(bytes);
  assert(quest::decode(bytes, restored));
  assert(restored.activeRequest == quest::kRequestCount);
  assert(quest::requestCount(restored, true) == 0 && restored.equipped[0] == p.equipped[0]);
  quest::encode(p, bytes);
  bytes[450] |= 0x80;
  checksum(bytes);
  assert(!quest::decode(bytes, restored));
  quest::encode(p, bytes);
  bytes[459] = 0xff;
  bytes[460] = 0xff;
  checksum(bytes);
  assert(!quest::decode(bytes, restored));
  quest::Profile all;
  all.campaign.initialized = true;
  all.campaign.prologueSeen = true;
  all.campaign.introducedBooks = 12;
  all.campaign.encounter = 719;
  all.campaign.narratedEncounters = 720;
  all.campaign.phase = quest::BattlePhase::Complete;
  all.campaign.observedSeconds = 10000;
  all.inkHuntDiscovered = true;
  const int visibleHunts = quest::requestCount(all, false);
  assert(visibleHunts == 4 && quest::requestAt(all, false, visibleHunts) == -1);
  assert(quest::requestAt(all, false, 0) == quest::kInkHunt);
  bool seen[quest::kRequestCount]{};
  for (int n = 0; n < visibleHunts; ++n) {
    const int id = quest::requestAt(all, false, n);
    assert(quest::validRequest(id) && !seen[id]);
    seen[id] = true;
  }
  int hunts = 0;
  for (int id = 0; id < quest::kRequestCount; ++id) {
    seen[id] = quest::requestUnlocked(all, id);
    hunts += seen[id];
  }
  assert(hunts == 74 + quest::kSpecialHuntCount);
  readingstats::Summary metrics;
  int collected = 0;
  for (int id = 0; id < quest::kRequestCount; ++id) {
    if (!seen[id]) continue;
    assert(quest::acceptRequest(all, id));
    assert(quest::requestAt(all, false, 0) == id);
    assert(quest::requestValue(all, id) == 0);
    quest::encode(all, bytes);
    assert(quest::decode(bytes, restored));
    assert(restored.activeRequest == id && restored.requestStarted == all.requestStarted);
    const auto target = quest::requestTarget(all, id);
    switch (all.requestGoal) {
      case quest::RequestGoal::Minutes:
        all.campaign.observedSeconds += target * 60;
        break;
      case quest::RequestGoal::Pages:
        metrics.pages += target;
        break;
      case quest::RequestGoal::Sessions:
        metrics.sessions += target;
        break;
      case quest::RequestGoal::Books:
        metrics.completed += target;
        break;
      case quest::RequestGoal::Days:
        metrics.totalDays += target;
        break;
    }
    assert(quest::collectRequest(all, id, &metrics));
    assert(quest::owns(all, quest::huntOutfitItem(id)));
    assert(!quest::collectRequest(all, id));
    ++collected;
    assert(quest::requestAt(all, true, 0) == id);
    assert(quest::requestCount(all, false) == 4);
    quest::encode(all, bytes);
    assert(quest::decode(bytes, restored));
    assert(quest::requestClaimed(restored, id));
  }
  assert(collected == hunts && quest::requestCount(all, false) == 4 && quest::requestCount(all, true) == hunts);
  assert(quest::requestAt(all, false, 0) >= 0 && quest::requestAt(all, true, hunts) == -1);
  quest::Profile capped = before;
  capped.activeRequest = quest::kRequestCount;
  capped.requestStarted = 0;
  assert(quest::acceptRequest(capped, 0));
  capped.campaign.observedSeconds += quest::requestTarget(capped, 0) * 60;
  assert(quest::requestValue(capped, 0) == quest::requestTarget(capped, 0));
  capped.campaign.extraCoins = std::numeric_limits<uint64_t>::max();
  const auto unchanged = capped.xp;
  assert(!quest::collectRequest(capped, 0, &metrics));
  assert(capped.activeRequest == 0 && capped.xp == unchanged && !quest::requestClaimed(capped, 0));
  readingstats::Summary summary;
  summary.seconds = all.campaign.observedSeconds;
  quest::restartAdventure(all, summary, 20000);
  assert(quest::requestCount(all, true) == 0 && all.activeRequest == quest::kRequestCount && all.requestStarted == 0);
  // Restored or shortened journals retain a valid accepted request.
  testStorage.files.clear();
  testStorage.writeLimit = -1;
  quest::StoreWork rollbackWork;
  quest::Profile rollback = before, reloaded;
  readingstats::Summary shorter;
  shorter.seconds = 950;
  bool rollbackWritable = true;
  assert(quest::save(rollback, true, rollbackWork));
  assert(quest::syncCampaign(rollback, shorter, 20000));
  assert(rollback.activeRequest == 0 && rollback.requestStarted == 950 && quest::requestValue(rollback, 0) == 0);
  assert(quest::save(rollback, true, rollbackWork));
  assert(quest::load(reloaded, rollbackWritable, rollbackWork) && rollbackWritable);
  assert(reloaded.activeRequest == 0 && !quest::syncCampaign(reloaded, shorter, 20000));
  shorter.seconds += quest::requestTarget(before, 0) * 60;
  quest::syncCampaign(reloaded, shorter, 20000);
  assert(quest::collectRequest(reloaded, 0) && !quest::collectRequest(reloaded, 0));
  quest::Profile partial = before;
  partial.campaign.observedSeconds = 1450;
  shorter.seconds = 1350;
  assert(quest::syncCampaign(partial, shorter, 20000));
  assert(partial.requestStarted == 1350 && quest::requestValue(partial, 0) == 7);
  testStorage.files.clear();
  testStorage.writeLimit = -1;
  quest::StoreWork work;
  quest::Profile stored = before, candidate, loaded;
  bool writable = true;
  assert(quest::save(stored, true, work));
  candidate = stored;
  candidate.campaign.observedSeconds += quest::requestTarget(before, 0) * 60;
  assert(quest::collectRequest(candidate, 0));
  testStorage.writeLimit = 10;
  assert(!quest::save(candidate, true, work));
  assert(quest::load(loaded, writable, work) && writable);
  assert(!quest::requestClaimed(loaded, 0) && loaded.activeRequest == 0 && loaded.xp == stored.xp);
  assert(!quest::owns(loaded, quest::requestDecoration(0)) && quest::owns(candidate, quest::requestDecoration(0)));
  testStorage.writeLimit = -1;
  assert(quest::save(candidate, true, work));
  assert(quest::load(loaded, writable, work) && quest::requestClaimed(loaded, 0));
  assert(loaded.xp == candidate.xp && loaded.campaign.extraCoins == candidate.campaign.extraCoins);
  assert(!quest::collectRequest(loaded, 0));
  std::printf(
      "Unique hunts: once-only XP/coins/objects, accepted-reading baseline, bounds, schema migration,74 round trips, "
      "overflow and restart passed; Profile=%zu bytes\n",
      sizeof(p));
}

#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/quest/QuestReadingReturn.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "../../tools_local/quest/legacy_request_fixture.h"
#include "../study-suite/stubs/HalStorage.h"

namespace {
quest::Profile p, restored;
readingstats::Summary stats;
uint8_t bytes[quest::kProfileBytes];
void advance(quest::RequestGoal goal, unsigned value) {
  switch (goal) {
    case quest::RequestGoal::Minutes:
      p.campaign.observedSeconds += uint64_t(value) * 60;
      break;
    case quest::RequestGoal::Pages:
      stats.pages += value;
      break;
    case quest::RequestGoal::Sessions:
      stats.sessions += value;
      break;
    case quest::RequestGoal::Books:
      stats.completed += value;
      break;
    case quest::RequestGoal::Days:
      stats.totalDays += value;
      break;
  }
}
void initialize() {
  p = {};
  p.campaign.initialized = p.campaign.prologueSeen = true;
  p.campaign.introducedBooks = 12;
  p.campaign.narratedEncounters = 720;
  p.campaign.encounter = 719;
  p.campaign.phase = quest::BattlePhase::Complete;
  p.campaign.state = quest::AdventureState::Peace;
  p.campaign.observedSeconds = 10000;
  p.campaign.startedDay = 20000;
  stats = {};
  stats.seconds = 10000;
  stats.pages = 140;
  stats.sessions = 5;
  stats.completed = 2;
  stats.totalDays = 8;
}
void checksum() {
  const uint32_t crc = readingstats::checksum(bytes, 508);
  for (int i = 0; i < 4; ++i) bytes[508 + i] = crc >> (i * 8);
  const auto full = readingstats::checksum(bytes, quest::kProfileBytes - 4);
  for (int i = 0; i < 4; ++i) bytes[quest::kProfileBytes - 4 + i] = full >> (i * 8);
}
}  // namespace
int main() {
  for (int oldId : {0, 1, 2, 7, 4}) {
    initialize();
    assert(questfixture::activateLegacyRequest(p, oldId, stats));
    advance(p.requestGoal, 1);
    p.requestReported = quest::requestTarget(p, oldId) > 1 ? 1 : 0;
    const auto original = p;
    for (int invalid : {-1, quest::kRequestCount, oldId}) {
      assert(!quest::replaceRequest(p, invalid));
      assert(p.activeRequest == original.activeRequest && p.requestStarted == original.requestStarted &&
             p.requestBaseline == original.requestBaseline && p.requestReported == original.requestReported);
    }
    assert(!quest::cancelRequest(p, (oldId + 1) % 300));
    assert(quest::cancelRequest(p, oldId));
    assert(p.activeRequest == quest::kRequestCount && p.requestGoal == quest::RequestGoal::Minutes &&
           !p.requestStarted && !p.requestBaseline && !p.requestReported);
    assert(!quest::requestClaimed(p, oldId) && !quest::cancelRequest(p, oldId));
    assert(p.xp == original.xp && p.bonus == original.bonus && p.campaign.extraCoins == original.campaign.extraCoins);
    assert(questfixture::activateLegacyRequest(p, oldId, stats));
    assert(quest::requestValue(p, oldId, &stats) == 0);
    for (int nextId : {0, 3, 6, 8, 10}) {
      if (quest::guardian(quest::huntEncounter(nextId)) == quest::guardian(quest::huntEncounter(p.activeRequest)))
        continue;
      const auto current = p.activeRequest;
      advance(p.requestGoal, 1);
      assert(quest::replaceRequest(p, nextId));
      assert(p.activeRequest == nextId && p.requestGoal == quest::RequestGoal::Minutes);
      assert(!p.requestReported && quest::requestValue(p, nextId, &stats) == 0);
      assert(!quest::collectRequest(p, current, &stats));
      assert(quest::requestAt(p, false, 0) == nextId);
      quest::encode(p, bytes);
      assert(quest::decode(bytes, restored) && restored.activeRequest == nextId &&
             quest::requestValue(restored, nextId, &stats) == 0);
    }
  }
  initialize();
  assert(quest::acceptRequest(p, 0));
  advance(quest::RequestGoal::Minutes, 5);
  const auto complete = p.campaign;
  p.campaign.encounter = quest::battleAnchor(10);
  p.campaign.phase = quest::BattlePhase::Reading;
  assert(!quest::replaceRequest(p, 25));
  p.campaign = complete;
  assert(p.activeRequest == 0 && quest::requestValue(p, 0, &stats) == 5);
  p.campaign.introducedBooks = 12;
  const int claimedHunt = quest::huntForPath(1);
  p.requestClaims[claimedHunt / 8] |= 1u << (claimedHunt % 8);
  auto repeated = p;
  assert(quest::replaceRequest(repeated, claimedHunt) && repeated.activeRequest == claimedHunt);
  assert(quest::requestValue(repeated, claimedHunt, &stats) == 0 && p.activeRequest == 0);
  quest::StoreWork storeWork;
  bool writable = true;
  testStorage = {};
  assert(quest::save(p, writable, storeWork));
  for (bool replacing : {false, true}) {
    auto candidate = p;
    assert(replacing ? quest::replaceRequest(candidate, quest::huntForPath(2)) : quest::cancelRequest(candidate, 0));
    testStorage.writeLimit = 32;
    assert(!quest::save(candidate, writable, storeWork));
    testStorage.writeLimit = -1;
    assert(quest::load(restored, writable, storeWork));
    assert(restored.activeRequest == 0 && quest::requestValue(restored, 0, &stats) == 5);
  }
  assert(quest::cancelRequest(p, 0));
  assert(quest::save(p, writable, storeWork));
  assert(quest::load(restored, writable, storeWork) && restored.activeRequest == quest::kRequestCount);
  std::puts(
      "Request cancellation/replacement: five goals, fresh baselines, invalid choices, reload and failed-save "
      "preservation passed");
  initialize();
  for (int id = 0; id < quest::kLegacyRequestCount; ++id) {
    initialize();
    const auto historical = quest::requestMetric(p, questfixture::goal(id), &stats);
    assert(quest::requestValue(p, id, &stats) == 0);
    assert(questfixture::activateLegacyRequest(p, id, stats));
    assert(p.requestStarted == p.campaign.observedSeconds && p.requestReported == 0);
    assert(p.requestGoal == quest::RequestGoal::Minutes ? p.requestStarted == historical
                                                        : p.requestBaseline == historical);
    assert(quest::requestAt(p, false, 0) == id);
    assert(!quest::acceptRequest(p, (id + 1) % 300));
    const auto target = quest::requestTarget(p, id);
    const auto goal = p.requestGoal;
    advance(goal, target - 1);
    assert(!quest::collectRequest(p, id, &stats));
    assert(quest::requestValue(p, id, &stats) == target - 1);
    advance(goal, 1);
    const auto before = p;
    questreturn::Report report;
    assert(questreturn::captureRequest(before, p, stats, report));
    assert(report.requestId == id && report.requestAfter == target && report.requestBefore == 0);
    assert(p.requestReported == target);
    questreturn::Report interrupted;
    assert(!questreturn::captureRequest(p, p, stats, interrupted) && interrupted.requestId == -1);
    assert(!questreturn::acknowledgeRequest(p, report));
    quest::encode(p, bytes);
    assert(quest::decode(bytes, restored));
    assert(restored.requestGoal == goal && restored.requestReported == target);
    questreturn::Report repeated;
    assert(!questreturn::captureRequest(p, restored, stats, repeated) && repeated.requestId == -1);
    assert(quest::collectRequest(p, id, &stats));
    assert(!quest::collectRequest(p, id, &stats));
    assert(p.activeRequest == quest::kRequestCount && p.requestGoal == quest::RequestGoal::Minutes &&
           !p.requestReported);
    quest::encode(p, bytes);
    assert(quest::decode(bytes, restored) && quest::requestClaimed(restored, id));
  }
  initialize();
  assert(questfixture::activateLegacyRequest(p, 7, stats));
  const auto before = p;
  stats.completed++;
  auto report = questreturn::capture(before, p);
  assert(report.kind == questreturn::Kind::None);
  assert(questreturn::captureRequest(before, p, stats, report) && report.requestAfter == 1 && report.requestId == 7);
  --stats.completed;
  const auto won = p;
  questreturn::Report reopened;
  assert(!questreturn::captureRequest(won, p, stats, reopened));
  assert(quest::requestValue(p, 7, &stats) == 1 && !quest::cancelRequest(p, 7, &stats));
  assert(quest::collectRequest(p, 7, &stats) && quest::acceptRequest(p, 6));
  initialize();
  assert(quest::acceptRequest(p, 0));
  const auto readingBefore = p;
  advance(quest::RequestGoal::Minutes, 5);
  auto both = questreturn::capture(readingBefore, p);
  assert(both.kind == questreturn::Kind::Complete);
  assert(!questreturn::captureRequest(readingBefore, p, stats, both) && both.requestId == 0);
  assert(both.seconds == 300 && both.requestAfter == 5 && both.requestTarget == quest::requestTarget(readingBefore, 0));
  initialize();
  p.campaign.encounter = quest::battleAnchor(1);
  p.campaign.narratedEncounters = p.campaign.encounter;
  p.campaign.introducedBooks = 1;
  p.campaign.phase = quest::BattlePhase::Reading;
  p.campaign.state = quest::AdventureState::Active;
  assert(quest::prepareBattle(p) && quest::acceptRequest(p, 0));
  const auto fighting = p;
  stats.seconds += std::max(quest::encounterTarget(p), quest::requestMinutes(0) * 60);
  assert(quest::syncCampaign(p, stats, stats.anchorDay));
  assert(quest::collectBattleResult(p));
  auto combined = questreturn::capture(fighting, p);
  assert(questreturn::captureRequest(fighting, p, stats, combined));
  assert(combined.victory && combined.requestId == 0 && combined.requestAfter == combined.requestTarget &&
         !questreturn::huntOnly(combined));
  assert(quest::advanceBattle(p) && quest::collectRequest(p, 0) && !quest::collectRequest(p, 0));
  for (int id : {1, 2, 7, 4}) {
    initialize();
    assert(questfixture::activateLegacyRequest(p, id, stats));
    const auto beforeRollback = p;
    switch (p.requestGoal) {
      case quest::RequestGoal::Pages:
        stats.pages = 0;
        break;
      case quest::RequestGoal::Sessions:
        stats.sessions = 0;
        break;
      case quest::RequestGoal::Books:
        stats.completed = 0;
        break;
      case quest::RequestGoal::Days:
        stats.totalDays = 0;
        break;
      case quest::RequestGoal::Minutes:
        assert(false);
        break;
    }
    questreturn::Report corrected;
    assert(questreturn::captureRequest(beforeRollback, p, stats, corrected));
    assert(!p.requestBaseline && !p.requestReported && corrected.requestId == -1);
    quest::encode(p, bytes);
    assert(quest::decode(bytes, restored));
    p = restored;
    advance(p.requestGoal, quest::requestTarget(p, id));
    assert(quest::collectRequest(p, id, &stats));
  }
  initialize();
  p.activeRequest = 1;
  p.requestTargetMinutes = quest::huntMinutes(p, 1);
  p.requestGoal = quest::RequestGoal::Minutes;
  p.requestStarted = p.campaign.observedSeconds - 300;
  quest::encode(p, bytes);
  std::memset(bytes + 500, 0, 8);
  checksum();
  assert(quest::decode(bytes, restored));
  assert(restored.requestGoal == quest::RequestGoal::Minutes && quest::requestValue(restored, 1, &stats) == 5);
  assert(quest::cancelRequest(restored, 1) && quest::acceptRequest(restored, 0));
  assert(restored.requestGoal == quest::RequestGoal::Minutes && quest::requestValue(restored, 0, &stats) == 0);
  assert(quest::requestTarget(restored, 0) == quest::huntMinutes(restored, 0));
  bytes[500] = 5;
  bytes[507] = 0x80;
  checksum();
  assert(!quest::decode(bytes, restored));
  initialize();
  for (int id = 0; id < quest::kRequestCount; ++id) {
    assert(quest::requestTarget(p, id) == quest::huntMinutes(p, id));
    if (id != quest::huntForPath(quest::requestChapter(id))) continue;
    assert(quest::acceptRequest(p, id));
    assert(p.requestGoal == quest::RequestGoal::Minutes && quest::requestValue(p, id) == 0);
    advance(quest::RequestGoal::Minutes, quest::requestMinutes(id));
    assert(quest::collectRequest(p, id) && !quest::collectRequest(p, id));
  }
  initialize();
  for (unsigned level : {1u, 2u, 10u, 50u, 99u, 100u}) {
    p.xp = quest::threshold(level);
    for (int slot = 0; slot < quest::kCategoryCount; ++slot) {
      const auto category = quest::Slot(slot);
      int count = 0;
      for (int n = 0; n < quest::count(category); ++n) {
        const int id = quest::catalogItem(category, n);
        if (!quest::shopUnlocked(p, id) || quest::owns(p, id)) continue;
        assert(quest::shopItem(p, category, count++) == id);
      }
      assert(count == quest::shopCount(p, category));
      assert(quest::shopItem(p, category, count) == quest::kGearCount);
    }
  }
  std::printf(
      "Request goals, 300 claim/reload cycles, legacy saves, return reports and shop level gates passed; Profile=%zu "
      "Report=%zu\n",
      sizeof(p), sizeof(questreturn::Report));
}

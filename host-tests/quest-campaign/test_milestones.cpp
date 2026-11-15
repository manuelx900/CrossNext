#include <HalStorage.h>

#include <array>
#include <cassert>
#include <cstdio>

#include "../../src/apps_local/quest/QuestMilestones.h"
#include "../../src/apps_local/quest/QuestReadingReturn.h"

int main() {
  std::array<uint8_t, quest::kProfileBytes> bytes{};
  for (unsigned n = 0; n < 3; ++n) {
    quest::Profile p, restored;
    p.xp = quest::threshold(questmilestone::LEVELS[n]) - 1;
    p.celebratedMilestones = uint8_t((1u << n) - 1);
    p.campaign.initialized = p.campaign.prologueSeen = true;
    p.campaign.state = quest::AdventureState::Active;
    p.campaign.encounter = 719;
    p.campaign.phase = quest::BattlePhase::Reading;
    p.campaign.introducedBooks = 12;
    p.campaign.narratedEncounters = 720;
    p.endingSeen = false;
    p.noticedLevel = quest::level(p.xp);
    assert(questmilestone::pending(p) == -1);
    readingstats::Summary summary;
    summary.seconds = quest::encounterTarget(p);
    assert(quest::syncCampaign(p, summary, 20000));
    assert(questmilestone::pending(p) == -1);
    assert(quest::collectBattleResult(p));
    assert(questmilestone::pending(p) == int(n));
    quest::acknowledgeLevel(p);
    assert(questmilestone::pending(p) == int(n));
    quest::encode(p, bytes.data());
    assert(quest::decode(bytes.data(), restored) && questmilestone::pending(restored) == int(n));
    testStorage = TestStorage{};
    quest::StoreWork work;
    assert(quest::save(p, true, work));
    auto draft = p;
    assert(questmilestone::acknowledge(draft));
    testStorage.writeLimit = 0;
    assert(!quest::save(draft, true, work));
    assert(questmilestone::pending(p) == int(n));
    testStorage.writeLimit = -1;
    bool writable = true;
    assert(quest::load(restored, writable, work) && questmilestone::pending(restored) == int(n));
    draft = p;
    assert(questmilestone::acknowledge(draft) && quest::save(draft, true, work));
    assert(quest::load(restored, writable, work) && questmilestone::pending(restored) == -1);
    quest::encode(p, bytes.data());
    bytes[7110] = 2;
    const auto crc = readingstats::checksum(bytes.data(), quest::kProfileBytes - 4);
    for (unsigned b = 0; b < 4; ++b) bytes[quest::kProfileBytes - 4 + b] = crc >> (8 * b);
    assert(quest::decode(bytes.data(), restored) && questmilestone::pending(restored) == -1);
  }
  quest::Profile p;
  p.xp = quest::threshold(150);
  for (int n = 0; n < 3; ++n) {
    assert(questmilestone::pending(p) == n);
    assert(questmilestone::acknowledge(p));
  }
  assert(!questmilestone::acknowledge(p));
  p.xp = 0;
  quest::encode(p, bytes.data());
  quest::Profile decoded;
  assert(!quest::decode(bytes.data(), decoded));
  questreturn::Report report;
  report.victory = true;
  p.campaign.phase = quest::BattlePhase::Story;
  p.campaign.encounter = quest::battleAnchor(0);
  assert(questreturn::nextPanel(questreturn::Panel::Reading, report, p) == questreturn::Panel::Reading);
  report.requestId = 0;
  assert(questreturn::nextPanel(questreturn::Panel::Reading, report, p) == questreturn::Panel::Request);
  assert(questreturn::nextPanel(questreturn::Panel::Request, report, p) == questreturn::Panel::Request);
  p.campaign.encounter = quest::battleAnchor(9);
  assert(questreturn::nextPanel(questreturn::Panel::Reading, report, p) == questreturn::Panel::Victory);
  assert(questreturn::nextPanel(questreturn::Panel::Victory, report, p) == questreturn::Panel::Request);
  std::printf(
      "Milestones: crossings, reopen, failed save, migration, ordered queue and return routing passed; Profile=%zu "
      "StoreWork=%zu\n",
      sizeof(p), sizeof(quest::StoreWork));
}

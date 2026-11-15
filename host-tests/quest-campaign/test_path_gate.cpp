#include <cassert>
#include <cstdio>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestReadingReturn.h"
int main() {
  quest::Profile p;
  readingstats::Summary summary;
  summary.anchorDay = 20000;
  assert(quest::syncCampaign(p, summary, 20000));
  assert(quest::acknowledgePrologue(p));
  quest::normalizeBattles(p);
  assert(quest::needsEncounterStory(p) && !quest::settleBattle(p));
  uint64_t expected = 0;
  for (unsigned chapter = 0; chapter < 120; ++chapter) {
    auto sample = p;
    sample.campaign.encounter = quest::battleAnchor(chapter);
    expected += quest::encounterTarget(sample) / 60;
  }
  assert(quest::campaignMinutes(p).read == 0 && quest::campaignMinutes(p).target == expected);
  uint8_t bytes[quest::kProfileBytes];
  quest::encode(p, bytes);
  quest::Profile copy;
  assert(quest::decode(bytes, copy) && quest::needsEncounterStory(copy));
  assert(quest::prepareBattle(p) && !quest::needsEncounterStory(p));
  summary.seconds = quest::encounterTarget(p) / 2;
  auto before = p;
  assert(quest::syncCampaign(p, summary, 20000));
  assert(p.campaign.phase == quest::BattlePhase::Reading);
  assert(quest::campaignMinutes(p).target == expected && quest::campaignMinutes(p).read == summary.seconds / 60);
  summary.seconds = quest::encounterTarget(p) + 1200;
  before = p;
  assert(quest::syncCampaign(p, summary, 20000));
  assert(quest::collectBattleResult(p));
  const auto report = questreturn::capture(before, p);
  assert(report.victory && report.after == report.target);
  assert(quest::campaignMinutes(p).target == expected);
  assert(quest::advanceBattle(p) && quest::needsEncounterStory(p));
  assert(p.campaign.encounterSeconds == 0 && p.campaign.phase == quest::BattlePhase::Reading);
  const auto reserve = p.campaign.bankSeconds;
  assert(reserve > 0);
  auto waiting = p;
  waiting.campaign.encounterSeconds = quest::encounterTarget(waiting);
  assert(!quest::settleBattle(waiting));
  assert(quest::campaignMinutes(p).target == expected);
  quest::encode(p, bytes);
  assert(quest::decode(bytes, copy) && quest::needsEncounterStory(copy));
  assert(quest::prepareBattle(p) && p.campaign.bankSeconds == reserve && p.campaign.encounterSeconds == 0);
  quest::encode(p, bytes);
  assert(quest::decode(bytes, copy) && !quest::needsEncounterStory(copy));
  assert(!quest::syncCampaign(p, summary, 20000));
  summary.seconds += 60;
  assert(quest::syncCampaign(p, summary, 20000));
  assert(p.campaign.phase == quest::BattlePhase::Victory);
  p.campaign.observedSeconds = UINT64_MAX;
  p.campaign.originSeconds = 0;
  p.campaign.bankSeconds = UINT64_MAX;
  const auto huge = quest::campaignMinutes(p);
  assert(huge.target >= huge.read && huge.target < UINT64_MAX);
  p.campaign.state = quest::AdventureState::Peace;
  p.campaign.phase = quest::BattlePhase::Complete;
  const auto peace = quest::campaignMinutes(p);
  assert(peace.read == peace.target);
  std::puts(
      "Path gate/counters passed: explicit selection, reload, reserve, partial/victory accounting, regional gear "
      "budgets, zero returns and uint64 bounds");
}

#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestJourney.h"
#include "../../src/apps_local/quest/QuestReadingReturn.h"
#include "../study-suite/stubs/HalStorage.h"

namespace {
void checksum(uint8_t* bytes, int at) {
  const auto crc = readingstats::checksum(bytes, at);
  for (int n = 0; n < 4; ++n) bytes[at + n] = crc >> (n * 8);
}
}  // namespace
void testJourney() {
  quest::Profile p;
  p.campaign.initialized = p.campaign.prologueSeen = true;
  for (unsigned chapter = 0; chapter < quest::CAMPAIGN_CHAPTERS; ++chapter) {
    p.campaign.encounter = quest::battleAnchor(chapter);
    p.campaign.introducedBooks = chapter / 10 + 1;
    p.campaign.narratedEncounters = p.campaign.encounter + 1;
    assert(questjourney::kind(p.campaign.encounter) == questjourney::Kind::Combat);
    for (const auto phase : {quest::BattlePhase::Reading, quest::BattlePhase::Victory, quest::BattlePhase::Story}) {
      p.campaign.phase = phase;
      const auto context = questjourney::context(p);
      assert(!context.prologue && context.chapter == chapter);
      assert(context.beat == (phase == quest::BattlePhase::Story ? 3u : 0u));
      assert(questjourney::rememberedChapters(p) == chapter);
      assert(!questjourney::canVisit(p, chapter));
      if (chapter) assert(questjourney::canVisit(p, chapter - 1));
    }
  }
  std::puts("Journey:120 battle episodes, earned endings and atlas boundaries passed");
}
void testStoryRewards() {
  quest::Profile p, loaded;
  quest::StoreWork work;
  p.xp = 50000;
  p.noticedLevel = quest::level(p.xp);
  p.campaign.encounter = 11;
  p.campaign.phase = quest::BattlePhase::Victory;
  p.campaign.victoryCoins = 100;
  assert(!quest::equip(p, 200) && !quest::owns(p, 200));
  assert(quest::collectBattle(p) && !quest::owns(p, 200) && !quest::visibleItem(p.campaign.pendingReward));
  // A save from before saga rewards may still have this ornament to acknowledge.
  assert(quest::grantItem(p, 200));
  p.campaign.pendingReward = 200;
  const auto xp = p.xp, wallet = quest::coins(p);
  assert(!quest::collectBattle(p) && !quest::continueCampaign(p));
  quest::encode(p, work.bytes);
  assert(quest::decode(work.bytes, loaded) && loaded.campaign.pendingReward == 200);
  testStorage.files.clear();
  assert(quest::save(p, true, work));
  loaded = p;
  assert(quest::dismissStoryReward(loaded, true) && quest::equipped(loaded, 200));
  testStorage.writeLimit = 10;
  assert(!quest::save(loaded, true, work));
  testStorage.writeLimit = -1;
  bool writable = false;
  assert(quest::load(loaded, writable, work) && writable && loaded.campaign.pendingReward == 200);
  assert(loaded.xp == xp && quest::coins(loaded) == wallet && !quest::equipped(loaded, 200));
  assert(quest::dismissStoryReward(loaded, true) && quest::save(loaded, true, work));
  assert(quest::load(p, writable, work) && p.campaign.pendingReward == quest::kGearCount);
  assert(quest::equipped(p, 200));
  assert(!quest::dismissStoryReward(p, true) && p.xp == xp && quest::coins(p) == wallet);
  assert(quest::equip(p, 212));
  const auto spent = p.spent;
  assert(!quest::equipped(p, 200) && quest::equipped(p, 212));
  assert(quest::grantItem(p, 203) && quest::equip(p, 203));
  assert(!quest::equipped(p, 200) && quest::owns(p, 200) && !quest::equipped(p, 212) && quest::equipped(p, 203) &&
         p.spent == spent);
  assert(quest::removeItem(p, 203) && !quest::equipped(p, 203) && !quest::equipped(p, 212));
  assert(quest::equip(p, 200));
  quest::encode(p, work.bytes);
  assert(quest::decode(work.bytes, loaded) && loaded.decorations[0] == 200 &&
         loaded.decorations[1] == quest::kGearCount);
  work.bytes[407] = 212;
  work.bytes[408] = 0;
  checksum(work.bytes, 508);
  assert(!quest::decode(work.bytes, loaded));
  // Old records retain wallet, purchases and optional empty slots; earned items migrate without popups.
  p.campaign.encounter = 119;
  p.campaign.phase = quest::BattlePhase::Story;
  p.equipped[1] = p.equipped[2] = quest::kGearCount;
  std::strcpy(p.name, "MILO");
  quest::encode(p, work.bytes);
  work.bytes[3] = '6';
  work.bytes[401] = 2;
  checksum(work.bytes, 508);
  assert(quest::decode(work.bytes, loaded));
  assert(loaded.spent == p.spent && loaded.xp == p.xp && !std::strcmp(loaded.name, "MILO"));
  assert(loaded.equipped[1] == quest::kGearCount && loaded.equipped[2] == quest::kGearCount);
  assert(loaded.decorationOwned == 3 && loaded.decorations[0] == quest::kGearCount &&
         loaded.decorations[1] == quest::kGearCount);
  assert(loaded.campaign.pendingReward == quest::kGearCount);
  assert(quest::owns(loaded, 200));
  assert(quest::shopUnlocked(loaded, quest::chapterReward(0)));
  // The two decorations join the same caps, and the equipped PET still helps in hero-only battles.
  p = quest::Profile{};
  const auto noDecor = quest::bonuses(p);
  quest::grantItem(p, 200);
  quest::equip(p, 200);
  quest::grantItem(p, 201);
  quest::equip(p, 201);
  const auto decor = quest::bonuses(p);
  assert(decor.power == noDecor.power + quest::gearBonus(201, 0).power && decor.experience == noDecor.experience);
  assert(decor.power > noDecor.power && decor.experience <= questbalance::XP_CAP &&
         decor.fortune <= questbalance::COIN_CAP);
  assert(quest::removeItem(p, 156) && quest::bonuses(p).power + quest::gearBonus(156, 0).power == decor.power);
  p.campaign.encounter = 0;
  p.campaign.phase = quest::BattlePhase::Story;
  assert(quest::nextStoryReward(p) == quest::chapterReward(0));
  p.campaign.encounter = 5;
  assert(!quest::visibleItem(quest::nextStoryReward(p)));
  std::puts(
      "Story rewards: once-only delivery, pending reward recovery, one active decoration, shared caps, old-save "
      "migration and "
      "failed writes passed");
}
void testReadingReturn() {
  quest::Profile p;
  p.campaign.initialized = true;
  p.campaign.prologueSeen = true;
  p.campaign.introducedBooks = 1;
  p.campaign.narratedEncounters = 1;
  p.campaign.observedSeconds = 100;
  readingstats::Summary summary;
  summary.seconds = 130;
  auto after = p;
  assert(quest::syncCampaign(after, summary, 20000));
  auto report = questreturn::capture(p, after);
  assert(report.kind == questreturn::Kind::Combat && report.seconds == 30 && report.after - report.before == 30);
  assert(report.xp == after.xp - p.xp && !report.victory);
  assert(!quest::syncCampaign(after, summary, 20000));
  assert(questreturn::capture(after, after).kind == questreturn::Kind::None);
  questreturn::Clock clock;
  clock.start(true);
  assert(!clock.advance(100000));
  clock.painted(UINT32_MAX - 99, questreturn::Phase::Swing);
  assert(!clock.advance(100000));
  clock.painted(UINT32_MAX - 99, questreturn::Phase::Raise);
  assert(!clock.advance(549) && clock.advance(550));
  assert(clock.phase() == questreturn::Phase::Swing && !clock.advance(100000));
  clock.painted(600, questreturn::Phase::Swing);
  assert(clock.advance(1250) && clock.phase() == questreturn::Phase::Impact);
  clock.painted(1300, questreturn::Phase::Impact);
  assert(clock.advance(1950) && !clock.playing());
  clock.cancel();
  assert(!clock.advance(100000));
  clock.start(false);
  assert(clock.phase() == questreturn::Phase::Result && !clock.playing());
  p.campaign.narratedEncounters = 0;
  after = p;
  assert(quest::syncCampaign(after, summary, 20000));
  report = questreturn::capture(p, after);
  assert(report.kind == questreturn::Kind::Logged && report.seconds == 30 && report.after == report.before);
  p.campaign.encounter = 719;
  p.campaign.phase = quest::BattlePhase::Complete;
  after = p;
  assert(quest::syncCampaign(after, summary, 20000));
  assert(questreturn::capture(p, after).kind == questreturn::Kind::Complete);
  p.campaign.initialized = false;
  assert(questreturn::capture(p, after).kind == questreturn::Kind::None);
  p.campaign.initialized = true;
  p.campaign.state = quest::AdventureState::Peace;
  assert(questreturn::capture(p, after).kind == questreturn::Kind::Complete);
}
int main() {
  testJourney();
  testStoryRewards();
  testReadingReturn();
  constexpr int today = 20000;
  quest::Profile p, loaded;
  readingstats::Summary summary;
  summary.anchorDay = today;
  assert(quest::syncCampaign(p, summary, today));
  assert(!quest::prepareBattle(p));
  assert(quest::acknowledgePrologue(p) && quest::prepareBattle(p));
  uint32_t previous = 0;
  for (unsigned chapter = 0; chapter < quest::CAMPAIGN_CHAPTERS; ++chapter) {
    assert(p.campaign.encounter == quest::battleAnchor(chapter));
    assert(p.campaign.phase == quest::BattlePhase::Reading && !quest::needsEncounterStory(p));
    assert(!quest::needsIntroduction(p) && quest::book(p) == chapter / 10);
    assert(p.campaign.encounterSeconds == 0);
    const uint64_t reserve = p.campaign.bankSeconds;
    assert(quest::prepareBattle(p) && p.campaign.bankSeconds == reserve);
    const auto before = p;
    summary.seconds += quest::encounterTarget(p) + 1200;
    assert(quest::syncCampaign(p, summary, today));
    assert(p.campaign.phase == quest::BattlePhase::Victory);
    assert(quest::collectBattleResult(p));
    const auto report = questreturn::capture(before, p);
    assert(report.kind == questreturn::Kind::Combat && report.victory);
    assert(report.xp == p.xp - before.xp && report.seconds > 0);
    assert(p.campaign.pendingReward == quest::kGearCount);
    for (auto category : {quest::Slot::Body, quest::Slot::Background, quest::Slot::Pet, quest::Slot::Decoration})
      if (quest::lastPathInSaga(chapter)) assert(quest::owns(p, quest::sagaReward(chapter / 10, category)));
    for (unsigned n = 0; n < quest::campaignUnlocks(chapter).count; ++n)
      assert(quest::shopUnlocked(p, quest::campaignUnlocks(chapter).items[n]));
    assert(p.equipped[0] == before.equipped[0] && p.equipped[1] == before.equipped[1] &&
           p.equipped[2] == before.equipped[2]);
    const auto xp = p.xp, money = quest::coins(p);
    assert(!quest::collectBattleResult(p) && p.xp == xp && quest::coins(p) == money);
    uint8_t encoded[quest::kProfileBytes];
    quest::encode(p, encoded);
    assert(quest::decode(encoded, loaded));
    assert(quest::campaignProgress(p) > previous);
    previous = quest::campaignProgress(p);
    const auto bank = p.campaign.bankSeconds;
    assert(quest::advanceBattle(p));
    assert(chapter == 119 || (p.campaign.phase == quest::BattlePhase::Reading && p.campaign.bankSeconds == bank));
    assert(!quest::syncCampaign(p, summary, today));
    assert(questreturn::capture(p, p).kind == questreturn::Kind::None);
    if (chapter < 119) {
      assert(quest::needsEncounterStory(p));
      auto unselected = p;
      unselected.campaign.encounterSeconds = quest::encounterTarget(unselected);
      assert(!quest::settleBattle(unselected));
      assert(quest::prepareBattle(p) && !quest::needsEncounterStory(p));
    }
  }
  assert(p.campaign.phase == quest::BattlePhase::Complete && p.campaign.state == quest::AdventureState::Peace);
  for (int item : {quest::kKingBody, quest::kKingPet, quest::kKingBackground, quest::kKingDecoration})
    assert(quest::owns(p, item));
  const auto xp = p.xp;
  summary.seconds += 1200;
  assert(quest::syncCampaign(p, summary, today) && p.xp == xp);
  assert(!quest::advanceBattle(p));
  // Every legacy slot migrates within its source chapter, preserving reading and inventory.
  for (unsigned legacy = 0; legacy < quest::CAMPAIGN_ENCOUNTERS; ++legacy) {
    quest::Profile old;
    old.campaign.initialized = old.campaign.prologueSeen = true;
    old.campaign.encounter = legacy;
    old.campaign.narratedEncounters = legacy + 1;
    old.campaign.introducedBooks = legacy / 60 + 1;
    old.campaign.encounterSeconds = 5000;
    old.campaign.bankSeconds = 700;
    old.xp = 9000;
    const auto equipped = old.equipped[0];
    quest::normalizeBattles(old);
    assert(quest::chapter(old) == legacy / 6 && old.campaign.encounter == quest::battleAnchor(legacy / 6));
    assert(old.campaign.encounterSeconds <= 5000 && old.campaign.bankSeconds <= 700);
    assert(old.xp == 9000 && old.equipped[0] == equipped && !quest::needsEncounterStory(old));
    assert(!quest::normalizeBattles(old));
  }
  // A short SD write cannot expose a partially credited victory.
  quest::StoreWork work;
  testStorage.files.clear();
  assert(quest::save(p, true, work));
  const auto sequence = p.sequence;
  testStorage.writeLimit = 10;
  assert(!quest::save(p, true, work) && p.sequence == sequence);
  testStorage.writeLimit = -1;
  bool writable = false;
  assert(quest::load(loaded, writable, work) && writable && loaded.xp == p.xp);
  std::printf(
      "Campaign:120 battles,720 legacy migrations, bank on reading only, automatic gifts, no double reward, continued "
      "reading and transactional save passed; Profile=%zu bytes\n",
      sizeof(p));
}

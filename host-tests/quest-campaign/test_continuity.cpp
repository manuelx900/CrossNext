#include <cassert>
#include <cstdio>

#include "../../src/apps_local/quest/QuestReadingReturn.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "../../src/apps_local/quest/QuestScenePlan.h"

int main() {
  quest::Profile p;
  p.campaign.prologueSeen = p.campaign.initialized = true;
  constexpr unsigned versions[] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 4};
  for (unsigned path = 0; path < quest::CAMPAIGN_CHAPTERS; ++path) {
    const unsigned book = path / quest::CHAPTERS_PER_BOOK;
    const int id = quest::huntForPath(path);
    const bool first = quest::requestChapter(id) == path;
    p.campaign.encounter = quest::battleAnchor(path);
    for (const auto phase : {quest::BattlePhase::Reading, quest::BattlePhase::Victory, quest::BattlePhase::Story}) {
      p.campaign.phase = phase;
      const bool won = phase == quest::BattlePhase::Story;
      assert(quest::completedPaths(p) == path + won);
      assert(questscene::sagaVersion(p, book) == versions[path % quest::CHAPTERS_PER_BOOK + won]);
      assert(quest::requestUnlocked(p, id) == (won || !first));
      if (book) assert(questscene::sagaVersion(p, book - 1) == 4);
      if (book + 1 < quest::CAMPAIGN_BOOKS) assert(questscene::sagaVersion(p, book + 1) == 0);
    }
  }
  assert(quest::huntForPath(0) == quest::huntForPath(5));
  assert(quest::huntEncounter(quest::kShadowHunt) == quest::battleAnchor(118));
  assert(quest::huntEncounter(quest::kKingHunt) == quest::battleAnchor(119));
  assert(quest::huntEncounter(299) == quest::battleAnchor(117));
  p.campaign.phase = quest::BattlePhase::Complete;
  p.campaign.state = quest::AdventureState::Peace;
  p.campaign.introducedBooks = quest::CAMPAIGN_BOOKS;
  p.requestClaims[299 / 8] |= 1u << (299 % 8);
  assert(quest::requestUnlocked(p, quest::kKingHunt) && quest::requestUnlocked(p, quest::kShadowHunt));
  assert(quest::requestUnlocked(p, quest::huntForPath(117)));
  assert(quest::requestDecoration(quest::huntForPath(2)) == quest::requestDecoration(6));
  assert(quest::visibleItem(quest::requestDecoration(6)));
  assert(quest::requestDecoration(5) == quest::kGearCount);
  assert(quest::acceptRequest(p, quest::kKingHunt));
  p.campaign.observedSeconds += quest::requestMinutes(quest::kKingHunt) * 60;
  assert(!quest::cancelRequest(p, quest::kKingHunt) && !quest::replaceRequest(p, quest::kShadowHunt));
  assert(quest::collectRequest(p, quest::kKingHunt));
  assert(quest::acceptRequest(p, quest::kKingHunt));
  assert(quest::cancelRequest(p, quest::kKingHunt));
  assert(quest::requestCount(p, true) == 2 && quest::requestAt(p, true, 0) == quest::kKingHunt);
  uint8_t bytes[quest::kProfileBytes];
  quest::Profile restored;
  quest::encode(p, bytes);
  assert(quest::decode(bytes, restored));
  assert(quest::requestClaimed(restored, 299) && quest::requestClaimed(restored, quest::kKingHunt));
  assert(quest::acceptRequest(restored, quest::kKingHunt));
  assert(quest::cancelRequest(restored, quest::kKingHunt));
  for (unsigned book = 0; book < quest::CAMPAIGN_BOOKS; ++book) {
    quest::Profile closing;
    closing.campaign.prologueSeen = closing.campaign.initialized = true;
    closing.campaign.introducedBooks = book + 1;
    const unsigned chapter = (book + 1) * quest::CHAPTERS_PER_BOOK - 1;
    closing.campaign.encounter = quest::battleAnchor(chapter);
    closing.campaign.narratedEncounters = closing.campaign.encounter + 1;
    closing.campaign.phase = quest::BattlePhase::Victory;
    quest::restoreExpansionRewards(closing);
    for (auto category : {quest::Slot::Body, quest::Slot::Background, quest::Slot::Pet, quest::Slot::Decoration}) {
      const int item = quest::sagaReward(book, category);
      assert(quest::availableItem(item) && quest::slot(item) == category && quest::rewardSaga(item) == int(book));
      assert(quest::rewardEncounter(item) == int(closing.campaign.encounter));
      assert(!quest::owns(closing, item) && quest::storyExclusive(item));
    }
    assert(quest::collectBattleResult(closing));
    for (auto category : {quest::Slot::Body, quest::Slot::Background, quest::Slot::Pet, quest::Slot::Decoration})
      assert(quest::owns(closing, quest::sagaReward(book, category)));
    assert(!quest::collectBattleResult(closing));
    assert(quest::advanceBattle(closing));
    if (book + 1 < quest::CAMPAIGN_BOOKS)
      assert(quest::needsIntroduction(closing) && quest::beginCampaign(closing) && !quest::needsIntroduction(closing));
    else
      assert(quest::hasWon(closing));
  }
  quest::Profile simulated;
  simulated.campaign.initialized = simulated.campaign.prologueSeen = true;
  simulated.campaign.introducedBooks = 1;
  simulated.campaign.encounter = quest::battleAnchor(6);
  simulated.campaign.narratedEncounters = simulated.campaign.encounter + 1;
  const auto target = quest::encounterTarget(simulated);
  assert(target > 120);
  simulated.campaign.bankSeconds = target * 2;
  readingstats::Summary session;
  session.seconds = 120;
  assert(quest::syncCampaign(simulated, session, 20000, {120}));
  assert(simulated.campaign.phase == quest::BattlePhase::Reading && simulated.campaign.encounterSeconds == 120);
  assert(simulated.campaign.bankSeconds == 0);
  session.seconds = target + 60;
  assert(quest::syncCampaign(simulated, session, 20000, {session.seconds - 120}));
  assert(simulated.campaign.phase == quest::BattlePhase::Victory && simulated.campaign.encounterSeconds == target);
  assert(simulated.campaign.bankSeconds == 0);
  quest::Profile logged;
  logged.campaign.initialized = logged.campaign.prologueSeen = true;
  const auto beforeLog = logged;
  session.seconds = 120;
  assert(quest::syncCampaign(logged, session, 20000, {120}));
  assert(questreturn::capture(beforeLog, logged, {120}).kind == questreturn::Kind::Logged);
  assert(logged.campaign.bankSeconds == 0);
  quest::Profile chapterGift;
  chapterGift.campaign.encounter = quest::battleAnchor(1);
  chapterGift.campaign.phase = quest::BattlePhase::Victory;
  assert(quest::collectBattleResult(chapterGift) && !quest::owns(chapterGift, quest::chapterDecoration(1)) &&
         quest::shopUnlocked(chapterGift, quest::chapterDecoration(1)));
  assert(!quest::owns(chapterGift, quest::sagaReward(0, quest::Slot::Decoration)));
  // Version 5 used 300 to mean that no hunt was active.
  bytes[3] = '6';
  bytes[401] = 5;
  bytes[450] &= 0x0f;
  bytes[459] = 300 & 255;
  bytes[460] = 300 >> 8;
  const auto crc = readingstats::checksum(bytes, 508);
  for (int n = 0; n < 4; ++n) bytes[508 + n] = crc >> (8 * n);
  assert(quest::decode(bytes, restored) && restored.activeRequest == quest::kRequestCount);
  assert(quest::requestClaimed(restored, 299) && quest::requestUnlocked(restored, quest::kKingHunt));
  readingstats::Summary summary;
  quest::restartAdventure(restored, summary, 20000);
  assert(quest::requestCount(restored, true) == 0 && quest::requestCount(restored, false) == 0);
  quest::Profile hunts;
  hunts.campaign.initialized = hunts.campaign.prologueSeen = true;
  hunts.campaign.encounter = quest::battleAnchor(25);
  hunts.campaign.introducedBooks = 3;
  hunts.campaign.observedSeconds = 10000;
  for (unsigned family = 0; family < 11; ++family) {
    const int id = quest::HUNT_OUTFIT_SOURCES[family];
    hunts.requestClaims[id / 8] |= 1u << (id % 8);
    assert(quest::owns(hunts, quest::huntOutfitItem(id)));
  }
  assert(quest::requestUnlocked(hunts, quest::kStormHunt));
  assert(quest::requestUnlocked(hunts, quest::kCrystalHunt));
  assert(quest::requestUnlocked(hunts, quest::kPhoenixHunt));
  assert(!quest::acceptRequest(hunts, quest::kInkHunt));
  assert(quest::acceptRequest(hunts, quest::kStormHunt));
  hunts.campaign.observedSeconds += quest::requestMinutes(quest::kStormHunt) * 60;
  assert(quest::collectRequest(hunts, quest::kStormHunt) && hunts.inkHuntDiscovered);
  assert(quest::requestAt(hunts, false, 0) == quest::kInkHunt);
  assert(quest::equip(hunts, quest::huntOutfitItem(quest::kStormHunt)));
  assert(quest::acceptRequest(hunts, quest::kInkHunt));
  hunts.campaign.observedSeconds += 121;
  assert(quest::replaceRequest(hunts, quest::kCrystalHunt) && hunts.pausedInkSeconds == 121);
  hunts.campaign.observedSeconds += quest::requestTarget(hunts, quest::kCrystalHunt) * 60;
  assert(quest::requestValue(hunts, quest::kInkHunt) == 2);
  assert(quest::collectRequest(hunts, quest::kCrystalHunt));
  for (unsigned n = 0; n < 2; ++n) assert(quest::owns(hunts, quest::huntExtraItem(quest::kCrystalHunt, n)));
  quest::encode(hunts, bytes);
  assert(quest::decode(bytes, restored));
  assert(quest::equipped(restored, quest::huntOutfitItem(quest::kStormHunt)) && restored.pausedInkSeconds == 121);
  assert(quest::acceptRequest(restored, quest::kInkHunt));
  restored.campaign.observedSeconds += quest::INK_HUNT_SECONDS - 121;
  restored.requestReported = quest::requestValue(restored, quest::kInkHunt);
  assert(restored.requestReported == quest::requestTarget(restored, quest::kInkHunt) &&
         !quest::cancelRequest(restored, quest::kInkHunt));
  quest::encode(restored, bytes);
  assert(quest::decode(bytes, hunts) && hunts.requestReported == quest::requestTarget(hunts, quest::kInkHunt));
  assert(quest::collectRequest(hunts, quest::kInkHunt) && quest::equip(hunts, quest::kInkRelic));
  assert(quest::equip(hunts, quest::huntOutfitItem(quest::kInkHunt)));
  assert(quest::acceptRequest(hunts, quest::kInkHunt));
  assert(quest::cancelRequest(hunts, quest::kInkHunt));
  quest::encode(hunts, bytes);
  assert(quest::decode(bytes, restored) && quest::equipped(restored, quest::kInkRelic) &&
         quest::equipped(restored, quest::huntOutfitItem(quest::kInkHunt)));
  const auto position = questscene::encodeBookmark(4, 2, 4, 2);
  assert(questscene::decodeBookmark(position, 4, 2).page == 2);
  assert(questscene::decodeBookmark(position, 4, 3).page == 0);
  std::puts("Progressive saga, hunt unlocks, once-only combat, replay history and old profile migration passed");
}

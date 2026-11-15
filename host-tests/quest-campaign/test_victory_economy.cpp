#include <cassert>
#include <cstdio>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestRequests.h"
static void ready(quest::Profile& p) {
  p.campaign.initialized = p.campaign.prologueSeen = true;
  p.campaign.introducedBooks = 1;
  p.campaign.encounter = quest::battleAnchor(0);
  p.campaign.narratedEncounters = p.campaign.encounter + 1;
}
int main() {
  constexpr int day = 20000;
  static quest::Profile p, loaded;
  static uint8_t bytes[quest::kProfileBytes];
  readingstats::Summary s;
  ready(p);
  const auto required = quest::encounterTarget(p);
  const auto prize = quest::battleXp(p);
  s.seconds = required - 1;
  assert(quest::syncCampaign(p, s, day));
  assert(!p.xp && p.campaign.phase == quest::BattlePhase::Reading);
  s.seconds = 999 * 60;
  assert(quest::syncCampaign(p, s, day));
  assert(!p.xp && p.campaign.bankSeconds == 0);
  assert(quest::collectBattleResult(p));
  assert(p.xp == prize && prize == questbalance::victoryXp(required / 60, quest::bonuses(loaded).experience));
  assert(!quest::collectBattleResult(p) && p.xp == prize);
  const int unlock = quest::chapterReward(0);
  assert(!quest::owns(p, unlock) && quest::shopUnlocked(p, unlock));
  assert(quest::advanceBattle(p) && quest::prepareBattle(p));
  s.seconds += 1;
  assert(quest::syncCampaign(p, s, day));
  assert(p.campaign.encounterSeconds == 1 && p.xp == prize);
  p.campaign.extraCoins = 100000;
  assert(quest::equip(p, unlock));
  assert(quest::owns(p, unlock));
  p = quest::Profile{};
  ready(p);
  p.campaign.encounter = quest::battleAnchor(9);
  p.campaign.narratedEncounters = p.campaign.encounter + 1;
  s.seconds = quest::encounterTarget(p);
  assert(quest::syncCampaign(p, s, day));
  assert(quest::collectBattleResult(p));
  const int outfit = quest::sagaReward(0, quest::Slot::Body);
  assert(quest::nextOutfitOffer(p) == outfit);
  quest::encode(p, bytes);
  assert(quest::decode(bytes, loaded) && quest::nextOutfitOffer(loaded) == outfit);
  assert(quest::finishOutfitOffer(loaded, outfit, true) && quest::equipped(loaded, outfit));
  assert(quest::nextOutfitOffer(loaded) == quest::kGearCount);
  p = quest::Profile{};
  ready(p);
  p.campaign.phase = quest::BattlePhase::Story;
  assert(quest::acceptRequest(p, 0));
  const auto huntTarget = quest::requestTarget(p, 0);
  const auto huntPrize = quest::requestXp(p, 0);
  s.seconds = 999 * 60;
  assert(quest::syncCampaign(p, s, day));
  assert(!p.xp);
  assert(quest::collectRequest(p, 0, &s, day));
  assert(p.xp == huntPrize &&
         huntPrize == questbalance::victoryXp(huntTarget, quest::bonuses(quest::Profile{}).experience));
  assert(quest::nextOutfitOffer(p) == quest::huntOutfitItem(0));
  assert(!quest::collectRequest(p, 0, &s, day));
  p = quest::Profile{};
  ready(p);
  p.campaign.phase = quest::BattlePhase::Story;
  p.xp = quest::threshold(2) - 1;
  p.activeRequest = 1;
  p.requestGoal = quest::RequestGoal::Pages;
  p.requestTargetMinutes = 31;
  s = readingstats::Summary{};
  s.pages = quest::requestTarget(p, 1);
  const auto legacyXp = p.xp;
  const auto legacyPrize = quest::requestXp(p, 1);
  assert(quest::collectRequest(p, 1, &s, day));
  assert(p.xp - legacyXp == legacyPrize &&
         p.victories[quest::huntOutfitIndex(quest::huntOutfitItem(1))].xp == legacyPrize);
  p = quest::Profile{};
  ready(p);
  p.xp = quest::threshold(10);
  p.campaign.encounter = quest::battleAnchor(5);
  p.campaign.narratedEncounters = p.campaign.encounter + 1;
  const auto oldTarget = quest::encounterTarget(p);
  p.campaign.encounterSeconds = oldTarget - 1;
  p.requestClaims[quest::kStormHunt / 8] |= 1u << (quest::kStormHunt % 8);
  quest::noteAcquired(p, quest::huntOutfitItem(quest::kStormHunt));
  quest::offerOutfit(p, quest::huntOutfitItem(quest::kStormHunt));
  assert(quest::finishOutfitOffer(p, quest::huntOutfitItem(quest::kStormHunt), true));
  assert(quest::encounterTarget(p) < oldTarget && quest::settleBattle(p));
  assert(p.campaign.phase == quest::BattlePhase::Victory);
  p = quest::Profile{};
  assert(quest::defeatedBeasts(p) == 0);
  p.campaign.encounter = quest::battleAnchor(2);
  p.campaign.phase = quest::BattlePhase::Story;
  assert(quest::defeatedBeasts(p) == 3);
  p.requestClaims[0] = 3;
  assert(quest::defeatedBeasts(p) == 4);  // duplicate legacy claims share one Hunt identity
  p.campaign.encounter = quest::battleAnchor(119);
  p.campaign.phase = quest::BattlePhase::Complete;
  for (int id = 0; id < quest::kRequestCount; ++id) p.requestClaims[id / 8] |= 1u << (id % 8);
  assert(quest::defeatedBeasts(p) == quest::CAMPAIGN_CHAPTERS + quest::kHuntOutfitCount);
  std::puts(
      "Victory economy: partial=0XP, 999-minute cap, once-only rewards, path purchase gate, saga outfit offer and hunt "
      "outfit offer passed");
}

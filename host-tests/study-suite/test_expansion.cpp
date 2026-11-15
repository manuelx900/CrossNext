#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestExpansionPolicy.h"
#include "../../src/apps_local/quest/QuestReadingReturn.h"
#include "../../src/apps_local/quest/QuestRequests.h"

static quest::Profile legacy(const char* name) {
  char path[160];
  std::snprintf(path, sizeof(path), "host-tests/quest-campaign/fixtures/schema4-%s.bin", name);
  auto* file = std::fopen(path, "rb");
  assert(file);
  uint8_t bytes[512];
  assert(std::fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
  std::fclose(file);
  assert(bytes[401] == 4);
  quest::Profile profile;
  assert(quest::decode(bytes, profile));
  return profile;
}
static void roundTrip(const quest::Profile& p) {
  uint8_t bytes[quest::kProfileBytes];
  quest::encode(p, bytes);
  assert(bytes[401] == 8);
  quest::Profile copy;
  assert(quest::decode(bytes, copy));
  assert(!std::memcmp(p.expansionOwned, copy.expansionOwned, sizeof(p.expansionOwned)));
  assert(!std::memcmp(p.equipped, copy.equipped, sizeof(p.equipped)));
  assert(!std::memcmp(p.decorations, copy.decorations, sizeof(p.decorations)));
  assert(copy.xp == p.xp && copy.spent == p.spent && copy.campaign.extraCoins == p.campaign.extraCoins);
  assert(copy.endingSeen == p.endingSeen);
}
int main() {
  auto active = legacy("active");
  assert(active.activeRequest == 0 && active.requestStarted == 4000);
  assert(active.equipped[1] == quest::kGearCount && active.equipped[2] == quest::kGearCount);
  assert(active.decorations[0] == quest::kGearCount && active.decorations[1] == quest::kGearCount);
  const auto xp = active.xp, coins = active.campaign.extraCoins;
  const auto rewardXp = quest::requestXp(active, 0), rewardCoins = quest::requestCoins(active, 0);
  assert(quest::collectRequest(active, 0));
  assert(quest::owns(active, quest::requestDecoration(0)));
  assert(active.xp == xp + rewardXp && active.campaign.extraCoins == coins + rewardCoins);
  assert(!quest::collectRequest(active, 0));
  roundTrip(active);

  auto peace = legacy("peace");
  assert(peace.decorationOwned == 0xffff && peace.decorations[0] == quest::kGearCount && peace.decorations[1] == 201);
  assert(peace.equipped[0] == 99 && peace.equipped[1] == 149 && peace.equipped[2] == 199);
  const auto oldXp = peace.xp, oldCoins = peace.campaign.extraCoins;
  assert(quest::restoreExpansionRewards(peace));
  assert(!quest::restoreExpansionRewards(peace));
  assert(peace.xp == oldXp && peace.campaign.extraCoins == oldCoins && !peace.endingSeen);
  assert(quest::hasWon(peace));
  for (int item : {quest::kKingBody, quest::kKingBackground, quest::kKingPet, quest::kKingDecoration}) {
    assert(quest::owns(peace, item));
    assert(quest::equip(peace, item));
    roundTrip(peace);
  }
  int earnedOutfits = 0;
  for (int n = 0; n < quest::kHuntOutfitCount; ++n) earnedOutfits += quest::owns(peace, quest::kHuntOutfitStart + n);
  assert(quest::ownedCount(peace, quest::Slot::Body) == 101 + earnedOutfits);
  assert(quest::ownedItem(peace, quest::Slot::Body, 100) == quest::kKingBody);
  peace.endingSeen = true;
  roundTrip(peace);
  auto before = peace;
  readingstats::Summary summary;
  summary.seconds = peace.campaign.observedSeconds + 600;
  assert(quest::syncCampaign(peace, summary, 20000));
  assert(peace.xp == before.xp && peace.campaign.encounter == 719 &&
         peace.campaign.phase == quest::BattlePhase::Complete);
  assert(questreturn::capture(before, peace).kind == questreturn::Kind::Complete);
  assert(quest::acceptRequest(peace, quest::kKingHunt));
  const auto hunting = peace;
  summary.seconds += quest::requestTarget(peace, quest::kKingHunt) * 60;
  assert(quest::syncCampaign(peace, summary, 20000));
  auto hunt = questreturn::capture(hunting, peace);
  assert(questreturn::captureRequest(hunting, peace, summary, hunt) && questreturn::huntOnly(hunt));
  assert(hunt.requestAfter == quest::requestTarget(hunting, quest::kKingHunt));
  assert(quest::collectRequest(peace, quest::kKingHunt, &summary));
  assert(quest::requestDecoration(quest::kKingHunt) == quest::kGearCount);
  assert(!quest::collectRequest(peace, quest::kKingHunt, &summary));
  roundTrip(peace);

  auto unfinished = legacy("unfinished");
  before = unfinished;
  summary = {};
  summary.anchorDay = 20000;
  summary.seconds = unfinished.campaign.observedSeconds + 900;
  summary.dailySeconds[0] = 900;
  assert(quest::syncCampaign(unfinished, summary, 20000));
  assert(unfinished.xp == before.xp && unfinished.campaign.encounter == before.campaign.encounter);
  assert(questreturn::capture(before, unfinished).kind == questreturn::Kind::Complete);
  assert(unfinished.campaign.encounterSeconds == before.campaign.encounterSeconds);
  assert(quest::claimMission(unfinished, summary, 20000, quest::Mission::Daily));
  assert(!quest::claimMission(unfinished, summary, 20000, quest::Mission::Daily));
  roundTrip(unfinished);

  for (const char* fixture : {"peace", "complete-active"}) {
    auto terminal = legacy(fixture);
    assert(quest::hasWon(terminal) && questreturn::pendingEnding(terminal));
    auto credited = terminal;
    readingstats::Summary added;
    added.seconds = terminal.campaign.observedSeconds + 1200;
    assert(quest::syncCampaign(credited, added, 20000));
    assert(credited.campaign.state == quest::AdventureState::Peace);
    assert(questreturn::capture(terminal, credited).kind == questreturn::Kind::Complete);
    assert(questreturn::pendingEnding(credited));
    credited.endingSeen = true;
    assert(!questreturn::pendingEnding(credited));
    roundTrip(credited);
  }
  quest::Profile all;
  all.xp = quest::threshold(100);
  all.noticedLevel = 100;
  int chapterGifts = 0, requestGifts = 0;
  for (unsigned chapter = 0; chapter < 120; ++chapter)
    chapterGifts += quest::visibleItem(quest::chapterDecoration(chapter));
  for (int request = 0; request < quest::kRequestCount; ++request)
    if (quest::huntForPath(quest::requestChapter(request)) == request)
      requestGifts += quest::visibleItem(quest::requestDecoration(request));
  assert(chapterGifts == 19 && requestGifts == 24);
  for (int item = 216; item < quest::kGearCount; ++item) {
    if (quest::availableItem(item) && !quest::storyExclusive(item)) assert(quest::pathUnlockChapter(item) >= 0);
    assert(!quest::equip(all, item));
    if (!quest::availableItem(item)) {
      assert(!quest::grantItem(all, item));
      continue;
    }
    assert(quest::grantItem(all, item));
    assert(!quest::grantItem(all, item));
    assert(quest::equip(all, item));
    roundTrip(all);
  }
  assert(quest::count(quest::Slot::Decoration) == quest::kDecorationCount + 1);
  quest::Profile acknowledged;
  acknowledged.campaign.encounter = 11;
  acknowledged.campaign.phase = quest::BattlePhase::Story;
  assert(quest::continueCampaign(acknowledged) && !quest::owns(acknowledged, 200));
  quest::Profile oldComplete;
  oldComplete.campaign.encounter = 719;
  oldComplete.campaign.phase = quest::BattlePhase::Complete;
  assert(quest::restoreExpansionRewards(oldComplete) && oldComplete.campaign.state == quest::AdventureState::Peace);
  assert(quest::owns(oldComplete, quest::kKingBody) && !quest::restoreExpansionRewards(oldComplete));
  assert(quest::count(quest::Slot::Body) == 101 + quest::kHuntOutfitCount &&
         quest::count(quest::Slot::Background) == 51 && quest::count(quest::Slot::Pet) == 51);
  quest::Profile shop;
  shop.xp = 1000000000;
  shop.campaign.encounter = 719;
  shop.campaign.phase = quest::BattlePhase::Complete;
  for (auto category : {quest::Slot::Body, quest::Slot::Background, quest::Slot::Pet}) {
    const auto total = quest::count(category);
    const auto eligible = quest::shopCount(shop, category) + quest::ownedCount(shop, category);
    for (int index = 0; index < total; ++index) {
      const int item = quest::catalogItem(category, index);
      assert(quest::slot(item) == category);
      if (quest::storyExclusive(item)) {
        assert(!quest::equip(shop, item));
        continue;
      }
      assert(quest::equip(shop, item));
      assert(quest::shopCount(shop, category) + quest::ownedCount(shop, category) == eligible);
    }
    assert(quest::shopCount(shop, category) == 0);
    for (unsigned book = 0; book < quest::CAMPAIGN_BOOKS; ++book) {
      const int item = quest::sagaReward(book, category);
      assert(quest::grantItem(shop, item) && quest::equip(shop, item));
    }
    const int huntItems = category == quest::Slot::Body  ? quest::kHuntOutfitCount
                          : category == quest::Slot::Pet ? 3
                                                         : 2;
    assert(quest::ownedCount(shop, category) == total - huntItems);
  }
  for (auto slot : {quest::Slot::Body, quest::Slot::Background, quest::Slot::Pet, quest::Slot::Decoration})
    for (int i = 0; i < quest::count(slot); ++i) assert(quest::slot(quest::catalogItem(slot, i)) == slot);
  uint8_t bytes[quest::kProfileBytes];
  quest::encode(all, bytes);
  bytes[474] = 2;
  auto crc = readingstats::checksum(bytes, 508);
  for (int n = 0; n < 4; ++n) bytes[508 + n] = crc >> (n * 8);
  crc = readingstats::checksum(bytes, quest::kProfileBytes - 4);
  for (int n = 0; n < 4; ++n) bytes[quest::kProfileBytes - 4 + n] = crc >> (n * 8);
  quest::Profile invalid;
  assert(!quest::decode(bytes, invalid));
  std::printf(
      "Expansion: schema4 golden migration,47 active earned items,60 decorations,king set,continued "
      "reading/requests,once-only gifts and round trips passed;Profile=%zu\n",
      sizeof(all));
}

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestCore.h"
#include "../../src/apps_local/quest/QuestReadingReturn.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "../study-suite/stubs/HalStorage.h"
namespace {
void crc(uint8_t* bytes, size_t at) {
  const auto sum = readingstats::checksum(bytes, at);
  for (int n = 0; n < 4; ++n) bytes[at + n] = uint8_t(sum >> (8 * n));
}
void open(quest::Profile& p) {
  p.campaign.initialized = p.campaign.prologueSeen = true;
  p.campaign.introducedBooks = 12;
  p.campaign.encounter = 719;
  p.campaign.narratedEncounters = 720;
  p.campaign.phase = quest::BattlePhase::Complete;
  p.campaign.originSeconds = p.campaign.observedSeconds = 1000;
}
}  // namespace
int main() {
  quest::Profile p;
  std::array<uint8_t, quest::kProfileBytes> bytes{};
  quest::Profile restored;
  for (unsigned n = 1; n <= quest::MAX_LEVEL; ++n) {
    assert(quest::level(quest::threshold(n)) == n);
    if (n > 1) assert(quest::level(quest::threshold(n) - 1) == n - 1);
    p.xp = quest::threshold(n);
    p.noticedLevel = n;
    quest::encode(p, bytes.data());
    assert(quest::decode(bytes.data(), restored) && restored.noticedLevel == n);
  }
  assert(quest::level(UINT64_MAX) == 150 && quest::MAX_LEVEL == 150);
  assert(quest::threshold(100) == 47124 && quest::threshold(150) == 90004);
  assert(quest::level(quest::threshold(151)) == 150);
  assert(quest::baseMinutes(quest::battleAnchor(119)) == 2 * quest::baseMinutes(quest::battleAnchor(118)));
  for (unsigned level = 1; level <= quest::MAX_LEVEL; ++level) {
    quest::Profile penultimate;
    penultimate.xp = quest::threshold(level);
    penultimate.campaign.encounter = quest::battleAnchor(118);
    for (int preview : {-1, quest::kKingBody, quest::kKingPet, quest::kKingBackground}) {
      auto final = penultimate;
      final.campaign.encounter = quest::battleAnchor(119);
      assert(quest::encounterTarget(final, preview) == 2 * quest::encounterTarget(penultimate, preview));
      if (preview < 0) {
        const auto target = quest::encounterTarget(final);
        for (uint32_t elapsed : {0u, 15u * 60, target - 1, target}) {
          final.campaign.encounterSeconds = elapsed;
          const auto estimate = quest::campaignMinutes(final);
          assert(estimate.target - estimate.read == std::max(1u, (target - elapsed + 59) / 60));
        }
      }
    }
  }
  {
    quest::Profile ongoing;
    ongoing.campaign.encounter = quest::battleAnchor(119);
    ongoing.campaign.encounterSeconds = 15 * 60;
    quest::encode(ongoing, bytes.data());
    assert(quest::decode(bytes.data(), restored));
    assert(restored.campaign.encounterSeconds == 15 * 60);
    ongoing.campaign.phase = quest::BattlePhase::Victory;
    assert(quest::encounterTarget(ongoing) == 15 * 60);
    ongoing.campaign.phase = quest::BattlePhase::Story;
    assert(quest::encounterTarget(ongoing) == 15 * 60);
  }
  for (unsigned ch = 0; ch < quest::CAMPAIGN_CHAPTERS; ++ch) {
    p = quest::Profile{};
    p.campaign.encounter = quest::battleAnchor(ch);
    const auto first = quest::encounterTarget(p);
    p.xp = quest::threshold(50);
    const auto middle = quest::encounterTarget(p);
    p.xp = quest::threshold(150);
    const auto later = quest::encounterTarget(p);
    assert(later <= middle && middle <= first);
    assert(later >= (quest::baseMinutes(p.campaign.encounter) * 40 + 99) / 100 * 60);
  }
  for (int item = 0; item < quest::kBaseGearCount; ++item) {
    if (!quest::availableItem(item) || quest::storyExclusive(item)) continue;
    const int next = item + 3;
    if (quest::slot(next) != quest::slot(item) || !quest::availableItem(next) || quest::storyExclusive(next)) continue;
    assert(quest::gearBonus(next, 11).power >= quest::gearBonus(item, 11).power);
  }
  p = quest::Profile{};
  unsigned lastShopLevel = 0;
  for (auto category : {quest::Slot::Body, quest::Slot::Background, quest::Slot::Pet, quest::Slot::Decoration}) {
    const auto catalogue = quest::shopCount(p, category, quest::ShopView::Catalogue);
    assert(catalogue >= quest::shopCount(p, category));
    for (int at = 0; at < catalogue; ++at) {
      const int item = quest::shopItem(p, category, at, quest::ShopView::Catalogue);
      assert(!quest::storyExclusive(item) && !quest::owns(p, item));
      const auto gate = quest::shopLevel(item);
      assert(gate >= 1 && gate <= quest::MAX_LEVEL);
      lastShopLevel = std::max(lastShopLevel, gate);
      for (int previous = 0; previous < at; ++previous) {
        const int other = quest::shopItem(p, category, previous, quest::ShopView::Catalogue);
        if (quest::shopLevel(other) < gate) assert(quest::price(other) < quest::price(item));
      }
      if (!quest::shopUnlocked(p, item)) {
        p.campaign.extraCoins = 100000;
        const auto balance = quest::coins(p);
        assert(!quest::equip(p, item) && quest::coins(p) == balance && !quest::owns(p, item));
      }
    }
  }
  assert(lastShopLevel == 150);
  p = quest::Profile{};
  open(p);
  p.inkHuntDiscovered = true;
  assert(quest::acceptRequest(p, quest::kInkHunt));
  const auto target = quest::requestTarget(p, quest::kInkHunt);
  assert(target >= 1350 && target <= 1800);
  p.campaign.observedSeconds += 3600;
  const auto progress = quest::requestValue(p, quest::kInkHunt);
  p.xp = quest::threshold(150);
  assert(quest::requestTarget(p, quest::kInkHunt) == target);
  assert(quest::cancelRequest(p, quest::kInkHunt));
  assert(quest::acceptRequest(p, quest::kInkHunt));
  assert(quest::requestTarget(p, quest::kInkHunt) == target && quest::requestValue(p, quest::kInkHunt) == progress);
  quest::encode(p, bytes.data());
  assert(quest::decode(bytes.data(), restored));
  assert(quest::requestTarget(restored, quest::kInkHunt) == target);
  p = quest::Profile{};
  open(p);
  p.xp = quest::threshold(80);
  p.noticedLevel = 80;
  p.spent = 1000;
  assert(quest::acceptRequest(p, 0));
  p.requestTargetMinutes = quest::legacyRequestMinutes(0);
  p.campaign.observedSeconds += 300;
  quest::encode(p, bytes.data());
  bytes[3] = '9';
  crc(bytes.data(), 508);
  crc(bytes.data(), quest::kProfileBytes - 4);
  const auto wallet = quest::coins(p);
  assert(quest::decodeV9(bytes.data(), restored));
  assert(restored.xp == p.xp && restored.spent == p.spent && quest::coins(restored) == wallet);
  assert(quest::requestTarget(restored, 0) == 10 && quest::requestValue(restored, 0) == 5);
  testStorage = TestStorage{};
  testStorage.files["/.crosspoint/reading-adventure-v2/reading-quest-v9.dat"] = {bytes.begin(), bytes.end()};
  quest::StoreWork work;
  bool writable = false;
  assert(quest::load(restored, writable, work) && writable);
  assert(quest::save(restored, writable, work));
  assert(testStorage.files.count("/.crosspoint/reading-adventure-v2/reading-quest-v10.dat") &&
         testStorage.files.count("/.crosspoint/reading-adventure-v2/reading-quest-v9.dat"));
  assert(quest::load(restored, writable, work) && writable);
  assert(quest::requestValue(restored, 0) == 5 && quest::coins(restored) == wallet);
  p = quest::Profile{};
  open(p);
  p.xp = quest::threshold(150);
  for (int id : {quest::kShadowHunt, quest::kKingHunt, quest::kInkHunt})
    assert(quest::huntMinutes(p, id) >= quest::requestMinutes(id) * 3 / 4);
  p = quest::Profile{};
  open(p);
  p.xp = 381524;
  p.noticedLevel = 150;
  p.spent = 120000;
  p.campaign.extraCoins = 77;
  quest::encode(p, bytes.data());
  bytes[7110] = 1;
  bytes[7108] = 300 & 255;
  bytes[7109] = 300 >> 8;
  crc(bytes.data(), quest::kProfileBytes - 4);
  assert(quest::decode(bytes.data(), restored));
  assert(restored.xp == p.xp && restored.spent == p.spent && restored.noticedLevel == 150);
  assert(quest::coins(restored) == 14507);
  const auto carriedCoins = restored.campaign.extraCoins;
  quest::encode(restored, bytes.data());
  assert(bytes[7110] == quest::BALANCE_REVISION && quest::decode(bytes.data(), p));
  assert(quest::coins(p) == 14507 && p.campaign.extraCoins == carriedCoins);
  const std::vector<uint8_t> supported(bytes.begin(), bytes.end());
  bytes[7110] = quest::BALANCE_REVISION + 1;
  crc(bytes.data(), quest::kProfileBytes - 4);
  assert(!quest::decode(bytes.data(), p));
  const std::vector<uint8_t> future(bytes.begin(), bytes.end());
  constexpr const char* primary = "/.crosspoint/reading-adventure-v2/reading-quest-v10.dat";
  constexpr const char* backup = "/Backups/AventuraLectora-v2/partida-v10.rqu";
  testStorage = TestStorage{};
  testStorage.files[primary] = future;
  testStorage.files[backup] = supported;
  assert(!quest::load(p, writable, work) && !writable);
  assert(testStorage.files[primary] == future);
  assert(!quest::save(restored, true, work) && testStorage.files[primary] == future);
  testStorage.files[primary] = supported;
  testStorage.files[primary].insert(testStorage.files[primary].end(), future.begin(), future.end());
  const auto mixed = testStorage.files[primary];
  assert(!quest::load(p, writable, work) && !writable && testStorage.files[primary] == mixed);
  testStorage.files[primary] = supported;
  testStorage.files[backup] = future;
  assert(testStorage.files[backup] == future);
  assert(quest::save(restored, true, work) && work.notice == quest::StoreNotice::None);
  assert(testStorage.files[backup] == future);
  testStorage.files[primary] = future;
  testStorage.files[primary].back() ^= 1;
  testStorage.files[backup] = supported;
  assert(quest::load(p, writable, work) && writable);
  bytes[7110] = 2;
  bytes[7108] = 151;
  bytes[7109] = 0;
  crc(bytes.data(), quest::kProfileBytes - 4);
  assert(!quest::decode(bytes.data(), p));
  bytes[7110] = 1;
  bytes[7108] = 1001 & 255;
  bytes[7109] = 1001 >> 8;
  crc(bytes.data(), quest::kProfileBytes - 4);
  assert(!quest::decode(bytes.data(), p));
  p = quest::Profile{};
  p.xp = quest::threshold(2) - 1;
  p.campaign.initialized = p.campaign.prologueSeen = true;
  p.campaign.introducedBooks = 12;
  p.campaign.encounter = quest::battleAnchor(100);
  p.campaign.narratedEncounters = p.campaign.encounter + 1;
  p.campaign.observedSeconds = 1000;
  p.campaign.encounterSeconds = quest::encounterTarget(p) - 60;
  const auto beforeLevel = p;
  readingstats::Summary reading;
  reading.seconds = 1020;
  assert(quest::syncCampaign(p, reading, 20500));
  const auto report = questreturn::capture(beforeLevel, p);
  assert(!report.victory && report.after == report.before + 20 && report.xp == 0 &&
         report.kind == questreturn::Kind::Combat);
  reading.seconds += 40;
  assert(quest::syncCampaign(p, reading, 20500));
  assert(p.campaign.phase == quest::BattlePhase::Victory && p.xp == beforeLevel.xp);
  assert(quest::collectBattleResult(p) && p.xp > beforeLevel.xp);
  assert(quest::missionXp(quest::Mission::Daily) < questbalance::XP_PER_MINUTE * quest::DAILY_MINUTES);
  std::printf(
      "Balance: 150 level round-trips, 120 resistance curves, gear tiers, secret floors, fixed hunt goals, v9 "
      "migration and protected journals passed; Profile=%zu StoreWork=%zu\n",
      sizeof(p), sizeof(work));
}

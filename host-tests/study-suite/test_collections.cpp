#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestReadingReturn.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "stubs/HalStorage.h"

namespace {
void ready(quest::Profile& p) {
  p.campaign.initialized = p.campaign.prologueSeen = true;
  p.campaign.introducedBooks = 12;
  p.campaign.encounter = 719;
  p.campaign.narratedEncounters = 720;
  p.campaign.phase = quest::BattlePhase::Complete;
  p.campaign.originSeconds = p.campaign.observedSeconds = 1000;
  quest::reconcileCollection(p);
}
void crc(uint8_t* bytes, size_t at) {
  const auto sum = readingstats::checksum(bytes, at);
  for (int n = 0; n < 4; ++n) bytes[at + n] = uint8_t(sum >> (8 * n));
}
void generationIsolation() {
  testStorage = {};
  quest::Profile old;
  ready(old);
  old.xp = 90000;
  std::array<uint8_t, quest::kProfileBytes> bytes{};
  quest::encode(old, bytes.data());
  for (const char* path :
       {"/.crosspoint/reading-quest-v10.dat", "/.crosspoint/reading-quest-v10.tmp",
        "/.crosspoint/reading-quest-v10.previous", "/.crosspoint/reading-quest-v9.dat",
        "/.crosspoint/reading-quest-v8.dat", "/.crosspoint/reading-quest-v6.dat", "/.crosspoint/reading-quest-v5.dat",
        "/.crosspoint/reading-quest-v4.dat", "/.crosspoint/reading-quest-v3.dat", "/.crosspoint/reading-quest-v2.dat",
        "/.crosspoint/reading-quest.dat", "/Backups/AventuraLectora/partida-v10.rqu",
        "/Backups/AventuraLectora/partida-v10.tmp", "/Backups/AventuraLectora/partida-v9.rqu",
        "/Backups/AventuraLectora/partida.rqu"})
    testStorage.files[path].assign(bytes.begin(), bytes.end());
  testStorage.files["/.crosspoint/reading-history/sessions.bin"] = {1, 2, 3};
  const auto originals = testStorage.files;
  quest::Profile fresh, loaded;
  quest::StoreWork work;
  bool writable = false, found = true;
  assert(quest::load(fresh, writable, work, &found) && writable && !found);
  assert(!fresh.campaign.initialized && fresh.xp == 0 && quest::defeatedBeasts(fresh) == 0);
  assert(testStorage.files == originals && testStorage.readCalls == 0);
  readingstats::Summary summary;
  summary.seconds = 500000;
  summary.completed = 50;
  assert(quest::syncCampaign(fresh, summary, 20310));
  assert(quest::adventureSeconds(fresh, summary) == 0 && quest::adventureBooks(fresh, summary) == 0);
  assert(quest::needsPrologue(fresh) && fresh.xp == 0);
  assert(quest::save(fresh, writable, work));
  assert(quest::load(loaded, writable, work, &found) && writable && found && loaded.xp == 0);
  const char* primary = "/.crosspoint/reading-adventure-v2/reading-quest-v10.dat";
  const char* copy = "/Backups/AventuraLectora-v2/partida-v10.rqu";
  testStorage.files[copy] = testStorage.files[primary];
  testStorage.files.erase(primary);
  assert(quest::load(loaded, writable, work) && work.notice == quest::StoreNotice::Recovered && loaded.xp == 0);
  assert(quest::save(loaded, writable, work));
  std::fill(testStorage.files[primary].begin(), testStorage.files[primary].end(), 0);
  std::fill(testStorage.files[copy].begin(), testStorage.files[copy].end(), 0);
  assert(!quest::load(loaded, writable, work) && !writable);
  for (const auto& file : originals) assert(testStorage.files.at(file.first) == file.second);
  testStorage = {};
}
}  // namespace
int main() {
  generationIsolation();
  {
    testStorage = {};
    constexpr const char* dir = "/Backups/AventuraLectora-v2";
    constexpr const char* copy = "/Backups/AventuraLectora-v2/partida-v10.rqu";
    constexpr const char* note = "/Backups/AventuraLectora-v2/my-notes.txt";
    testStorage.directories.insert("/Backups");
    testStorage.directories.insert(dir);
    quest::Profile live;
    ready(live);
    quest::StoreWork work;
    assert(quest::save(live, true, work));
    assert(!testStorage.directories.count(dir));
    testStorage.directories.insert("/Backups");
    testStorage.directories.insert(dir);
    std::array<uint8_t, quest::kProfileBytes> bytes;
    quest::encode(live, bytes.data());
    testStorage.files[copy] = {bytes.begin(), bytes.end()};
    testStorage.files[note] = {42};
    bool writable = false;
    assert(quest::load(live, writable, work) && writable);
    assert(!testStorage.files.count(copy) && testStorage.files.at(note)[0] == 42);
    assert(testStorage.directories.count(dir));
    testStorage.files[copy] = {bytes.begin(), bytes.end()};
    testStorage.files[copy][3] = 'X';
    const auto future = testStorage.files.at(copy);
    assert(quest::load(live, writable, work) && testStorage.files.at(copy) == future);
    testStorage = {};
  }
  quest::Profile p;
  ready(p);
  const auto originalCount = p.acquiredCount;
  assert(quest::grantItem(p, 3));
  assert(p.acquiredCount == originalCount + 1 && quest::isNewItem(p, 3));
  assert(quest::toggleFavorite(p, 3) && quest::isFavorite(p, 3));
  assert(quest::grantItem(p, 4));
  assert(quest::inventoryItem(p, quest::Slot::Body, false, 0) == 4);
  assert(quest::inventoryItem(p, quest::Slot::Body, true, 0) == 3);
  assert(quest::markItemSeen(p, 3) && !quest::isNewItem(p, 3));
  assert(!quest::markItemSeen(p, 3));
  assert(!quest::toggleFavorite(p, 99));
  assert(quest::acceptRequest(p, 0));
  const auto firstHuntSeconds = quest::requestTarget(p, 0) * 60;
  p.campaign.observedSeconds += firstHuntSeconds;
  assert(quest::collectRequest(p, 0, nullptr, 20300));
  assert(p.victories[0].day == 20300 && p.victories[0].seconds == firstHuntSeconds);
  assert(quest::owns(p, quest::huntOutfitItem(0)) && quest::isNewItem(p, quest::huntOutfitItem(0)));
  assert(!quest::acceptRequest(p, 0));
  std::array<uint8_t, quest::kProfileBytes> bytes{};
  quest::encode(p, bytes.data());
  quest::Profile restored;
  assert(quest::decode(bytes.data(), restored));
  assert(quest::isFavorite(restored, 3) && !quest::isNewItem(restored, 3) && quest::isNewItem(restored, 4));
  assert(restored.victories[0].seconds == firstHuntSeconds && restored.victories[0].day == 20300);
  bytes.back() ^= 1;
  assert(!quest::decode(bytes.data(), restored));
  quest::encode(p, bytes.data());
  bytes[3] = '6';
  crc(bytes.data(), 508);
  assert(quest::decodeV6(bytes.data(), restored));
  assert(quest::owns(restored, quest::huntOutfitItem(0)) && restored.acquiredCount);
  assert(!quest::isNewItem(restored, 4) && !restored.victories[0].day);
  // Four stable offers include both a campaign rematch and an exclusive hunt.
  assert(quest::requestCount(p, false) == 4);
  bool campaign = false, exclusive = false;
  for (int n = 0; n < 4; ++n) {
    const int id = quest::requestAt(p, false, n);
    assert(quest::requestUnlocked(p, id));
    campaign |= !quest::specialHunt(id);
    exclusive |= quest::specialHunt(id);
    for (int old = 0; old < n; ++old) assert(quest::requestAt(p, false, old) != id);
  }
  assert(campaign && exclusive && quest::requestAt(p, false, 4) == -1);
  for (unsigned chapter = 0; chapter < 120; ++chapter) {
    const int id = quest::huntForPath(chapter);
    const auto rank = quest::huntRank(id);
    assert(rank == (chapter >= 118   ? quest::HuntRank::Secret
                    : chapter >= 110 ? quest::HuntRank::Elite
                                     : quest::HuntRank::Basic));
  }
  for (int id = quest::kCampaignHuntCount; id < quest::kRequestCount; ++id)
    assert(quest::huntRank(id) != quest::HuntRank::Basic);
  // A fresh reading-history counter must not erase an accepted hunt's progress.
  quest::Profile moved;
  ready(moved);
  assert(quest::acceptRequest(moved, quest::kBestiaryHuntStart + 2));
  moved.campaign.observedSeconds += 1800;
  assert(quest::requestValue(moved, moved.activeRequest) == 30);
  readingstats::Summary empty;
  assert(quest::syncCampaign(moved, empty, 20301));
  assert(quest::requestValue(moved, moved.activeRequest) == 30);
  empty.seconds = 600;
  assert(quest::syncCampaign(moved, empty, 20301));
  assert(quest::requestValue(moved, moved.activeRequest) == 40);
  quest::encode(moved, bytes.data());
  assert(quest::decode(bytes.data(), restored) && quest::requestValue(restored, restored.activeRequest) == 40);
  assert(quest::cancelRequest(restored, restored.activeRequest));
  assert(restored.requestCarried == 0);

  // Total active reading is independent of the capped goal and survives journal replacement.
  for (auto goal : {quest::RequestGoal::Minutes, quest::RequestGoal::Pages}) {
    quest::Profile old;
    ready(old);
    assert(quest::acceptRequest(old, 0));
    old.requestGoal = goal;
    old.requestBaseline = 0;
    old.campaign.observedSeconds += 3600;
    old.requestReported = quest::requestTarget(old, 0);
    readingstats::Summary blank;
    assert(quest::syncCampaign(old, blank, 20302));
    quest::encode(old, bytes.data());
    assert(quest::decode(bytes.data(), restored));
    assert(quest::collectRequest(restored, 0, &blank, 20302));
    assert(quest::huntVictory(restored, 0).seconds == 3600);
  }
  quest::Profile ink;
  ready(ink);
  ink.inkHuntDiscovered = true;
  assert(quest::acceptRequest(ink, quest::kInkHunt));
  ink.campaign.observedSeconds += 3600;
  assert(quest::cancelRequest(ink, quest::kInkHunt));
  assert(ink.pausedInkSeconds == 3600 && ink.pausedInkElapsed == 3600);
  assert(quest::acceptRequest(ink, quest::kInkHunt));
  ink.campaign.observedSeconds += 7200;
  readingstats::Summary blank;
  assert(quest::syncCampaign(ink, blank, 20302));
  ink.campaign.observedSeconds += quest::INK_HUNT_SECONDS;
  assert(quest::collectRequest(ink, quest::kInkHunt, nullptr, 20302));
  assert(quest::huntVictory(ink, quest::kInkHunt).seconds == quest::INK_HUNT_SECONDS + 10800);

  constexpr const char* primary = "/.crosspoint/reading-adventure-v2/reading-quest-v10.dat";
  constexpr const char* copy = "/Backups/AventuraLectora-v2/partida-v10.rqu";
  testStorage = {};
  quest::StoreWork work;
  p.savedDay = 20300;
  assert(quest::save(p, true, work));
  assert(p.sequence == 1 && !testStorage.files.count(copy));
  const auto checkpoint = testStorage.files.at(primary);
  assert(quest::grantItem(p, 5));
  p.savedDay = 20301;
  assert(quest::save(p, true, work));
  assert(p.sequence == 2 && testStorage.files.at(primary).size() == 2 * quest::kProfileBytes);
  std::fill(testStorage.files[primary].begin(), testStorage.files[primary].begin() + quest::kProfileBytes, 0);
  bool writable = true, found = false;
  assert(quest::load(restored, writable, work, &found));
  assert(writable && found && work.notice == quest::StoreNotice::None && quest::owns(restored, 5));
  assert(quest::save(restored, true, work));
  assert(quest::load(p, writable, work) && quest::owns(p, 5));
  // A legacy external copy is only imported when the live journal is absent.
  testStorage.files.erase(primary);
  testStorage.files[copy] = checkpoint;
  assert(quest::load(p, writable, work) && writable && work.notice == quest::StoreNotice::Recovered);
  assert(!quest::owns(p, 5) && quest::isFavorite(p, 3));
  assert(quest::save(p, true, work));
  assert(quest::load(restored, writable, work) && !quest::owns(restored, 5));
  assert(quest::save(p, true, work));
  testStorage.writeLimit = 80;
  const auto sequence = p.sequence;
  assert(!quest::save(p, true, work) && p.sequence == sequence);
  testStorage.writeLimit = -1;
  assert(quest::load(restored, writable, work));
  // Forward-version data is never silently replaced by an older automatic copy.
  for (size_t at = 0; at < testStorage.files[primary].size(); at += quest::kProfileBytes)
    testStorage.files[primary][at + 3] = 'X';
  assert(!quest::load(restored, writable, work) && !writable);
  testStorage = {};
  testStorage.failRenameAt = 2;
  ready(p);
  p.sequence = 0;
  assert(quest::save(p, true, work) && p.sequence == 1 && work.notice == quest::StoreNotice::None);
  // A truncated existing primary is repairable without seeking beyond EOF.
  testStorage = {};
  p = {};
  ready(p);
  assert(quest::save(p, true, work));
  assert(quest::save(p, true, work));
  testStorage.files[primary].resize(quest::kProfileBytes + 100);
  assert(quest::load(restored, writable, work) && work.notice == quest::StoreNotice::None);
  assert(quest::save(restored, true, work));
  assert(quest::save(restored, true, work));
  assert(testStorage.files[primary].size() == 2 * quest::kProfileBytes);
  // Retired future-format copies never block or get replaced by an ordinary save.
  testStorage.files[copy] = checkpoint;
  testStorage.files[copy][3] = 'X';
  const auto futureCopy = testStorage.files[copy];
  work.retiredCopiesChecked = false;
  assert(quest::save(restored, true, work) && work.notice == quest::StoreNotice::None);
  assert(testStorage.files[copy] == futureCopy);
  for (size_t n = 0; n < testStorage.files[primary].size(); n += quest::kProfileBytes)
    testStorage.files[primary][n + 3] = 'X';
  const auto futurePrimary = testStorage.files[primary];
  assert(!quest::save(restored, true, work) && testStorage.files[primary] == futurePrimary);

  {
    quest::Profile journey;
    readingstats::Summary reading;
    reading.seconds = 1000;
    reading.completed = 3;
    assert(quest::syncCampaign(journey, reading, 20310));
    assert(quest::acknowledgePrologue(journey) && quest::beginCampaign(journey) && quest::beginEncounter(journey));
    reading.seconds += 120;
    reading.completed += 1;
    assert(quest::syncCampaign(journey, reading, 20311));
    assert(journey.campaign.phase == quest::BattlePhase::Reading);
    const auto campaignSeconds = quest::encounterTarget(journey);
    reading.seconds += campaignSeconds - 120;
    reading.completed += 1;
    assert(quest::syncCampaign(journey, reading, 20312));
    assert(journey.campaign.phase == quest::BattlePhase::Victory);
    const auto win = journey.campaignVictories[0];
    assert(win.day == 20312 && win.seconds == campaignSeconds && win.books == 2);
    assert(win.xp == quest::battleXp(journey) && win.coins == journey.campaign.victoryCoins);
    assert(quest::collectBattleResult(journey) && quest::advanceBattle(journey));
    assert(quest::acceptRequest(journey, quest::huntForPath(0)));
    const auto huntSeconds = quest::requestTarget(journey, journey.activeRequest) * 60;
    reading.seconds += huntSeconds;
    reading.completed += 1;
    assert(quest::syncCampaign(journey, reading, 20313));
    reading.seconds += 120;
    reading.completed += 1;
    assert(quest::syncCampaign(journey, reading, 20314));
    assert(quest::collectRequest(journey, journey.activeRequest, &reading, 20315));
    const auto& hunt = quest::huntVictory(journey, quest::huntForPath(0));
    assert(hunt.day == 20313 && hunt.seconds == huntSeconds && hunt.books == 1);
    quest::encode(journey, bytes.data());
    assert(quest::decode(bytes.data(), restored));
    assert(restored.campaignVictories[0].seconds == campaignSeconds && restored.campaignVictories[0].books == 2);
    assert(quest::huntVictory(restored, quest::huntForPath(0)).books == 1);
    // The previous format preserves its real hunt data, without inventing new statistics.
    bytes[3] = '8';
    crc(bytes.data(), 316);
    crc(bytes.data(), 508);
    crc(bytes.data(), quest::kV8ProfileBytes - 4);
    assert(quest::decodeV8(bytes.data(), restored));
    assert(!restored.campaignVictories[0].day && restored.campaignVictories[0].books == UINT32_MAX);
    assert(quest::huntVictory(restored, quest::huntForPath(0)).books == UINT32_MAX);
    testStorage = {};
    testStorage.files["/.crosspoint/reading-adventure-v2/reading-quest-v8.dat"].assign(
        bytes.begin(), bytes.begin() + quest::kV8ProfileBytes);
    const auto old = testStorage.files.at("/.crosspoint/reading-adventure-v2/reading-quest-v8.dat");
    assert(quest::load(restored, writable, work) && writable);
    assert(quest::save(restored, true, work));
    assert(testStorage.files.at("/.crosspoint/reading-adventure-v2/reading-quest-v8.dat") == old);
    testStorage = {};
    testStorage.files["/Backups/AventuraLectora-v2/partida.rqu"] = old;
    assert(quest::load(restored, writable, work) && work.notice == quest::StoreNotice::Recovered);
    assert(quest::save(restored, true, work));
    assert(testStorage.files.at("/Backups/AventuraLectora-v2/partida.rqu") == old);
  }

  {
    for (unsigned chapter = 0; chapter < quest::CAMPAIGN_CHAPTERS; ++chapter) {
      quest::Profile reward;
      reward.campaign.initialized = reward.campaign.prologueSeen = true;
      reward.campaign.introducedBooks = chapter / quest::CHAPTERS_PER_BOOK + 1;
      reward.campaign.encounter = quest::battleAnchor(chapter);
      reward.campaign.phase = quest::BattlePhase::Victory;
      assert(quest::collectBattleResult(reward));
      const auto gifts = quest::campaignRewards(chapter);
      assert(gifts.count <= 6);
      for (unsigned n = 0; n < gifts.count; ++n) {
        assert(quest::owns(reward, gifts.items[n]));
        for (unsigned earlier = 0; earlier < n; ++earlier) assert(gifts.items[n] != gifts.items[earlier]);
      }
      if (quest::lastPathInSaga(chapter))
        for (auto slot : {quest::Slot::Body, quest::Slot::Background, quest::Slot::Pet, quest::Slot::Decoration}) {
          bool found = false;
          for (unsigned n = 0; n < gifts.count; ++n) found |= gifts.items[n] == quest::sagaReward(chapter / 10, slot);
          assert(found);
        }
    }
    quest::Profile legacy;
    ready(legacy);
    legacy.inkHuntDiscovered = true;
    legacy.pausedInkSeconds = 120;
    assert(quest::acceptRequest(legacy, 0));
    for (char version : {'6', '7'}) {
      quest::encode(legacy, bytes.data());
      bytes[3] = version;
      crc(bytes.data(), 316);
      crc(bytes.data(), 508);
      assert(quest::decodeV6(bytes.data(), restored));
      assert(restored.requestBooks == UINT32_MAX && restored.pausedInkBooks == UINT32_MAX);
    }
    quest::encode(legacy, bytes.data());
    bytes[3] = '8';
    crc(bytes.data(), 316);
    crc(bytes.data(), 508);
    crc(bytes.data(), quest::kV8ProfileBytes - 4);
    const std::vector<uint8_t> record(bytes.begin(), bytes.begin() + quest::kV8ProfileBytes);
    for (const char* source : {"/.crosspoint/reading-adventure-v2/reading-quest-v8.tmp",
                               "/.crosspoint/reading-adventure-v2/reading-quest-v8.previous",
                               "/Backups/AventuraLectora-v2/partida.tmp", "/Backups/AventuraLectora-v2/partida.rqu"}) {
      testStorage = {};
      testStorage.files[source] = record;
      bool found = false;
      assert(quest::load(restored, writable, work, &found) && writable && found);
      assert(work.notice == quest::StoreNotice::Recovered && restored.activeRequest == 0);
      assert(quest::save(restored, true, work));
      if (std::strncmp(source, "/Backups/", 9) != 0) assert(testStorage.files.at(source) == record);
    }
  }
  {
    quest::Profile original;
    ready(original);
    original.observedBooks = 0;
    assert(quest::acceptRequest(original, 0));
    readingstats::Summary summary;
    questreturn::Report report;
    report.requestId = 0;
    report.requestTarget = quest::requestTarget(original, 0);
    report.requestAfter = 5;
    original.campaign.observedSeconds += 300;
    assert(questreturn::finishHuntReport(original, report, summary, 20300) ==
           questreturn::HuntAcknowledgement::Progress);
    assert(original.requestReported == 5 && original.activeRequest == 0);
    report.requestAfter = report.requestTarget;
    auto unchanged = original;
    assert(questreturn::finishHuntReport(original, report, summary, 20300) ==
           questreturn::HuntAcknowledgement::Rejected);
    assert(original.xp == unchanged.xp && original.activeRequest == 0);
    original.campaign.observedSeconds += report.requestTarget * 60 - 300;
    auto draft = original;
    const auto beforeXp = original.xp, beforeCoins = original.campaign.extraCoins;
    const auto victoryXp = quest::requestXp(original, 0);
    assert(questreturn::finishHuntReport(draft, report, summary, 20300) == questreturn::HuntAcknowledgement::Reward);
    assert(original.activeRequest == 0 && original.xp == beforeXp);
    original = draft;
    assert(original.activeRequest == quest::kRequestCount && original.xp == beforeXp + victoryXp);
    assert(original.campaign.extraCoins == beforeCoins + quest::requestCoins(unchanged, 0));
    assert(questreturn::finishHuntReport(original, report, summary, 20300) ==
           questreturn::HuntAcknowledgement::Rejected);
    assert(original.xp == beforeXp + victoryXp);
  }
  std::printf(
      "Collection state, four-offer ranks, card rebase, migration and journal recovery passed; Profile=%zu "
      "StoreWork=%zu bytes\n",
      sizeof(quest::Profile), sizeof(quest::StoreWork));
}

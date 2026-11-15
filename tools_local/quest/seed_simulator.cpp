// Virtual-card samples use the same profile and history codecs as the device.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestCore.h"
#include "../../src/apps_local/quest/QuestMilestones.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "legacy_request_fixture.h"

namespace {
void write(const char* directory, const char* name, const uint8_t* data, size_t size) {
  char path[1024];
  std::snprintf(path, sizeof(path), "%s/%s.bin", directory, name);
  FILE* file = std::fopen(path, "wb");
  assert(file && std::fwrite(data, 1, size, file) == size);
  std::fclose(file);
}
}  // namespace
namespace {
void collectionSamples(const char* directory, int64_t now, int offset) {
  for (int gender = 0; gender < 2; ++gender) {
    quest::Profile p;
    p.gender = quest::Gender(gender);
    std::strcpy(p.name, gender ? "LUNA" : "MILO");
    p.xp = quest::threshold(50);
    p.noticedLevel = 50;
    p.endingSeen = true;
    auto& c = p.campaign;
    c.initialized = c.prologueSeen = true;
    c.introducedBooks = 12;
    c.narratedEncounters = 720;
    c.encounter = 719;
    c.phase = quest::BattlePhase::Complete;
    c.state = quest::AdventureState::Peace;
    c.observedSeconds = 8400;
    c.startedDay = int((now + offset) / 86400) - 7;
    quest::restoreStoryRewards(p);
    quest::reconcileCollection(p);
    for (int item : {3, 4, 6, 9, 16, 20}) quest::grantItem(p, item);
    const int day = int((now + offset) / 86400);
    p.observedBooks = 0;
    for (unsigned n = 0; n < quest::CAMPAIGN_CHAPTERS; ++n)
      p.campaignVictories[n] = {uint32_t(day - 120 + n), uint32_t((n + 4) * 60), 200u + n * 20, 54u + n,
                                n == 1 ? 1u : 0u};
    int ago = 6;
    for (int id : {0, quest::huntForPath(1), quest::huntForPath(2), quest::kStormHunt, quest::kCrystalHunt,
                   quest::kPhoenixHunt}) {
      assert(quest::acceptRequest(p, id));
      c.observedSeconds += quest::requestMinutes(id) * 60;
      p.requestBooks = id == quest::kCrystalHunt ? 2 : id == quest::kPhoenixHunt ? 1 : 0;
      assert(quest::collectRequest(p, id, nullptr, day - ago--));
    }
    c.observedSeconds = 8400;
    assert(quest::toggleFavorite(p, 3) && quest::toggleFavorite(p, quest::huntOutfitItem(quest::kCrystalHunt)));
    assert(quest::acceptRequest(p, quest::kBestiaryHuntStart + 2));
    p.requestCarried = p.requestElapsed = 1800;
    p.requestReported = 30;
    p.savedDay = day;
    static uint8_t bytes[quest::kProfileBytes];
    p.celebratedMilestones = questmilestone::reached(quest::level(p.xp));
    quest::encode(p, bytes);
    quest::Profile checked;
    assert(quest::decode(bytes, checked));
    char name[64];
    std::snprintf(name, sizeof(name), "collections-hunts-%d-0", gender);
    write(directory, name, bytes, sizeof(bytes));
  }
}
void orderSamples(const char* directory, int64_t now, int offset) {
  for (int goal = 0; goal < 5; ++goal)
    for (int mode = 0; mode < 5; ++mode)
      for (int gender = 0; gender < 2; ++gender) {
        static uint8_t history[16 + 8 * readingstats::kRecordBytes];
        std::memset(history, 0, sizeof(history));
        std::memcpy(history, "CRS1", 4);
        history[4] = 0x80;
        history[5] = 1;
        static readingstats::Summary before, after;
        before = {};
        after = {};
        before.anchorDay = after.anchorDay = int((now + offset) / 86400);
        const bool complete = mode == 2 || (goal == 3 && mode >= 3);
        const int baselineDays = complete && (goal == 2 || goal == 4) ? 5 : 6;
        readingstats::Book previous;
        for (int day = 0; day < 7; ++day) {
          readingstats::Record record;
          record.epoch = now - 3600 - (6 - day) * 86400;
          record.localDay = int((record.epoch + offset) / 86400);
          record.secondOfDay = unsigned((record.epoch + offset) % 86400);
          record.seconds = day == 6 && goal == 0 ? (complete ? 600 : 300) : 1200;
          record.pages = day == 6 && goal == 1 ? (complete ? 10 : 5) : 25;
          std::strcpy(record.book.document, "0123456789abcdef0123456789abcdef");
          std::strcpy(record.book.title, "Lectura de muestra");
          readingstats::updateBook(record, previous);
          previous = record.book;
          after.add(record);
          if (day < baselineDays) before.add(record);
          readingstats::encode(record, history + 16 + day * readingstats::kRecordBytes);
        }
        int records = 7;
        if (goal == 3 && complete) {
          readingstats::Record record;
          record.epoch = now - 60;
          record.localDay = int((record.epoch + offset) / 86400);
          record.secondOfDay = unsigned((record.epoch + offset) % 86400);
          record.kind = readingstats::RecordKind::Completion;
          record.book = previous;
          record.book.progressBp = 10000;
          readingstats::updateBook(record, previous);
          assert(record.firstCompletion);
          after.add(record);
          readingstats::encode(record, history + 16 + records++ * readingstats::kRecordBytes);
        }
        quest::Profile profile;
        profile.xp = quest::threshold(50);
        profile.noticedLevel = 50;
        profile.gender = quest::Gender(gender);
        std::strcpy(profile.name, gender ? "LUNA" : "MILO");
        auto& c = profile.campaign;
        c.initialized = c.firstDayRead = c.prologueSeen = true;
        c.startedDay = before.anchorDay - 10;
        c.encounter = 18;
        c.introducedBooks = 1;
        c.narratedEncounters = 19;
        c.observedSeconds = before.seconds;
        int requestId = 0;
        while (requestId < quest::kRequestCount && int(questfixture::goal(requestId)) != goal) ++requestId;
        assert(requestId < quest::kRequestCount);
        assert(questfixture::activateLegacyRequest(profile, requestId, before));
        c.observedSeconds = mode == 4 ? before.seconds : after.seconds;
        if (mode == 0) {
          profile.activeRequest = quest::kRequestCount;
          profile.requestTargetMinutes = 0;
        }
        if (mode == 0) {
          profile.requestStarted = profile.requestBaseline = 0;
          profile.requestGoal = quest::RequestGoal::Minutes;
        }
        if (mode == 1 || mode == 2) profile.requestReported = quest::requestValue(profile, requestId, &after);
        quest::restoreStoryRewards(profile);
        static uint8_t bytes[quest::kProfileBytes];
        profile.celebratedMilestones = questmilestone::reached(quest::level(profile.xp));
        quest::encode(profile, bytes);
        quest::Profile decoded;
        assert(quest::decode(bytes, decoded));
        static readingstats::Summary verified;
        verified = {};
        verified.anchorDay = after.anchorDay;
        for (int n = 0; n < records; ++n) {
          readingstats::Record record;
          assert(readingstats::decode(history + 16 + n * readingstats::kRecordBytes, record));
          verified.add(record);
        }
        assert(verified.seconds == after.seconds && verified.pages == after.pages &&
               verified.completed == after.completed && verified.sessions == after.sessions &&
               verified.totalDays == after.totalDays);
        if (mode) {
          decoded.campaign.observedSeconds = verified.seconds;
          const auto value = quest::requestValue(decoded, requestId, &verified);
          assert(complete ? value == quest::requestTarget(decoded, requestId)
                          : value < quest::requestTarget(decoded, requestId));
          assert(mode >= 3 || value == decoded.requestReported);
        }
        char name[64];
        std::snprintf(name, sizeof(name), "demo-request-%d-%d-%d", goal, mode, gender);
        write(directory, name, bytes, sizeof(bytes));
        std::strcat(name, ".history");
        write(directory, name, history, 16 + records * readingstats::kRecordBytes);
      }
  for (unsigned level = 1; level <= quest::MAX_LEVEL; ++level)
    for (int gender = 0; gender < 2; ++gender) {
      quest::Profile profile;
      profile.xp = quest::threshold(level);
      profile.noticedLevel = level;
      profile.gender = quest::Gender(gender);
      std::strcpy(profile.name, gender ? "LUNA" : "MILO");
      auto& c = profile.campaign;
      c.initialized = c.firstDayRead = c.prologueSeen = true;
      c.startedDay = int((now + offset) / 86400) - 10;
      c.encounter = 18;
      c.introducedBooks = 1;
      c.narratedEncounters = 19;
      c.observedSeconds = 8400;
      quest::restoreStoryRewards(profile);
      static uint8_t bytes[quest::kProfileBytes];
      profile.celebratedMilestones = questmilestone::reached(quest::level(profile.xp));
      quest::encode(profile, bytes);
      quest::Profile decoded;
      assert(quest::decode(bytes, decoded));
      char name[64];
      std::snprintf(name, sizeof(name), "shop-level-%u-%d", level, gender);
      write(directory, name, bytes, sizeof(bytes));
    }
}
}  // namespace
void milestoneSamples(const char* directory, int64_t now, int offset) {
  for (unsigned target : questmilestone::LEVELS)
    for (int gender = 0; gender < 2; ++gender) {
      quest::Profile p;
      p.xp = quest::threshold(target) - 1;
      p.noticedLevel = quest::level(p.xp);
      p.celebratedMilestones = questmilestone::reached(p.noticedLevel);
      p.gender = quest::Gender(gender);
      std::strcpy(p.name, gender ? "LUNA" : "MILO");
      auto& c = p.campaign;
      c.initialized = c.firstDayRead = c.prologueSeen = true;
      c.startedDay = int((now + offset) / 86400) - 10;
      c.observedSeconds = 8400;
      c.introducedBooks = target == 50 ? 1 : 12;
      c.encounter = target == 50 ? quest::battleAnchor(0) : 719;
      c.narratedEncounters = c.encounter + 1;
      if (target > 50) {
        c.state = quest::AdventureState::Peace;
        c.phase = quest::BattlePhase::Complete;
        p.endingSeen = true;
      }
      quest::restoreStoryRewards(p);
      if (target == 50) {
        c.encounterSeconds = quest::encounterTarget(p) - 60;
        p.battleElapsed = c.encounterSeconds;
        p.battleStatsKnown = true;
      } else {
        assert(quest::acceptRequest(p, quest::kStormHunt));
        p.requestCarried = (quest::requestTarget(p, quest::kStormHunt) - 1) * 60;
        p.requestReported = quest::requestTarget(p, quest::kStormHunt) - 1;
      }
      static uint8_t bytes[quest::kProfileBytes];
      quest::encode(p, bytes);
      quest::Profile verified;
      assert(quest::decode(bytes, verified));
      char name[64];
      std::snprintf(name, sizeof(name), "milestone-%u-%d-0", target, gender);
      write(directory, name, bytes, sizeof(bytes));
    }
}
int main(int argc, char** argv) {
  assert(argc == 4);
  const int64_t now = std::strtoll(argv[2], nullptr, 10);
  const int offset = std::atoi(argv[3]);
  milestoneSamples(argv[1], now, offset);
  orderSamples(argv[1], now, offset);
  collectionSamples(argv[1], now, offset);
  uint8_t journal[16 + 7 * readingstats::kRecordBytes] = {'C', 'R', 'S', '1', 0x80, 1};
  readingstats::Book previous;
  for (int day = 0; day < 7; ++day) {
    readingstats::Record record;
    record.epoch = now - 3600 - (6 - day) * 86400;
    record.localDay = int((record.epoch + offset) / 86400);
    record.secondOfDay = unsigned((record.epoch + offset) % 86400);
    record.seconds = 1200;
    record.pages = 25;
    std::strcpy(record.book.document, "0123456789abcdef0123456789abcdef");
    std::strcpy(record.book.title, "Lectura de muestra");
    readingstats::updateBook(record, previous);
    previous = record.book;
    readingstats::encode(record, journal + 16 + day * readingstats::kRecordBytes);
  }
  write(argv[1], "journal", journal, sizeof(journal));
  for (int id = quest::kBestiaryHuntStart; id < quest::kRequestCount; ++id) {
    for (int gender = 0; gender < 2; ++gender) {
      quest::Profile hunt;
      hunt.gender = quest::Gender(gender);
      std::strcpy(hunt.name, gender ? "LUNA" : "MILO");
      auto& c = hunt.campaign;
      c.initialized = c.prologueSeen = true;
      const unsigned paths = quest::BESTIARY[id - quest::kBestiaryHuntStart].paths;
      c.encounter = quest::battleAnchor(paths);
      c.narratedEncounters = c.encounter;
      c.introducedBooks = paths / 10 + 1;
      c.startedDay = int((now + offset) / 86400) - 6;
      c.observedSeconds = 7 * 1200;
      uint8_t bytes[quest::kProfileBytes];
      hunt.celebratedMilestones = questmilestone::reached(quest::level(hunt.xp));
      quest::encode(hunt, bytes);
      quest::Profile restored;
      assert(quest::decode(bytes, restored) && quest::requestUnlocked(restored, id));
      char name[64];
      std::snprintf(name, sizeof(name), "bestiary-%d-%d", id, gender);
      write(argv[1], name, bytes, sizeof(bytes));
    }
  }
  for (int gender = 0; gender < 2; ++gender) {
    quest::Profile ranked;
    ranked.gender = quest::Gender(gender);
    ranked.xp = quest::threshold(25);
    ranked.noticedLevel = 25;
    std::strcpy(ranked.name, gender ? "LUNA" : "MILO");
    auto& progress = ranked.campaign;
    progress.initialized = progress.prologueSeen = progress.firstDayRead = true;
    progress.encounter = quest::battleAnchor(25);
    progress.narratedEncounters = progress.encounter;
    progress.introducedBooks = 3;
    progress.startedDay = int((now + offset) / 86400) - 6;
    progress.observedSeconds = 7 * 1200;
    ranked.inkHuntDiscovered = true;
    quest::restoreStoryRewards(ranked);
    for (const bool ready : {false, true}) {
      if (ready) {
        ranked.pausedInkSeconds = ranked.pausedInkElapsed = quest::INK_HUNT_SECONDS - 1;
        assert(quest::acceptRequest(ranked, quest::kInkHunt));
        --ranked.requestStarted;
        ranked.requestReported = quest::requestMinutes(quest::kInkHunt);
      }
      uint8_t bytes[quest::kProfileBytes];
      ranked.celebratedMilestones = questmilestone::reached(quest::level(ranked.xp));
      quest::encode(ranked, bytes);
      quest::Profile restored;
      assert(quest::decode(bytes, restored));
      char name[64];
      std::snprintf(name, sizeof(name), "ranked-%s-%d-0", ready ? "secret" : "hunts", gender);
      write(argv[1], name, bytes, sizeof(bytes));
    }
  }
  for (int gender = 0; gender < 2; ++gender) {
    quest::Profile finale;
    finale.xp = quest::threshold(100);
    finale.noticedLevel = 100;
    finale.gender = quest::Gender(gender);
    std::strcpy(finale.name, gender ? "LUNA" : "MILO");
    auto& c = finale.campaign;
    c.initialized = c.prologueSeen = c.firstDayRead = true;
    c.encounter = quest::battleAnchor(119);
    c.narratedEncounters = c.encounter + 1;
    c.introducedBooks = 12;
    c.startedDay = int((now + offset) / 86400) - 6;
    c.observedSeconds = 7200;
    c.encounterSeconds = quest::encounterTarget(finale) - 600;
    quest::restoreStoryRewards(finale);
    assert(quest::acceptRequest(finale, 0));
    uint8_t bytes[quest::kProfileBytes];
    finale.celebratedMilestones = questmilestone::reached(quest::level(finale.xp));
    quest::encode(finale, bytes);
    quest::Profile verified;
    assert(quest::decode(bytes, verified));
    char name[64];
    std::snprintf(name, sizeof(name), "finale-request-%d-0", gender);
    write(argv[1], name, bytes, sizeof(bytes));
  }
  static constexpr const char* STAGES[] = {"journey",
                                           "mythic",
                                           "divine",
                                           "collection",
                                           "victory",
                                           "story",
                                           "king",
                                           "complete",
                                           "unfinished",
                                           "peace",
                                           "read-combat",
                                           "read-banked",
                                           "read-victory",
                                           "reward",
                                           "decorated",
                                           "path-story",
                                           "atlas-bridge",
                                           "restore-step",
                                           "requests",
                                           "read-restoration",
                                           "requests-active",
                                           "requests-ready",
                                           "keepsakes",
                                           "atlas-advanced",
                                           "crown-victory",
                                           "crown-home",
                                           "crown-reading",
                                           "forest-pack",
                                           "royal-gate",
                                           "crown-return-unseen",
                                           "legacy-complete-reading"};
  static constexpr int LEVELS[] = {12, 60, 100, 100, 12, 12, 100, 100, 12,  100, 60,  60, 60,  12,  60, 60,
                                   12, 12, 12,  12,  12, 12, 60,  60,  100, 100, 100, 12, 100, 100, 100};
  static constexpr int OUTFITS[] = {0, 59, 99, 81, 0, 0, 99, 99, 0,  99, 59, 59, 59, 0,  59, 59,
                                    0, 0,  0,  0,  0, 0, 59, 59, 99, 99, 99, 0,  99, 99, 99};
  static constexpr int PETS[] = {156, 179, 199, 197, 156, 156, 199, 199, 156, 199, 179, 179, 179, 156, 179, 179,
                                 156, 156, 156, 156, 156, 156, 179, 179, 199, 199, 199, 156, 199, 199, 199};
  static constexpr int ENCOUNTERS[] = {12, 365, 650, 35, 17, 17, 719, 719, 12,  719, 365, 365, 365, 11,  365, 365,
                                       18, 3,   6,   3,  6,  6,  390, 390, 719, 719, 719, 11,  665, 719, 719};
  static constexpr unsigned STAGE_COUNT = sizeof(STAGES) / sizeof(STAGES[0]);
  static_assert(sizeof(LEVELS) / sizeof(LEVELS[0]) == STAGE_COUNT &&
                sizeof(OUTFITS) / sizeof(OUTFITS[0]) == STAGE_COUNT && sizeof(PETS) / sizeof(PETS[0]) == STAGE_COUNT &&
                sizeof(ENCOUNTERS) / sizeof(ENCOUNTERS[0]) == STAGE_COUNT);
  for (unsigned stage = 0; stage < STAGE_COUNT; ++stage)
    for (int gender = 0; gender < 2; ++gender)
      for (int background = 0; background < 50; ++background) {
        quest::Profile profile;
        const auto level = std::max(unsigned(LEVELS[stage]), quest::requiredLevel(100 + background));
        profile.xp = quest::threshold(level) + 72;
        profile.noticedLevel = quest::level(profile.xp);
        profile.gender = quest::Gender(gender);
        std::strcpy(profile.name, gender ? "LUNA" : "MILO");
        profile.equipped[0] = OUTFITS[stage];
        profile.equipped[1] = uint16_t(100 + background);
        profile.equipped[2] = uint16_t(PETS[stage]);
        if (stage == 3) std::memset(profile.owned, 255, sizeof(profile.owned));
        if (stage == 3) profile.decorationOwned = UINT16_MAX;
        for (int id : profile.equipped) profile.owned[id / 8] |= 1u << (id % 8);
        auto& c = profile.campaign;
        c.initialized = true;
        c.firstDayRead = true;
        c.startedDay = int((now - 3600 + offset) / 86400) - 6;
        c.observedSeconds = 8400;
        c.encounter = quest::battleAnchor(ENCOUNTERS[stage] / 6);
        c.introducedBooks = quest::book(profile) + 1;
        c.prologueSeen = true;
        c.narratedEncounters = c.encounter + 1;
        c.encounterSeconds = quest::encounterTarget(profile) / 2;
        if (stage == 4 || stage == 5 || (stage >= 7 && stage <= 9)) {
          c.encounterSeconds = quest::encounterTarget(profile);
          c.victoryCoins = quest::battleCoins(profile);
          c.phase = stage == 4   ? quest::BattlePhase::Victory
                    : stage == 5 ? quest::BattlePhase::Story
                                 : quest::BattlePhase::Complete;
        }
        if ((stage >= 10 && stage <= 12) || stage == 19) {
          c.observedSeconds = 7200;
          c.encounterSeconds = stage == 12 || stage == 19
                                   ? quest::encounterTarget(profile) - std::min(quest::encounterTarget(profile), 600u)
                                   : quest::encounterTarget(profile) / 4;
          if (stage == 11) c.narratedEncounters = c.encounter;
        }
        if (stage == 15) {
          c.narratedEncounters = c.encounter;
          c.encounterSeconds = 0;
        }
        if (stage == 8) {
          c.phase = quest::BattlePhase::Reading;
          c.state = quest::AdventureState::Unfinished;
        }
        if (stage == 9) c.state = quest::AdventureState::Peace;
        if (stage >= 24 && stage <= 26) {
          c.phase = quest::BattlePhase::Complete;
          c.state = quest::AdventureState::Peace;
          c.encounterSeconds = quest::encounterTarget(profile);
          c.observedSeconds = stage == 26 ? 7200 : 8400;
          profile.endingSeen = stage != 24;
        }
        if (stage == 27) {
          c.observedSeconds = 7200;
          c.encounterSeconds = quest::encounterTarget(profile) - std::min(quest::encounterTarget(profile), 600u);
        }
        if (stage == 29 || stage == 30) {
          c.phase = quest::BattlePhase::Complete;
          c.state = quest::AdventureState::Peace;
          c.encounterSeconds = quest::encounterTarget(profile);
          c.observedSeconds = 7200;
          profile.endingSeen = false;
        }
        quest::restoreStoryRewards(profile);
        if (stage == 30) c.state = quest::AdventureState::Active;
        if (stage == 3)
          for (int item = 0; item < quest::kGearCount; ++item) quest::grantItem(profile, item);
        if (stage == 25 || stage == 26) {
          for (int item = quest::kExpandedItemStart; item < quest::kGearCount; ++item) quest::grantItem(profile, item);
          for (int item : {quest::kKingBody, quest::kKingBackground, quest::kKingPet, quest::kKingDecoration})
            assert(quest::equip(profile, item));
        }
        if (stage == 13) {
          c.phase = quest::BattlePhase::Story;
          quest::grantItem(profile, 200);
          c.pendingReward = 200;
        }
        if (stage == 14 || stage == 3) {
          quest::grantItem(profile, 200);
          quest::grantItem(profile, 201);
          quest::equip(profile, 200);
          quest::equip(profile, 201);
        }
        if (stage == 20 || stage == 21) {
          assert(quest::acceptRequest(profile, 0));
          profile.requestStarted -= stage == 20 ? 300 : quest::requestMinutes(0) * 60;
        }
        if (stage == 22)
          for (int id = 0; id < 50; ++id) profile.requestClaims[id / 8] |= 1u << (id % 8);
        uint8_t bytes[quest::kProfileBytes];
        profile.celebratedMilestones = questmilestone::reached(quest::level(profile.xp));
        quest::encode(profile, bytes);
        quest::Profile decoded;
        assert(quest::decode(bytes, decoded) &&
               decoded.equipped[1] == (stage == 25 || stage == 26 ? quest::kKingBackground : 100 + background) &&
               !quest::needsPrologue(decoded) && (quest::needsEncounterStory(decoded) == (stage == 11 || stage == 15)));
        char name[64];
        std::snprintf(name, sizeof(name), "%s-%d-%d", STAGES[stage], gender, background);
        write(argv[1], name, bytes, sizeof(bytes));
      }
  for (unsigned encounter = quest::battleAnchor(0); encounter < quest::CAMPAIGN_ENCOUNTERS;
       encounter += quest::ENCOUNTERS_PER_CHAPTER)
    for (int gender = 0; gender < 2; ++gender)
      for (int moment = 0; moment < 6; ++moment) {
        quest::Profile profile;
        profile.xp = quest::threshold(100);
        profile.noticedLevel = 100;
        profile.gender = quest::Gender(gender);
        std::strcpy(profile.name, gender ? "LUNA" : "MILO");
        auto& c = profile.campaign;
        c.initialized = c.firstDayRead = c.prologueSeen = true;
        c.startedDay = int((now - 3600 + offset) / 86400) - 6;
        c.observedSeconds = moment == 5 ? 7200 : 8400;
        c.encounter = encounter;
        c.introducedBooks = encounter / 60 + 1;
        c.narratedEncounters = encounter + (moment != 0);
        c.encounterSeconds = moment == 2 ? quest::encounterTarget(profile) / 2 : 0;
        if (moment == 3 || moment == 4) {
          c.encounterSeconds = quest::encounterTarget(profile);
          c.victoryCoins = quest::battleCoins(profile);
          c.phase = moment == 3 ? quest::BattlePhase::Victory : quest::BattlePhase::Story;
        }
        quest::restoreStoryRewards(profile);
        uint8_t bytes[quest::kProfileBytes];
        profile.celebratedMilestones = questmilestone::reached(quest::level(profile.xp));
        quest::encode(profile, bytes);
        quest::Profile decoded;
        assert(quest::decode(bytes, decoded) && decoded.campaign.encounter == encounter);
        char name[64];
        std::snprintf(name, sizeof(name), "custom-%u-%d-%d", encounter, gender, moment);
        write(argv[1], name, bytes, sizeof(bytes));
      }
}

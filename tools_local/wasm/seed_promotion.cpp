// Host-only promotional fixtures, checked with the production save codecs.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "../../src/apps_local/focus/FocusCore.h"
#include "../../src/apps_local/pet/PetHistory.h"
#include "../../src/apps_local/pet/PetReadingTimeline.h"
#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestMilestones.h"
#include "../../src/apps_local/reading/ReadingStatsCore.h"
#include "../../src/apps_local/tools/ToolState.h"

namespace fs = std::filesystem;
namespace {
void write(const fs::path& path, const uint8_t* bytes, size_t size) {
  fs::create_directories(path.parent_path());
  auto* file = std::fopen(path.string().c_str(), "wb");
  assert(file && std::fwrite(bytes, 1, size, file) == size);
  assert(std::fclose(file) == 0);
}
static readingstats::Summary summary;
static readingstats::Record record;
static uint8_t bytes[focus::kFileBytes];
int64_t latest = 0;
}  // namespace

int main(int argc, char** argv) {
  assert(argc == 4);
  const auto card = fs::path(argv[1]);
  const auto out = fs::path(argv[2]);
  const int64_t now = std::strtoll(argv[3], nullptr, 10);
  assert(card.filename() == "resource-card" && fs::exists(card / "CROSSNEXT-SIMULATOR-CARD.json"));
  summary.anchorDay = int(now / 86400);
  auto* journal = std::fopen((card / ".crosspoint/reading-history/sessions.bin").string().c_str(), "rb");
  assert(journal && std::fseek(journal, 16, SEEK_SET) == 0);
  while (std::fread(bytes, 1, readingstats::kRecordBytes, journal) == readingstats::kRecordBytes) {
    assert(readingstats::decode(bytes, record));
    assert(record.epoch + record.seconds <= now);
    summary.add(record);
    latest = std::max(latest, record.epoch + record.seconds);
  }
  assert(!std::ferror(journal) && std::fclose(journal) == 0 && summary.seconds > 100000);
  constexpr const char* names[] = {"WILLOW", "LUNA", "EMBER"};
  for (unsigned species = 0; species < 3; ++species) {
    for (unsigned age = 0; age < 5; ++age) {
      pet::Profile p;
      p.created = true;
      p.sequence = 1;
      p.generation = 4;
      p.species = pet::Species(species);
      std::strcpy(p.name, names[species]);
      p.mode = pet::Mode::Relaxed;
      p.readingSeconds = pet::stageStart(pet::Stage(age)) + (age ? 60 : 600);
      p.baselineSeconds = summary.seconds - p.readingSeconds;
      p.noticedSeconds = p.readingSeconds;
      p.noticedStage = pet::Stage(age);
      p.bornDay = summary.anchorDay - 14;
      p.lastReadAt = latest;
      p.careAt = now;
      p.saciety = 82;
      p.joy = 92;
      p.hygiene = 96;
      p.earnedFood = p.noticedFood = 42;
      p.earnedMedicine = p.noticedMedicine = 12;
      p.gamesWon = 28;
      p.treatments = 6;
      p.memories = uint16_t((1u << (age + 1)) - 1);
      for (unsigned item = 0; item < 28; ++item) pet::own(p, item);
      for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
        const auto item = pet::objectAt(slot, species);
        p.equipped[slot] = uint8_t(pet::objectAvailable(item) ? item : pet::objectAt(slot, species + 3));
        pet::own(p, p.equipped[slot]);
      }
      p.project = 28;
      p.projectStartedSeconds = pet::projectReading(p);
      pet::Profile empty;
      pet::captureMoments(p, empty, summary.anchorDay);
      pet::encode(p, bytes);
      pet::Profile decoded;
      assert(pet::decode(bytes, decoded) && pet::stage(decoded.readingSeconds) == pet::Stage(age));
      pet::ReadingTimeline timeline;
      timeline.begin(decoded, now);
      // A complete, ordered journal is replayed by the app on entry.
      journal = std::fopen((card / ".crosspoint/reading-history/sessions.bin").string().c_str(), "rb");
      assert(journal && std::fseek(journal, 16, SEEK_SET) == 0);
      static uint8_t wire[readingstats::kRecordBytes];
      while (std::fread(wire, 1, sizeof(wire), journal) == sizeof(wire)) {
        assert(readingstats::decode(wire, record));
        timeline.add(record.epoch, record.seconds, record.localDay, record.secondOfDay);
      }
      assert(!std::ferror(journal) && std::fclose(journal) == 0);
      timeline.finish(now);
      assert(timeline.valid() && !timeline.endedAt());
      write(out / ("pet-" + std::to_string(species) + "-" + std::to_string(age) + ".bin"), bytes, pet::STATE_BYTES);
      if (species == 0 && age == 3) write(card / ".crosspoint/pet-v2/state-a.bin", bytes, pet::STATE_BYTES);
    }
  }
  pet::History history;
  for (unsigned n = 0; n < 3; ++n) {
    pet::Profile p;
    p.created = true;
    p.generation = n + 1;
    p.species = pet::Species(n);
    std::strcpy(p.name, names[n]);
    p.bornDay = summary.anchorDay - 80 + n * 20;
    p.sharedDays = 12 + n;
    p.readingSeconds = (10 + n * 5) * 3600;
    assert(pet::endLife(p, pet::LifeStatus::Retired, now - (50 - n * 15) * 86400));
    assert(pet::addLife(history, p, summary.anchorDay - (50 - n * 15)));
  }
  pet::encodeHistory(history, bytes);
  pet::History checkedHistory;
  assert(pet::decodeHistory(bytes, checkedHistory) && checkedHistory.count == 3);
  write(card / ".crosspoint/pet-v2/lives-a.bin", bytes, pet::HISTORY_BYTES);
  for (unsigned stage = 0; stage < 3; ++stage) {
    for (unsigned gender = 0; gender < 2; ++gender) {
      static quest::Profile p, decoded;
      p = {};
      const unsigned level = 20 + stage * 30;
      p.xp = quest::threshold(level) + 72;
      p.noticedLevel = quest::level(p.xp);
      p.celebratedMilestones = questmilestone::reached(p.noticedLevel);
      p.gender = quest::Gender(gender);
      std::strcpy(p.name, gender ? "LUNA" : "MILO");
      p.bonus = 1840;
      p.savedDay = summary.anchorDay;
      p.observedBooks = summary.completed;
      auto& c = p.campaign;
      c.initialized = c.firstDayRead = c.prologueSeen = true;
      c.startedDay = summary.anchorDay - 14;
      c.observedSeconds = summary.seconds;
      c.originSeconds = summary.seconds / 2;
      c.encounter = uint16_t(17 + stage * 180);
      c.narratedEncounters = c.encounter + 1;
      c.introducedBooks = uint8_t(c.encounter / 60 + 1);
      c.encounterSeconds = 180;
      c.phase = quest::BattlePhase::Reading;
      p.equipped[0] = uint16_t(3 + stage * 10);
      p.equipped[1] = uint16_t(100 + stage * 7);
      p.equipped[2] = uint16_t(156 + stage * 6);
      for (int item : {0, 1, 3, 4, 6, 9, 13, 23, 100, 107, 114, 156, 162, 168}) quest::grantItem(p, item);
      quest::restoreStoryRewards(p);
      quest::reconcileCollection(p);
      quest::encode(p, bytes);
      assert(quest::decode(bytes, decoded) && decoded.campaign.observedSeconds == summary.seconds);
      write(out / ("quest-" + std::to_string(stage) + "-" + std::to_string(gender) + ".bin"), bytes,
            quest::kProfileBytes);
      if (stage == 1 && gender == 0)
        write(card / ".crosspoint/reading-adventure-v2/reading-quest-v10.dat", bytes, quest::kProfileBytes);
    }
  }
  static focus::State focusState, focusCopy;
  const int project = focus::addProject(focusState, "A year of reading");
  assert(project >= 0);
  for (const char* name : {"Read a chapter", "Collect favorite passages", "Review new words", "Visit the library"}) {
    const int task = focus::addTask(focusState, project, name);
    assert(task >= 0);
    focusState.tasks[task].planned = 4;
    focusState.tasks[task].completed = 2;
  }
  const int todayTask = focus::addTask(focusState, project, "Twenty quiet minutes", focus::TaskList::Today);
  assert(todayTask >= 0 && focus::selectTask(focusState, todayTask));
  focusState.timer.completed = 12;
  uint32_t generation = 0;
  focus::encode(focusState, 1, bytes);
  assert(focus::decode(bytes, focusCopy, generation));
  write(card / ".crosspoint/focus-a.bin", bytes, focus::kFileBytes);
  tools::State counter, counterCopy;
  counter.sequence = 1;
  counter.count = 128;
  tools::encode(counter, bytes);
  assert(tools::decode(bytes, counterCopy) && counterCopy.count == 128);
  write(card / ".crosspoint/tools.dat", bytes, tools::kStateBytes);
  std::puts("Validated: 15 PET profiles, 3 generations, 6 Quest profiles and Focus; one shared reading journal.");
}

// Host-only virtual-card fixtures, encoded and validated by production PET rules.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../src/apps_local/pet/PetHistory.h"
#include "../../src/apps_local/pet/PetReadingTimeline.h"
#include "../../src/apps_local/reading/ReadingStatsCore.h"

namespace {
constexpr const char* AGES[] = {"egg", "newborn", "baby", "juvenile", "adult"};
constexpr const char* SPECIES[] = {"silva", "nox", "ember"};
constexpr const char* NAMES[] = {"BOSQUE", "LUNA", "BRASA"};
enum class Scenario { Normal, Hungry, Dirty, Sick, Evolution, Death };
struct Scene {
  Scenario kind;
  const char* name;
};
constexpr Scene SCENARIOS[] = {{Scenario::Normal, "normal"},       {Scenario::Hungry, "hungry"},
                               {Scenario::Dirty, "dirty"},         {Scenario::Sick, "sick"},
                               {Scenario::Evolution, "evolution"}, {Scenario::Death, "death"}};

void write(const char* folder, const char* name, const uint8_t* data, size_t size) {
  char path[1024];
  std::snprintf(path, sizeof(path), "%s/%s.bin", folder, name);
  FILE* file = std::fopen(path, "wb");
  assert(file && std::fwrite(data, 1, size, file) == size);
  std::fclose(file);
}

void journal(const char* folder, unsigned age, int64_t now, int offset, pet::Profile& base) {
  const auto total = pet::stageStart(pet::Stage(age)) + (age ? 60 : 600);
  const unsigned minutes = unsigned(total / 60), days = age + 1;
  char path[1024];
  std::snprintf(path, sizeof(path), "%s/%s-journal.bin", folder, AGES[age]);
  FILE* file = std::fopen(path, "wb");
  assert(file);
  const uint8_t header[16] = {'C', 'R', 'S', '1', 0x80, 1};
  assert(std::fwrite(header, 1, sizeof(header), file) == sizeof(header));
  readingstats::Book previous;
  uint8_t bytes[readingstats::kRecordBytes];
  uint64_t earnedFood = 0, earnedMedicine = 0;
  for (unsigned day = 0; day < days; ++day) {
    const unsigned count = minutes / days + (day < minutes % days ? 1 : 0);
    earnedFood += pet::dailyFood(count * 60);
    earnedMedicine += pet::dailyMedicine(count * 60);
    for (unsigned minute = 0; minute < count; ++minute) {
      readingstats::Record record;
      record.epoch = now - 300 - int64_t(days - day - 1) * 86400 - int64_t(count - minute) * 60;
      record.seconds = 60;
      record.localDay = int((record.epoch + offset) / 86400);
      record.secondOfDay = unsigned((record.epoch + offset) % 86400);
      std::strcpy(record.book.document, "0123456789abcdef0123456789abcdef");
      std::strcpy(record.book.title, "Lectura de muestra de PET");
      readingstats::updateBook(record, previous);
      previous = record.book;
      readingstats::encode(record, bytes);
      readingstats::Record decoded;
      assert(readingstats::decode(bytes, decoded));
      assert(decoded.seconds == 60 && decoded.epoch == record.epoch);
      assert(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
      if (!day && !minute) {
        base.bornDay = record.localDay;
        base.birthDaySeconds = 0;
      }
    }
  }
  std::fclose(file);
  base.created = true;
  base.generation = 4;
  base.careAt = now;
  base.lastReadAt = now - 300;
  base.readingSeconds = base.noticedSeconds = total;
  base.noticedStage = pet::Stage(age);
  base.earnedFood = base.noticedFood = earnedFood;
  base.earnedMedicine = base.noticedMedicine = earnedMedicine;
  base.saciety = 72;
  base.joy = 84;
  base.hygiene = 88;
  base.project = 14;
  base.projectStartedSeconds = total > 1500 ? total - 1500 : 0;
  base.memories = uint16_t((1u << (age + 1)) - 1);
  for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
    base.equipped[slot] = uint8_t(pet::objectAt(slot, 0));
    pet::own(base, base.equipped[slot]);
  }
  for (unsigned id = 0; id < pet::OBJECT_COUNT; ++id) pet::own(base, id);
  assert(pet::ownedCount(base) == pet::CATALOG_COUNT);

  pet::ReadingTimeline timeline;
  timeline.begin(base, now);
  file = std::fopen(path, "rb");
  assert(file && std::fseek(file, sizeof(header), SEEK_SET) == 0);
  readingstats::Summary summary;
  summary.anchorDay = int((now + offset) / 86400);
  while (std::fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes)) {
    readingstats::Record record;
    assert(readingstats::decode(bytes, record));
    summary.add(record);
    timeline.add(record.epoch, record.seconds, record.localDay, record.secondOfDay);
  }
  assert(!std::ferror(file));
  std::fclose(file);
  auto draft = base;
  timeline.applyProvisions(draft, base);
  timeline.finish(now);
  assert(timeline.valid() && !timeline.endedAt());
  assert(draft.earnedFood == base.earnedFood && draft.earnedMedicine == base.earnedMedicine);
  assert(summary.seconds == total && pet::stage(total) == pet::Stage(age));
}
void generations(const char* folder, int64_t now, int offset) {
  pet::History history;
  static constexpr unsigned days[] = {7, 12, 18};
  static constexpr unsigned endedDaysAgo[] = {35, 21, 7};
  static constexpr unsigned ages[] = {10, 16, 24};
  for (unsigned species = 0; species < 3; ++species) {
    pet::Profile p;
    p.created = true;
    p.generation = species + 1;
    p.species = pet::Species(species);
    p.readingSeconds = (8 + species * 12) * 3600;
    p.sharedDays = days[species];
    p.endedAt = now - int64_t(endedDaysAgo[species]) * 86400;
    const auto endedDay = int32_t((p.endedAt + offset) / 86400);
    p.bornDay = endedDay - ages[species];
    p.lifeStatus = species == 1 ? pet::LifeStatus::Lost : pet::LifeStatus::Retired;
    std::snprintf(p.name, sizeof(p.name), "%s", NAMES[species]);
    assert(pet::addLife(history, p, endedDay));
  }
  std::array<uint8_t, pet::HISTORY_BYTES> bytes{};
  pet::encodeHistory(history, bytes.data());
  pet::History decoded;
  assert(pet::decodeHistory(bytes.data(), decoded) && decoded.count == 3);
  for (unsigned i = 0; i < 3; ++i)
    assert(decoded.records[i].generation == i + 1 && decoded.records[i].species == pet::Species(i) &&
           decoded.records[i].ageKnown && decoded.records[i].ageDays == ages[i]);
  write(folder, "history", bytes.data(), bytes.size());
}
}  // namespace

int main(int argc, char** argv) {
  assert(argc == 4);
  const int64_t now = std::strtoll(argv[2], nullptr, 10);
  const int offset = std::atoi(argv[3]);
  generations(argv[1], now, offset);
  unsigned cases = 0, evolutions = 0, deaths = 0;
  std::array<uint8_t, pet::STATE_BYTES> bytes{};
  for (unsigned age = 0; age < 5; ++age) {
    pet::Profile base;
    journal(argv[1], age, now, offset, base);
    for (unsigned species = 0; species < 3; ++species)
      for (const auto& scenario : SCENARIOS) {
        if (!age && scenario.kind != Scenario::Normal) continue;
        auto p = base;
        p.species = pet::Species(species);
        std::snprintf(p.name, sizeof(p.name), "%s", NAMES[species]);
        switch (scenario.kind) {
          case Scenario::Normal:
            break;
          case Scenario::Hungry:
            p.saciety = 15;
            break;
          case Scenario::Dirty:
            p.hygiene = 15;
            break;
          case Scenario::Sick:
            p.illness = pet::Illness::Cold;
            p.hygiene = 45;
            break;
          case Scenario::Evolution:
            p.noticedStage = pet::Stage(age - 1);
            ++evolutions;
            break;
          case Scenario::Death:
            p.mode = pet::Mode::Normal;
            assert(pet::endLife(p, pet::LifeStatus::Lost, now - 60));
            ++deaths;
            break;
        }
        pet::encode(p, bytes.data());
        pet::Profile decoded;
        assert(pet::decode(bytes.data(), decoded));
        assert(pet::ownedCount(decoded) == pet::CATALOG_COUNT);
        assert(decoded.species == p.species && pet::stage(decoded.readingSeconds) == pet::Stage(age));
        assert(decoded.noticedStage == p.noticedStage && decoded.illness == p.illness);
        assert(decoded.lifeStatus == p.lifeStatus && decoded.endedAt == p.endedAt);
        assert(pet::deathPending(decoded) == (scenario.kind == Scenario::Death));
        char name[128];
        std::snprintf(name, sizeof(name), "%s-%s-%s", SPECIES[species], AGES[age], scenario.name);
        write(argv[1], name, bytes.data(), bytes.size());
        ++cases;
      }
  }
  assert(cases == 75 && evolutions == 12 && deaths == 12);
  std::printf("PET simulator: %u validated profiles, 15 species/stage combinations, %u evolutions, %u deaths\n", cases,
              evolutions, deaths);
}

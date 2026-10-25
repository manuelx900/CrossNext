#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <initializer_list>

#include "../../src/apps_local/pet/PetNeeds.h"
#include "../../src/apps_local/pet/PetReadingTimeline.h"
#include "../../src/apps_local/reading/ReadingStatsCore.h"

namespace {
int dayAt(int64_t epoch) {
  const auto stamp = time_t(epoch);
  tm date{};
  localtime_r(&stamp, &date);
  return readingstats::dayIndex({date.tm_year + 1900, date.tm_mon + 1, date.tm_mday});
}
unsigned secondAt(int64_t epoch) {
  const auto stamp = time_t(epoch);
  tm date{};
  localtime_r(&stamp, &date);
  return unsigned(date.tm_hour * 3600 + date.tm_min * 60 + date.tm_sec);
}
struct Run {
  pet::Profile profile;
  unsigned meals = 0, cleanings = 0, cures = 0;
};
Run simulate(unsigned days, unsigned minutes, int64_t end, bool attentive = true) {
  Run run;
  auto& p = run.profile;
  p.created = true;
  std::strcpy(p.name, "BOSQUE");
  p.bornDay = dayAt(end - int64_t(days - 1) * 86400 - minutes * 60);
  p.careAt = end - int64_t(days - 1) * 86400 - minutes * 60;
  p.lastReadAt = p.careAt;
  pet::Profile previous = p;
  previous.created = false;
  pet::captureMoments(p, previous, p.bornDay);
  for (unsigned day = 0; day < days; ++day) {
    const auto at = end - int64_t(days - day - 1) * 86400;
    const int today = dayAt(at);
    previous = p;
    pet::ReadingTimeline timeline;
    timeline.begin(p, at);
    for (unsigned oldDay = 0; oldDay <= day; ++oldDay) {
      const auto start = end - int64_t(days - oldDay - 1) * 86400 - minutes * 60;
      for (unsigned minute = 0; minute < minutes; ++minute) {
        const auto epoch = start + minute * 60;
        timeline.add(epoch, 60, dayAt(epoch), secondAt(epoch));
      }
    }
    assert(timeline.valid());
    timeline.applyProvisions(p, previous);
    assert(pet::reconcileReading(p, uint64_t(day + 1) * minutes * 60, at, at));
    if (pet::canCare(p) && attentive) {
      if (p.illness != pet::Illness::Healthy && pet::cure(p)) ++run.cures;
      if (p.hygiene < 80 && pet::clean(p)) ++run.cleanings;
      while (p.saciety <= 75 && pet::feed(p)) ++run.meals;
      pet::petOnce(p, today);
      // One visit to the catalogue per reading day, no automatic spending of credit.
      if (pet::owns(p, p.project))
        for (unsigned item = 0; item < pet::OBJECT_COUNT; ++item)
          if (!pet::owns(p, item)) {
            pet::chooseProject(p, item);
            break;
          }
    }
    p.sharedDays = p.streak = uint16_t(day + 1);
    pet::captureMoments(p, previous, today);
    assert(p.heritageSeconds == 0 && p.bonusMedicine == 0);
    assert(p.lifeStatus == pet::LifeStatus::Active && !pet::absenceExpired(p, at));
  }
  p.noticedStage = pet::stage(p.readingSeconds);
  p.noticedSeconds = p.readingSeconds;
  p.noticedFood = p.earnedFood;
  p.noticedMedicine = p.earnedMedicine;
  assert(p.readingSeconds == uint64_t(days) * minutes * 60);
  uint8_t bytes[pet::STATE_BYTES];
  pet::encode(p, bytes);
  pet::Profile decoded;
  assert(pet::decode(bytes, decoded));
  assert(decoded.readingSeconds == p.readingSeconds && pet::food(decoded) == pet::food(p));
  assert(decoded.hygiene == p.hygiene && pet::medicine(decoded) == pet::medicine(p));
  return run;
}
void writeState(const char* folder, const char* name, const pet::Profile& p) {
  char path[700];
  std::snprintf(path, sizeof(path), "%s/%s-state.bin", folder, name);
  auto* file = std::fopen(path, "wb");
  assert(file);
  uint8_t bytes[pet::STATE_BYTES];
  pet::encode(p, bytes);
  assert(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
  std::fclose(file);
}
void writeJournal(const char* folder, const char* name, unsigned days, unsigned minutes, int64_t end) {
  char path[700];
  std::snprintf(path, sizeof(path), "%s/%s-journal.bin", folder, name);
  auto* file = std::fopen(path, "wb");
  assert(file);
  const uint8_t header[16] = {'C', 'R', 'S', '1', 0x80, 1};
  assert(std::fwrite(header, 1, sizeof(header), file) == sizeof(header));
  readingstats::Book previous;
  for (unsigned day = 0; day < days; ++day)
    for (unsigned minute = 0; minute < minutes; ++minute) {
      readingstats::Record record;
      record.epoch = end - int64_t(days - day - 1) * 86400 - int64_t(minutes - minute) * 60;
      record.seconds = 60;
      record.localDay = dayAt(record.epoch);
      const auto stamp = time_t(record.epoch);
      tm date{};
      localtime_r(&stamp, &date);
      record.secondOfDay = unsigned(date.tm_hour * 3600 + date.tm_min * 60 + date.tm_sec);
      std::strcpy(record.book.document, "0123456789abcdef0123456789abcdef");
      std::strcpy(record.book.title, "Reading with a new companion");
      readingstats::updateBook(record, previous);
      previous = record.book;
      uint8_t bytes[readingstats::kRecordBytes];
      readingstats::encode(record, bytes);
      assert(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    }
  std::fclose(file);
}
}  // namespace
int main(int argc, char** argv) {
  const int64_t end = int64_t(std::time(nullptr)) - 60;
  pet::Profile fresh;
  assert(!fresh.created && pet::food(fresh) == 0 && pet::medicine(fresh) == 1);
  if (argc > 1) {
    writeState(argv[1], "fresh", fresh);
    writeJournal(argv[1], "fresh", 0, 0, end);
  }
  std::puts(
      "days,minutes_per_day,reading_minutes,food_earned,meals,food_left,remedies_left,cleanings,cures,decorations,"
      "stage");
  for (unsigned minutes : {15u, 30u, 60u})
    for (unsigned days : {1u, 7u, 14u}) {
      const auto run = simulate(days, minutes, end);
      const auto& p = run.profile;
      std::printf("%u,%u,%llu,%llu,%u,%llu,%llu,%u,%u,%u,%u\n", days, minutes,
                  static_cast<unsigned long long>(p.readingSeconds / 60), static_cast<unsigned long long>(p.earnedFood),
                  run.meals, static_cast<unsigned long long>(pet::food(p)),
                  static_cast<unsigned long long>(pet::medicine(p)), run.cleanings, run.cures, pet::ownedCount(p),
                  unsigned(pet::stage(p.readingSeconds)));
      if (days == 1 && minutes == 15) assert(pet::stage(p.readingSeconds) == pet::Stage::Egg);
      if (minutes == 15 && days == 7) assert(pet::food(p) == 11 && pet::medicine(p) == 1);
      if (minutes == 15 && days == 14) assert(pet::food(p) == 18 && pet::medicine(p) == 1);
      if (argc > 1) {
        char name[40];
        std::snprintf(name, sizeof(name), "day%u-%u", days, minutes);
        writeState(argv[1], name, p);
        writeJournal(argv[1], name, days, minutes, end);
      }
    }
  const auto unattended = simulate(14, 15, end, false);
  assert(unattended.profile.illness != pet::Illness::Healthy && unattended.profile.saciety == 20);
  std::puts("PASS: fresh stock, 9 reading/care/decoration scenarios, unattended needs and snapshot round trips.");
}

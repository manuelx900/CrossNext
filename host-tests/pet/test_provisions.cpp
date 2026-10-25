#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../../src/apps_local/pet/PetReadingTimeline.h"

namespace {
constexpr int64_t START = 1700000000;
void checksum(uint8_t* bytes, size_t length = pet::STATE_BYTES) {
  const auto hash = pet::checksum(bytes, length - 4);
  for (unsigned i = 0; i < 4; ++i) bytes[length - 4 + i] = uint8_t(hash >> (i * 8));
}
pet::Profile replay(const pet::Profile& saved, unsigned minutes, unsigned days = 1, bool legacyClock = false) {
  auto draft = saved;
  pet::ReadingTimeline timeline;
  timeline.begin(saved, START + days * 86400);
  if (legacyClock) assert(timeline.migration());
  for (unsigned day = 0; day < days; ++day)
    for (unsigned minute = 0; minute < minutes; ++minute)
      timeline.add(START + day * 86400 + minute * 60, 60, 20000 + day);
  assert(timeline.valid());
  timeline.applyProvisions(draft, saved);
  pet::reconcileReading(draft, uint64_t(minutes) * days * 60, START + (days - 1) * 86400 + minutes * 60,
                        START + days * 86400);
  return draft;
}
}  // namespace

int main() {
  pet::Profile fresh;
  fresh.created = true;
  fresh.lastReadAt = START;
  for (const auto minutes : {0u, 4u, 5u, 9u, 10u, 14u, 15u, 29u, 30u, 59u, 60u, 119u, 120u, 240u, 1200u}) {
    const auto p = replay(fresh, minutes);
    const unsigned food =
        (minutes >= 5) + (minutes >= 10) + (minutes >= 15) + (minutes >= 30) + (minutes >= 60) + (minutes >= 120);
    assert(pet::food(p) == food);
    assert(pet::medicine(p) == 1 + unsigned(minutes >= 30) + unsigned(minutes >= 120));
    assert(p.readingSeconds == uint64_t(minutes) * 60);
    const auto repeated = replay(p, minutes);
    assert(repeated.earnedFood == p.earnedFood && repeated.earnedMedicine == p.earnedMedicine);
  }
  const auto longDay = replay(fresh, 1200);
  assert(pet::food(longDay) == 6 && pet::medicine(longDay) == 3 && longDay.readingSeconds == 72000);
  const auto twoDays = replay(fresh, 120, 2);
  assert(pet::food(twoDays) == 12 && pet::medicine(twoDays) == 5);
  const auto day15 = replay(fresh, 15);
  const auto day30 = replay(day15, 30);
  assert(pet::food(day30) == 4 && pet::medicine(day30) == 2);
  assert(pet::food(replay(fresh, 30)) == pet::food(day30));

  // The same second-by-second journal crosses midnight without losing partial minutes.
  pet::ReadingTimeline midnight;
  midnight.begin(fresh, START + 1000);
  for (unsigned second = 0; second < 600; ++second) midnight.add(START + second, 1, second < 300 ? 20000 : 20001);
  auto atMidnight = fresh;
  midnight.applyProvisions(atMidnight, fresh);
  assert(pet::food(atMidnight) == 2);

  // ReaderSession also writes one unsplit record spanning one or more midnights.
  for (const auto seconds : {1200u, 3600u, 180000u}) {
    pet::ReadingTimeline whole, split;
    whole.begin(fresh, START + seconds);
    split.begin(fresh, START + seconds);
    whole.add(START, seconds, 20000, 84600);
    for (unsigned at = 0; at < seconds; ++at) {
      const auto local = 84600 + at;
      split.add(START + at, 1, 20000 + local / 86400, local % 86400);
    }
    auto a = fresh, b = fresh;
    whole.applyProvisions(a, fresh);
    split.applyProvisions(b, fresh);
    assert(a.earnedFood == b.earnedFood && a.earnedMedicine == b.earnedMedicine);
    if (seconds == 3600) assert(a.earnedFood == 8 && a.earnedMedicine == 2);
  }
  pet::ReadingTimeline shortCrossing;
  shortCrossing.begin(fresh, START + 1200);
  shortCrossing.add(START, 1200, 20000, 85800);
  auto tenEach = fresh;
  shortCrossing.applyProvisions(tenEach, fresh);
  assert(tenEach.earnedFood == 4 && tenEach.earnedMedicine == 0);
  // The saved checkpoint can cut through a single spanning session.
  auto halfSaved = fresh;
  halfSaved.readingSeconds = 1800;
  halfSaved.earnedFood = 4;
  halfSaved.earnedMedicine = 1;
  pet::ReadingTimeline partial;
  partial.begin(halfSaved, START + 3600);
  partial.add(START, 3600, 20000, 84600);
  auto afterPartial = halfSaved;
  partial.applyProvisions(afterPartial, halfSaved);
  assert(afterPartial.earnedFood == 8 && afterPartial.earnedMedicine == 2);
  partial.applyProvisions(afterPartial, halfSaved);
  assert(afterPartial.earnedFood == 8 && afterPartial.earnedMedicine == 2);

  // UTC epochs increase while switching between UTC+14 and UTC-12 revisits dates.
  pet::ReadingTimeline zones;
  zones.begin(fresh, START + 3600);
  zones.add(START, 300, 20002);
  zones.add(START + 300, 300, 20000);
  zones.add(START + 600, 300, 20001);
  zones.add(START + 900, 600, 20002);
  zones.add(START + 1500, 600, 20000);
  zones.add(START + 2100, 600, 20001);
  auto travelling = fresh;
  zones.applyProvisions(travelling, fresh);
  assert(zones.valid() && travelling.earnedFood == 9 && travelling.earnedMedicine == 0);

  // Baseline reading consumes the day's thresholds but is never paid to the new pet.
  auto bornLate = fresh;
  bornLate.baselineSeconds = 900;
  assert(pet::food(replay(bornLate, 30)) == 1);
  // Restarting a generation preserves stock and the global journal baseline.
  auto next = fresh;
  next.earnedFood = day15.earnedFood;
  next.earnedMedicine = day15.earnedMedicine;
  next.heritageSeconds = day15.readingSeconds;
  next.baselineSeconds = day15.readingSeconds;
  assert(pet::food(replay(next, 30)) == 4);

  auto clockless = fresh;
  clockless.lastReadAt = 0;
  assert(pet::food(replay(clockless, 30, 1, true)) == 4);
  pet::ReadingTimeline invalid;
  invalid.begin(fresh, START + 100);
  invalid.add(START + 101, 300, 20000);
  assert(!invalid.valid());

  // PET1–6 stock and spent counters survive migration; PET7 never recalculates it.
  uint8_t bytes[pet::STATE_BYTES];
  auto old = fresh;
  old.readingSeconds = 72000;
  old.heritageSeconds = 7200;
  old.noticedSeconds = 36000;
  old.spentFood = 17;
  old.spentMedicine = 3;
  old.bonusMedicine = 2;
  for (char version : {'1', '2', '3', '4', '5', '6'}) {
    pet::encode(old, bytes);
    bytes[3] = version;
    if (version < '6') bytes[pet::MOMENTS_VERSION_OFFSET] = 0;
    const auto size = version == '6' ? pet::STATE_BYTES : pet::LEGACY_STATE_BYTES;
    checksum(bytes, size);
    pet::Profile migrated;
    assert(pet::decode(bytes, migrated, size));
    assert(pet::food(migrated) == pet::projectReading(migrated) / 300 - migrated.spentFood);
    assert(pet::medicine(migrated) ==
           1 + pet::projectReading(migrated) / 900 + migrated.bonusMedicine - migrated.spentMedicine);
    const auto food = pet::food(migrated), medicine = pet::medicine(migrated);
    pet::encode(migrated, bytes);
    assert(!std::memcmp(bytes, "PET7", 4));
    pet::Profile restored;
    assert(pet::decode(bytes, restored) && pet::food(restored) == food && pet::medicine(restored) == medicine);
  }
  pet::encode(day30, bytes);
  pet::Profile restored;
  assert(pet::decode(bytes, restored));
  assert(restored.earnedFood == 4 && restored.earnedMedicine == 1);
  std::puts(
      "PET provisions: boundaries, daily cap, replay, partial sessions, midnight, generations and PET1-7 migration "
      "passed");
}

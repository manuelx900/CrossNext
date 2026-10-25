#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/apps_local/habits/HabitsCore.h"

namespace {
int checks = 0;
int failures = 0;
#define CHECK(expr)                                  \
  do {                                               \
    ++checks;                                        \
    if (!(expr)) {                                   \
      ++failures;                                    \
      std::printf("FAIL %d: %s\n", __LINE__, #expr); \
    }                                                \
  } while (0)

void dates() {
  CHECK(habits::dayIndex(2020, 1, 1) == habits::kMinDay);
  CHECK(habits::dayIndex(2099, 12, 31) == habits::kMaxDay);
  CHECK(habits::weekday(habits::dayIndex(2026, 10, 15)) == 3);
  CHECK(habits::dayIndex(2024, 3, 1) - habits::dayIndex(2024, 2, 28) == 2);
  CHECK(habits::dayIndex(2025, 2, 29) == -1);
  int day = -1;
  CHECK(!habits::parseDate("2026-1a-15", day));
  CHECK(!habits::parseDate("2026-10-15extra", day));
  CHECK(!habits::parseDate("2100-01-01", day));
  for (int n = habits::kMinDay; n <= habits::kMaxDay; ++n) {
    int y, m, d;
    habits::dateOfDay(n, y, m, d);
    CHECK(habits::dayIndex(y, m, d) == n);
    char text[16];
    habits::formatDate(n, text, sizeof(text));
    CHECK(habits::parseDate(text, day) && day == n);
  }
}

void checkins() {
  habits::Store store;
  const int start = habits::dayIndex(2026, 10, 1);
  CHECK(habits::add(store, "Exercise", start) == habits::NameResult::Ok);
  CHECK(habits::add(store, "exercise", start) == habits::NameResult::Duplicate);
  CHECK(habits::add(store, "   ", start) == habits::NameResult::Empty);
  CHECK(habits::add(store, "012345678901234567890123456789012", start) == habits::NameResult::TooLong);
  CHECK(habits::toggle(store, 0, start, start));
  CHECK(habits::stats(store, 0, start).current == 1);
  CHECK(habits::stats(store, 0, start + 1).current == 1);  // Today is still pending.
  CHECK(habits::stats(store, 0, start + 2).current == 0);
  CHECK(habits::toggle(store, 0, start + 1, start + 1));
  CHECK(habits::stats(store, 0, start + 1).current == 2);
  CHECK(!habits::toggle(store, 0, start + 2, start + 1));
  CHECK(!habits::toggle(store, 0, start - 1, start + 1));
  CHECK(habits::toggle(store, 0, start, start + 1));
  CHECK(habits::stats(store, 0, start + 1).current == 1);
  CHECK(habits::stats(store, 0, start + 1).best == 1);
  CHECK(habits::stats(store, 0, start + 1).recentDone == 1);
  CHECK(habits::stats(store, 0, start + 1).recentDays == 2);
  CHECK(habits::rename(store, 0, "Daily exercise") == habits::NameResult::Ok);
  CHECK(habits::checked(store, 0, start + 1));
  CHECK(habits::add(store, "Read", start + 1) == habits::NameResult::Ok);
  CHECK(habits::remove(store, 0));
  CHECK(store.count == 1 && std::strcmp(store.items[0].name, "Read") == 0);
  CHECK(habits::stats(store, 0, start + 1).total == 0);
  CHECK(!habits::remove(store, -1));
  CHECK(!habits::advance(store, start));
  for (int n = 1; n < habits::kMaxHabits; ++n) {
    char name[24];
    std::snprintf(name, sizeof(name), "Habit %d", n);
    CHECK(habits::add(store, name, start + 1) == habits::NameResult::Ok);
  }
  CHECK(habits::add(store, "One more", start + 1) == habits::NameResult::Full);
}

void rollingHistory() {
  habits::Store store;
  const int start = habits::dayIndex(2023, 1, 1);
  habits::add(store, "Read", start);
  for (int n = 0; n < 900; ++n) CHECK(habits::toggle(store, 0, start + n, start + n));
  const auto all = habits::stats(store, 0, start + 899);
  CHECK(all.total == 900 && all.best == 900 && all.current == 900);
  CHECK(all.recentDone == 30 && all.recentDays == 30);
  CHECK(!habits::toggle(store, 0, start, start + 899));
  CHECK(habits::toggle(store, 0, start + 899, start + 899));
  CHECK(habits::stats(store, 0, start + 899).best == 899);
  CHECK(habits::stats(store, 0, start + 899).current == 899);
  CHECK(habits::toggle(store, 0, start + 901, start + 901));
  CHECK(habits::stats(store, 0, start + 901).current == 1);
  CHECK(habits::stats(store, 0, start + 901).total == 900);
  CHECK(habits::advance(store, start + 2000));
  CHECK(habits::stats(store, 0, start + 2000).total == 900);
  CHECK(habits::stats(store, 0, start + 2000).best == 899);
  CHECK(habits::stats(store, 0, start + 2000).current == 0);
}

void referenceModel() {
  habits::Store store;
  const int start = habits::dayIndex(2020, 1, 1);
  habits::add(store, "Exercise", start);
  std::vector<bool> done(1800, false);
  uint32_t random = 123456789;
  for (int n = 0; n < 1800; ++n) {
    random = random * 1664525u + 1013904223u;
    if (random % 4 != 0) {
      done[n] = true;
      CHECK(habits::toggle(store, 0, start + n, start + n));
    } else
      CHECK(habits::advance(store, start + n));
    if (n > 0 && random % 7 == 0) {
      const int back = std::min(n, static_cast<int>(random % 100));
      done[n - back] = !done[n - back];
      CHECK(habits::toggle(store, 0, start + n - back, start + n));
    }
    unsigned total = 0, run = 0, best = 0, current = 0;
    for (int d = 0; d <= n; ++d) {
      if (done[d]) {
        ++total;
        ++run;
        best = std::max(best, run);
      } else
        run = 0;
    }
    int tail = done[n] ? n : n - 1;
    while (tail >= 0 && done[tail--]) ++current;
    const auto actual = habits::stats(store, 0, start + n);
    CHECK(actual.total == total && actual.best == best && actual.current == current);
  }
}

void persistence() {
  habits::Store store, restored;
  std::array<uint8_t, habits::kFileSize> bytes{};
  uint32_t generation = 0;
  CHECK(habits::encode(store, 1, bytes.data(), bytes.size()));
  CHECK(habits::decode(bytes.data(), bytes.size(), restored, generation));
  CHECK(restored.count == 0 && restored.latestDay == -1 && generation == 1);
  const int today = habits::dayIndex(2026, 10, 15);
  habits::add(store, "Read", today - 3);
  habits::toggle(store, 0, today - 2, today);
  CHECK(habits::encode(store, 42, bytes.data(), bytes.size()));
  CHECK(habits::decode(bytes.data(), bytes.size(), restored, generation));
  CHECK(generation == 42 && habits::checked(restored, 0, today - 2));
  CHECK(!habits::decode(bytes.data(), bytes.size() - 1, restored, generation));
  for (size_t n = 0; n < bytes.size(); ++n) {
    bytes[n] ^= 0x10;
    CHECK(!habits::decode(bytes.data(), bytes.size(), restored, generation));
    bytes[n] ^= 0x10;
  }
  bytes[12] = 255;
  const uint32_t crc = habits::checksum(bytes.data(), bytes.size() - 4);
  for (int n = 0; n < 4; ++n) bytes[bytes.size() - 4 + n] = static_cast<uint8_t>(crc >> (8 * n));
  CHECK(!habits::decode(bytes.data(), bytes.size(), restored, generation));
  std::printf("Store: %zu bytes; encoded file: %zu bytes\n", sizeof(habits::Store), habits::kFileSize);
}
}  // namespace

int main() {
  dates();
  checkins();
  rollingHistory();
  referenceModel();
  persistence();
  std::printf("Habits: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

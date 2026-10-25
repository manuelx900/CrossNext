#include <HalStorage.h>

#include <cstdio>

#include "../../src/apps_local/habits/HabitsStore.h"

namespace {
int checks = 0, failures = 0;
#define CHECK(expr)                                  \
  do {                                               \
    ++checks;                                        \
    if (!(expr)) {                                   \
      ++failures;                                    \
      std::printf("FAIL %d: %s\n", __LINE__, #expr); \
    }                                                \
  } while (0)
}  // namespace

int main() {
  habits::Store store, scratch;
  habits::Repository repo;
  CHECK(repo.load(store, scratch) == habits::LoadResult::Empty);
  CHECK(testStorage.writes == 0);
  const int today = habits::dayIndex(2026, 10, 15);
  habits::add(store, "Exercise", today);
  CHECK(repo.save(store));
  habits::toggle(store, 0, today, today);
  CHECK(repo.save(store));
  habits::Repository reopened;
  habits::Store restored;
  CHECK(reopened.load(restored, scratch) == habits::LoadResult::Loaded);
  CHECK(habits::checked(restored, 0, today));
  const auto goodFiles = testStorage.files;
  habits::toggle(store, 0, today, today);
  for (int length : {0, 4, 16, 120, static_cast<int>(habits::kFileSize - 1)}) {
    testStorage.files = goodFiles;
    testStorage.writeLimit = length;
    CHECK(!reopened.save(store));
    habits::Repository afterPowerLoss;
    CHECK(afterPowerLoss.load(restored, scratch) == habits::LoadResult::Recovered);
    CHECK(habits::checked(restored, 0, today));
  }
  testStorage.writeLimit = -1;
  testStorage.files = goodFiles;
  testStorage.corruptReadChecksum = true;
  CHECK(!reopened.save(store));
  testStorage.corruptReadChecksum = false;
  testStorage.files = goodFiles;
  testStorage.files["/.crosspoint/habits-b.bin"][30] ^= 16;
  habits::Repository corrupted;
  const int writes = testStorage.writes;
  CHECK(corrupted.load(restored, scratch) == habits::LoadResult::Recovered);
  CHECK(!habits::checked(restored, 0, today));
  testStorage.files["/.crosspoint/habits-a.bin"][20] ^= 16;
  CHECK(corrupted.load(restored, scratch) == habits::LoadResult::Damaged);
  CHECK(testStorage.writes == writes);
  testStorage.available = false;
  CHECK(corrupted.load(restored, scratch) == habits::LoadResult::Unavailable);
  CHECK(!reopened.save(store));
  std::printf("Habits storage: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

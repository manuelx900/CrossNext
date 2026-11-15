#include <HalStorage.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../src/apps_local/pomodoro/PomodoroHistoryStore.h"
#include "../../src/apps_local/ui/SectionNavigation.h"

namespace p = pomodoro;
unsigned checks = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                               \
    }                                             \
  } while (0)
int main() {
  const int today = readingstats::dayIndex({2026, 11, 5});
  p::State state;
  p::Pending credit;
  CHECK(!p::advanceTracked(state, 1000, {today, 100}, {today, 101}, credit) && credit.empty());
  state.running = true;
  CHECK(!p::advanceTracked(state, 90000, {today, 100}, {today, 190}, credit));
  CHECK(credit.dates[0].day == today && credit.dates[0].milliseconds == 90000 && !credit.dates[0].sessions);
  state.remainingMs = 120000;
  CHECK(p::advanceTracked(state, 180000, {today, 86340}, {today + 1, 120}, credit));
  CHECK(credit.dates[0].milliseconds == 60000 && !credit.dates[0].sessions);
  CHECK(credit.dates[1].day == today + 1 && credit.dates[1].milliseconds == 60000 && credit.dates[1].sessions == 1);
  CHECK(state.completed == 1 && !state.running);
  state.running = true;
  CHECK(!p::advanceTracked(state, 30000, {today, 0}, {today, 30}, credit) && credit.empty());
  p::selectStage(state, 0);
  state.running = true;
  CHECK(!p::advanceTracked(state, 999, {}, {}, credit));
  CHECK(credit.undatedMs == 999 && !credit.undatedSessions);
  CHECK(!p::advanceTracked(state, 5000, {today, 100}, {today - 1, 100}, credit));
  CHECK(credit.undatedMs == 5000);
  CHECK(!p::advanceTracked(state, 5000, {today, 100}, {today + 1, 100}, credit));
  CHECK(credit.undatedMs == 5000);
  p::selectStage(state, 0);
  state.running = true;
  state.remainingMs = 1000;
  CHECK(p::advanceTracked(state, 1000, {today, 86399}, {today + 1, 0}, credit));
  CHECK(credit.dates[0].milliseconds == 1000 && credit.dates[0].sessions == 0);
  CHECK(credit.dates[1].day == today + 1 && credit.dates[1].milliseconds == 0 && credit.dates[1].sessions == 1);
  p::Stats stats;
  stats.begin(p::Period::Day, today);
  stats.add(pomodoro::Delta{today - 1, 70000, 2});
  stats.add(pomodoro::Delta{today, 90000, 1});
  stats.add(pomodoro::Delta{today + 1, 90000, 1});
  CHECK(stats.sessions == 1 && stats.milliseconds == 90000 && stats.counts[5] == 2 && stats.counts[6] == 1);
  stats.begin(p::Period::Month, readingstats::dayIndex({2024, 2, 20}));
  CHECK(stats.count == 29);
  stats.add(pomodoro::Delta{readingstats::dayIndex({2024, 2, 29}), 60000, 1});
  stats.add(pomodoro::Delta{readingstats::dayIndex({2024, 3, 1}), 60000, 1});
  CHECK(stats.sessions == 1 && stats.counts[28] == 1);
  stats.begin(p::Period::Year, today);
  for (int m = 1; m <= 12; ++m)
    stats.add(pomodoro::Delta{readingstats::dayIndex({2026, m, 1}), uint64_t(m) * 60000, uint32_t(m)});
  CHECK(stats.sessions == 78 && stats.counts[10] == 11 && stats.buckets[11] == 720000);
  CHECK(p::shiftPeriod(p::Period::Day, today, -1, today) == today - 1);
  CHECK(p::shiftPeriod(p::Period::Day, today, 1, today) == today + 1);
  CHECK(p::shiftPeriod(p::Period::Month, today, -1, today) == readingstats::dayIndex({2026, 10, 1}));
  CHECK(p::shiftPeriod(p::Period::Year, today, -1, today) == readingstats::dayIndex({2025, 1, 1}));
  credit = {};
  for (int n = 0; n < 9; ++n) credit.add(today - n, 1234, 1);
  CHECK(credit.undatedMs == 5 * 1234 && credit.undatedSessions == 5);
  uint8_t bytes[p::kHistoryBytes];
  p::encodeHistory(state, credit, 123, bytes);
  p::State restored;
  p::Pending decoded;
  uint32_t sequence = 0;
  CHECK(p::decodeHistory(bytes, restored, decoded, sequence) && sequence == 123 && !restored.running);
  CHECK(decoded.undatedMs == credit.undatedMs && decoded.dates[3].day == today - 3);
  for (size_t n = 0; n < sizeof(bytes); ++n) {
    bytes[n] ^= 1;
    CHECK(!p::decodeHistory(bytes, restored, decoded, sequence));
    bytes[n] ^= 1;
  }
  testStorage = {};
  p::Store legacy;
  state = {};
  state.completed = 18;
  CHECK(legacy.load(restored) && legacy.save(state));
  p::HistoryStore store;
  CHECK(store.load(restored) && restored.completed == 18);
  p::Pending pending;
  CHECK(store.save(restored, pending));
  auto writes = testStorage.writes;
  CHECK(store.save(restored, pending) && testStorage.writes == writes);
  pending.add(today, 1500123, 1);
  state.completed = 19;
  CHECK(store.save(state, pending) && pending.empty());
  p::HistoryStore reload;
  CHECK(reload.load(restored) && restored.completed == 19);
  CHECK(reload.beginScan());
  CHECK(reload.next(decoded) == p::Scan::Record && decoded.empty());
  CHECK(reload.next(decoded) == p::Scan::Record && decoded.dates[0].milliseconds == 1500123);
  CHECK(reload.next(decoded) == p::Scan::Finished);
  // Failed writes leave both the checkpoint and study delta retryable.
  const auto good = testStorage.files;
  for (int cut = 0; cut < int(p::kHistoryBytes); ++cut) {
    testStorage.files = good;
    p::HistoryStore torn;
    CHECK(torn.load(restored));
    pending = {};
    pending.add(today - 1, 30000, 1);
    state.completed = 20;
    testStorage.writeLimit = cut;
    CHECK(!torn.save(state, pending) && !pending.empty());
    testStorage.writeLimit = -1;
    p::HistoryStore recover;
    CHECK(recover.load(restored) && restored.completed == 19);
    CHECK(torn.save(state, pending) && pending.empty());
    p::HistoryStore retry;
    CHECK(retry.load(restored) && restored.completed == 20);
    stats.begin(p::Period::Month, today);
    CHECK(retry.beginScan());
    while (retry.next(decoded) == p::Scan::Record) stats.add(decoded);
    CHECK(stats.sessions == 2 && stats.milliseconds == 1530123);
  }
  testStorage.files = good;
  p::HistoryStore failedRead;
  testStorage.refuseReads = true;
  CHECK(!failedRead.load(restored));
  testStorage.refuseReads = false;
  pending.add(today, 123, 0);
  const auto before = testStorage.files;
  CHECK(!failedRead.save(state, pending) && before == testStorage.files);
  testStorage.files = good;
  testStorage.files[p::kHistoryPath][4] = 2;
  p::HistoryStore future;
  CHECK(!future.load(restored));
  const auto futureFiles = testStorage.files;
  CHECK(!future.save(state, pending) && futureFiles == testStorage.files);
  testStorage.files = good;
  p::HistoryStore failedClose;
  CHECK(failedClose.load(restored));
  testStorage.failClose = true;
  CHECK(!failedClose.save(state, pending) && !pending.empty());
  testStorage.failClose = false;
  CHECK(failedClose.save(state, pending) && pending.empty());
  p::HistoryStore afterClose;
  CHECK(afterClose.load(restored) && restored.completed == 20);
  // A short initial header write is retryable in the same repository.
  for (int cut = 0; cut < 16; ++cut) {
    testStorage = {};
    p::HistoryStore first;
    state = {};
    pending = {};
    pending.add(today, 5000, 0);
    CHECK(first.load(restored));
    testStorage.writeLimit = cut;
    CHECK(!first.save(state, pending));
    testStorage.writeLimit = -1;
    CHECK(first.save(state, pending) && pending.empty());
    p::HistoryStore firstReload;
    CHECK(firstReload.load(restored) && firstReload.beginScan());
    CHECK(firstReload.next(decoded) == p::Scan::Record && decoded.dates[0].milliseconds == 5000);
  }
  // A full checkpoint whose verification fails can be retried or recovered
  // after reboot, with its study delta counted exactly once.
  testStorage.files = good;
  p::HistoryStore verifyFailure;
  CHECK(verifyFailure.load(restored));
  pending = {};
  pending.add(today, 1000, 0);
  state.completed = 20;
  testStorage.corruptReadChecksum = true;
  CHECK(!verifyFailure.save(state, pending) && !pending.empty());
  testStorage.corruptReadChecksum = false;
  p::HistoryStore committed;
  CHECK(committed.load(restored) && restored.completed == 20);
  pending.add(today, 2000, 0);
  CHECK(verifyFailure.save(state, pending));
  p::HistoryStore once;
  CHECK(once.load(restored) && once.beginScan());
  stats.begin(p::Period::Month, today);
  while (once.next(decoded) == p::Scan::Record) stats.add(decoded);
  CHECK(stats.milliseconds == 1503123 && stats.sessions == 1);
  // Snapshot scans exclude subsequently appended records; live deltas can
  // be merged by the activity without double-counting them.
  CHECK(once.beginScan());
  pending.add(today, 1000, 0);
  CHECK(once.save(state, pending));
  stats.begin(p::Period::Month, today);
  while (once.next(decoded) == p::Scan::Record) stats.add(decoded);
  CHECK(stats.milliseconds == 1503123);
  CHECK(once.beginScan());
  stats.begin(p::Period::Month, today);
  while (once.next(decoded) == p::Scan::Record) stats.add(decoded);
  CHECK(stats.milliseconds == 1504123);
  // Reset after every possible interrupted first-header prefix recovers
  // the untouched legacy state before accepting a fresh checkpoint.
  for (int cut = 0; cut < 16; ++cut) {
    testStorage = {};
    p::Store old;
    state = {};
    state.completed = 18;
    CHECK(old.load(restored) && old.save(state));
    const auto legacyFile = testStorage.files["/.crosspoint/pomodoro-a.bin"];
    p::HistoryStore initial;
    CHECK(initial.load(restored));
    pending = {};
    state.completed = 19;
    testStorage.writeLimit = cut;
    CHECK(!initial.save(state, pending));
    testStorage.writeLimit = -1;
    p::HistoryStore reboot;
    CHECK(reboot.load(restored) && restored.completed == 18);
    CHECK(reboot.save(restored, pending));
    CHECK(testStorage.files["/.crosspoint/pomodoro-a.bin"] == legacyFile);
  }
  // A foreign or unreadable short header is not eligible for prefix recovery.
  testStorage = {};
  testStorage.files[p::kHistoryPath] = {'P', 'O', 'M', 'H', 2};
  p::HistoryStore shortFuture;
  CHECK(!shortFuture.load(restored));
  auto protectedFiles = testStorage.files;
  CHECK(!shortFuture.save(state, pending) && protectedFiles == testStorage.files);
  testStorage = {};
  testStorage.files[p::kHistoryPath] = {'P', 'O', 'M', 'H'};
  testStorage.refuseReads = true;
  p::HistoryStore shortUnreadable;
  CHECK(!shortUnreadable.load(restored));
  testStorage.refuseReads = false;
  protectedFiles = testStorage.files;
  CHECK(!shortUnreadable.save(state, pending) && protectedFiles == testStorage.files);
  sectionnav::Input navigation;
  CHECK(navigation.push(sectionnav::Input::Kind::Step, 1));
  CHECK(navigation.push(sectionnav::Input::Kind::Step, 1));
  sectionnav::Input::Event event{};
  CHECK(navigation.pop(event) && event.amount == 2 && !navigation.hasPending());
  CHECK(p::shiftPeriod(p::Period::Day, today, event.amount, today) == today + 2);
  stats.begin(p::Period::Week, today);
  CHECK(stats.first == readingstats::dayIndex({2026, 11, 2}) && stats.last == readingstats::dayIndex({2026, 11, 8}));
  stats.add(p::Delta{stats.first - 1, 60000, 1});
  stats.add(p::Delta{stats.first, 120000, 2});
  stats.add(p::Delta{stats.last, 180000, 3});
  CHECK(stats.milliseconds == 300000 && stats.sessions == 5);
  for (auto period : {p::Period::Day, p::Period::Week, p::Period::Month, p::Period::Year})
    for (bool shown : {false, true}) {
      testStorage = {};
      p::HistoryStore prefs;
      state = {};
      pending = {};
      p::DisplayOptions options{period, shown}, loaded;
      CHECK(prefs.load(restored, &loaded) && loaded.accumulation == p::Period::Week && loaded.showAccumulation);
      CHECK(prefs.save(state, pending, options));
      p::HistoryStore prefsReload;
      CHECK(prefsReload.load(restored, &loaded));
      CHECK(loaded.accumulation == period && loaded.showAccumulation == shown);
      const int beforeWrites = testStorage.writes;
      CHECK(prefsReload.save(state, pending, loaded) && beforeWrites == testStorage.writes);
      loaded.showAccumulation = !shown;
      CHECK(prefsReload.save(state, pending, loaded) && testStorage.writes > beforeWrites);
    }
  // Running study remains durable but is published only when a run ends.
  p::Pending active;
  p::Stats visible;
  visible.begin(p::Period::Day, today);
  visible.add(p::Delta{today, 120000, 1});
  state = {};
  state.config.focus = 2;
  p::reset(state);
  state.running = true;
  for (int n = 0; n < 2; ++n) {
    CHECK(!p::advanceTracked(state, 30000, {today, uint32_t(n * 30)}, {today, uint32_t((n + 1) * 30)}, credit));
    visible.add(credit);
    for (const auto& delta : credit.dates) active.add(delta.day, delta.milliseconds, 0);
    CHECK(visible.millisecondsWithout(&active) == 120000 && visible.bucketWithout(6, &active) == 120000);
  }
  state.running = false;
  active = {};
  CHECK(visible.millisecondsWithout(&active) == 180000);
  state.running = true;
  CHECK(!p::advanceTracked(state, 30000, {today, 60}, {today, 90}, credit));
  visible.add(credit);
  active.add(today, 30000, 0);
  CHECK(visible.millisecondsWithout(&active) == 180000);
  CHECK(p::advanceTracked(state, 30000, {today, 90}, {today, 120}, credit));
  visible.add(credit);
  active = {};
  CHECK(visible.millisecondsWithout(&active) == 240000 && visible.sessions == 2);
  // Period changes and historical replay can exclude the same active span.
  for (auto period : {p::Period::Day, p::Period::Week, p::Period::Month, p::Period::Year}) {
    visible.begin(period, today);
    visible.add(p::Delta{today, 90000, 0});
    active = {};
    active.add(today, 30000, 0);
    CHECK(visible.millisecondsWithout(&active) == 60000);
    int index = period == p::Period::Day ? 6 : period == p::Period::Year ? 10 : today - visible.plotFirst;
    CHECK(visible.bucketWithout(index, &active) == 60000 && visible.bucketWithout(-1, &active) == 0);
  }
  visible.begin(p::Period::Month, readingstats::dayIndex({2026, 11, 1}));
  visible.add(p::Delta{today, 30000, 0});
  active = {};
  active.add(today - 5, 50000, 0);
  active.add(today, 10000, 0);
  CHECK(visible.millisecondsWithout(&active) == 20000);
  active = {};
  active.add(today, UINT64_MAX, 0);
  CHECK(visible.millisecondsWithout(&active) == 0);
  // Five dates during a write outage must not let an unrepresented active
  // date subtract the preceding days' settled study after reprojection.
  p::Pending unsaved, withheld;
  const int friday = readingstats::dayIndex({2026, 11, 6});
  for (int n = 1; n <= 4; ++n) CHECK(unsaved.add(friday - n, 3600000, 1) == friday - n);
  const int recordedDay = unsaved.add(friday, 300000, 0);
  CHECK(recordedDay == 0 && unsaved.undatedMs == 300000);
  withheld.add(recordedDay, 300000, 0);
  for (auto period : {p::Period::Week, p::Period::Month, p::Period::Year}) {
    visible.begin(period, friday);
    visible.add(unsaved);
    CHECK(visible.millisecondsWithout(&withheld) == 14400000);
    CHECK(visible.sessions == 4);
  }
  std::printf("Pomodoro history: %u checks; pending=%zu stats=%zu store=%zu bytes\n", checks, sizeof(p::Pending),
              sizeof(p::Stats), sizeof(p::HistoryStore));
}

#include <HalStorage.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/activities/reader/ReaderSession.h"
#include "../../src/apps_local/reading/ReadingCompletionIndex.h"
#include "../../src/apps_local/reading/ReadingStatsStore.h"
int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
constexpr const char* document = "0123456789abcdef0123456789abcdef";
void completionTests() {
  using namespace readingstats;
  testStorage = {};
  Repository journal;
  constexpr int64_t epoch = 1793059200;  // 2026-10-27 00:00 UTC
  CHECK(!journal.complete("invalid", "Book", "Author", epoch));
  CHECK(!journal.complete(document, "Book", "Author", 0));
  CHECK(journal.complete(document, "Book", "Author", epoch));
  Book book;
  CHECK(journal.loadBook(document, book));
  CHECK(book.completed && book.progressBp == 10000 && book.finishedEpoch == epoch);
  CHECK(!book.seconds && !book.pages && !book.sessions && !book.readingDays && !book.firstEpoch);
  const auto bytes = testStorage.files[kJournalPath];
  CHECK(journal.complete(document, "Book", "Author", epoch + 86400));
  CHECK(testStorage.files[kJournalPath] == bytes);
  Record record;
  CHECK(decode(bytes.data() + 16, record) && record.kind == RecordKind::Completion && record.firstCompletion);
  Summary summary;
  summary.anchorDay = record.localDay;
  summary.add(record);
  CHECK(summary.completed == 1 && !summary.seconds && !summary.pages && !summary.sessions);
  CHECK(!summary.totalDays && !summary.longest && !summary.onDay(record.localDay));
  PeriodStats day, year;
  setPeriod(day, Period::Day, record.localDay, false);
  setPeriod(year, Period::Year, record.localDay, false);
  day.add(record);
  year.add(record);
  CHECK(day.completed == 1 && !day.sessions && !day.seconds && !day.pages);
  CHECK(year.completed == 1 && year.annualCompleted[9] == 1 && !year.daysReadThrough(year.last));
  // A completion does not invent a day or a session, even exactly at midnight.
  for (uint8_t kind : {uint8_t(2), uint8_t(255)}) {
    auto malformed = bytes;
    malformed[16 + 13] = kind;
    const auto hash = checksum(malformed.data() + 16, kRecordBytes - 4);
    for (int n = 0; n < 4; ++n) malformed[16 + kRecordBytes - 4 + n] = hash >> (8 * n);
    CHECK(!decode(malformed.data() + 16, record));
  }
  // Actual re-reading remains actual reading; marking it again cannot double count a finish.
  bool completionState = false;
  CHECK(journal.record(document, "Book", "Author", epoch + 100, 120, 3, 6000, &completionState));
  CHECK(completionState);
  CHECK(journal.complete(document, "Book", "Author", epoch + 300));
  CHECK(journal.loadBook(document, book));
  CHECK(book.seconds == 120 && book.pages == 3 && book.sessions == 1 && book.readingDays == 1);
  CHECK(book.firstEpoch == epoch + 100 && book.finishedEpoch == epoch && book.progressBp == 10000);
  CHECK(journal.begin());
  summary = {};
  summary.anchorDay = dayIndex({2026, 10, 27});
  for (;;) {
    const auto result = journal.next(record);
    if (result == ScanResult::Finished) break;
    CHECK(result == ScanResult::Record);
    summary.add(record);
  }
  journal.close();
  CHECK(summary.completed == 1 && summary.seconds == 120 && summary.pages == 3 && summary.sessions == 1);
  CHECK(summary.totalDays == 1);
  testStorage = {};
  // The full session append can succeed while its first verification read fails.
  ReaderSession pending;
  pending.onRenderComplete(0, epoch, 5000);
  pending.noteTurn(true, true);
  pending.onRenderComplete(1000, epoch + 1, 6000);
  testStorage.failJournalVerification = true;
  CHECK(!recordSession(document, "Book", "Author", pending.startTime(), pending.durationSeconds(), pending.pagesRead(),
                       pending.endProgressBp()));
  CHECK(testStorage.files[kJournalPath].size() == 16 + kRecordBytes);
  // Flush consumes an attempted session, including an unacknowledged append.
  pending.reset();
  CHECK(!pending.isEmitWorthy());
  CHECK(journal.complete(document, "Book", "Author", epoch + 2));
  CHECK(journal.loadBook(document, book));
  CHECK(book.completed && book.seconds == 1 && book.pages == 1 && book.sessions == 1);
  CHECK(journal.complete(document, "Book", "Author", epoch + 3));
  CHECK(journal.loadBook(document, book) && book.seconds == 1 && book.sessions == 1);
  testStorage = {};
  testStorage.writeLimit = 4;
  CHECK(!journal.complete(document, "Book", "Author", epoch));
  testStorage = {};
  testStorage.refuseReads = true;
  CHECK(!journal.complete(document, "Book", "Author", epoch));
  testStorage = {};
}
void reopeningTests() {
  using namespace readingstats;
  testStorage = {};
  Repository journal;
  static CompletionIndex::Entry entries[64];
  CompletionIndex index;
  index.reset(entries, 64);
  constexpr int64_t epoch = 1793102400;  // Midday avoids host timezone boundaries.
  const int completedDay = dayIndex({2026, 10, 27});
  CHECK(journal.record(document, "Book", "Author", epoch, 600, 12, 4500));
  CHECK(journal.complete(document, "Book", "Author", epoch + 1000));
  Book finished;
  CHECK(journal.loadBook(document, finished));
  CHECK(finished.completed && finished.finishedDay == completedDay);
  CHECK(journal.reopen(document, "Book", "Author", epoch + 86400, 4500));
  Book reopened;
  CHECK(journal.loadBook(document, reopened));
  CHECK(!reopened.completed && reopened.manuallyUnfinished && reopened.progressBp == 4500);
  CHECK(reopened.seconds == 600 && reopened.pages == 12 && reopened.sessions == 1 && reopened.readingDays == 1);
  CHECK(reopened.firstEpoch == finished.firstEpoch && reopened.lastEpoch == finished.lastEpoch);
  const auto saved = testStorage.files[kJournalPath];
  CHECK(journal.reopen(document, "Book", "Author", epoch + 86401, 4500));
  CHECK(testStorage.files[kJournalPath] == saved);
  Summary summary;
  PeriodStats day, year;
  setPeriod(day, Period::Day, completedDay, false);
  setPeriod(year, Period::Year, completedDay, false);
  Record record;
  CHECK(journal.begin());
  int currentFinishes = 0;
  while (journal.next(record) == ScanResult::Record) {
    summary.add(record);
    CHECK(index.apply(record, journal.recordOffset()));
  }
  journal.close();
  CHECK(summary.completed == 0 && summary.seconds == 600 && summary.pages == 12 && summary.sessions == 1);

  CHECK(journal.begin(true));
  while (journal.next(record) == ScanResult::Record) {
    currentFinishes += index.current(record, journal.recordOffset());
    day.add(record);
    year.add(record);
  }
  journal.close();
  CHECK(currentFinishes == 0);
  CHECK(day.completed == 0 && day.seconds == 600 && day.pages == 12 && day.sessions == 1);
  CHECK(year.completed == 0 && year.annualCompleted[9] == 0);
  // Reading the last page does not silently reverse an explicit choice.
  bool completionState = true;
  CHECK(journal.record(document, "Book", "Author", epoch + 86500, 10, 1, 10000, &completionState));
  CHECK(!completionState);
  CHECK(journal.loadBook(document, reopened) && !reopened.completed && reopened.progressBp == 9999);
  CHECK(journal.complete(document, "Book", "Author", epoch + 86600));
  CHECK(journal.loadBook(document, reopened) && reopened.completed && !reopened.manuallyUnfinished);
  CHECK(reopened.seconds == 610 && reopened.pages == 13 && reopened.sessions == 2);
  // Finish, undo and finish within one clock second: only the newest completion is listed.
  CHECK(journal.reopen(document, "Book", "Author", epoch + 86600, 5000));
  CHECK(journal.complete(document, "Book", "Author", epoch + 86600));
  CHECK(journal.begin());
  summary = {};
  index.reset(entries, 64);
  currentFinishes = 0;
  while (journal.next(record) == ScanResult::Record) {
    summary.add(record);
    CHECK(index.apply(record, journal.recordOffset()));
  }
  journal.close();
  CHECK(summary.completed == 1 && summary.seconds == 610 && summary.pages == 13 && summary.sessions == 2);
  CHECK(journal.begin(true));
  while (journal.next(record) == ScanResult::Record) currentFinishes += index.current(record, journal.recordOffset());
  journal.close();
  CHECK(currentFinishes == 1);
  const auto beforeFailure = testStorage.files[kJournalPath];
  testStorage.writeLimit = 3;
  CHECK(!journal.reopen(document, "Book", "Author", epoch + 90000, 5000));
  testStorage.writeLimit = -1;
  CHECK(journal.loadBook(document, reopened) && reopened.completed);
  testStorage.files[kJournalPath] = beforeFailure;
  // Every interrupted reopening is ignored; a complete, verified record survives a fresh repository.
  CHECK(journal.reopen(document, "Book", "Author", epoch + 90000, 5000));
  const auto after = testStorage.files[kJournalPath];
  for (size_t tail = 0; tail < kRecordBytes; ++tail) {
    testStorage.files[kJournalPath] = after;
    testStorage.files[kJournalPath].resize(beforeFailure.size() + tail);
    Repository rebooted;
    CHECK(rebooted.loadBook(document, reopened) && reopened.completed);
  }
  testStorage.files[kJournalPath] = after;
  Repository rebooted;
  CHECK(rebooted.loadBook(document, reopened) && !reopened.completed && reopened.seconds == 610);
  testStorage = {};
}
void completionIndexTests() {
  using namespace readingstats;
  testStorage = {};
  auto& bytes = testStorage.files[kJournalPath];
  bytes.assign(kJournalHeader, kJournalHeader + sizeof(kJournalHeader));
  bytes.reserve(16 + 10100 * kRecordBytes);
  for (int n = 0; n < 100; ++n) {
    Book previous;
    for (int session = 0; session <= 100; ++session) {
      Record record;
      record.epoch = 1793102400 + n * 1000 + session;
      record.localDay = dayIndex({2026, 10, 27});
      record.secondOfDay = 12 * 3600;
      std::snprintf(record.book.document, sizeof(record.book.document), "%032x", n);
      if (session == 100) {
        record.kind = RecordKind::Completion;
        record.book.progressBp = 10000;
      } else {
        record.seconds = 1;
        record.pages = 1;
        record.book.progressBp = session * 90;
      }
      updateBook(record, previous);
      previous = record.book;
      const auto at = bytes.size();
      bytes.resize(at + kRecordBytes);
      encode(record, bytes.data() + at);
    }
  }
  const auto slots = CompletionIndex::slotsFor(10100);
  std::vector<CompletionIndex::Entry> entries(slots);
  CompletionIndex index;
  index.reset(entries.data(), slots);
  Repository journal;
  Record record;
  CHECK(journal.begin());
  int read = 0;
  while (journal.next(record) == ScanResult::Record) {
    ++read;
    CHECK(index.apply(record, journal.recordOffset()));
  }
  journal.close();
  CHECK(read == 10100);
  CHECK(journal.begin(true));
  int current = 0;
  while (journal.next(record) == ScanResult::Record) current += index.current(record, journal.recordOffset());
  journal.close();
  CHECK(current == 100);
  CHECK(testStorage.readBytes == bytes.size() * 2);
  std::printf("Completion index: %d records, %llu bytes read, no nested journal scans\n", read,
              static_cast<unsigned long long>(testStorage.readBytes));
  testStorage = {};
}
int main() {
  completionTests();
  reopeningTests();
  completionIndexTests();
  std::printf("Manual completion: %d checks, %d failed\n", checks, failures);
  return failures != 0;
}

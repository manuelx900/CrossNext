#include <HalStorage.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/activities/reader/ReaderSession.h"
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
  CHECK(journal.record(document, "Book", "Author", epoch + 100, 120, 3, 6000));
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
int main() {
  completionTests();
  std::printf("Manual completion: %d checks, %d failed\n", checks, failures);
  return failures != 0;
}

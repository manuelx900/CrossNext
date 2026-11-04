#include <HalStorage.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/reading/ReadingPageStats.h"
#include "../../src/apps_local/reading/ReadingStatsStore.h"
namespace {
int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
using namespace readingstats;
constexpr const char* bookA = "0123456789abcdef0123456789abcdef";
constexpr const char* bookB = "fedcba9876543210fedcba9876543210";
constexpr int64_t epoch = 1793059200;  // 2026-10-27 00:00 UTC

Record session(const uint32_t seconds, const uint32_t pages, const uint16_t endBp) {
  Record record;
  record.epoch = epoch;
  record.seconds = seconds;
  record.pages = pages;
  record.book.progressBp = endBp;
  return record;
}
size_t count(const std::string& text, const char* needle) {
  size_t found = 0;
  for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++found;
  return found;
}
struct Server {
  std::vector<std::string> bodies;
  bool accept = true;
};
bool post(const std::string& books, void* user) {
  auto& server = *static_cast<Server*>(user);
  server.bodies.push_back(books);
  return server.accept;
}
void add(const char* document, const int64_t at, const uint32_t seconds, const uint32_t pages, const uint16_t endBp,
         const int startBp = -1) {
  Repository journal;
  CHECK(journal.record(document, "Book", "Author", at, seconds, pages, endBp, nullptr, startBp));
}

void expansionTests() {
  // The server sums durations and takes progress from the first and last event,
  // so these three properties are what the reading log ends up showing.
  const auto long_session = session(2381, 82, 3077);
  const auto events = pageEventCount(long_session);
  CHECK(events == 82);
  uint64_t total = 0;
  int64_t expected = epoch;
  for (uint32_t n = 0; n < events; ++n) {
    const auto event = pageEvent(long_session, 2156, n, events);
    CHECK(event.startTime == expected);
    CHECK(event.durationSeconds > 0);
    expected += event.durationSeconds;
    total += event.durationSeconds;
  }
  CHECK(total == 2381);
  CHECK(pageEvent(long_session, 2156, 0, events).page == 2156);
  CHECK(pageEvent(long_session, 2156, events - 1, events).page == 3077);

  // Fewer seconds than pages: a zero-length event would collide on the server's key.
  CHECK(pageEventCount(session(4, 10, 100)) == 4);
  CHECK(pageEventCount(session(5000, 900, 100)) == kMaxPageEvents);
  CHECK(pageEventCount(session(5, 0, 100)) == 0);
  // A single page has no span, so it reports where the session ended.
  CHECK(pageEvent(session(5, 1, 1746), 68, 0, 1).page == 1746);
  // The server rejects the whole request above a day per event.
  CHECK(pageEvent(session(UINT32_MAX, 1, 100), 0, 0, 1).durationSeconds == 86400);
  // Rereading backwards stays in range.
  const auto back = session(100, 10, 1000);
  CHECK(pageEvent(back, 2000, 0, 10).page == 2000);
  CHECK(pageEvent(back, 2000, 9, 10).page == 1000);
}

void journalStartTests() {
  testStorage = {};
  add(bookA, epoch, 60, 3, 500, 400);
  Repository journal;
  Record record;
  CHECK(journal.begin());
  CHECK(journal.next(record) == ScanResult::Record);
  CHECK(record.startProgressBp == 400);
  journal.close();
  // Start of a book is 0, which must stay distinct from "not recorded".
  testStorage = {};
  add(bookA, epoch, 60, 3, 500, 0);
  CHECK(journal.begin());
  CHECK(journal.next(record) == ScanResult::Record);
  CHECK(record.startProgressBp == 0);
  journal.close();
  // Records written by older firmware carry zeros in these bytes.
  testStorage = {};
  add(bookA, epoch, 60, 3, 500);
  CHECK(journal.begin());
  CHECK(journal.next(record) == ScanResult::Record);
  CHECK(record.startProgressBp == -1);
  journal.close();

  Record previous = session(60, 3, 700);
  std::strcpy(previous.book.document, bookA);
  Record current = session(60, 3, 900);
  std::strcpy(current.book.document, bookA);
  current.startProgressBp = 850;
  CHECK(sessionStartBp(current, &previous) == 850);
  current.startProgressBp = -1;
  CHECK(sessionStartBp(current, &previous) == 700);
  std::strcpy(previous.book.document, bookB);
  CHECK(sessionStartBp(current, &previous) == 900);
  CHECK(sessionStartBp(current, nullptr) == 900);
}

void uploadTests() {
  testStorage = {};
  Server server;
  CHECK(uploadPageStats(post, &server));  // no journal yet
  CHECK(server.bodies.empty());

  add(bookA, epoch, 60, 3, 500, 400);
  add(bookA, epoch + 100, 40, 2, 600);
  add(bookB, epoch + 200, 30, 2, 100, 0);
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.size() == 1);
  const auto body = server.bodies[0];
  CHECK(body.front() == '[' && body.back() == ']');
  CHECK(count(body, "\"hash\"") == 2);  // consecutive sessions share one book entry
  CHECK(count(body, "\"page\"") == 7);
  CHECK(body.find("{\"page\":400,\"startTime\":1793059200,\"durationSeconds\":20,\"totalPages\":10000}") !=
        std::string::npos);
  CHECK(body.find("\"page\":500,\"startTime\":1793059300") != std::string::npos);  // starts at the previous end

  // Nothing new: no request at all.
  server.bodies.clear();
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.empty());

  // Only what is new, and non-session records are not reading.
  Repository journal;
  CHECK(journal.complete(bookB, "Book", "Author", epoch + 300));
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.empty());
  add(bookA, epoch + 400, 20, 1, 700, 650);
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.size() == 1);
  CHECK(count(server.bodies[0], "\"page\"") == 1);
}

void failureTests() {
  // A rejected batch must resend byte for byte: the server dedupes on page and
  // start time, so a retry that shifted either would double-count the reading.
  testStorage = {};
  Server server;
  server.accept = false;
  add(bookA, epoch, 60, 3, 500, 400);
  CHECK(!uploadPageStats(post, &server));
  server.accept = true;
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.size() == 2);
  CHECK(server.bodies[0] == server.bodies[1]);
  server.bodies.clear();
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.empty());

  // A lost or stale cursor resends everything, which the server ignores. It must
  // never leave the upload stuck.
  testStorage.files[kPageStatsCursorPath] = {1, 2, 3};
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.size() == 1);
  server.bodies.clear();
  testStorage.files[kPageStatsCursorPath] = {0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0};
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.size() == 1);
}

void batchTests() {
  // 150 pages over three sessions cannot share one request.
  testStorage = {};
  Server server;
  for (int n = 0; n < 3; ++n) add(bookA, epoch + n * 1000, 500, 50, 100 * (n + 1), 100 * n);
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.size() == 2);
  size_t total = 0;
  for (const auto& body : server.bodies) {
    CHECK(count(body, "\"page\"") <= kMaxPageEvents);
    CHECK(body.front() == '[' && body.back() == ']');
    total += count(body, "\"page\"");
  }
  CHECK(total == 150);

  // A failure part way keeps the batches already accepted.
  testStorage = {};
  server = {};
  for (int n = 0; n < 3; ++n) add(bookA, epoch + n * 1000, 500, 50, 100 * (n + 1), 100 * n);
  struct Flaky {
    static bool once(const std::string&, void* calls) { return ++*static_cast<int*>(calls) == 1; }
  };
  int calls = 0;
  CHECK(!uploadPageStats(Flaky::once, &calls));
  CHECK(calls == 2);
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.size() == 1);
  CHECK(count(server.bodies[0], "\"page\"") == 50);
}

void damagedRecordTests() {
  testStorage = {};
  Server server;
  add(bookA, epoch, 60, 3, 500, 400);
  add(bookA, epoch + 100, 40, 2, 600, 500);
  add(bookA, epoch + 200, 40, 2, 700, 600);
  testStorage.files[kJournalPath][16 + kRecordBytes + 5] ^= 0xff;
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.size() == 1);
  CHECK(count(server.bodies[0], "\"page\"") == 5);
  server.bodies.clear();
  CHECK(uploadPageStats(post, &server));
  CHECK(server.bodies.empty());
}
}  // namespace

int main() {
  expansionTests();
  journalStartTests();
  uploadTests();
  failureTests();
  batchTests();
  damagedRecordTests();
  std::printf("Page stats: %d checks, %d failed\n", checks, failures);
  return failures != 0;
}

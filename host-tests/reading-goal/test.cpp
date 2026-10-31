#include <cstdio>
#include <cstring>

#include "../../lib/Xtc/Xtc/CoverPixels.h"
#include "../../src/apps_local/reading/ReadingGoal.h"
#include "../../src/apps_local/reading/ReadingStatsCloud.h"
#include "../../src/apps_local/reading/ReadingStatsCloudClient.h"
#include "../../src/apps_local/reading/ReadingStatsStore.h"

bool cloudEnabled = false;
uint64_t cloudAccount = 123;
namespace readingstats::cloud {
bool Client::configured() { return cloudEnabled; }
uint64_t Client::accountId() { return cloudAccount; }
uint64_t Client::deviceTag() { return 123; }
}  // namespace readingstats::cloud
int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (false)
void append(int day, unsigned second, unsigned duration) {
  using namespace readingstats;
  Record record;
  record.localDay = day;
  record.secondOfDay = second;
  record.seconds = duration;
  record.epoch = int64_t(day) * 86400 + second;
  record.book.sessions = 1;
  record.book.seconds = duration;
  record.book.firstEpoch = record.epoch;
  record.book.lastEpoch = record.epoch + duration;
  record.book.progressBp = 100;
  copyText(record.book.document, sizeof(record.book.document), "0123456789abcdef0123456789abcdef");
  auto& data = testStorage.files[kJournalPath];
  if (data.empty()) data.assign(kJournalHeader, kJournalHeader + 16);
  const auto end = data.size();
  data.resize(end + kRecordBytes);
  encode(record, data.data() + end);
}
int main() {
  using namespace readingstats;
  const int day = dayIndex({2026, 10, 30});
  CHECK(readinggoal::target(0) == 0 && readinggoal::target(5) == 30 && readinggoal::target(99) == 0);
  CHECK(!readinggoal::met(99999, 0));
  CHECK(!readinggoal::met(1799, 30));
  CHECK(readinggoal::met(1800, 30));
  Summary s;
  s.anchorDay = day;
  s.dailySeconds[1] = 1800;
  s.dailySeconds[2] = 3600;
  s.dailySeconds[4] = 1800;
  CHECK(readinggoal::streak(s, day, 30) == 2);
  s.dailySeconds[0] = 1900;
  CHECK(readinggoal::streak(s, day, 30) == 3);
  CHECK(readinggoal::streak(s, day + 2, 30) == 0);
  CHECK(readinggoal::streak(s, day, 60) == 0);
  testStorage = {};
  CHECK(readinggoal::load(s, day));
  CHECK(s.onDay(day) == 0);
  append(day, 0, 1500);
  CHECK(readinggoal::load(s, day));
  CHECK(s.onDay(day) == 1500);
  const auto writes = testStorage.writes;
  testStorage.readBytes = 0;
  CHECK(readinggoal::load(s, day));
  CHECK(testStorage.writes == writes);
  CHECK(testStorage.readBytes < 5000);
  append(day, 2000, 600);
  CHECK(readinggoal::load(s, day));
  CHECK(s.onDay(day) == 2100);
  append(day + 1, 0, 300);
  CHECK(readinggoal::load(s, day));
  CHECK(s.onDay(day + 1) == 0);
  CHECK(readinggoal::load(s, day + 1));
  CHECK(s.onDay(day + 1) == 300 && s.onDay(day) == 2100);
  CHECK(readinggoal::load(s, day));
  CHECK(readinggoal::load(s, day + 1));
  CHECK(s.onDay(day + 1) == 300 && s.onDay(day) == 2100);
  CHECK(readinggoal::load(s, day));
  CHECK(s.onDay(day) == 2100);
  testStorage.files["/.crosspoint/reading-history/goal-days.bin"][120] ^= 1;
  CHECK(readinggoal::load(s, day));
  CHECK(s.onDay(day) == 2100);
  testStorage.files[kJournalPath].resize(16 + kRecordBytes);
  CHECK(readinggoal::load(s, day));
  CHECK(s.onDay(day) == 1500);
  testStorage.files.erase(kJournalPath);
  append(day, 86390, 30);
  CHECK(readinggoal::load(s, day + 1));
  CHECK(s.onDay(day) == 10 && s.onDay(day + 1) == 20);
  testStorage.available = false;
  CHECK(!readinggoal::load(s, day));
  testStorage.available = true;
  testStorage.refuseReads = true;
  CHECK(!readinggoal::load(s, day));
  testStorage.refuseReads = false;
  testStorage.files["/.crosspoint/reading-history/goal-days.bin"].clear();
  testStorage.refuseRename = true;
  CHECK(readinggoal::load(s, day + 1));
  CHECK(s.onDay(day + 1) == 20);
  testStorage.refuseRename = false;
  // Warm cache cost does not grow with the number of books or sessions.
  testStorage = {};
  for (int n = 0; n < 2000; ++n) append(day - n % 700, 0, 30);
  CHECK(readinggoal::load(s, day));
  testStorage.readBytes = 0;
  CHECK(readinggoal::load(s, day));
  CHECK(testStorage.readBytes < 5000);
  testStorage = {};
  cloudEnabled = true;
  auto snapshot = [&](unsigned generation, unsigned seconds) {
    cloud::Header h;
    h.account = cloudAccount;
    h.generation = generation;
    h.days = 1;
    h.summary.anchorDay = day;
    h.summary.seconds = seconds;
    h.summary.dailySeconds[0] = seconds;
    h.active[0] = h.known[0] = 1;
    auto& data = testStorage.files[cloud::slotPath(0)];
    data.resize(cloud::kHeaderBytes + cloud::kDayBytes);
    cloud::encodeHeader(h, data.data());
    auto put = [&](unsigned at, uint64_t v, unsigned n) {
      for (unsigned k = 0; k < n; ++k) data[at + k] = uint8_t(v >> (8 * k));
    };
    put(cloud::kHeaderBytes, day, 4);
    put(cloud::kHeaderBytes + 4, seconds, 8);
    put(cloud::kHeaderBytes + 12, checksum(data.data() + cloud::kHeaderBytes, 12), 4);
  };
  snapshot(1, 1200);
  CHECK(readinggoal::load(s, day) && s.onDay(day) == 1200);
  auto cloudWrites = testStorage.writes;
  testStorage.readBytes = 0;
  CHECK(readinggoal::load(s, day) && s.onDay(day) == 1200);
  CHECK(testStorage.writes == cloudWrites && testStorage.readBytes < 10000);
  append(day, 4000, 600);
  CHECK(readinggoal::load(s, day) && s.onDay(day) == 1800);
  snapshot(2, 1500);
  CHECK(readinggoal::load(s, day) && s.onDay(day) == 2100);
  cloudAccount = 456;
  CHECK(readinggoal::load(s, day) && s.onDay(day) == 600);
  cloudEnabled = false;
  CHECK(readinggoal::load(s, day) && s.onDay(day) == 600);
  // Non-byte-aligned XTC and column-major XTCH use the same CCW transform.
  for (unsigned depth : {1u, 2u}) {
    constexpr unsigned w = 13, h = 9;
    const size_t plane = w * ((h + 7) / 8);
    std::vector<uint8_t> pixels(depth == 1 ? ((w + 7) / 8) * h : plane * 2, 0);
    for (unsigned y = 0; y < h; ++y)
      for (unsigned x = 0; x < w; ++x) {
        if (depth == 1) {
          if ((x + y) % 3) pixels[y * ((w + 7) / 8) + x / 8] |= 0x80 >> (x % 8);
        } else {
          unsigned v = (x + y) % 4;
          const size_t at = (w - 1 - x) * ((h + 7) / 8) + y / 8;
          pixels[at] |= ((v >> 1) & 1) << (7 - y % 8);
          pixels[at + plane] |= (v & 1) << (7 - y % 8);
        }
      }
    for (unsigned y = 0; y < w; ++y)
      for (unsigned x = 0; x < h; ++x)
        CHECK(xtc::coverGray(pixels.data(), w, h, depth, x, y, true) ==
              xtc::coverGray(pixels.data(), w, h, depth, w - 1 - y, x, false));
  }
  std::printf("Reading goal and XTC rotation: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

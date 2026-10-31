#include <Logging.h>
#include <Memory.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>

#include "ReadingGoal.h"
#include "ReadingStatsCloudClient.h"
#include "ReadingStatsProjection.h"
#include "ReadingStatsStore.h"
namespace readinggoal {
namespace {
constexpr const char* cachePath = "/.crosspoint/reading-history/goal-days.bin";
constexpr const char* tempPath = "/.crosspoint/reading-history/goal-days.part";
constexpr size_t bytes = 48 + 4 * readingstats::kHistoryDays + 4;
struct Work {
  uint8_t cache[bytes]{}, raw[readingstats::kRecordBytes]{};
  readingstats::Record record;
  readingstats::Repository repository;
};
uint64_t get(const uint8_t* p, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i) v |= uint64_t(p[i]) << (8 * i);
  return v;
}
void put(uint8_t* p, uint64_t v, unsigned n) {
  for (unsigned i = 0; i < n; ++i) p[i] = uint8_t(v >> (8 * i));
}
uint64_t hash(uint64_t h, const uint8_t* p, size_t n) {
  while (n--) h = (h ^ *p++) * 1099511628211ULL;
  return h;
}
bool tailAt(HalFile& file, uint64_t end, uint8_t* raw, uint64_t& value) {
  value = 0;
  if (end == 16) return true;
  if (end < 16 + readingstats::kRecordBytes || !file.seek64(end - readingstats::kRecordBytes) ||
      file.read(raw, readingstats::kRecordBytes) != readingstats::kRecordBytes)
    return false;
  value = hash(14695981039346656037ULL, raw, readingstats::kRecordBytes);
  return true;
}
bool cloudStamp(uint8_t* raw, uint64_t& stamp) {
  stamp = 0;
  if (!readingstats::cloud::Client::configured()) return true;
  stamp = readingstats::cloud::Client::accountId();
  for (int slot = 0; slot < 2; ++slot) {
    const char* path = readingstats::cloud::slotPath(slot);
    if (!Storage.exists(path)) continue;
    HalFile file;
    if (!Storage.openFileForRead("GOAL", path, file)) return false;
    size_t left = readingstats::cloud::kHeaderBytes;
    while (left) {
      const size_t n = std::min(left, readingstats::kRecordBytes);
      if (file.read(raw, n) != int(n)) return false;
      stamp = hash(stamp, raw, n);
      left -= n;
    }
  }
  return true;
}
void save(Work& w, const readingstats::Summary& s, uint64_t length, uint64_t identity, uint64_t tail, uint64_t cloud) {
  std::memset(w.cache, 0, bytes);
  std::memcpy(w.cache, "CGD1", 4);
  put(w.cache + 4, s.anchorDay, 4);
  put(w.cache + 8, length, 8);
  put(w.cache + 16, identity, 8);
  put(w.cache + 24, tail, 8);
  put(w.cache + 32, cloud, 8);
  for (int n = 0; n < readingstats::kHistoryDays; ++n) put(w.cache + 48 + 4 * n, s.dailySeconds[n], 4);
  put(w.cache + bytes - 4, readingstats::checksum(w.cache, bytes - 4), 4);
  HalFile file;
  if (!Storage.ensureDirectoryExists("/.crosspoint/reading-history") ||
      !Storage.openFileForWrite("GOAL", tempPath, file) || file.write(w.cache, bytes) != bytes) {
    LOG_ERR("GOAL", "Could not write disposable daily cache");
    return;
  }
  file.close();
  if (Storage.exists(cachePath)) Storage.remove(cachePath);
  if (!Storage.rename(tempPath, cachePath)) LOG_ERR("GOAL", "Could not install disposable daily cache");
}
}  // namespace
bool load(readingstats::Summary& summary, int day) {
  if (!Storage.ready()) return false;
  if (day < readingstats::dayIndex({2020, 1, 1})) return false;
  // A bounded workspace keeps the journal record and daily cache off the task stack.
  auto w = makeUniqueNoThrow<Work>();
  if (!w) {
    LOG_ERR("GOAL", "OOM: daily history workspace");
    return false;
  }
  uint64_t length = 16, identity = 0, tail = 0, cloud = 0;
  HalFile journal;
  if (Storage.exists(readingstats::kJournalPath)) {
    if (!Storage.openFileForRead("GOAL", readingstats::kJournalPath, journal) || journal.read(w->raw, 16) != 16 ||
        std::memcmp(w->raw, readingstats::kJournalHeader, 16))
      return false;
    const uint64_t size = journal.fileSize64();
    if (size < 16 || (size - 16) % readingstats::kRecordBytes) return false;
    length = size;
    if (size > 16) {
      if (journal.read(w->raw, readingstats::kRecordBytes) != readingstats::kRecordBytes) return false;
      identity = hash(14695981039346656037ULL, w->raw, readingstats::kRecordBytes);
    }
    if (!tailAt(journal, length, w->raw, tail)) return false;
  }
  if (!cloudStamp(w->raw, cloud)) return false;
  uint64_t offset = 16;
  bool cached = false;
  HalFile file;
  if (Storage.exists(cachePath) && Storage.openFileForRead("GOAL", cachePath, file) && file.fileSize64() == bytes &&
      file.read(w->cache, bytes) == bytes && !std::memcmp(w->cache, "CGD1", 4) &&
      get(w->cache + bytes - 4, 4) == readingstats::checksum(w->cache, bytes - 4)) {
    const int anchor = int(get(w->cache + 4, 4));
    offset = get(w->cache + 8, 8);
    uint64_t oldTail = 0;
    cached = anchor == day && offset >= 16 && offset <= length && (offset - 16) % readingstats::kRecordBytes == 0 &&
             get(w->cache + 16, 8) == identity && get(w->cache + 32, 8) == cloud &&
             (offset == 16 || tailAt(journal, offset, w->raw, oldTail)) && oldTail == get(w->cache + 24, 8);
    if (cached) {
      summary = {};
      summary.anchorDay = anchor;
      for (int n = 0; n < readingstats::kHistoryDays; ++n) summary.dailySeconds[n] = get(w->cache + 48 + 4 * n, 4);
    }
  }
  file.close();
  journal.close();
  if (!cached) {
    summary = {};
    summary.anchorDay = day;
    offset = 16;
    if (cloud && readingstats::cloud::Client::configured()) {
      auto reader = makeUniqueNoThrow<readingstats::cloud::Reader>();
      if (!reader) {
        LOG_ERR("GOAL", "OOM: imported daily history");
        return false;
      }
      const auto result = reader->open();
      if (result == readingstats::cloud::Result::Success &&
          reader->data().account == readingstats::cloud::Client::accountId()) {
        auto projection = makeUniqueNoThrow<readingstats::cloud::Projection>();
        if (!projection) {
          LOG_ERR("GOAL", "OOM: daily projection");
          return false;
        }
        if (projection->open(reader.get(), day, readingstats::cloud::Client::deviceTag()) !=
            readingstats::cloud::Result::Success)
          return false;
        summary = projection->data().summary;
        offset = length;
      } else if (result != readingstats::cloud::Result::Empty && result != readingstats::cloud::Result::Success)
        return false;
    }
  }
  if (offset < length) {
    if (!w->repository.beginFrom(offset)) return false;
    unsigned n = 0;
    while (true) {
      const auto status = w->repository.next(w->record);
      if (status == readingstats::ScanResult::Finished) break;
      if (status != readingstats::ScanResult::Record) return false;
      summary.add(w->record);
      if ((++n & 63) == 0) vTaskDelay(1);
    }
    w->repository.close();
  }
  if (!cached || offset != length || int(get(w->cache + 4, 4)) != day) save(*w, summary, length, identity, tail, cloud);
  return true;
}
}  // namespace readinggoal

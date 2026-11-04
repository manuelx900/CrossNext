#include "ReadingPageStats.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ReadingStatsStore.h"

namespace readingstats {
namespace {
constexpr uint64_t kFirstRecord = 16;
constexpr uint32_t kMaxEventSeconds = 86400;  // the server refuses the whole request above this
constexpr size_t kEventJsonBytes = 80;
struct Workspace {
  Repository journal;
  Record record, previous;
};
// A missing or unusable cursor only costs a resend: the server ignores events it
// already has, so nothing here needs to be stronger than a plain offset.
uint64_t loadCursor() {
  HalFile file;
  uint8_t bytes[8];
  if (!Storage.openFileForRead("READ-STATS", kPageStatsCursorPath, file) || file.size() != sizeof(bytes) ||
      file.read(bytes, sizeof(bytes)) != static_cast<int>(sizeof(bytes)))
    return kFirstRecord;
  uint64_t offset = 0;
  for (int n = 0; n < 8; ++n) offset |= uint64_t(bytes[n]) << (n * 8);
  return offset;
}
void saveCursor(const uint64_t offset) {
  uint8_t bytes[8];
  for (int n = 0; n < 8; ++n) bytes[n] = static_cast<uint8_t>(offset >> (n * 8));
  HalFile file;
  if (!Storage.openFileForWrite("READ-STATS", kPageStatsCursorPath, file) ||
      file.write(bytes, sizeof(bytes)) != sizeof(bytes))
    LOG_ERR("READ-STATS", "Could not save page stats cursor");
}
void appendEvents(std::string& books, const Record& record, const int startBp, const uint32_t count) {
  char json[kEventJsonBytes + 16];
  for (uint32_t n = 0; n < count; ++n) {
    const auto event = pageEvent(record, startBp, n, count);
    std::snprintf(json, sizeof(json), "%s{\"page\":%u,\"startTime\":%lld,\"durationSeconds\":%lu,\"totalPages\":%u}",
                  n ? "," : "", static_cast<unsigned>(event.page), static_cast<long long>(event.startTime),
                  static_cast<unsigned long>(event.durationSeconds), static_cast<unsigned>(kPageEventScale));
    books += json;
  }
}
}  // namespace

uint32_t pageEventCount(const Record& record) { return std::min({record.pages, record.seconds, kMaxPageEvents}); }

PageEvent pageEvent(const Record& record, const int startBp, const uint32_t index, const uint32_t count) {
  const uint64_t seconds = record.seconds;
  const auto from = seconds * index / count;
  const auto to = seconds * (index + 1) / count;
  const int endBp = record.book.progressBp;
  PageEvent event;
  // The first and last events carry the session's real start and end, because the
  // server derives progress from exactly those two.
  event.page = static_cast<uint16_t>(
      count > 1 ? startBp + (endBp - startBp) * static_cast<int>(index) / static_cast<int>(count - 1) : endBp);
  event.startTime = record.epoch + static_cast<int64_t>(from);
  event.durationSeconds = static_cast<uint32_t>(std::min<uint64_t>(to - from, kMaxEventSeconds));
  return event;
}

int sessionStartBp(const Record& record, const Record* previous) {
  if (record.startProgressBp >= 0) return record.startProgressBp;
  return previous && !std::strcmp(previous->book.document, record.book.document) ? previous->book.progressBp
                                                                                 : record.book.progressBp;
}

bool uploadPageStats(const PostPageStats post, void* user) {
  auto work = makeUniqueNoThrow<Workspace>();
  if (!work) {
    LOG_ERR("READ-STATS", "OOM: page stats workspace");
    return false;
  }
  auto& journal = work->journal;
  uint64_t offset = loadCursor();
  std::string books;
  books.reserve(kMaxPageEvents * kEventJsonBytes);
  for (;;) {
    // One record back: a session from older firmware starts where that one ended.
    if (!journal.beginFrom(offset > kFirstRecord ? offset - kRecordBytes : offset)) {
      offset = kFirstRecord;
      if (!journal.beginFrom(offset)) return false;
    }
    books = "[";
    uint32_t events = 0;
    uint64_t next = offset;
    bool hasPrevious = false;
    char openBook[sizeof(Book::document)] = {};
    for (;;) {
      const auto status = journal.next(work->record);
      if (status == ScanResult::Finished) break;
      if (status == ScanResult::Error) {
        journal.close();
        return false;
      }
      if (status == ScanResult::More) {  // a damaged record
        hasPrevious = false;
        continue;
      }
      const auto& record = work->record;
      const auto at = journal.recordOffset();
      if (at >= offset) {
        const auto count = pageEventCount(record);  // zero for completion and reopening records
        if (events && events + count > kMaxPageEvents) break;
        if (count) {
          if (std::strcmp(openBook, record.book.document)) {
            if (events) books += "]},";
            books += "{\"hash\":\"";
            books += record.book.document;
            books += "\",\"events\":[";
            std::strcpy(openBook, record.book.document);
          } else {
            books += ",";
          }
          appendEvents(books, record, sessionStartBp(record, hasPrevious ? &work->previous : nullptr), count);
          events += count;
        }
        next = at + kRecordBytes;
      }
      work->previous = record;
      hasPrevious = true;
    }
    journal.close();
    if (!events) {
      if (next != offset) saveCursor(next);
      return true;
    }
    books += "]}]";
    if (!post(books, user)) return false;
    saveCursor(next);
    offset = next;
  }
}
}  // namespace readingstats

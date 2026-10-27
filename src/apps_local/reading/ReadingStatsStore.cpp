#include "ReadingStatsStore.h"

#include <HalClock.h>
#include <Logging.h>
#include <Memory.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>
#include <ctime>

namespace readingstats {
namespace {
constexpr const char* kRoot = "/.crosspoint/reading-history";
void cachePath(char* path, size_t size, const char* document) { snprintf(path, size, "%s/%s.idx", kRoot, document); }
uint64_t getOffset(const uint8_t* bytes) {
  uint64_t v = 0;
  for (int n = 0; n < 8; ++n) v |= uint64_t(bytes[n]) << (n * 8);
  return v;
}
void setOffset(uint8_t* bytes, uint64_t v) {
  for (int n = 0; n < 8; ++n) bytes[n] = static_cast<uint8_t>(v >> (n * 8));
}
}  // namespace
int today() {
  tm local{};
  const auto now = std::time(nullptr);
  if (!(now > 0 && localtime_r(&now, &local) && local.tm_year >= 120) && !halClock.localTime(local)) return 0;
  return dayIndex({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
}
bool Repository::header(HalFile& input) {
  uint8_t bytes[sizeof(kJournalHeader)];
  return input.seek(0) && input.read(bytes, sizeof(bytes)) == static_cast<int>(sizeof(bytes)) &&
         !std::memcmp(bytes, kJournalHeader, sizeof(bytes));
}
bool Repository::begin(bool reverse, uint8_t* ahead, uint32_t aheadBytes) {
  close();
  readAhead = ahead;
  readAheadCapacity = ahead ? aheadBytes / kRecordBytes * kRecordBytes : 0;
  damaged = false;
  backwards = reverse;
  if (!Storage.ensureDirectoryExists(kRoot)) return false;
  if (!Storage.exists(kJournalPath)) {
    end = 16;
    cursor = 16;
    return true;
  }
  if (!Storage.openFileForRead("READ-STATS", kJournalPath, file) || !header(file)) {
    close();
    return false;
  }
  const auto length = static_cast<uint64_t>(file.size());
  end = 16 + (length - 16) / kRecordBytes * kRecordBytes;
  damaged = end != length;
  cursor = reverse ? end : 16;
  return true;
}
void Repository::close() {
  file.close();
  cursor = end = 0;
  readAhead = nullptr;
  readAheadCapacity = bufferedBytes = 0;
}
bool Repository::readAt(uint64_t offset, Record& record) {
  return file.seek64(offset) && file.read(buffer, sizeof(buffer)) == static_cast<int>(sizeof(buffer)) &&
         decode(buffer, record);
}
ScanResult Repository::next(Record& record) {
  if (backwards ? cursor <= 16 : cursor >= end) return ScanResult::Finished;
  const auto at = backwards ? cursor - kRecordBytes : cursor;
  cursor = backwards ? at : at + kRecordBytes;
  if (readAheadCapacity) {
    if (!bufferedBytes || at < bufferedAt || at + kRecordBytes > bufferedAt + bufferedBytes) {
      bufferedAt = backwards && at + kRecordBytes > 16 + readAheadCapacity ? at + kRecordBytes - readAheadCapacity
                   : backwards                                             ? 16
                                                                           : at;
      bufferedBytes = static_cast<uint32_t>(std::min<uint64_t>(readAheadCapacity, end - bufferedAt));
      if (!file.seek64(bufferedAt) || file.read(readAhead, bufferedBytes) != static_cast<int>(bufferedBytes)) {
        bufferedBytes = 0;
        return ScanResult::Error;
      }
    }
    std::memcpy(buffer, readAhead + (at - bufferedAt), sizeof(buffer));
  } else if (!file.seek64(at) || file.read(buffer, sizeof(buffer)) != static_cast<int>(sizeof(buffer)))
    return ScanResult::Error;
  if (decode(buffer, record)) return ScanResult::Record;
  damaged = true;
  return ScanResult::More;
}
bool Repository::loadBook(const char* document, Book& book) {
  if (!validDocument(document) || !begin()) return false;
  book = {};
  // The index is disposable: every cumulative book snapshot is also in the journal.
  char path[100];
  cachePath(path, sizeof(path), document);
  uint8_t index[12];
  {
    HalFile cached;
    if (Storage.openFileForRead("READ-STATS", path, cached) && cached.size() == sizeof(index) &&
        cached.read(index, sizeof(index)) == static_cast<int>(sizeof(index))) {
      const auto expected = checksum(index, 8);
      const uint32_t found =
          uint32_t(index[8]) | uint32_t(index[9]) << 8 | uint32_t(index[10]) << 16 | uint32_t(index[11]) << 24;
      const auto offset = getOffset(index);
      if (expected == found && offset >= 16 && offset < end && (offset - 16) % kRecordBytes == 0 &&
          readAt(offset, scratch) && !std::strcmp(scratch.book.document, document)) {
        book = scratch.book;
        cursor = offset + kRecordBytes;
      }
    }
  }
  int batch = 0;
  for (;;) {
    const auto status = next(scratch);
    if (status == ScanResult::Finished) break;
    if (status == ScanResult::Error) {
      close();
      return false;
    }
    if (status == ScanResult::Record && !std::strcmp(scratch.book.document, document)) book = scratch.book;
    if (++batch == 64) {
      batch = 0;
      vTaskDelay(1);
    }
  }
  close();
  return true;
}
bool Repository::append(const Record& record) {
  if (!Storage.ensureDirectoryExists(kRoot)) return false;
  if (!Storage.exists(kJournalPath)) {
    HalFile fresh;
    if (!Storage.openFileForWrite("READ-STATS", kJournalPath, fresh) ||
        fresh.write(kJournalHeader, sizeof(kJournalHeader)) != sizeof(kJournalHeader))
      return false;
  }
  uint64_t offset = 0;
  {
    HalFile output;
    if (!Storage.openFileForUpdate("READ-STATS", kJournalPath, output) || !header(output)) return false;
    const auto length = static_cast<uint64_t>(output.size());
    offset = 16 + (length - 16) / kRecordBytes * kRecordBytes;
    if (offset != length && !output.truncate(offset)) return false;
    encode(record, buffer);
    if (!output.seek64(offset) || output.write(buffer, sizeof(buffer)) != sizeof(buffer)) return false;
  }
  if (!Storage.openFileForRead("READ-STATS", kJournalPath, file) || !readAt(offset, scratch)) {
    close();
    return false;
  }
  close();
  char path[100];
  cachePath(path, sizeof(path), record.book.document);
  uint8_t index[12];
  setOffset(index, offset);
  const auto hash = checksum(index, 8);
  for (int n = 0; n < 4; ++n) index[8 + n] = static_cast<uint8_t>(hash >> (n * 8));
  HalFile cached;
  if (!Storage.openFileForWrite("READ-STATS", path, cached) || cached.write(index, sizeof(index)) != sizeof(index))
    LOG_ERR("READ-STATS", "Index save failed; journal remains recoverable");
  return true;
}
bool Repository::record(const char* document, const char* title, const char* author, int64_t epoch, uint32_t seconds,
                        uint32_t pages, uint16_t progressBp) {
  if (!seconds || !validDocument(document)) return false;
  const auto timestamp = static_cast<time_t>(epoch);
  tm local{};
  if (!localtime_r(&timestamp, &local) || local.tm_year < 120) return false;
  if (!loadBook(document, previous)) return false;
  scratch = {};
  scratch.epoch = epoch;
  scratch.seconds = seconds;
  scratch.pages = pages;
  scratch.localDay = dayIndex({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
  scratch.secondOfDay = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  scratch.book.progressBp = std::min<uint16_t>(10000, progressBp);
  copyText(scratch.book.document, sizeof(scratch.book.document), document);
  copyText(scratch.book.title, sizeof(scratch.book.title), title);
  copyText(scratch.book.author, sizeof(scratch.book.author), author);
  updateBook(scratch, previous);
  return append(scratch);
}
bool Repository::complete(const char* document, const char* title, const char* author, int64_t epoch) {
  if (!validDocument(document)) return false;
  const auto timestamp = static_cast<time_t>(epoch);
  tm local{};
  if (!localtime_r(&timestamp, &local) || local.tm_year < 120 || local.tm_year > 299) return false;
  if (!loadBook(document, previous)) return false;
  if (previous.completed && previous.progressBp == 10000) return true;
  scratch = {};
  scratch.kind = RecordKind::Completion;
  scratch.epoch = epoch;
  scratch.localDay = dayIndex({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
  scratch.secondOfDay = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  scratch.book.progressBp = 10000;
  copyText(scratch.book.document, sizeof(scratch.book.document), document);
  copyText(scratch.book.title, sizeof(scratch.book.title), title);
  copyText(scratch.book.author, sizeof(scratch.book.author), author);
  updateBook(scratch, previous);
  return append(scratch);
}
bool recordSession(const char* document, const char* title, const char* author, int64_t epoch, uint32_t seconds,
                   uint32_t pages, uint16_t progressBp) {
  // Journal descriptors/buffers are about 1 KiB, allocated only while flushing.
  auto repository = makeUniqueNoThrow<Repository>();
  if (!repository) {
    LOG_ERR("READ-STATS", "OOM: journal workspace");
    return false;
  }
  if (repository->record(document, title, author, epoch, seconds, pages, progressBp)) return true;
  LOG_ERR("READ-STATS", "Could not record reading session");
  return false;
}
bool recordCompletion(const char* document, const char* title, const char* author, int64_t epoch) {
  // Reuse the same bounded, short-lived journal workspace as session flushing.
  auto repository = makeUniqueNoThrow<Repository>();
  if (!repository) {
    LOG_ERR("READ-STATS", "OOM: completion workspace");
    return false;
  }
  if (repository->complete(document, title, author, epoch)) return true;
  LOG_ERR("READ-STATS", "Could not record book completion");
  return false;
}
}  // namespace readingstats

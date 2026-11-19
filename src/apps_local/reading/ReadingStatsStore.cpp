#include "ReadingStatsStore.h"

#include <HalClock.h>
#include <Logging.h>
#include <Memory.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>
#include <ctime>

#include "util/ReadingTimeMemo.h"

namespace readingstats {
namespace {
constexpr const char* kRoot = "/.crosspoint/reading-history";
constexpr const char* kResetStage = "/.crosspoint/reading-history/.reset.tmp";
constexpr const char* kResetBackup = "/.crosspoint/reading-history/.reset.bak";
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
    if (Storage.exists(kResetBackup)) {
      if (!Storage.rename(kResetBackup, kJournalPath)) {
        LOG_ERR("READ-STATS", "Cannot recover interrupted book reset");
        return false;
      }
    } else {
      end = 16;
      cursor = 16;
      return true;
    }
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
bool Repository::beginFrom(uint64_t offset) {
  if (!begin() || offset < 16 || offset > end || (offset - 16) % kRecordBytes) {
    close();
    return false;
  }
  cursor = offset;
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
bool Repository::cachedBook(const char* document, Book& book) {
  if (!validDocument(document) || !begin()) return false;
  char path[100];
  cachePath(path, sizeof(path), document);
  uint8_t index[12];
  HalFile cached;
  if (!Storage.openFileForRead("READ-STATS", path, cached) || cached.size() != sizeof(index) ||
      cached.read(index, sizeof(index)) != static_cast<int>(sizeof(index)))
    return false;
  const auto offset = getOffset(index);
  const uint32_t found =
      uint32_t(index[8]) | uint32_t(index[9]) << 8 | uint32_t(index[10]) << 16 | uint32_t(index[11]) << 24;
  if (checksum(index, 8) != found || offset < 16 || offset >= end || (offset - 16) % kRecordBytes ||
      !readAt(offset, scratch) || std::strcmp(scratch.book.document, document))
    return false;
  book = scratch.book;
  return true;
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
  bool savedIndex = false;
  {
    HalFile cached;
    savedIndex = Storage.openDerivedCacheForWrite("READ-STATS", path, cached) &&
                 cached.write(index, sizeof(index)) == sizeof(index) && cached.close();
  }
  if (!savedIndex)
    LOG_ERR("READ-STATS", "Index save failed; journal remains recoverable");
  else
    readingtime::publish(record.book.document, record.book.seconds);
  return true;
}
bool Repository::repairBookIndex(const char* document, uint64_t offset) {
  char path[100];
  cachePath(path, sizeof(path), document);
  uint8_t index[12];
  {
    HalFile cached;
    if (Storage.openFileForRead("READ-STATS", path, cached) && cached.size() == sizeof(index) &&
        cached.read(index, sizeof(index)) == static_cast<int>(sizeof(index))) {
      const auto saved = getOffset(index);
      const uint32_t hash =
          uint32_t(index[8]) | uint32_t(index[9]) << 8 | uint32_t(index[10]) << 16 | uint32_t(index[11]) << 24;
      if (hash == checksum(index, 8) && saved >= offset && saved < end && (saved - 16) % kRecordBytes == 0 &&
          readAt(saved, scratch) && !std::strcmp(scratch.book.document, document))
        return true;
    }
  }
  setOffset(index, offset);
  const auto hash = checksum(index, 8);
  for (int n = 0; n < 4; ++n) index[8 + n] = static_cast<uint8_t>(hash >> (n * 8));
  HalFile cached;
  return Storage.openDerivedCacheForWrite("READ-STATS", path, cached) &&
         cached.write(index, sizeof(index)) == sizeof(index) && cached.close();
}
bool Repository::resetBook(const char* document) {
  if (!validDocument(document) || !begin()) return false;
  if (!file) return true;
  const auto revision = Storage.stateFileRevision(kJournalPath);
  const auto bytes = file.fileSize64();
  if ((Storage.exists(kResetBackup) && !Storage.remove(kResetBackup)) ||
      (Storage.exists(kResetStage) && !Storage.remove(kResetStage))) {
    LOG_ERR("READ-STATS", "Cannot clear previous reset workspace");
    close();
    return false;
  }
  HalFile output;
  if (!Storage.openFileForWrite("READ-STATS", kResetStage, output) || !file.seek64(0)) {
    LOG_ERR("READ-STATS", "Cannot begin book reset copy");
    close();
    return false;
  }
  uint64_t copied = 0;
  uint32_t expected = 2166136261u;
  const auto copy = [&](size_t size) {
    if (output.write(buffer, size) != size) return false;
    for (size_t n = 0; n < size; ++n) expected = (expected ^ buffer[n]) * 16777619u;
    copied += size;
    return true;
  };
  bool okay = file.read(buffer, 16) == 16 && copy(16);
  unsigned batch = 0;
  for (uint64_t at = 16; okay && at < bytes;) {
    const auto size = static_cast<size_t>(std::min<uint64_t>(kRecordBytes, bytes - at));
    okay = file.read(buffer, size) == static_cast<int>(size);
    if (okay && !(size == kRecordBytes && decode(buffer, scratch) && !std::strcmp(document, scratch.book.document)))
      okay = copy(size);
    at += size;
    if (++batch == 32) {
      batch = 0;
      vTaskDelay(1);
    }
  }
  okay = output.close() && okay;
  close();
  if (okay) okay = Storage.openFileForRead("READ-STATS", kResetStage, output) && output.fileSize64() == copied;
  uint32_t actual = 2166136261u;
  for (uint64_t at = 0; okay && at < copied;) {
    const auto size = static_cast<size_t>(std::min<uint64_t>(sizeof(buffer), copied - at));
    okay = output.read(buffer, size) == static_cast<int>(size);
    for (size_t n = 0; okay && n < size; ++n) actual = (actual ^ buffer[n]) * 16777619u;
    at += size;
    if (++batch == 32) {
      batch = 0;
      vTaskDelay(1);
    }
  }
  output.close();
  if (!okay || actual != expected || revision != Storage.stateFileRevision(kJournalPath)) {
    LOG_ERR("READ-STATS", "Book reset verification failed; history preserved");
    Storage.remove(kResetStage);
    return false;
  }
  if (!Storage.rename(kJournalPath, kResetBackup)) {
    LOG_ERR("READ-STATS", "Cannot begin book reset publication");
    return false;
  }
  if (!Storage.rename(kResetStage, kJournalPath)) {
    LOG_ERR("READ-STATS", "Cannot publish book reset");
    if (!Storage.rename(kResetBackup, kJournalPath)) LOG_ERR("READ-STATS", "Book reset rollback deferred to reopen");
    return false;
  }
  if (!Storage.remove(kResetBackup)) LOG_ERR("READ-STATS", "Reset committed; backup cleanup pending");
  char path[100];
  cachePath(path, sizeof(path), document);
  if (Storage.exists(path) && !Storage.remove(path)) LOG_ERR("READ-STATS", "Reset index cleanup pending");
  // Reverse order repairs each retained book's latest offset before older snapshots.
  if (begin(true)) {
    batch = 0;
    for (;;) {
      const auto state = next(scratch);
      if (state == ScanResult::Finished) break;
      if (state == ScanResult::Error) {
        LOG_ERR("READ-STATS", "Reset committed; index repair interrupted");
        break;
      }
      if (state == ScanResult::Record) {
        char retained[33];
        copyText(retained, sizeof(retained), scratch.book.document);
        if (!repairBookIndex(retained, recordOffset())) LOG_ERR("READ-STATS", "Reset index repair failed");
      }
      if (++batch == 32) {
        batch = 0;
        vTaskDelay(1);
      }
    }
    close();
  } else
    LOG_ERR("READ-STATS", "Reset committed; index repair unavailable");
  readingtime::publish(document, 0);
  return true;
}
bool Repository::record(const char* document, const char* title, const char* author, int64_t epoch, uint32_t seconds,
                        uint32_t pages, uint16_t progressBp, bool* completed) {
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
  if (!append(scratch)) return false;
  if (completed) *completed = scratch.book.completed;
  return true;
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
bool Repository::completionStatus(const char* document, bool& completed, bool& manuallyUnfinished) {
  if (!loadBook(document, previous)) return false;
  completed = previous.completed;
  manuallyUnfinished = previous.manuallyUnfinished;
  return true;
}
bool Repository::reopen(const char* document, const char* title, const char* author, int64_t epoch,
                        uint16_t progressBp) {
  if (!validDocument(document)) return false;
  const auto timestamp = static_cast<time_t>(epoch);
  tm local{};
  if (!localtime_r(&timestamp, &local) || local.tm_year < 120 || local.tm_year > 299 || !loadBook(document, previous))
    return false;
  if (previous.manuallyUnfinished && !previous.completed) return true;
  if (previous.completed && !previous.finishedDay) {
    // Legacy snapshots lack the day; the original journal uses the time zone
    // at completion, which can differ from the device's current time zone.
    if (!begin(true)) return false;
    unsigned batch = 0;
    for (;;) {
      const auto status = next(scratch);
      if (status == ScanResult::Error) {
        close();
        return false;
      }
      if (status == ScanResult::Finished) break;
      if (status == ScanResult::Record && scratch.firstCompletion && !std::strcmp(scratch.book.document, document) &&
          scratch.book.finishedEpoch == previous.finishedEpoch) {
        previous.finishedDay =
            scratch.localDay + static_cast<int>((uint64_t(scratch.secondOfDay) + scratch.seconds) / 86400);
        break;
      }
      if (++batch == 64) {
        batch = 0;
        vTaskDelay(1);
      }
    }
    close();
    if (!previous.finishedDay) return false;
  }
  scratch = {};
  scratch.kind = RecordKind::Reopen;
  scratch.epoch = epoch;
  scratch.localDay = dayIndex({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
  scratch.secondOfDay = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  scratch.book.progressBp = std::min<uint16_t>(9999, progressBp);
  copyText(scratch.book.document, sizeof(scratch.book.document), document);
  copyText(scratch.book.title, sizeof(scratch.book.title), title);
  copyText(scratch.book.author, sizeof(scratch.book.author), author);
  updateBook(scratch, previous);
  return append(scratch);
}
bool recordSession(const char* document, const char* title, const char* author, int64_t epoch, uint32_t seconds,
                   uint32_t pages, uint16_t progressBp, bool* completed) {
  // Journal descriptors/buffers are about 1 KiB, allocated only while flushing.
  auto repository = makeUniqueNoThrow<Repository>();
  if (!repository) {
    LOG_ERR("READ-STATS", "OOM: journal workspace");
    return false;
  }
  if (repository->record(document, title, author, epoch, seconds, pages, progressBp, completed)) return true;
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
bool recordReopening(const char* document, const char* title, const char* author, int64_t epoch, uint16_t progressBp) {
  auto repository = makeUniqueNoThrow<Repository>();
  if (!repository) {
    LOG_ERR("READ-STATS", "OOM: reopening workspace");
    return false;
  }
  if (repository->reopen(document, title, author, epoch, progressBp)) return true;
  LOG_ERR("READ-STATS", "Could not reopen book");
  return false;
}
bool completionStatus(const char* document, bool& completed, bool& manuallyUnfinished) {
  // Cold reader/menu lookup; reuse the bounded journal workspace, never a Book on the task stack.
  auto repository = makeUniqueNoThrow<Repository>();
  if (!repository) {
    LOG_ERR("READ-STATS", "OOM: completion-status workspace");
    return false;
  }
  return repository->completionStatus(document, completed, manuallyUnfinished);
}
}  // namespace readingstats

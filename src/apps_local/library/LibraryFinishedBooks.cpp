#include "LibraryFinishedBooks.h"

#include <KOReaderDocumentId.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
namespace libraryui {
namespace {
constexpr char StampPath[] = "/.crosspoint/collections/finished-cache.bin";
}
bool FinishedCacheStamp::capture(const char* libraryPath, uint64_t recentPaths) {
  std::memset(bytes, 0, sizeof(bytes));
  std::memcpy(bytes, "CFC1", 4);
  const char* paths[] = {readingstats::kJournalPath, libraryPath};
  for (int n = 0; n < 2; ++n) {
    if (!Storage.exists(paths[n])) continue;
    HalFile file;
    if (!Storage.openFileForRead("COLL", paths[n], file)) return false;
    const auto size = file.fileSize64();
    const auto modified = file.modificationTime();
    for (int b = 0; b < 8; ++b) bytes[4 + n * 12 + b] = uint8_t(size >> (8 * b));
    for (int b = 0; b < 4; ++b) bytes[12 + n * 12 + b] = uint8_t(modified >> (8 * b));
    if (n == 1) {
      // CLIX includes the path/content card signature in its header; FAT mtime may be fixed.
      uint8_t header[64];
      const auto count = size_t(std::min<uint64_t>(size, sizeof(header)));
      if (file.read(header, count) != int(count)) return false;
      const auto hash = readingstats::checksum(header, count);
      for (int b = 0; b < 4; ++b) bytes[28 + b] = uint8_t(hash >> (8 * b));
    }
  }
  for (int b = 0; b < 8; ++b) bytes[32 + b] = uint8_t(recentPaths >> (8 * b));
  return true;
}
bool FinishedCacheStamp::matches(uint32_t generation) const {
  HalFile file;
  uint8_t saved[48];
  if (!Storage.openFileForRead("COLL", StampPath, file) || file.size() != sizeof(saved) ||
      file.read(saved, sizeof(saved)) != int(sizeof(saved)) || std::memcmp(bytes, saved, 40))
    return false;
  uint32_t storedGeneration = 0, storedHash = 0;
  for (int b = 0; b < 4; ++b) {
    storedGeneration |= uint32_t(saved[40 + b]) << (8 * b);
    storedHash |= uint32_t(saved[44 + b]) << (8 * b);
  }
  return generation == storedGeneration && readingstats::checksum(saved, 44) == storedHash;
}
bool FinishedCacheStamp::save(uint32_t generation) const {
  if (!Storage.ensureDirectoryExists("/.crosspoint/collections")) return false;
  uint8_t saved[48];
  std::memcpy(saved, bytes, 40);
  for (int b = 0; b < 4; ++b) saved[40 + b] = uint8_t(generation >> (8 * b));
  const auto hash = readingstats::checksum(saved, 44);
  for (int b = 0; b < 4; ++b) saved[44 + b] = uint8_t(hash >> (8 * b));
  {
    HalFile file;
    if (!Storage.openFileForWrite("COLL", StampPath, file) || file.write(saved, sizeof(saved)) != sizeof(saved))
      return false;
  }
  return matches(generation);
}
bool FinishedBooks::begin() {
  hasCompletions = false;
  index.reset();
  memory.reset();
  hashScratch.reset();
  readAhead.reset();
  if (!Storage.exists(readingstats::kJournalPath)) return true;
  // One optional PSRAM block amortizes journal reads across both reconstruction passes.
  readAhead = HalMemory::allocatePsram(16 * 1024);
  if (!repository.begin(false, readAhead.get(), readAhead ? 16 * 1024 : 0)) return false;
  uint64_t events = 0, steps = 0;
  for (;;) {
    const auto state = repository.next(record);
    if (state == readingstats::ScanResult::Finished) break;
    if (state == readingstats::ScanResult::Error) return false;
    if (state == readingstats::ScanResult::Record &&
        (record.book.completed || record.firstCompletion || record.kind == readingstats::RecordKind::Reopen))
      ++events;
    if ((++steps & 63) == 0) delay(1);
  }
  if (repository.hadDamage()) return false;
  repository.close();
  if (!events) return true;
  const auto slots = readingstats::CompletionIndex::slotsFor(events);
  memory = HalMemory::allocatePsram(slots * sizeof(readingstats::CompletionIndex::Entry));
  hashScratch = makeUniqueNoThrow<uint8_t[]>(1024);
  if (!memory || !hashScratch) {
    LOG_ERR("LIB", "OOM: finished-book filter (%u slots)", unsigned(slots));
    return false;
  }
  index.reset(reinterpret_cast<readingstats::CompletionIndex::Entry*>(memory.get()), slots);
  if (!repository.begin(false, readAhead.get(), readAhead ? 16 * 1024 : 0)) return false;
  for (;;) {
    const auto state = repository.next(record);
    if (state == readingstats::ScanResult::Finished) break;
    if (state == readingstats::ScanResult::Error) return false;
    if (state == readingstats::ScanResult::Record) {
      record.firstCompletion = record.book.completed || record.firstCompletion;
      if (!index.apply(record, repository.recordOffset())) return false;
    }
    if ((++steps & 63) == 0) delay(1);
  }
  const bool valid = !repository.hadDamage();
  repository.close();
  hasCompletions = valid;
  return valid;
}
bool FinishedBooks::contains(const std::string& path, bool& finished) {
  finished = false;
  if (!hasCompletions) return true;
  const auto id = KOReaderDocumentId::calculate(path, hashScratch.get(), 1024);
  if (id.empty()) return false;
  finished = index.completed(id.c_str());
  return true;
}
}  // namespace libraryui

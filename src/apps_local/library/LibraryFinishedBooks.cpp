#include "LibraryFinishedBooks.h"

#include <KOReaderDocumentId.h>
#include <Logging.h>
#include <Memory.h>
namespace libraryui {
bool FinishedBooks::begin() {
  if (!Storage.exists(readingstats::kJournalPath)) return true;
  if (!repository.begin()) return false;
  uint64_t events = 0, steps = 0;
  for (;;) {
    const auto state = repository.next(record);
    if (state == readingstats::ScanResult::Finished) break;
    if (state == readingstats::ScanResult::Error) return false;
    if (state == readingstats::ScanResult::Record &&
        (record.firstCompletion || record.kind == readingstats::RecordKind::Reopen))
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
  if (!repository.begin()) return false;
  for (;;) {
    const auto state = repository.next(record);
    if (state == readingstats::ScanResult::Finished) break;
    if (state == readingstats::ScanResult::Error) return false;
    if (state == readingstats::ScanResult::Record && !index.apply(record, repository.recordOffset())) return false;
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

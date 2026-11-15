#include "ReadingTimeMemo.h"

#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>

#include <array>
#include <cstring>
#include <new>

namespace readingtime {
namespace {
constexpr const char* journal = "/.crosspoint/reading-history/sessions.bin";
struct Stamp {
  uint32_t resource, media, state;
  bool operator==(const Stamp&) const = default;
  bool valid() const { return resource && media && state; }
  bool sameSource(const Stamp& other) const {
    return valid() && other.valid() && resource == other.resource && media == other.media;
  }
};
Stamp current() { return {Storage.resourceRevision(), Storage.mediaRevision(), Storage.stateFileRevision(journal)}; }
struct Entry {
  char path[512]{}, document[33]{};
  uint64_t seconds = 0;
  Stamp stamp{};
  bool known = false;
};
struct Data {
  std::array<Entry, 4> entries{};
  size_t next = 0;
};
HalMemory::PsramBuffer memory;
Data* data = nullptr;
bool attempted = false;
void ensure() {
  if (attempted) return;
  attempted = true;
  // Four summaries (~2.4 KiB) survive activity recreation; never task-stack/internal heap.
  if (HalMemory::getPsramHeap().freeBytes >= sizeof(Data) + 512 * 1024) memory = HalMemory::allocatePsram(sizeof(Data));
  if (memory)
    data = new (memory.get()) Data;
  else
    LOG_ERR("HOME", "Reading summary memo unavailable (%u bytes)", unsigned(sizeof(Data)));
}
}  // namespace
bool lookup(const char* path, uint64_t& seconds, bool& known) {
  if (!data) return false;
  const auto stamp = current();
  if (!stamp.valid()) return false;
  for (const auto& entry : data->entries)
    if (entry.stamp == stamp && !std::strcmp(entry.path, path)) {
      seconds = entry.seconds;
      known = entry.known;
      return true;
    }
  return false;
}
void remember(const char* path, const char* document, uint64_t seconds, bool known, uint32_t resourceBefore,
              uint32_t mediaBefore, uint32_t journalBefore) {
  if (!known || !document || !document[0]) return;
  ensure();
  const Stamp before{resourceBefore, mediaBefore, journalBefore};
  if (!data || !before.valid() || !(before == current()) || std::strlen(path) >= 512 || std::strlen(document) > 32)
    return;
  auto* entry = &data->entries[data->next++ % data->entries.size()];
  for (auto& value : data->entries)
    if (!std::strcmp(value.path, path)) {
      entry = &value;
      break;
    }
  std::strcpy(entry->path, path);
  std::strcpy(entry->document, document);
  entry->seconds = seconds;
  entry->known = known;
  entry->stamp = before;
}
void publish(const char* document, uint64_t seconds) {
  if (!data) return;
  const auto stamp = current();
  for (auto& entry : data->entries)
    if (entry.stamp.sameSource(stamp) && !std::strcmp(entry.document, document)) {
      entry.seconds = seconds;
      entry.known = true;
      entry.stamp = stamp;
    }
}
}  // namespace readingtime

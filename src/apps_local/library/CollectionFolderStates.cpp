#include "CollectionFolderStates.h"

#include <Logging.h>

#include <cstring>

namespace collections {
namespace {
bool within(const char* path, const char* folder) {
  const auto length = std::strlen(folder);
  return !std::strcmp(folder, "/") ||
         (!std::strncmp(path, folder, length) && (path[length] == '/' || path[length] == 0));
}
void update(FolderSummary& value) {
  value.state = !value.selected                 ? FolderSelection::None
                : value.selected == value.total ? FolderSelection::All
                                                : FolderSelection::Partial;
}
}  // namespace
bool FolderStates::begin() {
  if (arena_) return true;
  // At most 64 exact paths and summaries (~67 KiB), reused during one edit.
  // PSRAM avoids a large activity/stack array and unbounded per-folder growth.
  arena_ = HalMemory::allocatePsram(Capacity * sizeof(Entry));
  if (!arena_) {
    LOG_ERR("COLL", "OOM: folder states (%u bytes)", unsigned(Capacity * sizeof(Entry)));
    return false;
  }
  clear();
  return true;
}
void FolderStates::clear() {
  if (arena_) std::memset(arena_.get(), 0, Capacity * sizeof(Entry));
  clock_ = 0;
}
void FolderStates::close() {
  arena_.reset();
  clock_ = 0;
}
bool FolderStates::get(const char* path, FolderSummary& summary, bool touch) {
  if (!arena_) return false;
  for (unsigned n = 0; n < Capacity; ++n) {
    auto& entry = entries()[n];
    if (entry.valid && !std::strcmp(entry.path, path)) {
      if (touch) entry.used = ++clock_;
      summary = entry.summary;
      return true;
    }
  }
  return false;
}
void FolderStates::put(const char* path, FolderSummary summary) {
  if (!arena_ || std::strlen(path) >= PathBytes) return;
  unsigned slot = 0;
  for (unsigned n = 0; n < Capacity; ++n) {
    const auto& entry = entries()[n];
    if (!std::strcmp(entry.path, path)) {
      slot = n;
      break;
    }
    if (entry.used < entries()[slot].used) slot = n;
  }
  auto& entry = entries()[slot];
  std::strcpy(entry.path, path);
  entry.summary = summary;
  entry.used = ++clock_;
  entry.valid = true;
}
void FolderStates::changed(const char* path, int64_t delta, bool subtree, bool checked, bool empty) {
  if (!arena_) return;
  for (unsigned n = 0; n < Capacity; ++n) {
    auto& entry = entries()[n];
    if (!*entry.path) continue;
    if (empty) {
      entry.summary.selected = 0;
      entry.summary.state = FolderSelection::None;
      entry.valid = true;
    } else if (subtree && within(entry.path, path)) {
      if (entry.summary.counted) {
        entry.summary.selected = checked ? entry.summary.total : 0;
        update(entry.summary);
        entry.valid = true;
      } else if (!checked) {
        entry.summary.selected = 0;
        entry.summary.state = FolderSelection::None;
        entry.valid = true;
      } else if (entry.summary.state != FolderSelection::None) {
        entry.summary.state = FolderSelection::All;
        entry.valid = true;
      } else
        entry.valid = false;
    } else if (within(path, entry.path) && delta) {
      const int64_t next = int64_t(entry.summary.selected) + delta;
      if (entry.valid && entry.summary.counted && next >= 0 && next <= entry.summary.total) {
        entry.summary.selected = uint32_t(next);
        update(entry.summary);
      } else
        entry.valid = false;
    }
  }
}
void FolderStates::invalidate(const char* path, bool subtree) {
  if (!arena_) return;
  for (unsigned n = 0; n < Capacity; ++n) {
    auto& entry = entries()[n];
    if (*entry.path && (within(path, entry.path) || (subtree && within(entry.path, path)))) entry.valid = false;
  }
}
}  // namespace collections

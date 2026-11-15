#include "FocusStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

namespace focus {
namespace {
constexpr const char* paths[] = {"/.crosspoint/focus-a.bin", "/.crosspoint/focus-b.bin"};
}
bool Repository::read(int index) {
  HalFile file;
  if (!Storage.openFileForRead("FOCUS", paths[index], file)) return false;
  if (file.read(buffer, 4) == 4 && !std::memcmp(buffer, "FCS", 3) && buffer[3] > '4') readOnly = true;
  loadedBytes = file.size();
  return (loadedBytes == kFileBytes || loadedBytes == kV3FileBytes || loadedBytes == kV2FileBytes ||
          loadedBytes == kV1FileBytes) &&
         file.seek(0) && file.read(buffer, loadedBytes) == int(loadedBytes);
}
bool Repository::load(State& state, State& scratch) {
  slot = -1;
  generation = savedHash = 0;
  readOnly = false;
  bool existed = false;
  for (int index = 0; index < 2; ++index) {
    existed |= Storage.exists(paths[index]);
    uint32_t found = 0;
    if (read(index) && decode(buffer, scratch, found, loadedBytes) &&
        (slot < 0 || (found != generation && found - generation < 0x80000000u))) {
      state = scratch;
      slot = index;
      generation = found;
    }
  }
  if (slot < 0 && existed) readOnly = true;
  if (slot >= 0) {
    encode(state, 0, buffer);
    savedHash = checksum(buffer, kFileBytes - 4);
  }
  if (readOnly) LOG_ERR("FOCUS", "Unsupported or damaged save; preserving files");
  return !readOnly;
}
bool Repository::save(const State& state) {
  if (readOnly || !valid(state) || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  encode(state, 0, buffer);
  const uint32_t hash = checksum(buffer, kFileBytes - 4);
  if (slot >= 0 && hash == savedHash) return true;
  const int next = slot == 0 ? 1 : 0;
  encode(state, generation + 1, buffer);
  {
    HalFile file;
    if (!Storage.openFileForWrite("FOCUS", paths[next], file) || file.write(buffer, kFileBytes) != kFileBytes) {
      LOG_ERR("FOCUS", "Failed to write save");
      return false;
    }
  }
  {
    HalFile file;
    uint8_t chunk[128];
    if (!Storage.openFileForRead("FOCUS", paths[next], file) || file.size() != kFileBytes) return false;
    for (size_t at = 0; at < kFileBytes; at += sizeof(chunk)) {
      const size_t size = std::min(sizeof(chunk), kFileBytes - at);
      if (file.read(chunk, size) != int(size) || std::memcmp(chunk, buffer + at, size)) {
        LOG_ERR("FOCUS", "Save verification failed");
        return false;
      }
    }
  }
  slot = next;
  ++generation;
  savedHash = hash;
  return true;
}
}  // namespace focus

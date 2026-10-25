#include "HabitsStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace habits {
namespace {
constexpr const char* kPaths[] = {"/.crosspoint/habits-a.bin", "/.crosspoint/habits-b.bin"};

bool newer(const uint32_t a, const uint32_t b) { return a != b && a - b < 0x80000000u; }
}  // namespace

bool Repository::readSlot(const int slot) {
  HalFile file;
  return Storage.openFileForRead("HABITS", kPaths[slot], file) && file.size() == kFileSize &&
         file.read(buffer, sizeof(buffer)) == static_cast<int>(sizeof(buffer));
}

LoadResult Repository::load(Store& store, Store& scratch) {
  if (!Storage.ensureDirectoryExists("/.crosspoint")) {
    LOG_ERR("HABITS", "SD storage unavailable");
    return LoadResult::Unavailable;
  }
  activeSlot = -1;
  generation = 0;
  bool existed = false;
  bool damaged = false;
  for (int slot = 0; slot < 2; ++slot) {
    if (!Storage.exists(kPaths[slot])) continue;
    existed = true;
    uint32_t found = 0;
    if (!readSlot(slot) || !decode(buffer, sizeof(buffer), scratch, found)) {
      LOG_ERR("HABITS", "Invalid save: %s", kPaths[slot]);
      damaged = true;
      continue;
    }
    if (activeSlot < 0 || newer(found, generation)) {
      store = scratch;
      generation = found;
      activeSlot = slot;
    }
  }
  if (activeSlot >= 0) return damaged ? LoadResult::Recovered : LoadResult::Loaded;
  if (existed) return LoadResult::Damaged;
  return LoadResult::Empty;
}

bool Repository::save(const Store& store) {
  const int slot = activeSlot == 0 ? 1 : 0;
  const uint32_t next = generation + 1;
  if (!encode(store, next, buffer, sizeof(buffer))) {
    LOG_ERR("HABITS", "Cannot encode habit data");
    return false;
  }
  const uint32_t expected = checksum(buffer, sizeof(buffer) - 4);
  {
    HalFile file;
    if (!Storage.openFileForWrite("HABITS", kPaths[slot], file) ||
        file.write(buffer, sizeof(buffer)) != sizeof(buffer)) {
      LOG_ERR("HABITS", "Failed to write %s", kPaths[slot]);
      return false;
    }
  }
  // Read after the scoped handle closes, so a short write never replaces
  // the last good in-memory state or the other file's generation.
  bool verified = readSlot(slot) && checksum(buffer, sizeof(buffer) - 4) == expected;
  for (int n = 0; n < 4; ++n) {
    verified = verified && buffer[sizeof(buffer) - 4 + n] == static_cast<uint8_t>(expected >> (8 * n));
  }
  if (!verified) {
    LOG_ERR("HABITS", "Save verification failed: %s", kPaths[slot]);
    return false;
  }
  generation = next;
  activeSlot = slot;
  return true;
}

}  // namespace habits

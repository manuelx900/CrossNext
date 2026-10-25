#include "ChessPuzzleStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace chesspuzzle {
namespace {
constexpr const char* kPaths[] = {"/.crosspoint/chess-puzzles-a.bin", "/.crosspoint/chess-puzzles-b.bin"};
}
bool Store::read(int index) {
  HalFile file;
  if (!Storage.openFileForRead("CHESS-PUZZLE", kPaths[index], file)) return false;
  if (file.read(buffer, 4) == 4 && !std::memcmp(buffer, "CPZ", 3) && buffer[3] > '2') readOnly = true;
  return file.size() == kProgressBytes && file.seek(0) &&
         file.read(buffer, sizeof(buffer)) == static_cast<int>(sizeof(buffer));
}
bool Store::load(Progress& progress) {
  bool existed = false;
  for (int n = 0; n < 2; ++n) {
    existed |= Storage.exists(kPaths[n]);
    if (!read(n)) continue;
    uint32_t found = 0;
    if (decode(buffer, scratch, found) && (slot < 0 || (found != generation && found - generation < 0x80000000u))) {
      saved = scratch;
      generation = found;
      slot = n;
    }
  }
  if (existed && slot < 0) readOnly = true;
  progress = saved;
  return !readOnly;
}
bool Store::save(const Progress& progress) {
  if (progress.lastIndex == saved.lastIndex && !std::memcmp(progress.cursor, saved.cursor, sizeof(progress.cursor)) &&
      !std::memcmp(progress.solved, saved.solved, sizeof(progress.solved)))
    return !readOnly;
  if (readOnly || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  const int next = slot == 0 ? 1 : 0;
  encode(progress, generation + 1, buffer);
  {
    HalFile file;
    if (!Storage.openFileForWrite("CHESS-PUZZLE", kPaths[next], file) ||
        file.write(buffer, sizeof(buffer)) != sizeof(buffer)) {
      LOG_ERR("CHESS-PUZZLE", "Could not save progress");
      return false;
    }
  }
  uint32_t found = 0;
  if (!read(next) || !decode(buffer, scratch, found) || found != generation + 1) {
    LOG_ERR("CHESS-PUZZLE", "Progress verification failed");
    return false;
  }
  saved = scratch;
  generation = found;
  slot = next;
  return true;
}
bool Store::reset(Progress& progress) {
  // Only called after the explicit in-app reset confirmation.
  for (const auto* path : kPaths) {
    if (Storage.exists(path) && !Storage.remove(path)) {
      LOG_ERR("CHESS-PUZZLE", "Could not reset progress");
      return false;
    }
  }
  saved = {};
  progress = {};
  generation = 0;
  slot = -1;
  readOnly = false;
  return true;
}
}  // namespace chesspuzzle

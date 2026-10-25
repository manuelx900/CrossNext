#include "PocketGamesStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace pocketgames {
namespace {
constexpr const char* paths[2] = {"/.crosspoint/othello-a.bin", "/.crosspoint/othello-b.bin"};
}
uint32_t Store::payloadHash(const State& state) {
  encode(state, 0, buffer);
  const auto* p = buffer + kSaveBytes - 4;
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
bool Store::read(int index) {
  HalFile file;
  if (!Storage.openFileForRead("POCKET-GAMES", paths[index], file)) return false;
  if (file.read(buffer, 4) == 4 && !std::memcmp(buffer, "PGM", 3) && buffer[3] > '1') readOnly = true;
  return file.size() == kSaveBytes && file.seek(0) && file.read(buffer, kSaveBytes) == static_cast<int>(kSaveBytes);
}
bool Store::load(Kind kind, State& state) {
  bool exists = false;
  for (int n = 0; n < 2; ++n) {
    exists |= Storage.exists(paths[n]);
    uint32_t found = 0;
    if (read(n) && decode(buffer, kind, scratch, found) &&
        (slot < 0 || (found != generation && found - generation < 0x80000000u))) {
      state = scratch;
      generation = found;
      slot = n;
    }
  }
  if (exists && slot < 0) readOnly = true;
  if (slot >= 0) savedHash = payloadHash(state);
  return !readOnly;
}
bool Store::save(const State& state) {
  if (readOnly) return false;
  const uint32_t expected = payloadHash(state);
  if (slot >= 0 && expected == savedHash) return true;
  if (!Storage.ensureDirectoryExists("/.crosspoint")) return false;
  const int next = slot == 0 ? 1 : 0;
  encode(state, generation + 1, buffer);
  {
    HalFile file;
    if (!Storage.openFileForWrite("POCKET-GAMES", paths[next], file) || file.write(buffer, kSaveBytes) != kSaveBytes) {
      LOG_ERR("POCKET-GAMES", "Save failed");
      return false;
    }
  }
  uint32_t found = 0;
  if (!read(next) || !decode(buffer, state.kind, scratch, found) || found != generation + 1 ||
      payloadHash(scratch) != expected) {
    LOG_ERR("POCKET-GAMES", "Save verification failed");
    return false;
  }
  savedHash = expected;
  generation = found;
  slot = next;
  return true;
}
}  // namespace pocketgames

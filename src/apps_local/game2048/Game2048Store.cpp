#include "Game2048Store.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
namespace game2048 {
namespace {
constexpr const char* paths[] = {"/.crosspoint/2048-a.bin", "/.crosspoint/2048-b.bin"};
}
bool Repository::load(State& state, uint64_t& best) {
  writable = Storage.ensureDirectoryExists("/.crosspoint");
  if (!writable) return false;
  bool existed = false, future = false;
  for (int n = 0; n < 2; ++n) {
    if (!Storage.exists(paths[n])) continue;
    existed = true;
    HalFile file;
    if (!Storage.openFileForRead("2048", paths[n], file)) continue;
    if (file.read(buffer, 4) == 4 && !std::memcmp(buffer, "204", 3) && buffer[3] > '1') future = true;
    if (file.size() != kSaveBytes || !file.seek(0) || file.read(buffer, kSaveBytes) != static_cast<int>(kSaveBytes))
      continue;
    State candidate;
    uint64_t candidateBest;
    uint32_t found;
    if (!decode(buffer, candidate, candidateBest, found)) continue;
    if (slot < 0 || (found != generation && found - generation < 0x80000000u)) {
      slot = n;
      generation = found;
      state = candidate;
      best = candidateBest;
    }
  }
  writable = !future && (!existed || slot >= 0);
  if (!writable) LOG_ERR("2048", "Save unavailable or unsupported; existing files preserved");
  return writable;
}
bool Repository::save(const State& state, uint64_t best) {
  if (!writable) return false;
  const int next = slot == 0 ? 1 : 0;
  const uint32_t revision = generation + 1;
  encode(state, best, revision, buffer);
  uint8_t expected[4];
  std::memcpy(expected, buffer + kSaveBytes - 4, 4);
  {
    HalFile file;
    if (!Storage.openFileForWrite("2048", paths[next], file) || file.write(buffer, kSaveBytes) != kSaveBytes) {
      LOG_ERR("2048", "Save write failed");
      return false;
    }
  }
  HalFile file;
  if (!Storage.openFileForRead("2048", paths[next], file) || file.size() != kSaveBytes ||
      file.read(buffer, kSaveBytes) != static_cast<int>(kSaveBytes) ||
      std::memcmp(expected, buffer + kSaveBytes - 4, 4)) {
    LOG_ERR("2048", "Save verification failed");
    return false;
  }
  State check;
  uint64_t checkBest;
  uint32_t checkGeneration;
  if (!decode(buffer, check, checkBest, checkGeneration) || checkGeneration != revision) return false;
  slot = next;
  generation = revision;
  return true;
}
}  // namespace game2048

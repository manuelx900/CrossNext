#include "PomodoroStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
namespace pomodoro {
namespace {
constexpr const char* paths[] = {"/.crosspoint/pomodoro-a.bin", "/.crosspoint/pomodoro-b.bin"};
}
uint32_t Store::hash(const State& state) {
  encode(state, 0, buffer);
  const auto* p = buffer + kSaveBytes - 4;
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
bool Store::read(int index) {
  HalFile file;
  if (!Storage.openFileForRead("POMODORO", paths[index], file)) return false;
  if (file.read(buffer, 4) == 4 && !std::memcmp(buffer, "POM", 3) && buffer[3] > '2') readOnly = true;
  return file.size() == kSaveBytes && file.seek(0) && file.read(buffer, kSaveBytes) == static_cast<int>(kSaveBytes);
}
bool Store::load(State& state) {
  bool exists = false;
  for (int n = 0; n < 2; ++n) {
    exists |= Storage.exists(paths[n]);
    uint32_t found = 0;
    if (read(n) && decode(buffer, scratch, found) &&
        (slot < 0 || (found != generation && found - generation < 0x80000000u))) {
      state = scratch;
      generation = found;
      slot = n;
    }
  }
  if (exists && slot < 0) readOnly = true;
  if (slot >= 0) savedHash = hash(state);
  return !readOnly;
}
bool Store::save(const State& state) {
  if (readOnly || !valid(state)) return false;
  const uint32_t expected = hash(state);
  if (slot >= 0 && expected == savedHash) return true;
  if (!Storage.ensureDirectoryExists("/.crosspoint")) return false;
  const int next = slot == 0 ? 1 : 0;
  encode(state, generation + 1, buffer);
  {
    HalFile file;
    if (!Storage.openFileForWrite("POMODORO", paths[next], file) || file.write(buffer, kSaveBytes) != kSaveBytes) {
      LOG_ERR("POMODORO", "Save failed");
      return false;
    }
  }
  uint32_t found = 0;
  if (!read(next) || !decode(buffer, scratch, found) || found != generation + 1 || hash(scratch) != expected) {
    LOG_ERR("POMODORO", "Save verification failed");
    return false;
  }
  slot = next;
  generation = found;
  savedHash = expected;
  return true;
}
}  // namespace pomodoro

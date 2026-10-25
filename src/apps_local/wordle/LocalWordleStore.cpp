#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "LocalWordle.h"

namespace localwordle {
namespace {
constexpr const char* paths[] = {"/.crosspoint/wordle-local-a.bin", "/.crosspoint/wordle-local-b.bin"};
uint32_t storedHash(const uint8_t* p) {
  return uint32_t(p[252]) | uint32_t(p[253]) << 8 | uint32_t(p[254]) << 16 | uint32_t(p[255]) << 24;
}
}  // namespace
bool Store::read(int which) {
  HalFile file;
  if (!Storage.openFileForRead("WORDLE", paths[which], file)) return false;
  const int headerBytes = file.read(buffer, 12);
  if (headerBytes >= 4 && !std::memcmp(buffer, "WDL", 3) && buffer[3] > '1') readOnly = true;
  if (headerBytes == 12 && !std::memcmp(buffer, "WDL1", 4) && buffer[11] > kVocabularyVersion) readOnly = true;
  return file.size() == kSaveBytes && file.seek(0) && file.read(buffer, kSaveBytes) == static_cast<int>(kSaveBytes);
}
void Store::importLegacy(State& state) {
  {
    HalFile file;
    if (Storage.openFileForRead("WORDLE", "/.crosspoint/wordle.res", file) && file.size() <= 65536) {
      int got = 0;
      while ((got = file.read(buffer, kSaveBytes)) > 0)
        for (int n = 0; n < got; ++n) {
          if (!(buffer[n] & wordle::kOnTheDay)) continue;
          const int result = buffer[n] & 0x7f;
          auto& record = state.record;
          if (result >= 1 && result <= 6) {
            ++record.played;
            ++record.won;
            ++record.streak;
            ++record.distribution[result - 1];
            record.maximum = std::max(record.maximum, record.streak);
          } else if (result == wordle::kLost) {
            ++record.played;
            record.streak = 0;
          }
        }
    }
  }
  HalFile file;
  if (!Storage.openFileForRead("WORDLE", "/.crosspoint/wordle.games", file) || file.size() > 256 * 1024) return;
  char line[80]{};
  size_t used = 0;
  bool overflow = false;
  int newest = -1;
  const auto consume = [&] {
    line[used] = 0;
    int day = -1, offset = 0;
    if (!overflow && std::sscanf(line, "%d %n", &day, &offset) == 1 && offset > 0 && day > newest &&
        scratch.game.load(day, std::string(line + offset)) && !scratch.game.over()) {
      state.game = scratch.game;
      state.active = true;
      state.letters = 5;
      newest = day;
    }
    used = 0;
    overflow = false;
  };
  int got = 0;
  while ((got = file.read(buffer, kSaveBytes)) > 0)
    for (int n = 0; n < got; ++n) {
      const char c = buffer[n];
      if (c == '\n')
        consume();
      else if (c != '\r') {
        if (used + 1 < sizeof(line))
          line[used++] = c;
        else
          overflow = true;
      }
    }
  if (used) consume();
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
  if (slot >= 0) {
    encode(state, 0, buffer);
    savedHash = storedHash(buffer);
  } else if (!exists)
    importLegacy(state);
  return !readOnly;
}
bool Store::save(const State& state) {
  if (readOnly) return false;
  encode(state, 0, buffer);
  const uint32_t expected = storedHash(buffer);
  uint32_t decodedGeneration = 0;
  if (!decode(buffer, scratch, decodedGeneration)) return false;
  if (slot >= 0 && savedHash == expected) return true;
  if (!Storage.ensureDirectoryExists("/.crosspoint")) return false;
  const int nextSlot = slot == 0 ? 1 : 0;
  encode(state, generation + 1, buffer);
  {
    HalFile file;
    if (!Storage.openFileForWrite("WORDLE", paths[nextSlot], file) || file.write(buffer, kSaveBytes) != kSaveBytes) {
      LOG_ERR("WORDLE", "Save failed");
      return false;
    }
  }
  if (!read(nextSlot) || !decode(buffer, scratch, decodedGeneration) || decodedGeneration != generation + 1) {
    LOG_ERR("WORDLE", "Save verification failed");
    return false;
  }
  encode(scratch, 0, buffer);
  if (storedHash(buffer) != expected) {
    LOG_ERR("WORDLE", "Save content mismatch");
    return false;
  }
  slot = nextSlot;
  generation = decodedGeneration;
  savedHash = expected;
  return true;
}
}  // namespace localwordle

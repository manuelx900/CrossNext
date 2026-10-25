#include <HalStorage.h>

#include <cstring>

#include "InterceptCore.h"
namespace intercept {
namespace {
constexpr char path[] = "/.crosspoint/intercept.dat";
}
bool load(State& s, bool& writable) {
  s = {};
  writable = true;
  if (!Storage.exists(path)) return true;
  HalFile f;
  if (!Storage.openFileForRead("INTERCEPT", path, f)) {
    writable = false;
    return false;
  }
  State candidate;
  uint8_t bytes[32];
  bool found = false;
  for (int n = 0; n < 2; ++n) {
    if (f.read(bytes, 32) != 32) break;
    if (!std::memcmp(bytes, "INT", 3) && bytes[3] != '1') {
      writable = false;
      return false;
    }
    if (decode(bytes, candidate) && (!found || int32_t(candidate.sequence - s.sequence) > 0)) {
      s = candidate;
      found = true;
    }
  }
  if (!found) writable = false;
  return found;
}
bool save(State& s, bool writable) {
  if (!writable) return false;
  Storage.ensureDirectoryExists("/.crosspoint");
  HalFile f;
  if (!(f = Storage.open(path, O_RDWR | O_CREAT))) return false;
  auto next = s;
  ++next.sequence;
  uint8_t bytes[32];
  encode(next, bytes);
  const auto offset = (next.sequence - 1) % 2 * 32;
  if (!f.seekSet(offset) || f.write(bytes, 32) != 32) return false;
  f.flush();
  State check;
  if (!f.seekSet(offset) || f.read(bytes, 32) != 32 || !decode(bytes, check) || check.sequence != next.sequence)
    return false;
  s.sequence = next.sequence;
  return true;
}
}  // namespace intercept

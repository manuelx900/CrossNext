#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "ToolState.h"
namespace tools {
namespace {
constexpr char path[] = "/.crosspoint/tools.dat";
}
bool load(State& s, bool& writable) {
  writable = true;
  s = {};
  if (!Storage.exists(path)) return true;
  HalFile f;
  if (!Storage.openFileForRead("TOOLS", path, f)) {
    writable = false;
    return false;
  }
  uint8_t bytes[kStateBytes];
  State candidate;
  bool found = false;
  for (int n = 0; n < 2; ++n) {
    if (f.read(bytes, sizeof(bytes)) != sizeof(bytes)) break;
    if (!std::memcmp(bytes, "TLS", 3) && bytes[3] != '1') {
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
  Storage.mkdir("/.crosspoint");
  HalFile f;
  if (!(f = Storage.open(path, O_RDWR | O_CREAT))) {
    LOG_ERR("TOOLS", "Cannot open state");
    return false;
  }
  State next = s;
  ++next.sequence;
  uint8_t bytes[kStateBytes];
  encode(next, bytes);
  if (!f.seekSet(((next.sequence - 1) % 2) * kStateBytes) || f.write(bytes, sizeof(bytes)) != sizeof(bytes)) {
    LOG_ERR("TOOLS", "Could not save state");
    return false;
  }
  f.flush();
  if (!f.seekSet(((next.sequence - 1) % 2) * kStateBytes) || f.read(bytes, sizeof(bytes)) != sizeof(bytes))
    return false;
  State check;
  if (!decode(bytes, check) || check.sequence != next.sequence) return false;
  s.sequence = next.sequence;
  return true;
}
}  // namespace tools

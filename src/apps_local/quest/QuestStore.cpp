#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "QuestCore.h"
namespace quest {
namespace {
constexpr char path[] = "/.crosspoint/reading-quest-v5.dat", pending[] = "/.crosspoint/reading-quest-v5.tmp";
struct Format {
  const char* path;
  int bytes;
  char version;
  bool (*decode)(const uint8_t*, Profile&);
};
constexpr Format formats[] = {
    {path, kProfileBytes, '5', decode},
    {"/.crosspoint/reading-quest-v4.dat", kProfileBytes, '4', decodeV4},
    {"/.crosspoint/reading-quest-v3.dat", kProfileBytes, '3', decodeV3},
    {"/.crosspoint/reading-quest-v2.dat", kV2ProfileBytes, '2', decodeV2},
    {"/.crosspoint/reading-quest.dat", 64, '1', decodeLegacy},
};
}  // namespace
bool load(Profile& p, bool& writable, StoreWork& w) {
  p = {};
  writable = true;
  const Format* source = nullptr;
  for (const auto& format : formats)
    if (Storage.exists(format.path)) {
      source = &format;
      break;
    }
  if (!source) return true;
  HalFile f;
  if (!Storage.openFileForRead("QUEST", source->path, f)) {
    writable = false;
    return false;
  }
  const int size = source->bytes;
  bool found = false;
  for (int n = 0; n < 2; ++n) {
    if (f.read(w.bytes, size) != size) break;
    if (!std::memcmp(w.bytes, "RQU", 3) && w.bytes[3] != source->version) {
      writable = false;
      return false;
    }
    if (source->decode(w.bytes, w.candidate) && (!found || int32_t(w.candidate.sequence - p.sequence) > 0)) {
      p = w.candidate;
      found = true;
    }
  }
  writable = found;
  return found;
}
bool save(Profile& p, bool writable, StoreWork& w) {
  if (!writable) return false;
  Storage.ensureDirectoryExists("/.crosspoint");
  const bool initial = !Storage.exists(path);
  HalFile f = Storage.open(initial ? pending : path, O_RDWR | O_CREAT | (initial ? O_TRUNC : 0));
  if (!f) {
    LOG_ERR("QUEST", "Open profile failed");
    return false;
  }
  w.candidate = p;
  // The new-format file starts a new sequence epoch in its first slot.
  // SdFat cannot seek past EOF when migrating an odd legacy sequence.
  w.candidate.sequence = initial ? 1 : p.sequence + 1;
  const uint32_t nextSequence = w.candidate.sequence;
  encode(w.candidate, w.bytes);
  const size_t at = ((w.candidate.sequence - 1) % 2) * kProfileBytes;
  if (!f.seekSet(at) || f.write(w.bytes, kProfileBytes) != kProfileBytes) {
    LOG_ERR("QUEST", "Write profile failed");
    return false;
  }
  f.flush();
  if (!f.seekSet(at) || f.read(w.bytes, kProfileBytes) != kProfileBytes || !decode(w.bytes, w.candidate) ||
      w.candidate.sequence != nextSequence) {
    LOG_ERR("QUEST", "Verify profile failed");
    return false;
  }
  if (initial) {
    f.close();
    if (!Storage.rename(pending, path)) {
      LOG_ERR("QUEST", "Install profile failed");
      return false;
    }
  }
  p.sequence = w.candidate.sequence;
  return true;
}
}  // namespace quest

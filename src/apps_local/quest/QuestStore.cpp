#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "QuestCore.h"
namespace quest {
namespace {
constexpr char path[] = "/.crosspoint/reading-adventure-v2/reading-quest-v10.dat";
constexpr char pending[] = "/.crosspoint/reading-adventure-v2/reading-quest-v10.tmp";
constexpr char previous[] = "/.crosspoint/reading-adventure-v2/reading-quest-v10.previous";
constexpr char copyPath[] = "/Backups/AventuraLectora-v2/partida-v10.rqu";
constexpr char copyPending[] = "/Backups/AventuraLectora-v2/partida-v10.tmp";
enum class ReadStatus : uint8_t { Missing, Valid, Damaged, Unsupported, Unavailable };
struct Format {
  const char* path;
  int bytes;
  char version;
  bool (*decode)(const uint8_t*, Profile&);
};
constexpr Format current{path, kProfileBytes, 'A', decode};
constexpr Format checkpoint{copyPath, kProfileBytes, 'A', decode};
constexpr Format v9Current{"/.crosspoint/reading-adventure-v2/reading-quest-v9.dat", kProfileBytes, '9', decodeV9};
constexpr Format v9Checkpoint{"/Backups/AventuraLectora-v2/partida-v9.rqu", kProfileBytes, '9', decodeV9};
constexpr Format legacyCurrent{"/.crosspoint/reading-adventure-v2/reading-quest-v8.dat", kV8ProfileBytes, '8',
                               decodeV8};
constexpr Format legacyCheckpoint{"/Backups/AventuraLectora-v2/partida.rqu", kV8ProfileBytes, '8', decodeV8};
constexpr Format formats[] = {
    {"/.crosspoint/reading-adventure-v2/reading-quest-v6.dat", kV6ProfileBytes, '6', decodeV6},
    {"/.crosspoint/reading-adventure-v2/reading-quest-v5.dat", kV5ProfileBytes, '5', decodeV5},
    {"/.crosspoint/reading-adventure-v2/reading-quest-v4.dat", kV5ProfileBytes, '4', decodeV4},
    {"/.crosspoint/reading-adventure-v2/reading-quest-v3.dat", kV5ProfileBytes, '3', decodeV3},
    {"/.crosspoint/reading-adventure-v2/reading-quest-v2.dat", kV2ProfileBytes, '2', decodeV2},
    {"/.crosspoint/reading-adventure-v2/reading-quest.dat", 64, '1', decodeLegacy},
};
bool newerBalanceRecord(const uint8_t* bytes) {
  if (!hasNewerBalance(bytes)) return false;
  uint32_t crc = 0;
  for (int n = 0; n < 4; ++n) crc |= uint32_t(bytes[kProfileBytes - 4 + n]) << (8 * n);
  return crc == readingstats::checksum(bytes, kProfileBytes - 4);
}
ReadStatus read(const Format& format, Profile& out, StoreWork& w) {
  if (!Storage.ready()) return ReadStatus::Unavailable;
  if (!Storage.exists(format.path)) return ReadStatus::Missing;
  HalFile file;
  if (!Storage.openFileForRead("QUEST", format.path, file)) return ReadStatus::Unavailable;
  int best = -1;
  uint32_t sequence = 0;
  for (int n = 0; n < 2; ++n) {
    if (file.read(w.bytes, format.bytes) != format.bytes) break;
    if (!std::memcmp(w.bytes, "RQU", 3) && w.bytes[3] != format.version &&
        !(format.version == '6' && w.bytes[3] == '7'))
      return ReadStatus::Unsupported;
    if (format.version == 'A' && newerBalanceRecord(w.bytes)) return ReadStatus::Unsupported;
    if (format.decode(w.bytes, w.candidate) && (best < 0 || int32_t(w.candidate.sequence - sequence) > 0)) {
      best = n;
      sequence = w.candidate.sequence;
    }
  }
  if (best < 0) return ReadStatus::Damaged;
  if (!file.seekSet(size_t(best) * format.bytes) || file.read(w.bytes, format.bytes) != format.bytes ||
      !format.decode(w.bytes, w.candidate))
    return ReadStatus::Unavailable;
  out = w.candidate;
  return ReadStatus::Valid;
}
bool journalSlot(const char* destination, StoreWork& w, int& latest, uint32_t& sequence) {
  latest = -1;
  sequence = 0;
  if (!Storage.ready()) return false;
  if (!Storage.exists(destination)) return true;
  HalFile file;
  if (!Storage.openFileForRead("QUEST", destination, file)) {
    LOG_ERR("QUEST", "Read journal before write failed");
    return false;
  }
  for (int n = 0; n < 2; ++n) {
    const int bytes = file.read(w.bytes, kProfileBytes);
    if (bytes >= 4 && !std::memcmp(w.bytes, "RQU", 3) && w.bytes[3] != 'A') {
      LOG_ERR("QUEST", "Unsupported journal version; preserving file");
      return false;
    }
    if (bytes != kProfileBytes) break;
    if (newerBalanceRecord(w.bytes)) {
      LOG_ERR("QUEST", "Newer balance revision; preserving journal");
      return false;
    }
    uint32_t crc = 0, epoch = 0;
    for (int i = 0; i < 4; ++i) {
      crc |= uint32_t(w.bytes[kProfileBytes - 4 + i]) << (8 * i);
      epoch |= uint32_t(w.bytes[4 + i]) << (8 * i);
    }
    if (!std::memcmp(w.bytes, "RQUA", 4) && crc == readingstats::checksum(w.bytes, kProfileBytes - 4) &&
        (latest < 0 || int32_t(epoch - sequence) > 0)) {
      latest = n;
      sequence = epoch;
    }
  }
  return true;
}
bool journal(const char* destination, const char* temporary, const Profile& p, StoreWork& w) {
  int latest = -1;
  uint32_t previousSequence = 0;
  if (!journalSlot(destination, w, latest, previousSequence)) return false;
  const bool initial = !Storage.exists(destination);
  HalFile file = Storage.open(initial ? temporary : destination, O_RDWR | O_CREAT | (initial ? O_TRUNC : 0));
  if (!file) {
    LOG_ERR("QUEST", "Open save journal failed");
    return false;
  }
  w.candidate = p;
  const uint32_t sequence = w.candidate.sequence;
  encode(w.candidate, w.bytes);
  const size_t offset = latest < 0 ? 0 : size_t(1 - latest) * kProfileBytes;
  if (!file.seekSet(offset) || file.write(w.bytes, kProfileBytes) != kProfileBytes) {
    LOG_ERR("QUEST", "Write save journal failed");
    return false;
  }
  file.flush();
  if (!file.seekSet(offset) || file.read(w.bytes, kProfileBytes) != kProfileBytes || !decode(w.bytes, w.candidate) ||
      w.candidate.sequence != sequence) {
    LOG_ERR("QUEST", "Verify save journal failed");
    return false;
  }
  if (initial) {
    file.close();
    if (!Storage.rename(temporary, destination)) {
      LOG_ERR("QUEST", "Install save journal failed");
      return false;
    }
  }
  return true;
}
void discardRetiredCopies(StoreWork& w) {
  if (w.retiredCopiesChecked) return;
  w.retiredCopiesChecked = true;
  constexpr char folder[] = "/Backups/AventuraLectora-v2";
  if (!Storage.exists(folder)) return;
  static constexpr Format retired[] = {
      checkpoint,       {copyPending, kProfileBytes, 'A', decode},
      v9Checkpoint,     {"/Backups/AventuraLectora-v2/partida-v9.tmp", kProfileBytes, '9', decodeV9},
      legacyCheckpoint, {"/Backups/AventuraLectora-v2/partida.tmp", kV8ProfileBytes, '8', decodeV8}};
  for (const auto& format : retired) {
    const auto status = read(format, w.candidate, w);
    if ((status == ReadStatus::Valid || status == ReadStatus::Damaged) && !Storage.remove(format.path))
      LOG_ERR("QUEST", "Remove retired copy failed: %s", format.path);
  }
  // Non-recursive removal preserves unrelated files and newer save formats.
  Storage.rmdir(folder);
  Storage.rmdir("/Backups");
}
}  // namespace
bool load(Profile& p, bool& writable, StoreWork& w, bool* foundProfile) {
  static constexpr Profile fresh;
  p = fresh;
  writable = Storage.ready();
  w.notice = StoreNotice::None;
  w.retiredCopiesChecked = false;
  if (foundProfile) *foundProfile = false;
  if (!writable) return false;
  const auto status = read(current, p, w);
  if (status == ReadStatus::Valid) {
    reconcileCollection(p);
    discardRetiredCopies(w);
    if (foundProfile) *foundProfile = true;
    return true;
  }
  if (status == ReadStatus::Unsupported) {
    writable = false;
    return false;
  }
  bool recovered = false;
  for (const char* source : {copyPath, copyPending, pending, previous}) {
    const Format recovery{source, kProfileBytes, 'A', decode};
    const auto result = read(recovery, w.candidate, w);
    if (result == ReadStatus::Unsupported && source == copyPath) {
      writable = false;
      return false;
    }
    if (result == ReadStatus::Valid && (!recovered || int32_t(w.candidate.sequence - p.sequence) > 0)) {
      p = w.candidate;
      recovered = true;
    }
  }
  if (recovered) {
    reconcileCollection(p);
    w.notice = StoreNotice::Recovered;
    if (foundProfile) *foundProfile = true;
    return true;
  }
  if (status != ReadStatus::Missing || Storage.exists(copyPath) || Storage.exists(previous)) {
    writable = false;
    return false;
  }
  {
    const auto legacyStatus = read(v9Current, p, w);
    if (legacyStatus == ReadStatus::Valid) {
      if (foundProfile) *foundProfile = true;
      return true;
    }
    if (legacyStatus == ReadStatus::Unsupported) {
      writable = false;
      return false;
    }
    bool legacyRecovered = false;
    bool legacyPresent = legacyStatus != ReadStatus::Missing;
    for (const char* source : {v9Checkpoint.path, "/Backups/AventuraLectora-v2/partida-v9.tmp",
                               "/.crosspoint/reading-adventure-v2/reading-quest-v9.tmp",
                               "/.crosspoint/reading-adventure-v2/reading-quest-v9.previous"}) {
      const Format recovery{source, kProfileBytes, '9', decodeV9};
      const auto result = read(recovery, w.candidate, w);
      legacyPresent |= result != ReadStatus::Missing;
      if (result == ReadStatus::Unsupported && source == v9Checkpoint.path) {
        writable = false;
        return false;
      }
      if (result == ReadStatus::Valid && (!legacyRecovered || int32_t(w.candidate.sequence - p.sequence) > 0)) {
        p = w.candidate;
        legacyRecovered = true;
      }
    }
    if (legacyRecovered) {
      w.notice = StoreNotice::Recovered;
      if (foundProfile) *foundProfile = true;
      return true;
    }
    if (legacyPresent) {
      writable = false;
      return false;
    }
  }
  const auto legacyStatus = read(legacyCurrent, p, w);
  if (legacyStatus == ReadStatus::Valid) {
    if (foundProfile) *foundProfile = true;
    return true;
  }
  if (legacyStatus == ReadStatus::Unsupported) {
    writable = false;
    return false;
  }
  bool legacyRecovered = false;
  bool legacyPresent = legacyStatus != ReadStatus::Missing;
  for (const char* source : {legacyCheckpoint.path, "/Backups/AventuraLectora-v2/partida.tmp",
                             "/.crosspoint/reading-adventure-v2/reading-quest-v8.tmp",
                             "/.crosspoint/reading-adventure-v2/reading-quest-v8.previous"}) {
    const Format recovery{source, kV8ProfileBytes, '8', decodeV8};
    const auto result = read(recovery, w.candidate, w);
    legacyPresent |= result != ReadStatus::Missing;
    if (result == ReadStatus::Unsupported && source == legacyCheckpoint.path) {
      writable = false;
      return false;
    }
    if (result == ReadStatus::Valid && (!legacyRecovered || int32_t(w.candidate.sequence - p.sequence) > 0)) {
      p = w.candidate;
      legacyRecovered = true;
    }
  }
  if (legacyRecovered) {
    w.notice = StoreNotice::Recovered;
    if (foundProfile) *foundProfile = true;
    return true;
  }
  if (legacyPresent) {
    writable = false;
    return false;
  }
  for (const auto& format : formats) {
    const auto result = read(format, p, w);
    if (result == ReadStatus::Missing) continue;
    writable = result == ReadStatus::Valid;
    if (writable) {
      reconcileCollection(p);
      if (foundProfile) *foundProfile = true;
    }
    return writable;
  }
  return true;
}
bool save(Profile& p, bool writable, StoreWork& w) {
  if (!writable || !Storage.ensureDirectoryExists("/.crosspoint") ||
      !Storage.ensureDirectoryExists("/.crosspoint/reading-adventure-v2"))
    return false;
  w.candidate = p;
  ++w.candidate.sequence;
  if (!journal(path, pending, w.candidate, w)) return false;
  p.sequence = w.candidate.sequence;
  w.notice = StoreNotice::None;
  discardRetiredCopies(w);
  return true;
}
}  // namespace quest

#include "VocabularyNotice.h"

#include <Logging.h>

#include <cstdio>
#include <cstring>

namespace vocabulary {
namespace {
constexpr char currentPath[] = "/.crosspoint/vocabulary/.notice-current.tmp";
constexpr char pendingPath[] = "/.crosspoint/vocabulary/.notice-accepted.bin";
constexpr char markerRoot[] = "/.crosspoint/vocabulary/acknowledged";
constexpr size_t recordBytes = 40;
constexpr uint8_t magic[] = {'W', 'N', '1', 0};
void encodeNotice(uint8_t* record, const char* id) {
  std::memcpy(record, magic, sizeof(magic));
  std::memcpy(record + 4, id, 32);
  const auto crc = checksum(record, 36);
  for (unsigned n = 0; n < 4; ++n) record[36 + n] = static_cast<uint8_t>(crc >> (8 * n));
}
bool validNotice(const uint8_t* record) {
  uint32_t crc = 0;
  for (unsigned n = 0; n < 4; ++n) crc |= uint32_t(record[36 + n]) << (8 * n);
  return !std::memcmp(record, magic, sizeof(magic)) && checksum(record, 36) == crc;
}
void markerPath(char* path, size_t size, const char* id) {
  std::snprintf(path, size, "%s/%c%c/%s.ack", markerRoot, id[0], id[1], id);
}
}  // namespace
bool VocabularyNotice::markerKnown(const char* id) {
  char path[96];
  markerPath(path, sizeof(path), id);
  if (!Storage.exists(path)) return false;
  HalFile file;
  uint8_t record[recordBytes];
  return Storage.openFileForRead("WORDS", path, file) && file.size() == recordBytes &&
         file.read(record, sizeof(record)) == int(sizeof(record)) && validNotice(record) &&
         !std::memcmp(record + 4, id, 32);
}
bool VocabularyNotice::beginLookup(const Lookup kind) {
  const char* path = kind == Lookup::Accepted ? pendingPath : currentPath;
  if (kind != Lookup::Accepted || lookupKind != kind || !lookup) {
    lookup.close();
    lookupKind = Lookup::None;
    if (!Storage.exists(path) || !Storage.openFileForRead("WORDS", path, lookup)) return false;
    lookupKind = kind;
  }
  lookupRemaining = lookup.fileSize64() / recordBytes;
  looking = lookupRemaining != 0;
  return looking;
}
void VocabularyNotice::resume() {
  pending.close();
  failed = false;
  if (!Storage.exists(pendingPath)) return;
  if (!Storage.openFileForRead("WORDS", pendingPath, pending) || pending.size() % recordBytes) {
    failed = true;
    LOG_ERR("WORDS", "Cannot resume vocabulary notice acknowledgment");
  }
}
void VocabularyNotice::beginScan() {
  lookup.close();
  lookupKind = Lookup::None;
  looking = false;
  current.close();
  currentValid = volatileAccepted || !Storage.exists(currentPath) || Storage.remove(currentPath);
}
bool VocabularyNotice::observe(const char* id) {
  if (!validId(id) || markerKnown(id)) return false;
  if (looking) {
    LOG_ERR("WORDS", "Vocabulary notice lookup already active");
    return false;
  }
  copy(lookupId, sizeof(lookupId), id);
  if (beginLookup(Lookup::Accepted)) return false;
  if (volatileAccepted) {
    if (current) current.flush();
    if (beginLookup(Lookup::Current)) return false;
  }
  return appendNew(id);
}
bool VocabularyNotice::stepObservation() {
  if (!looking) return false;
  uint8_t record[recordBytes];
  for (int budget = 0; budget < 8 && lookupRemaining; ++budget) {
    if ((lookup.position() >= lookup.fileSize64() && !lookup.seekSet(0)) ||
        lookup.read(record, sizeof(record)) != int(sizeof(record))) {
      looking = false;
      LOG_ERR("WORDS", "Cannot check accepted vocabulary notice; batch retained");
      return appendNew(lookupId);
    }
    --lookupRemaining;
    if (validNotice(record) && !std::memcmp(record + 4, lookupId, 32)) {
      looking = false;
      return false;
    }
    if (budget == 7 && lookupRemaining) return false;
  }
  looking = false;
  if (lookupKind == Lookup::Accepted && volatileAccepted) {
    if (current) current.flush();
    if (beginLookup(Lookup::Current)) return false;
  }
  return appendNew(lookupId);
}
bool VocabularyNotice::appendNew(const char* id) {
  if (!current) {
    const bool exists = Storage.exists(currentPath);
    if (!(exists ? Storage.openFileForUpdate("WORDS", currentPath, current)
                 : Storage.openFileForWrite("WORDS", currentPath, current)) ||
        (exists && (current.size() % recordBytes || !current.seekSet(current.size())))) {
      currentValid = false;
      return true;
    }
  }
  uint8_t record[recordBytes];
  encodeNotice(record, id);
  if (current.write(record, sizeof(record)) != sizeof(record)) currentValid = false;
  return true;
}
bool VocabularyNotice::acknowledge() {
  if (lookupKind == Lookup::Current) {
    lookup.close();
    lookupKind = Lookup::None;
    looking = false;
  }
  if (current) current.flush();
  if (current && !current.close()) currentValid = false;
  // Rename commits the complete choice before the UI hides it. The accepted
  // batch survives exit/power loss while markers are built cooperatively.
  if (!currentValid || Storage.exists(pendingPath) || !Storage.exists(currentPath) ||
      !Storage.rename(currentPath, pendingPath)) {
    volatileAccepted = true;
    LOG_ERR("WORDS", "Cannot persist vocabulary notice acknowledgment");
    return false;
  }
  volatileAccepted = false;
  resume();
  return true;
}
bool VocabularyNotice::writeMarker(const char* id, const uint8_t* record) {
  if (markerKnown(id)) return true;
  if (!Storage.ensureDirectoryExists(markerRoot)) return false;
  char directory[64];
  std::snprintf(directory, sizeof(directory), "%s/%c%c", markerRoot, id[0], id[1]);
  if (!Storage.ensureDirectoryExists(directory)) return false;
  char path[96];
  markerPath(path, sizeof(path), id);
  if (Storage.exists(path) && !Storage.remove(path)) return false;
  {
    HalFile file;
    if (!Storage.openFileForWrite("WORDS", path, file) || file.write(record, recordBytes) != recordBytes) return false;
    file.flush();
  }
  return markerKnown(id);
}
void VocabularyNotice::step() {
  if (!busy() || observing()) return;
  if (pending.position() == pending.size()) {
    lookup.close();
    lookupKind = Lookup::None;
    pending.close();
    if (!Storage.remove(pendingPath)) failed = true;
    return;
  }
  uint8_t record[recordBytes];
  char id[kIdBytes];
  if (pending.read(record, sizeof(record)) == int(sizeof(record)) && validNotice(record)) {
    std::memcpy(id, record + 4, 32);
    id[32] = 0;
    if (validId(id) && writeMarker(id, record)) return;
  }
  failed = true;
  LOG_ERR("WORDS", "Vocabulary notice marker pending; accepted batch retained");
}
void VocabularyNotice::close() {
  lookup.close();
  lookupKind = Lookup::None;
  looking = false;
  current.close();
  pending.close();
  if (Storage.ready() && Storage.exists(currentPath)) Storage.remove(currentPath);
}
}  // namespace vocabulary

#include "PetHistory.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace pet {
namespace {
constexpr unsigned ENTRY_BYTES = 52;
static_assert(NAME_BYTES <= ENTRY_BYTES - 24 && 12 + HISTORY_CAPACITY * ENTRY_BYTES + 4 <= HISTORY_BYTES);
constexpr const char* PATHS[] = {"/.crosspoint/pet/lives-a.bin", "/.crosspoint/pet/lives-b.bin"};
void reset(History& h) {
  h.sequence = 0;
  h.count = 0;
  for (auto& record : h.records) record = LifeRecord{};
}
void put(uint8_t* out, uint64_t value, unsigned n) {
  for (unsigned i = 0; i < n; ++i) out[i] = uint8_t(value >> (i * 8));
}
uint64_t get(const uint8_t* in, unsigned n) {
  uint64_t value = 0;
  for (unsigned i = 0; i < n; ++i) value |= uint64_t(in[i]) << (i * 8);
  return value;
}
}  // namespace
bool addLife(History& h, const Profile& p) {
  if (!p.created || p.lifeStatus == LifeStatus::Active || !p.generation || p.endedAt < 1577836800) return false;
  for (unsigned i = 0; i < h.count; ++i) {
    auto& record = h.records[i];
    if (record.generation != p.generation) continue;
    if (record.readingSeconds == p.readingSeconds && record.endedAt == p.endedAt && record.days == p.sharedDays &&
        record.species == p.species && record.reason == p.lifeStatus && !std::strcmp(record.name, p.name))
      return false;
    record.generation = p.generation;
    record.readingSeconds = p.readingSeconds;
    record.endedAt = p.endedAt;
    record.days = p.sharedDays;
    record.species = p.species;
    record.reason = p.lifeStatus;
    std::memcpy(record.name, p.name, NAME_BYTES);
    return true;
  }
  if (h.count == HISTORY_CAPACITY) {
    for (unsigned i = 1; i < h.count; ++i) h.records[i - 1] = h.records[i];
    --h.count;
  }
  auto& record = h.records[h.count++];
  record.generation = p.generation;
  record.readingSeconds = p.readingSeconds;
  record.endedAt = p.endedAt;
  record.days = p.sharedDays;
  record.species = p.species;
  record.reason = p.lifeStatus;
  std::memcpy(record.name, p.name, NAME_BYTES);
  return true;
}
void encodeHistory(const History& h, uint8_t* out) {
  std::memset(out, 0, HISTORY_BYTES);
  std::memcpy(out, "PTL2", 4);
  put(out + 4, h.sequence, 4);
  out[8] = h.count;
  for (unsigned i = 0; i < h.count; ++i) {
    const auto& record = h.records[i];
    auto* entry = out + 12 + i * ENTRY_BYTES;
    put(entry, record.generation, 4);
    put(entry + 4, record.readingSeconds, 8);
    put(entry + 12, uint64_t(record.endedAt), 8);
    put(entry + 20, record.days, 2);
    entry[22] = uint8_t(record.species);
    entry[23] = uint8_t(record.reason);
    std::memcpy(entry + 24, record.name, NAME_BYTES);
  }
  put(out + HISTORY_BYTES - 4, checksum(out, HISTORY_BYTES - 4), 4);
}
bool decodeHistory(const uint8_t* in, History& h, size_t bytes) {
  const bool legacy = bytes == LEGACY_HISTORY_BYTES && !std::memcmp(in, "PTL1", 4);
  if ((!legacy && (bytes != HISTORY_BYTES || std::memcmp(in, "PTL2", 4))) || in[8] > HISTORY_CAPACITY ||
      get(in + bytes - 4, 4) != checksum(in, bytes - 4))
    return false;
  reset(h);
  h.sequence = uint32_t(get(in + 4, 4));
  h.count = in[8];
  for (unsigned i = 0; i < h.count; ++i) {
    const auto* entry = in + 12 + i * (legacy ? 32 : ENTRY_BYTES);
    if (!get(entry, 4) || entry[22] > 2 || entry[23] < 1 || entry[23] > 2 || get(entry + 12, 8) < 1577836800 ||
        get(entry + 12, 8) > INT64_MAX)
      return false;
    auto& record = h.records[i];
    record.generation = uint32_t(get(entry, 4));
    for (unsigned previous = 0; previous < i; ++previous)
      if (h.records[previous].generation == record.generation) return false;
    record.readingSeconds = get(entry + 4, 8);
    record.endedAt = int64_t(get(entry + 12, 8));
    record.days = uint16_t(get(entry + 20, 2));
    record.species = Species(entry[22]);
    record.reason = LifeStatus(entry[23]);
    if (!legacy) {
      if (!validName(reinterpret_cast<const char*>(entry + 24))) return false;
      std::memcpy(record.name, entry + 24, NAME_BYTES);
    }
  }
  return true;
}
bool HistoryStore::load(History& h, bool& writable) {
  reset(h);
  writable = true;
  bool exists = false, found = false;
  for (const auto* path : PATHS) {
    if (!Storage.exists(path)) continue;
    exists = true;
    HalFile file;
    if (!Storage.openFileForRead("PET", path, file)) continue;
    const auto length = file.read(bytes, sizeof(bytes));
    if (length >= 4 && !std::memcmp(bytes, "PTL", 3) && bytes[3] != '1' && bytes[3] != '2') {
      writable = false;
      LOG_ERR("PET", "Unsupported life history version");
      return false;
    }
    if ((file.size() == HISTORY_BYTES || file.size() == LEGACY_HISTORY_BYTES) && length > 0 &&
        size_t(length) == file.size() && decodeHistory(bytes, candidate, length) &&
        (!found || int32_t(candidate.sequence - h.sequence) > 0)) {
      h = candidate;
      found = true;
    }
  }
  writable = found || !exists;
  if (!writable) LOG_ERR("PET", "No valid life history");
  return writable;
}
bool HistoryStore::save(History& h, bool writable) {
  if (!writable || !Storage.ensureDirectoryExists("/.crosspoint") ||
      !Storage.ensureDirectoryExists("/.crosspoint/pet")) {
    LOG_ERR("PET", "Life history is not writable");
    return false;
  }
  candidate = h;
  ++candidate.sequence;
  encodeHistory(candidate, bytes);
  const auto* path = PATHS[candidate.sequence & 1u];
  {
    HalFile file;
    if (!Storage.openFileForWrite("PET", path, file) || file.write(bytes, sizeof(bytes)) != sizeof(bytes)) {
      LOG_ERR("PET", "Could not write life history");
      return false;
    }
  }
  HalFile verify;
  if (!Storage.openFileForRead("PET", path, verify) || verify.size() != HISTORY_BYTES ||
      verify.read(bytes, sizeof(bytes)) != sizeof(bytes) || !decodeHistory(bytes, candidate) ||
      candidate.sequence != h.sequence + 1) {
    LOG_ERR("PET", "Life history read-back failed");
    return false;
  }
  h = candidate;
  return true;
}
}  // namespace pet

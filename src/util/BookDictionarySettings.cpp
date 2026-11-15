#include "BookDictionarySettings.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace bookdictionary {
namespace {
constexpr const char* ROOT = "/.crosspoint/book-dictionaries";
constexpr unsigned HEADER_SIZE = 80;
uint32_t digest(const uint8_t* bytes, size_t count, uint32_t hash = 2166136261u) {
  while (count--) hash = (hash ^ *bytes++) * 16777619u;
  return hash;
}
void pathFor(const char* book, unsigned slot, char (&path)[64]) {
  uint64_t hash = 14695981039346656037ULL;
  for (const auto* p = reinterpret_cast<const uint8_t*>(book); *p; ++p) hash = (hash ^ *p) * 1099511628211ULL;
  std::snprintf(path, sizeof(path), "%s/%08lx%08lx.%u.bin", ROOT, static_cast<unsigned long>(hash >> 32),
                static_cast<unsigned long>(hash & 0xffffffff), slot);
}
bool validName(const char* name) {
  return std::memchr(name, 0, 32) && !std::strchr(name, '/') && !std::strchr(name, '\\') &&
         std::strcmp(name, "..") != 0;
}
bool matches(HalFile& file, const char* book, unsigned length) {
  char chunk[64];
  for (unsigned at = 0; at < length;) {
    const unsigned n = std::min(unsigned(sizeof(chunk)), length - at);
    if (file.read(chunk, n) != int(n) || std::memcmp(chunk, book + at, n)) return false;
    at += n;
  }
  return true;
}
bool writeSlot(const char* book, const Settings& settings, unsigned slot, uint32_t sequence) {
  const size_t length = std::strlen(book);
  uint8_t header[HEADER_SIZE]{};
  std::memcpy(header, "BDIC", 4);
  header[4] = 2;
  header[5] = settings.overrides;
  header[6] = uint8_t(length);
  header[7] = uint8_t(length >> 8);
  for (unsigned n = 0; n < 4; ++n) header[8 + n] = uint8_t(sequence >> (8 * n));
  std::memcpy(header + 12, settings.definition, 32);
  std::memcpy(header + 44, settings.translation, 32);
  const uint32_t hash = digest(header, 76);
  for (unsigned n = 0; n < 4; ++n) header[76 + n] = uint8_t(hash >> (8 * n));
  char temporary[64];
  pathFor(book, slot, temporary);
  HalFile file;
  if (!Storage.openFileForWrite("BDICT", temporary, file)) return false;
  const bool ok = file.write(header, sizeof(header)) == sizeof(header) &&
                  file.write(reinterpret_cast<const uint8_t*>(book), length) == length;
  return ok;
}
}  // namespace
static bool readSlot(const char* book, unsigned slot, Settings& settings, uint32_t& sequence) {
  settings = {};
  if (!book || !*book || std::strlen(book) > 4095) return false;
  char path[64];
  pathFor(book, slot, path);
  HalFile file;
  if (!Storage.openFileForRead("BDICT", path, file)) return false;
  uint8_t header[HEADER_SIZE];
  if (file.read(header, sizeof(header)) != sizeof(header) || std::memcmp(header, "BDIC", 4) || header[4] != 2 ||
      header[5] > 3)
    return false;
  uint32_t hash = 0;
  sequence = 0;
  for (unsigned n = 0; n < 4; ++n) {
    hash |= uint32_t(header[76 + n]) << (8 * n);
    sequence |= uint32_t(header[8 + n]) << (8 * n);
  }
  const unsigned length = unsigned(header[6]) | unsigned(header[7]) << 8;
  if (hash != digest(header, 76) || length != std::strlen(book) || file.size() != sizeof(header) + length ||
      !validName(reinterpret_cast<const char*>(header + 12)) ||
      !validName(reinterpret_cast<const char*>(header + 44)) || !matches(file, book, length))
    return false;
  settings.overrides = header[5];
  std::memcpy(settings.definition, header + 12, 32);
  std::memcpy(settings.translation, header + 44, 32);
  return true;
}
static bool latest(const char* book, Settings& settings, unsigned& active, uint32_t& sequence) {
  settings = {};
  bool found = false;
  for (unsigned slot = 0; slot < 2; ++slot) {
    Settings candidate;
    uint32_t candidateSequence = 0;
    if (readSlot(book, slot, candidate, candidateSequence) && (!found || int32_t(candidateSequence - sequence) > 0)) {
      settings = candidate;
      active = slot;
      sequence = candidateSequence;
      found = true;
    }
  }
  return found;
}
bool load(const char* book, Settings& settings) {
  settings = {};
  if (!book || !*book || std::strlen(book) > 4095) return false;
  unsigned active = 0;
  uint32_t sequence = 0;
  return latest(book, settings, active, sequence);
}
bool save(const char* book, const Settings& settings) {
  if (!book || !*book || std::strlen(book) > 4095 || settings.overrides > 3 || !validName(settings.definition) ||
      !validName(settings.translation))
    return false;
  Settings previous;
  unsigned active = 1;
  uint32_t sequence = 0;
  latest(book, previous, active, sequence);
  // Only overwrite the inactive slot; an interrupted write leaves the previous
  // checksum-validated record readable. No replacing rename is needed.
  uint32_t writtenSequence = 0;
  if (Storage.mkdir(ROOT) && writeSlot(book, settings, 1 - active, sequence + 1) &&
      readSlot(book, 1 - active, previous, writtenSequence) && writtenSequence == sequence + 1 &&
      !std::memcmp(&previous, &settings, sizeof(settings)))
    return true;
  LOG_ERR("BDICT", "Could not save book dictionaries");
  return false;
}
bool move(const char* from, const char* to) {
  if (!from || !to || !*from || !*to || std::strlen(from) > 4095 || std::strlen(to) > 4095) return false;
  if (!std::strcmp(from, to)) return true;
  Settings settings;
  if (!load(from, settings)) return true;
  if (!save(to, settings)) return false;
  for (unsigned slot = 0; slot < 2; ++slot) {
    char path[64];
    pathFor(from, slot, path);
    if (Storage.exists(path) && !Storage.remove(path)) LOG_ERR("BDICT", "Could not remove moved preferences");
  }
  return true;
}
}  // namespace bookdictionary

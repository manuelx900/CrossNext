#include "ScenePack.h"

#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace libraryui {
namespace {
void put32(uint8_t* out, uint32_t value) {
  for (size_t n = 0; n < 4; ++n) out[n] = uint8_t(value >> (n * 8));
}
}  // namespace

bool ScenePack::Key::operator==(const Key& other) const {
  return scene == other.scene && art == other.art && library == other.library && resource == other.resource &&
         media == other.media && bytes == other.bytes && region.x == other.region.x && region.y == other.region.y &&
         region.width == other.region.width && region.height == other.region.height;
}

uint32_t ScenePack::hash(uint32_t value, const uint8_t* data, size_t bytes) {
  for (size_t n = 0; n < bytes; ++n) value = (value ^ data[n]) * 16777619u;
  return value;
}

void ScenePack::path(size_t slot, char* out, size_t bytes) {
  std::snprintf(out, bytes, "/.crosspoint/library-grid-%u.pack", unsigned(slot));
}

void ScenePack::makeHeader(const Key& key, uint32_t crc) {
  std::memset(header, 0, sizeof(header));
  std::memcpy(header, "XNGRID1", 7);
  put32(header + 8, uint32_t(key.scene));
  put32(header + 12, uint32_t(key.scene >> 32));
  put32(header + 16, key.art);
  put32(header + 20, key.library);
  put32(header + 24, key.resource);
  put32(header + 28, key.media);
  put32(header + 32, uint16_t(key.region.x) | uint32_t(uint16_t(key.region.y)) << 16);
  put32(header + 36, uint16_t(key.region.width) | uint32_t(uint16_t(key.region.height)) << 16);
  put32(header + 40, key.bytes);
  put32(header + 44, crc);
}

bool ScenePack::contains(const Key& key) const {
  if (!key.valid() || key.resource != Storage.resourceRevision() || key.media != Storage.mediaRevision()) return false;
  for (const auto& entry : entries)
    if (entry.valid && entry.key == key) return true;
  return false;
}

bool ScenePack::load(const Key& key, uint8_t* pixels, size_t capacity, bool (*continueWork)(void*), void* context) {
  if (!pixels || key.bytes > capacity || !contains(key) || (continueWork && !continueWork(context))) return false;
  for (size_t n = 0; n < entries.size(); ++n) {
    auto& entry = entries[n];
    if (!entry.valid || !(entry.key == key)) continue;
    char name[64];
    path(n, name, sizeof(name));
    HalFile file;
    uint8_t expected[64];
    makeHeader(key, entry.checksum);
    std::memcpy(expected, header, sizeof(expected));
    if (!Storage.openFileForRead("LIB", name, file) || file.fileSize64() != sizeof(header) + key.bytes ||
        file.read(header, sizeof(header)) != sizeof(header) || std::memcmp(expected, header, sizeof(header))) {
      entry.valid = false;
      return false;
    }
    size_t read = 0;
    while (read < key.bytes) {
      if (continueWork && !continueWork(context)) return false;
      const size_t bytes = continueWork ? std::min(Chunk, size_t(key.bytes) - read) : key.bytes - read;
      if (file.read(pixels + read, bytes) != int(bytes)) {
        entry.valid = false;
        return false;
      }
      read += bytes;
    }
    if (continueWork && !continueWork(context)) return false;
    if (hash(2166136261u, pixels, key.bytes) != entry.checksum || key.resource != Storage.resourceRevision() ||
        key.media != Storage.mediaRevision()) {
      entry.valid = false;
      return false;
    }
    entry.age = ++age;
    return true;
  }
  return false;
}

void ScenePack::queue(const Key& key, const uint8_t* pixels) {
  if (!pixels || !key.valid() || contains(key)) return;
  cancel();
  writingKey = key;
  source = pixels;
  written = 0;
  checksum = 2166136261u;
  slot = 0;
  for (size_t n = 0; n < entries.size(); ++n) {
    if (!entries[n].valid) {
      slot = n;
      break;
    }
    if (entries[n].age < entries[slot].age) slot = n;
  }
}

void ScenePack::step() {
  if (!pending()) return;
  if (writingKey.resource != Storage.resourceRevision() || writingKey.media != Storage.mediaRevision()) {
    cancel();
    return;
  }
  if (!writer.isOpen()) {
    entries[slot].valid = false;
    char name[64];
    path(slot, name, sizeof(name));
    if (!Storage.openDerivedCacheForWrite("LIB", name, writer)) {
      cancel();
      return;
    }
    std::memset(header, 0, sizeof(header));
    if (writer.write(header, sizeof(header)) != sizeof(header)) {
      cancel();
      return;
    }
  }
  const auto bytes = std::min(Chunk, size_t(writingKey.bytes) - written);
  if (writer.write(source + written, bytes) != bytes) {
    cancel();
    return;
  }
  checksum = hash(checksum, source + written, bytes);
  written += bytes;
  if (written < writingKey.bytes) return;
  makeHeader(writingKey, checksum);
  const bool committed = writer.seek(0) && writer.write(header, sizeof(header)) == sizeof(header);
  const bool closed = writer.close();
  writer = HalFile{};
  if (committed && closed && writingKey.resource == Storage.resourceRevision() &&
      writingKey.media == Storage.mediaRevision()) {
    entries[slot] = {writingKey, checksum, ++age, true};
  } else {
    LOG_ERR("LIB", "Discarding incomplete Library scene pack");
  }
  source = nullptr;
}

void ScenePack::cancel() {
  if (writer.isOpen()) writer.close();
  writer = HalFile{};
  source = nullptr;
}
}  // namespace libraryui

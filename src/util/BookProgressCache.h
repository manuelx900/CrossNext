#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace bookprogress {
struct LiveStamp {
  uint32_t state = 0, resource = 0, media = 0, library = 0;
  bool operator==(const LiveStamp&) const = default;
  bool valid() const { return state && resource && media && library; }
  bool sameSource(const LiveStamp& other) const {
    return valid() && other.valid() && resource == other.resource && media == other.media && library == other.library;
  }
};
struct Stamp {
  std::array<uint8_t, 10> progress{};
  uint64_t sourceBytes = 0, metadataBytes = 0;
  uint32_t sourceModified = 0, metadataModified = 0;
  int progressBytes = 0;
  uint32_t libraryRevision = 0, artRevision = 0;
  uint32_t resource = 0, media = 0;
  bool operator==(const Stamp&) const = default;
};

// Validated results, without retaining EPUB objects or metadata vectors.
class Cache {
 public:
  static constexpr size_t CAPACITY = 64;
  static constexpr size_t PATH_SIZE = 512;
  int findLive(const char* path, LiveStamp stamp) {
    if (!stamp.valid()) return -1;
    for (auto& entry : entries)
      if (entry.percent >= 0 && entry.live == stamp && !std::strcmp(entry.path, path)) {
        entry.age = ++age;
        return entry.percent;
      }
    return -1;
  }
  void bindLive(const char* path, LiveStamp stamp) {
    if (!stamp.valid()) return;
    for (auto& entry : entries)
      if (entry.percent >= 0 && !std::strcmp(entry.path, path)) {
        entry.live = stamp;
        return;
      }
  }
  // Publish only into a previously validated source snapshot.
  void publish(const char* path, LiveStamp stamp, int percent) {
    if (percent < 0 || percent > 100) return;
    for (auto& entry : entries)
      if (entry.percent >= 0 && entry.live.sameSource(stamp) && !std::strcmp(entry.path, path)) {
        entry.live = stamp;
        entry.stamp.progressBytes = 0;
        entry.percent = percent;
        entry.age = ++age;
        return;
      }
  }
  int findProgress(const char* path, const uint8_t* bytes, int length, uint32_t libraryRevision, uint32_t artRevision,
                   uint32_t resource = 0, uint32_t media = 0) {
    for (auto& entry : entries)
      if (entry.percent >= 0 && !std::strcmp(entry.path, path) && entry.stamp.progressBytes == length &&
          entry.stamp.libraryRevision == libraryRevision && entry.stamp.artRevision == artRevision &&
          entry.stamp.resource == resource && entry.stamp.media == media &&
          !std::memcmp(entry.stamp.progress.data(), bytes, 10)) {
        entry.age = ++age;
        return entry.percent;
      }
    return -1;
  }
  int find(const char* path, const Stamp& stamp) {
    for (auto& entry : entries)
      if (entry.percent >= 0 && !std::strcmp(entry.path, path) && entry.stamp == stamp) {
        entry.age = ++age;
        return entry.percent;
      }
    return -1;
  }
  void store(const char* path, const Stamp& stamp, int percent) {
    if (percent < 0 || std::strlen(path) >= PATH_SIZE) return;
    auto* slot = &entries[0];
    for (auto& entry : entries) {
      if (entry.percent < 0 || !std::strcmp(entry.path, path)) {
        slot = &entry;
        break;
      }
      if (entry.age < slot->age) slot = &entry;
    }
    std::strcpy(slot->path, path);
    slot->stamp = stamp;
    slot->live = {};
    slot->percent = percent;
    slot->age = ++age;
  }

 private:
  struct Entry {
    char path[PATH_SIZE]{};
    Stamp stamp{};
    LiveStamp live;
    uint32_t age = 0;
    int percent = -1;
  };
  std::array<Entry, CAPACITY> entries{};
  uint32_t age = 0;
};
}  // namespace bookprogress

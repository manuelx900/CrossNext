#pragma once

#include <FreeInkUI.h>
#include <HalStorage.h>

#include <array>
#include <cstdint>

namespace libraryui {
// The descriptors live only for this boot. A pack is never trusted solely
// because a file from a previous session happens to exist.
class ScenePack {
 public:
  static constexpr size_t Slots = 8, Capacity = 48 * 1024, Chunk = 4096;
  struct Key {
    uint64_t scene = 0;
    uint32_t art = 0, library = 0, resource = 0, media = 0;
    freeink::ui::Rect region{};
    uint32_t bytes = 0;
    bool operator==(const Key& other) const;
    bool valid() const { return scene && resource && media && bytes && bytes <= Capacity; }
  };
  bool contains(const Key& key) const;
  bool load(const Key& key, uint8_t* pixels, size_t capacity, bool (*continueWork)(void*) = nullptr,
            void* context = nullptr);
  // Borrowed pixels must stay unchanged until pending() becomes false or cancel().
  void queue(const Key& key, const uint8_t* pixels);
  bool pending() const { return source != nullptr; }
  void step();
  void cancel();

 private:
  struct Entry {
    Key key;
    uint32_t checksum = 0, age = 0;
    bool valid = false;
  };
  std::array<Entry, Slots> entries{};
  HalFile writer;
  Key writingKey;
  const uint8_t* source = nullptr;
  size_t written = 0, slot = 0;
  uint32_t checksum = 2166136261u, age = 0;
  uint8_t header[64]{};
  static uint32_t hash(uint32_t value, const uint8_t* data, size_t bytes);
  void makeHeader(const Key& key, uint32_t checksum);
  static void path(size_t slot, char* out, size_t bytes);
};
}  // namespace libraryui

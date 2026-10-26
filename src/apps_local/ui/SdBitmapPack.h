#pragma once

#include <FreeInkUI.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <cstdint>

namespace sdart {
// Little-endian CNART001/2: 24-byte header, index and BW1 pixels.
inline constexpr uint32_t HEADER_BYTES = 24, RECORD_BYTES = 12;
inline constexpr uint16_t MAX_WIDTH = 280, MAX_HEIGHT = 512;
struct Image {
  uint32_t offset = 0, bytes = 0, checksum = 0;
  uint16_t width = 0, height = 0;
};
class Pack {
 public:
  bool open(const char* path, uint32_t expectedCount, size_t pixelCacheBytes = 0);
  void close();
  bool ready() const { return valid; }
  Image image(uint32_t index);
  void draw(freeink::ui::DrawTarget&, freeink::ui::Rect, uint32_t index,
            freeink::ui::BitmapMode = freeink::ui::BitmapMode::Contain, freeink::ui::Color = freeink::ui::Color::Black);

 private:
  HalFile file;
  // Activity-owned scratch: reused by validation and every sprite, never a framebuffer.
  uint8_t buffer[512]{};
  uint8_t row[(MAX_WIDTH + 7) / 8]{};
  static constexpr size_t READ_AHEAD_BYTES = 4096;
  HalMemory::PsramBuffer cache;
  struct CachedImage {
    uint32_t index = 0, offset = 0;
  } cachedImages[16]{};
  size_t pixelCapacity = 0, pixelUsed = 0;
  unsigned cachedCount = 0;
  uint32_t count = 0, fileBytes = 0;
  uint8_t recordBytes = RECORD_BYTES;
  bool perImageChecksums = false;
  uint8_t verified[4096 / 8]{};
  bool valid = false;
  bool read(void*, size_t);
  bool fail(const char* reason);
  Image decodeRecord(const uint8_t* record);
  bool verify(uint32_t index, const Image& image);
  const uint8_t* pixels(uint32_t index, const Image& image);
  uint8_t* readBuffer() { return cache ? cache.get() + count * recordBytes : buffer; }
  size_t readBufferBytes() const { return cache ? READ_AHEAD_BYTES : sizeof(buffer); }
};
}  // namespace sdart

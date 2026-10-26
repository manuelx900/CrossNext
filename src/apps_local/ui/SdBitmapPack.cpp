#include "SdBitmapPack.h"

#include <Arduino.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace sdart {
namespace {
uint16_t le16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t le32(const uint8_t* p) { return uint32_t(le16(p)) | uint32_t(le16(p + 2)) << 16; }
uint32_t crc32(uint32_t crc, const uint8_t* data, size_t size) {
  static constexpr auto TABLE = [] {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < table.size(); ++i) {
      uint32_t value = i;
      for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xedb88320U : 0U);
      table[i] = value;
    }
    return table;
  }();
  crc = ~crc;
  for (size_t i = 0; i < size; ++i) {
    crc = (crc >> 8) ^ TABLE[(crc ^ data[i]) & 255];
  }
  return ~crc;
}
}  // namespace
void Pack::close() {
  valid = false;
  count = fileBytes = 0;
  cache.reset();
  pixelCapacity = pixelUsed = cachedCount = 0;
  std::memset(verified, 0, sizeof(verified));
  file.close();
}
bool Pack::fail(const char* reason) {
  LOG_ERR("SDART", "%s", reason);
  close();
  return false;
}
bool Pack::read(void* out, size_t bytes) { return file.read(out, bytes) == int(bytes) || fail("Short resource read"); }
bool Pack::open(const char* path, uint32_t expectedCount, size_t pixelCacheBytes) {
  close();
  if (!Storage.openFileForRead("SDART", path, file)) return fail("Resource pack missing");
  uint8_t header[HEADER_BYTES];
  if (!read(header, sizeof(header))) return false;
  perImageChecksums = std::memcmp(header, "CNART002", 8) == 0;
  recordBytes = perImageChecksums ? 16 : RECORD_BYTES;
  if ((!perImageChecksums && std::memcmp(header, "CNART001", 8)) || le32(header + 8) != expectedCount ||
      !expectedCount || expectedCount > 4096 || (!perImageChecksums && le32(header + 20) != 0))
    return fail("Incompatible resource pack");
  count = expectedCount;
  fileBytes = le32(header + 12);
  if (fileBytes < HEADER_BYTES + count * recordBytes || fileBytes > 8 * 1024 * 1024 || file.fileSize64() != fileBytes)
    return fail("Incomplete resource pack");
  const uint32_t expectedCrc = le32(header + (perImageChecksums ? 20 : 16));
  const uint32_t indexBytes = count * recordBytes;
  // The activity reuses one PSRAM allocation for the index and read-ahead.
  // Base cache is bounded to 69,632 bytes; optional retained sprites add at most 64 KiB.
  if (HalMemory::getPsramHeap().totalBytes > 0) {
    pixelCapacity = std::min<size_t>(pixelCacheBytes, 64 * 1024);
    cache = HalMemory::allocatePsram(indexBytes + READ_AHEAD_BYTES + pixelCapacity);
    if (!cache && pixelCapacity) {
      LOG_ERR("SDART", "OOM: %u-byte sprite cache; using read-ahead only", unsigned(pixelCapacity));
      pixelCapacity = 0;
      cache = HalMemory::allocatePsram(indexBytes + READ_AHEAD_BYTES);
    }
    if (!cache)
      LOG_ERR("SDART", "OOM: %u-byte PSRAM cache; using small reads", unsigned(indexBytes + READ_AHEAD_BYTES));
  }
  auto* const io = readBuffer();
  uint32_t crc = 0, remaining = perImageChecksums ? indexBytes : fileBytes - HEADER_BYTES, offset = 0;
  auto lastYield = millis();
  while (remaining) {
    const size_t bytes = std::min<size_t>(remaining, readBufferBytes());
    if (!read(io, bytes)) return false;
    if (cache && offset < indexBytes)
      std::memcpy(cache.get() + offset, io, std::min<size_t>(bytes, indexBytes - offset));
    crc = crc32(crc, io, bytes);
    remaining -= bytes;
    offset += bytes;
    if (millis() - lastYield >= 250) {
      delay(1);
      lastYield = millis();
    }
  }
  if (crc != expectedCrc) return fail("Resource checksum mismatch");
  valid = true;
  uint32_t next = HEADER_BYTES + count * recordBytes;
  if (!cache && !file.seek(HEADER_BYTES)) return fail("Cannot seek resource index");
  const uint32_t recordsPerBatch = sizeof(buffer) / recordBytes;
  for (uint32_t first = 0; first < count; first += recordsPerBatch) {
    const uint32_t records = std::min(recordsPerBatch, count - first);
    const uint8_t* entries = cache ? cache.get() + first * recordBytes : buffer;
    if (!cache && !read(buffer, records * recordBytes)) return false;
    for (uint32_t i = 0; i < records; ++i) {
      const auto entry = decodeRecord(entries + i * recordBytes);
      if (!valid) return false;
      if (entry.offset != next) return fail("Invalid resource index");
      next += entry.bytes;
    }
  }
  if (next != fileBytes) return fail("Invalid resource data size");
  return true;
}
Image Pack::image(uint32_t index) {
  Image result;
  if (!valid) return result;
  if (index >= count) {
    fail("Resource index out of range");
    return result;
  }
  if (cache) return decodeRecord(cache.get() + index * recordBytes);
  uint8_t record[16];
  if (!file.seek(HEADER_BYTES + index * recordBytes)) {
    fail("Cannot seek resource index");
    return result;
  }
  if (!read(record, recordBytes)) return result;
  return decodeRecord(record);
}
Image Pack::decodeRecord(const uint8_t* record) {
  Image result;
  result.offset = le32(record);
  result.width = le16(record + 4);
  result.height = le16(record + 6);
  result.bytes = le32(record + 8);
  result.checksum = perImageChecksums ? le32(record + 12) : 0;
  if (!result.width || result.width > MAX_WIDTH || !result.height || result.height > MAX_HEIGHT ||
      result.bytes != uint32_t((result.width + 7) / 8) * result.height ||
      result.offset < HEADER_BYTES + count * recordBytes || result.offset > fileBytes ||
      result.bytes > fileBytes - result.offset) {
    fail("Invalid resource dimensions or offset");
    return {};
  }
  return result;
}
bool Pack::verify(uint32_t index, const Image& image) {
  if (!perImageChecksums || (verified[index >> 3] & (1u << (index & 7)))) return true;
  if (!file.seek(image.offset)) return fail("Cannot seek resource checksum");
  uint32_t remaining = image.bytes, crc = 0;
  while (remaining) {
    const auto bytes = std::min<size_t>(remaining, readBufferBytes());
    if (!read(readBuffer(), bytes)) return false;
    crc = crc32(crc, readBuffer(), bytes);
    remaining -= bytes;
  }
  if (crc != image.checksum) return fail("Resource image checksum mismatch");
  verified[index >> 3] |= uint8_t(1u << (index & 7));
  return true;
}
const uint8_t* Pack::pixels(uint32_t index, const Image& image) {
  if (!cache || image.bytes > pixelCapacity) return nullptr;
  auto* const data = cache.get() + count * recordBytes + READ_AHEAD_BYTES;
  for (unsigned i = 0; i < cachedCount; ++i)
    if (cachedImages[i].index == index) return data + cachedImages[i].offset;
  if (cachedCount == sizeof(cachedImages) / sizeof(cachedImages[0]) || image.bytes > pixelCapacity - pixelUsed) {
    pixelUsed = 0;
    cachedCount = 0;
  }
  if (!file.seek(image.offset)) {
    fail("Cannot seek cached resource pixels");
    return nullptr;
  }
  auto* const result = data + pixelUsed;
  if (!read(result, image.bytes)) return nullptr;
  cachedImages[cachedCount++] = {index, uint32_t(pixelUsed)};
  pixelUsed += image.bytes;
  return result;
}
void Pack::draw(freeink::ui::DrawTarget& target, freeink::ui::Rect area, uint32_t index, freeink::ui::BitmapMode mode,
                freeink::ui::Color color) {
  namespace fui = freeink::ui;
  if (!valid || area.empty()) return;
  const auto entry = image(index);
  if (!valid || !verify(index, entry)) return;
  const auto* const retained = pixels(index, entry);
  if (!valid) return;
  if (!retained && !file.seek(entry.offset)) {
    fail("Cannot seek resource pixels");
    return;
  }
  auto rect = area;
  if (mode == fui::BitmapMode::Contain) {
    const int width = std::min(int(area.width), int(area.height) * entry.width / entry.height);
    const int height = width * entry.height / entry.width;
    rect = fui::makeRect(area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height);
  }
  size_t cursor = 0, available = 0;
  auto* const io = readBuffer();
  uint32_t remaining = entry.bytes;
  const int stride = (entry.width + 7) / 8;
  for (int y = 0; y < entry.height; ++y) {
    for (int b = 0; !retained && b < stride; ++b) {
      if (cursor == available) {
        available = std::min<size_t>(remaining, readBufferBytes());
        if (!read(io, available)) return;
        remaining -= available;
        cursor = 0;
      }
      row[b] = io[cursor++];
    }
    const int top = rect.y + y * rect.height / entry.height;
    const int bottom = rect.y + (y + 1) * rect.height / entry.height;
    if (bottom <= top) continue;
    const auto* const source = retained ? retained + y * stride : row;
    if (color == fui::Color::Black || color == fui::Color::White) {
      for (int x = 0; x < entry.width;) {
        if (!(source[x / 8] & (0x80u >> (x % 8)))) {
          ++x;
          continue;
        }
        const int start = x++;
        while (x < entry.width && (source[x / 8] & (0x80u >> (x % 8)))) ++x;
        // Ceil boundaries preserve bitmap Stretch's floor-based source sampling.
        const int left = (start * rect.width + entry.width - 1) / entry.width;
        const int right = (x * rect.width + entry.width - 1) / entry.width;
        if (right > left)
          target.fill(fui::makeRect(rect.x + left, top, right - left, bottom - top), fui::Paint::solid(color));
      }
    } else {
      target.bitmap(fui::makeRect(rect.x, top, rect.width, bottom - top),
                    {source, entry.width, 1, fui::BitmapFormat::BW1}, fui::BitmapMode::Stretch,
                    fui::Paint::solid(color));
    }
  }
}
}  // namespace sdart

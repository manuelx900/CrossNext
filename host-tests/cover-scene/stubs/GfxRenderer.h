#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
class GfxRenderer {
 public:
  std::vector<uint8_t> frame = std::vector<uint8_t>(48000);
  size_t getRegionByteSize(int, int, int w, int h) const { return size_t((w + 7) / 8) * h; }
  bool copyBufferToRegion(int x, int y, int w, int h, const uint8_t* data, size_t bytes) {
    return transfer(x, y, w, h, const_cast<uint8_t*>(data), bytes, false);
  }
  bool copyRegionToBuffer(int x, int y, int w, int h, uint8_t* data, size_t bytes) {
    return transfer(x, y, w, h, data, bytes, true);
  }

 private:
  bool transfer(int x, int y, int w, int h, uint8_t* data, size_t bytes, bool save) {
    const auto size = getRegionByteSize(x, y, w, h);
    if (bytes < size || size > frame.size()) return false;
    std::memcpy(save ? data : frame.data(), save ? frame.data() : data, size);
    return true;
  }
};

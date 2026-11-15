#pragma once

#include <strings.h>

#include <cstdint>
#include <cstring>

namespace bookprep {
enum Format : uint8_t {
  FormatEpub = 1,
  FormatXtc = 2,
  FormatXtch = 4,
  AllFormats = FormatEpub | FormatXtc | FormatXtch
};
inline bool acceptsFormat(const char* path, uint8_t formats) {
  const char* dot = std::strrchr(path, '.');
  if (!dot) return false;
  return (!strcasecmp(dot, ".epub") && (formats & FormatEpub)) || (!strcasecmp(dot, ".xtc") && (formats & FormatXtc)) ||
         (!strcasecmp(dot, ".xtch") && (formats & FormatXtch));
}
enum class SizeMode : uint8_t { Single, All };

// A bounded snapshot of available point sizes. Font files are not opened by the UI.
struct PointSizes {
  uint8_t bits[32]{};
  void add(uint8_t point) {
    if (point) bits[point / 8] |= uint8_t(1U << (point % 8));
  }
  bool contains(uint8_t point) const { return point && (bits[point / 8] & (1U << (point % 8))); }
  int next(int previous) const {
    for (int point = previous + 1; point <= 255; ++point)
      if (contains(uint8_t(point))) return point;
    return -1;
  }
  unsigned count() const {
    unsigned result = 0;
    for (int point = next(0); point > 0; point = next(point)) ++result;
    return result;
  }
  uint8_t nearest(uint8_t requested) const {
    int best = next(0), distance = 256;
    for (int point = best; point > 0; point = next(point)) {
      const int delta = point > requested ? point - requested : requested - point;
      if (delta < distance) {
        distance = delta;
        best = point;
      }
    }
    return best > 0 ? uint8_t(best) : 12;
  }
  void include(const PointSizes& other) {
    for (unsigned n = 0; n < sizeof(bits); ++n) bits[n] |= other.bits[n];
  }
};
}  // namespace bookprep

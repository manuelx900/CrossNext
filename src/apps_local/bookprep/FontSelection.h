#pragma once

#include <cstring>

#include "PreparationChoices.h"

namespace bookprep {
// Same bounds as installed families plus the resident reader fallback. Names
// must fit the existing saved-reader font setting; the activity owns this array.
class FontSelection {
 public:
  static constexpr unsigned kCapacity = 129, kNameBytes = 32;
  struct Choice {
    char family[kNameBytes]{};
    bool selected = false;
    PointSizes sizes;
  };
  Choice fonts[kCapacity]{};
  unsigned count = 0, omitted = 0;
  int add(const char* family) {
    if (!family || std::strlen(family) >= kNameBytes || count == kCapacity) {
      ++omitted;
      return -1;
    }
    for (unsigned n = 0; n < count; ++n)
      if (!std::strcmp(fonts[n].family, family)) return int(n);
    std::strcpy(fonts[count].family, family);
    fonts[count].sizes.add(12);
    return int(count++);
  }
  unsigned selectedCount() const {
    unsigned selected = 0;
    for (unsigned n = 0; n < count; ++n) selected += fonts[n].selected;
    return selected;
  }
  void selectAll(bool selected) {
    for (unsigned n = 0; n < count; ++n) fonts[n].selected = selected;
  }
  void selectOnly(int index) {
    if (index < 0 || unsigned(index) >= count) return;
    selectAll(false);
    fonts[index].selected = true;
  }
  int nextSelected(int previous) const {
    for (unsigned n = unsigned(previous + 1); n < count; ++n)
      if (fonts[n].selected) return int(n);
    return -1;
  }
  PointSizes availableSizes() const {
    PointSizes result;
    for (unsigned n = 0; n < count; ++n)
      if (fonts[n].selected) result.include(fonts[n].sizes);
    return result;
  }
  unsigned passes(SizeMode mode) const {
    unsigned result = 0;
    for (unsigned n = 0; n < count; ++n)
      if (fonts[n].selected) result += mode == SizeMode::All ? fonts[n].sizes.count() : 1;
    return result;
  }
  struct Cursor {
    int font = -1;
    uint8_t point = 0;
  };
  bool advance(Cursor& cursor, SizeMode mode, uint8_t requested) const {
    if (mode == SizeMode::All && cursor.font >= 0) {
      const int next = fonts[cursor.font].sizes.next(cursor.point);
      if (next > 0) {
        cursor.point = uint8_t(next);
        return true;
      }
    }
    const int next = nextSelected(cursor.font);
    if (next < 0) return false;
    cursor.font = next;
    cursor.point = mode == SizeMode::All ? uint8_t(fonts[next].sizes.next(0)) : fonts[next].sizes.nearest(requested);
    return cursor.point > 0;
  }
};
static_assert(sizeof(FontSelection) < 8704);
}  // namespace bookprep

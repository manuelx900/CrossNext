#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace readerbars {
enum class Position : uint8_t { Top, Bottom };
enum class Item : uint8_t { Empty, Clock, Battery, TimeBook, TimeChapter, Pages, Percent, Book, Chapter, Count };
enum class Progress : uint8_t { Book, Chapter, Hidden };
constexpr unsigned SLOT_COUNT = 7;
constexpr uint32_t UNKNOWN_TIME = UINT32_MAX;

struct Config {
  std::array<Item, SLOT_COUNT> slots{};
  Progress progress = Progress::Hidden;
  uint8_t thickness = 1;
  bool visible = true;
  bool contains(Item item) const { return visible && std::find(slots.begin(), slots.end(), item) != slots.end(); }
  bool hasText() const {
    return visible && std::any_of(slots.begin(), slots.end(), [](Item item) { return item != Item::Empty; });
  }
  bool needsPace() const { return contains(Item::TimeBook) || contains(Item::TimeChapter); }
  int progressHeight() const { return visible && progress != Progress::Hidden ? (thickness + 1) * 2 : 0; }
};
struct Settings {
  Config top, bottom;
  bool customized = false;
  bool needsPace() const { return customized && (top.needsPace() || bottom.needsPace()); }
};

inline int height(const Config& config, int textHeight, int progressGap) {
  const int bar = config.progressHeight();
  return (config.hasText() ? textHeight + 4 : 0) + (bar ? bar + progressGap : 0);
}

// Side clusters share the available width; the centre gets only the remainder.
inline void fitWidths(std::array<int, SLOT_COUNT>& widths, int available, int gap) {
  int count = 0;
  for (unsigned i = 0; i < SLOT_COUNT; ++i)
    if (i != 3 && widths[i] > 0) ++count;
  const int budget = std::max(0, available - std::max(0, count - 1) * gap);
  int low = 0, high = budget;
  while (low < high) {
    const int cap = low + (high - low + 1) / 2;
    int used = 0;
    for (unsigned i = 0; i < SLOT_COUNT; ++i)
      if (i != 3) used += std::min(widths[i], cap);
    if (used <= budget)
      low = cap;
    else
      high = cap - 1;
  }
  for (unsigned i = 0; i < SLOT_COUNT; ++i)
    if (i != 3) widths[i] = std::min(widths[i], low);
}
}  // namespace readerbars

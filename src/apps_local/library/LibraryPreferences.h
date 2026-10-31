#pragma once
#include <cstdint>
#include <string_view>
namespace libraryui {
enum class Layout : uint8_t { Classic = 0, SortBar = 1 };
constexpr Layout layoutFromStored(uint8_t value) { return value == 1 ? Layout::SortBar : Layout::Classic; }
constexpr int sortCount(uint8_t layout) { return layoutFromStored(layout) == Layout::Classic ? 3 : 7; }
constexpr int sortForLayout(uint8_t layout, int sort) { return sort < 0 || sort >= sortCount(layout) ? 0 : sort; }
constexpr int nextSort(uint8_t layout, int current, int direction) {
  const int count = sortCount(layout);
  return (sortForLayout(layout, current) + (direction > 0 ? 1 : count - 1)) % count;
}
enum class Preference : uint8_t {
  Metadata,
  ListView,
  RecentView,
  Finished,
  Series,
  Genre,
  Epub,
  Xtc,
  Text,
  Markdown,
  Count
};
inline constexpr uint8_t kFinished = 1, kSeries = 2, kGenre = 4, kAllFormats = 15;
struct Preferences {
  bool metadata = true, expanded = true, grid = false;
  uint8_t display = 7, formats = kAllFormats;
  bool operator==(const Preferences&) const = default;
};
inline uint8_t formatBit(std::string_view name) {
  const auto dot = name.find_last_of('.');
  if (dot == std::string_view::npos) return 0;
  name.remove_prefix(dot + 1);
  auto equal = [name](std::string_view suffix) {
    if (name.size() != suffix.size()) return false;
    for (size_t n = 0; n < name.size(); ++n) {
      const char value = name[n] >= 'A' && name[n] <= 'Z' ? name[n] + 'a' - 'A' : name[n];
      if (value != suffix[n]) return false;
    }
    return true;
  };
  if (equal("epub")) return 1;
  if (equal("xtc") || equal("xtch")) return 2;
  if (equal("txt")) return 4;
  if (equal("md") || equal("markdown")) return 8;
  return 0;
}
inline bool acceptsFormat(std::string_view name, uint8_t mask) { return (formatBit(name) & mask) != 0; }
inline constexpr bool resultMapRequired(Preferences p) { return p.formats != kAllFormats || !(p.display & kFinished); }
}  // namespace libraryui

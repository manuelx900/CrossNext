#pragma once

#include <cstdint>

namespace menufont {
enum class Style : uint8_t { Pixel = 0, Classic = 1, NotoSans = 2, NotoSerif = 3, Count = 4 };
inline constexpr Style kDefault = Style::NotoSans;
constexpr Style normalized(uint8_t value) { return value < uint8_t(Style::Count) ? Style(value) : kDefault; }
enum class Override : uint8_t { General = 0, Pixel = 1, Classic = 2, NotoSans = 3, NotoSerif = 4, Count = 5 };
enum class Section : uint8_t {
  General = 0,
  Home = 1,
  Files = 2,
  Library = 3,
  Transfer = 4,
  Statistics = 5,
  Games = 6,
  Apps = 7,
  Settings = 8,
  Reader = 9,
  Controls = 10,
  Count = 11
};
inline constexpr int kSectionCount = int(Section::Count);
constexpr Override normalizedOverride(uint8_t value) {
  return value < uint8_t(Override::Count) ? Override(value) : Override::General;
}
constexpr Style resolve(uint8_t general, uint8_t section) {
  switch (normalizedOverride(section)) {
    case Override::Pixel:
      return Style::Pixel;
    case Override::Classic:
      return Style::Classic;
    case Override::NotoSans:
      return Style::NotoSans;
    case Override::NotoSerif:
      return Style::NotoSerif;
    case Override::General:
      return normalized(general);
    case Override::Count:
      break;
  }
  return normalized(general);
}
// Stable keys, independent of shelf order and translated labels.
inline constexpr const char* kItemKeys[] = {
    "CHESS",         "CHECKERS",  "GO",       "MINESWEEPER", "PICROSS",   "WORDLE",       "2048",       "THE INTERCEPT",
    "BATTLESHIP",    "SOLITAIRE", "HEARTS",   "MURDLE",      "CARNIVORO", "CONNECT FOUR", "SUDOKU",     "OTHELLO",
    "READING QUEST", "PET",       "HABITS",   "TO DO LIST",  "NOTES",     "POMODORO",     "WALLPAPERS", "QUOTES",
    "FLASHCARDS",    "WORDS",     "CALENDAR", "CLOCK",       "COUNTER"};
inline constexpr int kItemCount = sizeof(kItemKeys) / sizeof(kItemKeys[0]);
inline constexpr uint8_t kNoItem = kItemCount;
constexpr bool sameKey(const char* a, const char* b) {
  if (!a || !b) return a == b;
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return *a == *b;
}
constexpr uint8_t itemFor(const char* title) {
  if (title)
    for (int n = 0; n < kItemCount; ++n)
      if (sameKey(title, kItemKeys[n])) return uint8_t(n);
  return kNoItem;
}
constexpr Style resolve(uint8_t general, uint8_t section, uint8_t item) {
  return resolve(uint8_t(resolve(general, section)), item);
}
struct Context {
  Section section = Section::General;
  uint8_t item = kNoItem;
  bool assigned = false;
  void set(Section value, uint8_t content = kNoItem) {
    section = value;
    item = content;
    assigned = true;
  }
  void inherit(const Context& parent) {
    if (!assigned) set(parent.section, parent.item);
  }
};
}  // namespace menufont

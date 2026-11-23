#pragma once
#include <FreeInkUI.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "WrapLine.h"
namespace pagedtext {
enum class Alignment : uint8_t { Left, Center, Right, Justified };
inline constexpr int kAlignmentCount = int(Alignment::Justified) + 1;
// Stable text callers must clear Work::pages before replacing the buffer contents.
enum class Source { VerifyContents, InvalidateOnChange };

struct Work {
  char line[384]{}, page[2048]{};
  const void* pageOwner = nullptr;
  const char* source = nullptr;
  uint64_t fingerprint = 0;
  int width = 0, height = 0, font = -1, lineHeight = 0, probeWidth = 0, pages = 0;
  bool ownsPages(const void* owner) {
    if (pageOwner == owner) return true;
    pageOwner = owner;
    pages = 0;
    return false;
  }
  template <typename Entry>
  Entry pageAt(unsigned index) const {
    Entry value{};
    if (index < sizeof(page) / sizeof(Entry)) std::memcpy(&value, page + index * sizeof(Entry), sizeof(value));
    return value;
  }
  template <typename Entry>
  void rememberPage(unsigned index, const Entry& value) {
    if (index < sizeof(page) / sizeof(Entry)) std::memcpy(page + index * sizeof(Entry), &value, sizeof(value));
  }
};
inline unsigned next(const char* p, unsigned at) {
  if (!p[at]) return at;
  ++at;
  while ((static_cast<unsigned char>(p[at]) & 0xc0) == 0x80) ++at;
  return at;
}

template <typename Visit>
void forEachWord(char* line, const Visit& visit) {
  char* word = line;
  while (*word) {
    while (*word == ' ') ++word;
    if (!*word) return;
    char* end = std::strchr(word, ' ');
    if (end) *end = '\0';
    visit(word);
    if (!end) return;
    *end = ' ';
    word = end + 1;
  }
}

inline bool drawJustified(freeink::ui::DrawTarget& target, freeink::ui::Rect box, char* line,
                          freeink::ui::TextStyle style) {
  // Bidirectional scripts need the renderer's whole-line reordering.
  if (std::strpbrk(line, "\xd6\xd7\xd8\xd9\xda\xdb")) return false;
  constexpr int kCachedWords = 32;
  uint16_t widths[kCachedWords];
  int words = 0, inkWidth = 0;
  forEachWord(line, [&](const char* word) {
    const int width = target.measureText(style.font, word, style).width;
    if (words < kCachedWords) widths[words] = width;
    ++words;
    inkWidth += width;
  });
  if (words < 2 || inkWidth >= box.width) return false;
  const int space = box.width - inkWidth;
  const int gaps = words - 1;
  int x = box.x, ordinal = 0;
  forEachWord(line, [&](const char* word) {
    target.text(freeink::ui::makeRect(x, box.y, box.right() - x, box.height), word, style);
    if (ordinal < gaps) {
      const int width = ordinal < kCachedWords ? widths[ordinal] : target.measureText(style.font, word, style).width;
      x += width + (ordinal + 1) * space / gaps - ordinal * space / gaps;
      ++ordinal;
    }
  });
  return true;
}

// A bounded, allocation-free word wrapper. Explicit lines also make the page
// boundary identical for preview, measurement and the final e-ink rendering.
inline int draw(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* text, freeink::ui::FontId font,
                int selected, Work& work, int* paintedBottom = nullptr, Alignment alignment = Alignment::Left,
                Source source = Source::VerifyContents) {
  namespace fui = freeink::ui;
  if (paintedBottom) *paintedBottom = box.y;
  if (!text || box.empty()) return 1;
  const int lineHeight = std::max(1, int(target.lineHeight(font)));
  const int rows = std::max(1, int(box.height) / lineHeight);
  fui::TextStyle style;
  style.font = font;
  style.maxLines = 1;
  auto paintStyle = style;
  if (alignment == Alignment::Center) paintStyle.align = fui::TextAlign::Center;
  if (alignment == Alignment::Right) paintStyle.align = fui::TextAlign::Right;
  uint64_t fingerprint = 0;
  int probe = 0;
  if (source == Source::VerifyContents) {
    fingerprint = 14695981039346656037ull;
    for (const auto* at = reinterpret_cast<const unsigned char*>(text); *at; ++at)
      fingerprint = (fingerprint ^ *at) * 1099511628211ull;
    probe = target.measureText(font, "Mm", style).width;
  }
  const bool hit = work.ownsPages(&work) && work.pages && work.source == text && work.fingerprint == fingerprint &&
                   work.width == box.width && work.height == box.height && work.font == font &&
                   work.lineHeight == lineHeight && work.probeWidth == probe;
  if (hit && selected < 0) return work.pages;
  if (hit) selected = std::min(selected, work.pages - 1);
  unsigned at = 0;
  int lineIndex = 0;
  constexpr int cachePages = sizeof(work.page) / sizeof(uint32_t);
  if (hit) {
    const int first = std::min(std::max(0, selected), cachePages - 1);
    at = work.pageAt<uint32_t>(first);
    lineIndex = first * rows;
  }
  while (text[at]) {
    if (!hit && lineIndex % rows == 0) work.rememberPage(lineIndex / rows, uint32_t(at));
    const unsigned start = at;
    const auto wrapped = wrapLine(target, text, at, box.width, style, work.line, sizeof(work.line));
    const unsigned end = wrapped.end, after = wrapped.after;
    const size_t length = end - start;
    if (lineIndex / rows == selected) {
      std::memcpy(work.line, text + start, length);
      work.line[length] = 0;
      const int y = box.y + (lineIndex % rows) * lineHeight;
      const auto lineBox = fui::makeRect(box.x, y, box.width, lineHeight);
      if (alignment != Alignment::Justified || !text[after] || text[end] == '\n' || text[after] == '\n' ||
          !drawJustified(target, lineBox, work.line, paintStyle))
        target.text(lineBox, work.line, paintStyle);
      if (paintedBottom) *paintedBottom = y + lineHeight;
    }
    ++lineIndex;
    at = after;
    if (hit && lineIndex / rows > selected) return work.pages;
  }
  if (!hit) {
    work.source = text;
    work.fingerprint = fingerprint;
    work.width = box.width;
    work.height = box.height;
    work.font = font;
    work.lineHeight = lineHeight;
    work.probeWidth = probe;
    work.pages = std::max(1, (lineIndex + rows - 1) / rows);
  }
  return work.pages;
}
}  // namespace pagedtext

#pragma once
#include <FreeInkUI.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "WrapLine.h"
namespace pagedtext {
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
// A bounded, allocation-free word wrapper. Explicit lines also make the page
// boundary identical for preview, measurement and the final e-ink rendering.
inline int draw(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* text, freeink::ui::FontId font,
                int selected, Work& work) {
  namespace fui = freeink::ui;
  if (!text || box.empty()) return 1;
  const int lineHeight = std::max(1, int(target.lineHeight(font)));
  const int rows = std::max(1, int(box.height) / lineHeight);
  fui::TextStyle style;
  style.font = font;
  style.maxLines = 1;
  uint64_t fingerprint = 14695981039346656037ull;
  for (const auto* at = reinterpret_cast<const unsigned char*>(text); *at; ++at)
    fingerprint = (fingerprint ^ *at) * 1099511628211ull;
  const int probe = target.measureText(font, "Mm", style).width;
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
      target.text(fui::makeRect(box.x, box.y + (lineIndex % rows) * lineHeight, box.width, lineHeight), work.line,
                  style);
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

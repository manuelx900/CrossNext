#pragma once
#include "../study/StudyText.h"
#include "../ui/PagedText.h"
#include "../ui/WrapLine.h"
namespace wordsui {
struct Paragraph {
  const char* text;
  freeink::ui::TextStyle style;
  const char* prefix = nullptr;
  bool separator = false;
  bool labelLine = false;
};
struct ParagraphPages {
  struct Start {
    uint32_t offset = 0;
    uint16_t part = 0;
  };
  static constexpr int kPages = sizeof(pagedtext::Work::page) / sizeof(Start);
  int pages = 0, width = 0, height = 0, gap = 0, used = 0;
  uint32_t metrics = 0;
  uint8_t wordBreaks = 0;
  void invalidate() { pages = 0; }
};
// Paginate styled source paragraphs without copying or truncating the entry.
inline int paragraphs(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const Paragraph* parts, int count,
                      int selected, pagedtext::Work& work, int gap = 12, int* usedHeight = nullptr,
                      ParagraphPages* cache = nullptr, uint8_t* wordBreaks = nullptr) {
  namespace fui = freeink::ui;
  const bool fitOnly = selected == -2;
  if (fitOnly) cache = nullptr;
  const auto overflow = [&](int page, uint8_t broken) {
    if (wordBreaks) *wordBreaks = broken;
    return fitOnly && page > 0;
  };
  uint32_t metrics = 0;
  for (int n = 0; n < count; ++n)
    metrics = metrics * 16777619u + target.lineHeight(parts[n].style.font) * 257u +
              target.measureText(parts[n].style.font, "Mm", parts[n].style).width;
  const bool hit = cache && work.ownsPages(cache) && cache->pages && cache->width == box.width &&
                   cache->height == box.height && cache->gap == gap && cache->metrics == metrics;
  if (hit && selected < 0) {
    if (usedHeight) *usedHeight = cache->used;
    if (wordBreaks) *wordBreaks = cache->wordBreaks;
    return cache->pages;
  }
  if (hit) selected = std::min(selected, cache->pages - 1);
  int page = 0, y = 0;
  uint8_t broken = 0;
  int firstPart = 0;
  unsigned firstOffset = 0;
  if (hit) {
    page = std::min(std::max(0, selected), ParagraphPages::kPages - 1);
    const auto start = work.pageAt<ParagraphPages::Start>(page);
    firstPart = start.part;
    firstOffset = start.offset;
  } else if (cache) {
    cache->width = box.width;
    cache->height = box.height;
    cache->gap = gap;
    cache->metrics = metrics;
    cache->pages = 0;
    work.rememberPage(0, ParagraphPages::Start{});
  }
  for (int n = firstPart; n < count; ++n) {
    const auto& part = parts[n];
    if (!part.text || !*part.text) continue;
    auto style = part.style;
    style.maxLines = 1;
    const int lineHeight = std::max(1, int(target.lineHeight(style.font)));
    if (y) {
      if (y + gap + lineHeight > box.height) {
        ++page;
        if (overflow(page, broken)) return 2;
        y = 0;
        if (cache && !hit) work.rememberPage(page, ParagraphPages::Start{0, uint16_t(n)});
      } else
        y += gap;
    }
    unsigned at = n == firstPart ? firstOffset : 0;
    bool first = at == 0;
    auto prefixStyle = style;
    prefixStyle.bold = true;
    prefixStyle.italic = false;
    const bool labelLine =
        part.labelLine || (part.prefix && target.measureText(style.font, part.prefix, prefixStyle).width + 8 +
                                                  target.measureText(style.font, part.text, style).width >
                                              box.width);
    if (first && part.prefix && labelLine) {
      if (y && y + 2 * lineHeight > box.height) {
        ++page;
        if (overflow(page, broken)) return 2;
        y = 0;
        if (cache && !hit) work.rememberPage(page, ParagraphPages::Start{0, uint16_t(n)});
      }
      if (hit && page > selected) return cache->pages;
      if (page == selected) {
        if (part.separator && y >= 10)
          target.line({box.x, int16_t(box.y + y - 10)}, {box.right(), int16_t(box.y + y - 10)}, 1,
                      fui::Paint::solid(fui::Color::Black));
        auto label = style;
        label.italic = false;
        label.bold = true;
        target.text(fui::makeRect(box.x, box.y + y, box.width, lineHeight), part.prefix, label);
      }
      y += lineHeight;
      first = false;
    }
    while (part.text[at]) {
      auto prefixStyle = style;
      prefixStyle.italic = false;
      prefixStyle.bold = true;
      const int prefix = first && part.prefix ? target.measureText(style.font, part.prefix, prefixStyle).width + 8 : 0;
      const int width = std::max(1, int(box.width) - prefix);
      const unsigned start = at;
      const auto wrapped = pagedtext::wrapLine(target, part.text, at, width, style, work.line, sizeof(work.line));
      const unsigned end = wrapped.end, after = wrapped.after;
      if (study::isWordContinuation(part.text + end)) broken |= style.font == 2 ? 1 : 2;
      if (y + lineHeight > box.height) {
        ++page;
        if (overflow(page, broken)) return 2;
        y = 0;
        if (cache && !hit) work.rememberPage(page, ParagraphPages::Start{start, uint16_t(n)});
      }
      if (hit && page > selected) return cache->pages;
      if (page == selected) {
        if (first && part.separator && y >= 10)
          target.line({box.x, int16_t(box.y + y - 10)}, {box.right(), int16_t(box.y + y - 10)}, 1,
                      fui::Paint::solid(fui::Color::Black));
        if (prefix) target.text(fui::makeRect(box.x, box.y + y, prefix - 8, lineHeight), part.prefix, prefixStyle);
        std::memcpy(work.line, part.text + start, end - start);
        work.line[end - start] = 0;
        target.text(fui::makeRect(box.x + prefix, box.y + y, width, lineHeight), work.line, style);
      }
      y += lineHeight;
      at = after;
      first = false;
    }
  }
  if (usedHeight) *usedHeight = y;
  if (wordBreaks) *wordBreaks = hit ? cache->wordBreaks : broken;
  if (cache && !hit) {
    cache->pages = page + 1;
    cache->used = y;
    cache->wordBreaks = broken;
  }
  return hit ? cache->pages : page + 1;
}
}  // namespace wordsui

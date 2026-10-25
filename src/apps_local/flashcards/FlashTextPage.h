#pragma once

#include <FreeInkUI.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "../study/StudyText.h"
#include "../ui/WrapLine.h"

namespace flashui {
// Page starts, not text copies. Pages beyond this bound remain accessible.
struct TextPageCache {
  static constexpr int kPages = 128;
  const char* source = nullptr;
  uint32_t starts[kPages]{};
  int width = 0, height = 0, lineHeight = 0, font = -1, pages = 0;
  bool wordBroken = false;
  void invalidate() { pages = 0; }
};

inline int cachedTextPage(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* text, int page,
                          char* line, size_t bytes, TextPageCache* cache = nullptr, int fontKey = -1) {
  namespace fui = freeink::ui;
  if (!text || box.empty() || bytes < 5) return 1;
  fui::TextStyle style;
  style.font = 4;
  style.maxLines = 1;
  const int height = std::max(1, int(target.lineHeight(4))), per = std::max(1, int(box.height) / height);
  const bool hit = cache && cache->pages && cache->source == text && cache->width == box.width &&
                   cache->height == box.height && cache->lineHeight == height && cache->font == fontKey;
  if (hit && page < 0) return cache->pages;
  if (hit) page = std::min(page, cache->pages - 1);
  int count = 0;
  const char* cursor = text;
  if (hit) {
    const int first = std::min(std::max(0, page), TextPageCache::kPages - 1);
    count = first * per;
    cursor += cache->starts[first];
  } else if (cache) {
    cache->source = text;
    cache->width = box.width;
    cache->height = box.height;
    cache->lineHeight = height;
    cache->font = fontKey;
    cache->pages = 0;
    cache->wordBroken = false;
  }
  while (*cursor) {
    if (count % per == 0 && cache && !hit && count / per < TextPageCache::kPages)
      cache->starts[count / per] = uint32_t(cursor - text);
    const char* start = cursor;
    const auto wrapped = pagedtext::wrapLine(target, text, unsigned(start - text), box.width, style, line, bytes);
    cursor = text + wrapped.end;
    if (cache && !hit && study::isWordContinuation(cursor)) cache->wordBroken = true;
    if (count / per == page)
      target.text(fui::makeRect(box.x, box.y + (count % per) * height, box.width, height), line, style);
    ++count;
    cursor = text + wrapped.after;
    if (page == -2 && count >= per && *cursor) return 2;
    if (hit && count / per > page) return cache->pages;
  }
  const int pages = std::max(1, (count + per - 1) / per);
  if (cache && !hit) cache->pages = pages;
  return hit ? cache->pages : pages;
}
}  // namespace flashui

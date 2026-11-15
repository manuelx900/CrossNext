#pragma once
#include <FreeInkUI.h>

#include <algorithm>

namespace homeui {
inline freeink::ui::Rect coverLoadingNoticeArea(freeink::ui::Rect safe, freeink::ui::Rect cover, bool framed) {
  if (!framed) return safe;
  cover.x = safe.x;
  cover.width = safe.width;
  cover.height = std::min<int>(cover.height, 96);
  return cover;
}
}  // namespace homeui

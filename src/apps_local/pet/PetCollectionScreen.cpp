#include "PetWidgets.h"

namespace petui {
using namespace widget;
void collectionWindow(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const int width = std::min(int(r.width), wide ? 720 : 448);
  const int height = std::min(int(r.height) - 12, wide ? 330 : 660);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  panel(s, box);
  const auto art = wide ? rect(box.x + 16, box.y + 22, 245, box.height - 98)
                        : rect(box.x + 28, box.y + 26, box.width - 56, box.height * 40 / 100);
  if (!m.collectionArt.draw || !m.collectionArt.draw(m.collectionArt.context, s.target(), art, 0, false))
    icon(s, art, extra_trophy_64);
  const auto banner = wide ? rect(art.right() + 36, box.y + 23, box.right() - art.right() - 72, 48)
                           : rect(box.x + 42, art.bottom() + 7, box.width - 84, 58);
  panel(s, banner);
  text(s, banner.inset({8, 8, 8, 8}), tr(STR_PET_COLLECTION_TITLE), 1, false, fui::TextAlign::Center, true);
  const auto count = rect(banner.x, banner.bottom() + (wide ? 13 : 23), banner.width, wide ? 38 : 48);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_COLLECTION_COUNT), pet::ownedCount(*m.profile),
                pet::CATALOG_COUNT);
  panel(s, count);
  text(s, count, w.label, wide ? 1 : 2, false, fui::TextAlign::Center);
  const auto copy =
      wide ? rect(banner.x - 12, count.bottom() + 14, banner.width + 24, box.bottom() - count.bottom() - 100)
           : rect(box.x + 24, count.bottom() + 22, box.width - 48, box.bottom() - count.bottom() - 110);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_COLLECTION_TEXT), pet::CATALOG_COUNT);
  prose(s, copy, w.label, wide ? 5 : 4, fui::TextAlign::Center);
  button(s, w,
         rect(box.x + UI_INSET, box.bottom() - UI_INSET - SHADOW_DEPTH - CONTROL_HEIGHT, box.width - UI_INSET * 2,
              CONTROL_HEIGHT),
         tr(STR_PET_CONTINUE), Back, 0, true);
}
}  // namespace petui

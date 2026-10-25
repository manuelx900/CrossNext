#include "PetWidgets.h"

namespace petui {
using namespace widget;
namespace {
void corners(toybox::Screen& s, fui::Rect r) {
  for (int right : {0, 1})
    for (int bottom : {0, 1}) {
      const int x = right ? r.right() - 20 : r.x + 10;
      const int y = bottom ? r.bottom() - 10 : r.y + 10;
      s.target().fill(rect(x, y, 10, 4), ink);
      s.target().fill(rect(right ? x + 6 : x, bottom ? y - 6 : y, 4, 10), ink);
    }
}
void ribbon(toybox::Screen& s, fui::Rect r) {
  for (int right : {0, 1}) {
    const int edge = right ? r.right() + 25 : r.x - 25;
    const int inner = right ? r.right() - 4 : r.x + 4;
    const int notch = right ? edge - 10 : edge + 10;
    const fui::Point points[] = {{int16_t(inner), int16_t(r.y + 12)},
                                 {int16_t(edge), int16_t(r.y + 12)},
                                 {int16_t(notch), int16_t(r.y + 36)},
                                 {int16_t(edge), int16_t(r.bottom() + 12)},
                                 {int16_t(inner), int16_t(r.bottom() + 12)}};
    for (unsigned i = 0; i < 5; ++i) s.target().line(points[i], points[(i + 1) % 5], BORDER, ink);
  }
  s.target().fill(rect(r.x + 3, r.y + 3, r.width, r.height), ink);
  s.target().fill(r, paper);
  frame(s, r);
  text(s, r.inset({4, 8, 4, 8}), tr(STR_PET_COLLECTION_TITLE), 1, false, fui::TextAlign::Center);
}
}  // namespace
void collectionWindow(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const int width = std::min(int(r.width), wide ? 720 : 448);
  const int height = std::min(int(r.height) - 12, wide ? 330 : 660);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  frame(s, box);
  corners(s, box);
  const auto art = wide ? rect(box.x + 16, box.y + 22, 245, box.height - 98)
                        : rect(box.x + 28, box.y + 26, box.width - 56, box.height * 40 / 100);
  if (!m.collectionArt.draw || !m.collectionArt.draw(m.collectionArt.context, s.target(), art, 0, false))
    icon(s, art, extra_trophy_64);
  const auto banner = wide ? rect(art.right() + 36, box.y + 23, box.right() - art.right() - 72, 48)
                           : rect(box.x + 42, art.bottom() + 7, box.width - 84, 58);
  ribbon(s, banner);
  const auto count = rect(banner.x, banner.bottom() + (wide ? 13 : 23), banner.width, wide ? 38 : 48);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_COLLECTION_COUNT), pet::ownedCount(*m.profile), pet::OBJECT_COUNT);
  text(s, count, w.label, wide ? 1 : 2, false, fui::TextAlign::Center);
  const auto copy =
      wide ? rect(banner.x - 12, count.bottom() + 14, banner.width + 24, box.bottom() - count.bottom() - 100)
           : rect(box.x + 24, count.bottom() + 22, box.width - 48, box.bottom() - count.bottom() - 110);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_COLLECTION_TEXT), pet::OBJECT_COUNT);
  prose(s, copy, w.label, wide ? 5 : 4, fui::TextAlign::Center);
  button(s, w, rect(box.x + 14, box.bottom() - 68, box.width - 28, 54), tr(STR_PET_CONTINUE), Back, 0, true);
}
}  // namespace petui

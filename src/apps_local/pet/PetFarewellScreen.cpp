#include "PetWidgets.h"

namespace petui {
using namespace widget;
void deathWindow(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const int width = std::min(int(r.width), wide ? 700 : 440);
  const int height = std::min(int(r.height) - 8, wide ? 320 : 650);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  frame(s, box);
  const auto banner = rect(box.x + BORDER, box.y + BORDER, box.width - BORDER * 2, 40);
  lightui::surface(s.target(), banner, ink);
  text(s, banner, tr(STR_PET_FAREWELL), 1, true, fui::TextAlign::Center);
  const auto content = rect(box.x + 18, banner.bottom() + 10, box.width - 36, box.bottom() - banner.bottom() - 90);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_DIED_TITLE), displayName(m.profile->name, m.profile->species));
  const int titleY = content.y;
  const int titleFont = s.target().measureText(1, w.label, {}).width <= content.width ? 1 : 0;
  text(s, rect(content.x, titleY, content.width, 44), w.label, titleFont, false, fui::TextAlign::Center);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GENERATION), static_cast<unsigned long>(m.profile->generation));
  text(s, rect(content.x, titleY + 44, content.width, 22), w.label, 3, false, fui::TextAlign::Center);
  if (wide) {
    const int imageW = 160;
    const auto picture = rect(content.x, content.y + 76, imageW, content.height - 76);
    farewellPicture(s, m, picture);
    const auto copy = rect(picture.right() + 16, picture.y, content.width - imageW - 16, picture.height);
    prose(s, rect(copy.x, copy.y, copy.width, 52), tr(STR_PET_DIED_REASON), 3);
    prose(s, rect(copy.x, copy.y + 54, copy.width, copy.height - 54), tr(STR_PET_DIED_KEPT), 4);
  } else {
    const int imageY = content.y + 77;
    farewellPicture(s, m, rect(content.x, imageY, content.width, 207));
    const int copyY = content.y + 294;
    prose(s, rect(content.x, copyY, content.width, 58), tr(STR_PET_DIED_REASON), 3, fui::TextAlign::Center);
    prose(s, rect(content.x, copyY + 68, content.width, content.bottom() - copyY - 68), tr(STR_PET_DIED_KEPT), 5,
          fui::TextAlign::Center);
  }
  const auto actions = rect(box.x + 14, box.bottom() - 66, box.width - 28, 52);
  button(s, w, cell(actions, 0, 2, 10), tr(STR_PET_CONTINUE), AcknowledgeDeath, 0, true);
  button(s, w, cell(actions, 1, 2, 10), tr(STR_PET_GENERATIONS_SHORT), AcknowledgeDeath, 1);
}
}  // namespace petui

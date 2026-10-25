#include "PetWidgets.h"

namespace petui {
using namespace widget;
fui::ActionEvent routeNaming(toybox::Interactions& interactions, const wordlekeys::Layout& keys,
                             const fui::InputSnapshot& input) {
  auto event = interactions.route(input);
  if (event.action != NameKey || event.value != 0) return event;
  if (input.touchReleased) {
    event.value = wordlekeys::keyAt(keys, input.touchX, input.touchY);
  } else if (input.confirm) {
    fui::Rect key;
    if (interactions.buttonFocusRect(key))
      event.value = wordlekeys::keyAt(keys, key.x + key.width / 2, key.y + key.height / 2);
  }
  return event;
}
void naming(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const int leftW = wide ? r.width * 45 / 100 : r.width;
  const auto left = rect(r.x, r.y, leftW, r.height);
  text(s, rect(left.x + 6, left.y, left.width - 12, 44), tr(STR_PET_NAME_TITLE), 2);
  prose(s, rect(left.x + 8, left.y + 48, left.width - 16, wide ? 54 : 31), tr(STR_PET_NAME_INTRO), wide ? 2 : 1);
  const auto controls =
      wide ? rect(left.right() + UI_GAP, r.y + 20, r.right() - left.right() - UI_GAP, r.height - 20) : r;
  const auto footer = rect(controls.x, r.bottom() - 54, controls.width, 54);
  const auto space = rect(controls.x, footer.y - UI_GAP - 42, controls.width, 42);
  const auto keys = rect(controls.x, space.y - 12 - 200, controls.width, 200);
  const auto field =
      wide ? rect(left.x, left.bottom() - 94, left.width, 94) : rect(left.x, keys.y - UI_GAP - 94, left.width, 94);
  const auto preview =
      rect(left.x, left.y + (wide ? 109 : 91), left.width, field.y - UI_GAP - left.y - (wide ? 109 : 91));
  frame(s, preview);
  const int side = std::min(112, int(preview.width) / 3);
  const auto image = rect(preview.right() - side - 12, preview.y + 10, side, preview.height - 20);
  const auto copy = rect(preview.x + UI_INSET, preview.y + 16, preview.width - side - 40, preview.height - 32);
  art(s, m, image, unsigned(m.choice), speciesIcon(m.choice), false);
  text(s, rect(copy.x, copy.y, copy.width, 27), eggName(m.choice), 0);
  prose(s, rect(copy.x, copy.y + 38, copy.width, copy.height - 38), tr(STR_PET_NAME_COMPANION), 3);
  frame(s, field);
  text(s, rect(field.x + UI_INSET, field.y + 9, field.width - 110, 22), tr(STR_PET_NAME_LABEL), 3);
  std::snprintf(w.label, sizeof(w.label), "%u / %u", pet::nameLength(m.nameDraft), pet::NAME_LIMIT);
  text(s, rect(field.right() - 100, field.y + 9, 84, 22), w.label, 3, false, fui::TextAlign::Right);
  text(s, rect(field.x + UI_INSET, field.y + 34, field.width - UI_INSET * 2, 48),
       m.nameDraft[0] ? m.nameDraft : tr(STR_PET_NAME_PLACEHOLDER), 1, false, fui::TextAlign::Center);
  w.keys = wordlekeys::keyboardLayout(keys, keys.y);
  wordlekeys::draw(s, keys, w.keys, NameKey);
  button(s, w, space, tr(STR_PET_NAME_SPACE), NameKey, ' ');
  button(s, w, footer, tr(STR_PET_NAME_CONFIRM), ConfirmName, 0, true, &extra_check_32,
         m.nameDraft[0] != 0 && m.writable && m.clockKnown && !m.historyChanged);
}
}  // namespace petui

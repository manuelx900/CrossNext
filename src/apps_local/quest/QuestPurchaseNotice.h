#pragma once
#include "QuestWidgets.h"

namespace questui {
inline void insufficientCoins(toybox::Screen& s, const Model& m, Work& w, freeink::ui::Rect area, int item) {
  using namespace widget;
  namespace fui = freeink::ui;
  const int width = std::min(440, int(area.width) - 16);
  const int height = std::min(336, int(area.height) - 16);
  const auto box = rect(area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height);
  panel(s, box);
  questicons::coin(s.target(), rect(box.x + 18, box.y + 22, 36, 36));
  text(s, rect(box.x + 68, box.y + 12, box.width - 86, 68), tr(STR_QUEST_NOT_ENOUGH_COINS), 0, true,
       fui::TextAlign::Left, false, 2);
  const int inner = box.width - 36, half = (inner - 12) / 2;
  const uint64_t available = quest::coins(*m.profile), needed = quest::price(item);
  for (int n = 0; n < 2; ++n) {
    const auto cell = rect(box.x + 18 + n * (half + 12), box.y + 86, n ? inner - half - 12 : half, 124);
    const bool inverse = n == 1;
    s.target().fill(cell, inverse ? ink : paper, RADIUS);
    s.target().stroke(cell, ink, 2, RADIUS);
    text(s, rect(cell.x + 8, cell.y + 8, cell.width - 16, 38),
         n ? tr(STR_QUEST_COLLECTION_PRICE) : tr(STR_QUEST_COLLECTION_YOUR_COINS), 4, true, fui::TextAlign::Center,
         inverse, 2);
    std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(n ? needed : available));
    text(s, rect(cell.x + 8, cell.y + 48, cell.width - 16, 68), w.label, COIN_VALUE_FONT, false, fui::TextAlign::Center,
         inverse);
  }
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COINS_SHORT),
                static_cast<unsigned long long>(needed - available));
  text(s, rect(box.x + 18, box.bottom() - 112, inner, 40), w.label, 0, true, fui::TextAlign::Center);
  button(s, w, rect(box.x + 18, box.bottom() - 62, inner, 48), tr(STR_OK_BUTTON), Back, 0, true);
  s.frame().limitButtonActions({Back});
}
}  // namespace questui

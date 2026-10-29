#pragma once
#include "../cards/CardArt.h"
#include "../cards/CardsLabels.h"
#include "GameGuide.h"
#include "ReferenceArt.h"

namespace cardpanel {
namespace fui = freeink::ui;
inline fui::Rect begin(toybox::Screen& s, const char* title, int actionReserve = 0) {
  const auto safe = s.device().safeRect();
  const int h = safe.y + 56;
  chrome::headerHeight = h;
  s.target().fill(fui::makeRect(0, 0, s.device().width, h), dashboard::ink);
  const auto back = fui::makeRect(safe.x + 6, safe.y + 6, 44, 44);
  backui::paint(s.target(), back);
  backui::publish(s.device().hasTouch, back);
  const auto battery = fui::makeRect(safe.right() - 39, safe.y + 20, 28, 17);
  s.target().stroke(battery, dashboard::paper, 2);
  s.target().fill(fui::makeRect(battery.right(), battery.y + 4, 4, 9), dashboard::paper);
  const auto status = s.headerStatus();
  int right = battery.x - 10;
  if (status.percent >= 0) {
    s.target().fill(fui::makeRect(battery.x + 4, battery.y + 4, 20 * std::clamp(status.percent, 0, 100) / 100, 9),
                    dashboard::paper);
    if (status.showPercent) {
      char label[8];
      std::snprintf(label, sizeof(label), "%d%%", status.percent);
      dashboard::text(s, fui::makeRect(battery.x - 60, safe.y, 54, 56), label, 0, fui::TextAlign::Right, true);
      right = battery.x - 70;
    }
  }
  if (actionReserve > 0) right = std::min(right, safe.right() - actionReserve - 12);
  const int left = back.right() + 6;
  fui::TextStyle titleStyle;
  titleStyle.font = 1;
  titleStyle.bold = true;
  char titleLabel[96];
  pixelui::copyCaps(titleLabel, sizeof(titleLabel), title);
  const int titleEnd = left + s.target().measureText(1, titleLabel, titleStyle).width + 14;
  fui::TextStyle clockStyle;
  clockStyle.font = fui::HEADER_CLOCK_FONT;
  clockStyle.color = fui::Color::White;
  const auto clock = fui::headerClockRect(
      s.target(), fui::makeRect(titleEnd, safe.y, std::max(0, right - titleEnd), 56), status.clock, clockStyle);
  if (!clock.empty()) {
    s.target().text(clock, status.clock, clockStyle);
    right = clock.x - 14;
  }
  // Compact boards give their fixed action buttons precedence over the title.
  if (!actionReserve || s.target().measureText(3, titleLabel, {}).width <= right - left)
    dashboard::text(s, fui::makeRect(left, safe.y, std::max(0, right - left), 56), title, 1, fui::TextAlign::Left,
                    true);
  const auto body = fui::makeRect(safe.x + 12, h + 12, safe.width - 24, safe.bottom() - h - 24);
  s.setContentMarginAbsolute(
      {body.y, int16_t(s.device().width - body.right()), int16_t(s.device().height - body.bottom()), body.x});
  return body;
}
inline void title(toybox::Screen& s, fui::Rect r, const char* label) {
  dashboard::text(s, r, label, 2, fui::TextAlign::Left);
}
inline void choice(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, int value, bool selected) {
  dashboard::button(s, r, label, action, value, selected, nullptr, 1);
}
inline void setting(toybox::Screen& s, fui::Rect r, const char* label, const char* const* values, int count,
                    int selected, fui::ActionId action, int first) {
  dashboard::frame(s, r);
  const int split = r.x + r.width * (count == 3 ? 45 : 59) / 100;
  dashboard::text(s, fui::makeRect(r.x + 14, r.y, split - r.x - 24, r.height), label, 1, fui::TextAlign::Left);
  const int gap = 8, width = (r.right() - split - 12 - gap * (count - 1)) / count;
  for (int n = 0; n < count; ++n)
    choice(s, fui::makeRect(split + n * (width + gap), r.y + 8, width, r.height - 16), values[n], action, first + n,
           n == selected);
}
inline void stats(toybox::Screen& s, fui::Rect r, const char* const* labels, const int* values) {
  dashboard::frame(s, r);
  for (int n = 0; n < 3; ++n) {
    auto cell = fui::makeRect(r.x + n * r.width / 3, r.y, r.width / 3, r.height);
    if (n) s.target().line({cell.x, int16_t(cell.y + 10)}, {cell.x, int16_t(cell.bottom() - 10)}, 2, dashboard::ink);
    dashboard::text(s, fui::makeRect(cell.x + 4, cell.y + 6, cell.width - 8, 24), labels[n], 0);
    char value[12];
    if (values[n] < 0)
      std::snprintf(value, sizeof(value), "--");
    else
      std::snprintf(value, sizeof(value), "%d", values[n]);
    dashboard::digits(s, fui::makeRect(cell.x + 8, cell.y + 36, cell.width - 16, cell.height - 48), value);
  }
}
inline void crown(toybox::Screen& s, fui::Rect r) {
  const int x = r.x, y = r.y, w = r.width, h = r.height;
  s.target().fill(fui::makeRect(x + 2, y + h / 2, w - 4, h / 3), dashboard::ink);
  for (int n = 0; n < 3; ++n)
    s.target().triangle({int16_t(x + 2 + n * w / 3), int16_t(y + 2)}, {int16_t(x + n * w / 3), int16_t(y + h * 2 / 3)},
                        {int16_t(x + (n + 1) * w / 3), int16_t(y + h * 2 / 3)}, dashboard::ink);
}
inline void lesson(toybox::Screen& s, fui::Rect r, const char* heading, const fui::BitmapRef& art,
                   const char* caption) {
  dashboard::frame(s, r);
  const bool compact = r.height < 180;
  const int titleH = compact ? 28 : 34, captionH = compact ? 56 : 64;
  dashboard::text(s, fui::makeRect(r.x + 8, r.y + 4, r.width - 16, titleH), heading, 1);
  referenceart::draw(s, fui::makeRect(r.x + 16, r.y + titleH + 10, r.width - 32, r.height - titleH - captionH - 20),
                     art);
  guideui::text(s, fui::makeRect(r.x + 10, r.bottom() - captionH - 4, r.width - 20, captionH), caption, 2,
                fui::TextAlign::Center, 4);
}
inline void deckLesson(toybox::Screen& s, fui::Rect r, bool aceHigh) {
  const bool wide = r.width > r.height;
  guideui::text(s, fui::makeRect(r.x, r.y, r.width, 30), tr(STR_CARDS_GUIDE_DECK), 1);
  const int leftW = wide ? r.width * 2 / 5 : r.width;
  const auto suits = fui::makeRect(r.x, r.y + 40, leftW, wide ? 148 : 164);
  const int columns = 2, rows = 2;
  static constexpr cards::Suit order[] = {cards::Suit::Clubs, cards::Suit::Spades, cards::Suit::Hearts,
                                          cards::Suit::Diamonds};
  for (int n = 0; n < 4; ++n) {
    const int w = (suits.width - 8 * (columns - 1)) / columns, h = (suits.height - 8 * (rows - 1)) / rows;
    const auto box = fui::makeRect(suits.x + (n % columns) * (w + 8), suits.y + (n / columns) * (h + 8), w, h);
    dashboard::frame(s, box, 2);
    const int iconSize = std::min(44, h - 32);
    const auto mark = fui::makeRect(box.x + (w - iconSize) / 2, box.y + 4, iconSize, iconSize);
    cardart::drawSuit(s, mark, order[n], cards::isRed(cards::makeCard(order[n], 0)));
    pixelui::text(s.target(), fui::makeRect(box.x + 4, box.bottom() - 28, w - 8, 24), cardsui::suitName(order[n]), 0,
                  fui::Color::Black, fui::TextAlign::Center);
  }
  guideui::text(s, fui::makeRect(r.x, suits.bottom() + 10, leftW, 64), tr(STR_CARDS_GUIDE_COLOURS), 2);
  const int faceX = wide ? r.x + leftW + 18 : r.x, faceY = wide ? r.y + 40 : suits.bottom() + 82;
  const int faceW = wide ? r.width - leftW - 18 : r.width;
  static constexpr int ranks[] = {cards::kAce, cards::kJack, cards::kQueen, cards::kKing};
  static constexpr StrId labels[] = {StrId::STR_CARDS_GUIDE_ACE, StrId::STR_CARDS_GUIDE_JACK,
                                     StrId::STR_CARDS_GUIDE_QUEEN, StrId::STR_CARDS_GUIDE_KING};
  const int cw = (faceW - 24) / 4, ch = wide ? 84 : 118;
  for (int n = 0; n < 4; ++n) {
    const auto box = fui::makeRect(faceX + n * (cw + 8), faceY, cw, ch);
    cardart::drawCardFace(s, box, cards::faceUp(cards::makeCard(order[n], ranks[n])), ch);
    pixelui::text(s.target(), fui::makeRect(box.x, box.bottom() + 4, cw, 28), I18N.get(labels[n]), 0, fui::Color::Black,
                  fui::TextAlign::Center);
  }
  const int orderY = faceY + ch + (wide ? 34 : 42);
  pixelui::text(s.target(), fui::makeRect(faceX, orderY, faceW, 28), tr(STR_CARDS_GUIDE_ORDER), 0);
  const int key = faceW / 13;
  for (int n = 0; n < 13; ++n) {
    const int rank = aceHigh ? (n + 1) % 13 : n;
    const auto box = fui::makeRect(faceX + n * key, orderY + (wide ? 28 : 34), key, wide ? 32 : 38);
    dashboard::frame(s, box, 1);
    pixelui::text(s.target(), box, cards::rankLabel(rank), 0, fui::Color::Black, fui::TextAlign::Center);
  }
  const int captionH = std::max(wide ? 30 : 52, int(s.target().lineHeight(4)));
  const int captionY = std::min(orderY + (wide ? 68 : 80), r.bottom() - captionH);
  guideui::text(s, fui::makeRect(faceX, captionY, faceW, captionH),
                aceHigh ? tr(STR_CARDS_GUIDE_ACE_HIGH) : tr(STR_CARDS_GUIDE_ACE_LOW), 2);
}
}  // namespace cardpanel

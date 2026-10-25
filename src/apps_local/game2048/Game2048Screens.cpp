#include "Game2048Screens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/DPad.h"
#include "../ui/GameGuide.h"
#include "../ui/GamePanel.h"
namespace game2048ui {
namespace fui = freeink::ui;
namespace {
void board(toybox::Screen& s, fui::Rect r, const game2048::State& state) {
  const int cell = r.width / 4;
  for (int n = 0; n < 16; ++n) {
    const auto square = fui::makeRect(r.x + n % 4 * cell, r.y + n / 4 * cell, cell, cell);
    dashboard::frame(s, square, 2);
    const int exponent = state.cells[n];
    if (!exponent) continue;
    const auto tile = square.inset(fui::Insets{5, 5, 5, 5});
    const bool dark = exponent >= 4;
    if (dark)
      s.target().fill(tile, dashboard::ink);
    else
      dashboard::frame(s, tile, exponent >= 3 ? 4 : 2);
    char value[24];
    std::snprintf(value, sizeof(value), "%llu", static_cast<unsigned long long>(uint64_t(1) << exponent));
    const int h = std::max(12, cell * 44 / 100);
    dashboard::digits(s, fui::makeRect(tile.x + 5, tile.y + (tile.height - h) / 2, tile.width - 10, h), value, dark);
  }
  dashboard::frame(s, r, 4);
}
void score(toybox::Screen& s, fui::Rect r, const char* label, uint64_t value) {
  dashboard::frame(s, r, 3);
  dashboard::text(s, fui::makeRect(r.x + 5, r.y + 4, r.width - 10, 24), label, 0);
  char number[24];
  std::snprintf(number, sizeof(number), "%llu", static_cast<unsigned long long>(value));
  dashboard::digits(s, fui::makeRect(r.x + 12, r.y + 32, r.width - 24, r.height - 40), number);
}
}  // namespace
fui::Rect boardRect(const fui::DeviceContext& device) {
  const auto body = pixelui::geometry(device).body;
  const bool wide = body.width > body.height;
  const int size =
      std::max(4, std::min(wide ? body.width - 218 : body.width - 8, body.height - (wide ? 8 : 270))) / 4 * 4;
  return fui::makeRect(body.x + (wide ? 0 : (body.width - size) / 2), body.y + (wide ? 4 : 90), size, size);
}
void build(toybox::Screen& s, const Model& m, Work&) {
  pixelui::begin(s);
  pixelui::Header h;
  h.title = tr(STR_GAME_2048);
  h.back = s.device().hasTouch;
  pixelui::header(s, h);
  backui::publish(h.back, pixelui::geometry(s.device()).back);
  const auto body = s.body();
  const auto& state = *m.state;
  const bool wide = body.width > body.height;
  if (m.view == View::Help) {
    dashboard::text(s, s.takeTop(wide ? 34 : 48, 8), tr(STR_2048_HELP), pixelui::kBody, fui::TextAlign::Left);
    dashboard::button(s, s.takeBottom(wide ? 48 : 54, 8), tr(STR_UI_GOT_IT_C3A894), CancelNew, 0, true, &extra_check_32,
                      pixelui::kBody);
    dashboard::text(s, s.takeBottom(28, 8), tr(STR_GUIDE_BACK_MENU), 3, fui::TextAlign::Left);
    const auto area = s.body();
    const int gap = 10;
    const int w = wide ? (area.width - 2 * gap) / 3 : area.width;
    const int h = wide ? area.height : (area.height - 2 * gap) / 3;
    static constexpr StrId titles[] = {StrId::STR_GUIDE_2048_SLIDE, StrId::STR_GUIDE_2048_MERGE,
                                       StrId::STR_GUIDE_2048_GOAL};
    static constexpr StrId copy[] = {StrId::STR_GUIDE_2048_SLIDE_BODY, StrId::STR_GUIDE_2048_MERGE_BODY,
                                     StrId::STR_GUIDE_2048_GOAL_BODY};
    for (int n = 0; n < 3; ++n) {
      const auto card = fui::makeRect(area.x + (wide ? n * (w + gap) : 0), area.y + (wide ? 0 : n * (h + gap)), w, h);
      dashboard::frame(s, card, 3);
      guideui::badge(s, fui::makeRect(card.x + 10, card.y + 10, 34, 34), n + 1);
      dashboard::text(s, fui::makeRect(card.x + 54, card.y + 8, card.width - 62, 38), I18N.get(titles[n]), 0,
                      fui::TextAlign::Left);
      const auto art = wide ? fui::makeRect(card.x + 12, card.y + 52, card.width - 24, wide ? 58 : 90)
                            : fui::makeRect(card.x + 14, card.y + 54, 106, card.height - 64);
      if (n == 0) guideui::fourArrows(s, art);
      if (n == 2) {
        const int side = std::min(88, std::min(int(art.width), int(art.height)));
        guideui::tile(s, fui::makeRect(art.x + (art.width - side) / 2, art.y + (art.height - side) / 2, side, side),
                      "2048");
      }
      if (n == 1) {
        const int side = std::min(48, std::max(32, (int(card.width) - 100) / 5));
        const int x = card.x + (card.width - (3 * side + 80)) / 2, y = card.y + 44;
        guideui::tile(s, fui::makeRect(x, y, side, side), "2");
        dashboard::text(s, fui::makeRect(x + side, y, 32, side), "+", 1);
        guideui::tile(s, fui::makeRect(x + side + 32, y, side, side), "2");
        guideui::arrow(s, fui::makeRect(x + 2 * side + 32, y, 48, side));
        guideui::tile(s, fui::makeRect(x + 2 * side + 80, y, side, side), "4");
        guideui::text(s, fui::makeRect(card.x + 14, y + side + 8, card.width - 28, card.bottom() - y - side - 14),
                      I18N.get(copy[n]), 3, fui::TextAlign::Left, 4);
      } else {
        const auto desc =
            wide ? fui::makeRect(card.x + 14, art.bottom() + 8, card.width - 28, card.bottom() - art.bottom() - 14)
                 : fui::makeRect(art.right() + 12, card.y + 60, card.right() - art.right() - 24, card.height - 70);
        guideui::text(s, desc, I18N.get(copy[n]), 4, fui::TextAlign::Left, 4);
      }
    }
    return;
  }
  if (m.view == View::Confirm) {
    const auto left = wide ? fui::makeRect(body.x, body.y, body.width / 3, body.height)
                           : fui::makeRect(body.x, body.y, body.width, 200);
    guideui::restartArt(s, fui::makeRect(left.x + (left.width - 120) / 2, left.y + 14, 120, 120));
    dashboard::text(s, fui::makeRect(left.x, left.y + 146, left.width, 48), tr(STR_2048_NEW), pixelui::kBody);
    const auto right = wide ? fui::makeRect(left.right() + 18, body.y, body.right() - left.right() - 18, body.height)
                            : fui::makeRect(body.x, body.y + 214, body.width, body.height - 214);
    const int buttons = wide ? 50 : 64;
    const auto card = fui::makeRect(right.x, right.y, right.width, right.height - 2 * buttons - 32);
    dashboard::frame(s, card, 4);
    dashboard::text(s, fui::makeRect(card.x + 10, card.y + 20, card.width - 20, 42), tr(STR_GUIDE_START_AGAIN),
                    pixelui::kBody);
    guideui::text(s, fui::makeRect(card.x + 24, card.y + 76, card.width - 48, card.height - 140),
                  tr(STR_GUIDE_RESTART_BODY), 3, fui::TextAlign::Center);
    s.target().fill(fui::makeRect(card.x + 24, card.bottom() - 62, card.width - 48, 2), dashboard::ink);
    dashboard::text(s, fui::makeRect(card.x + 12, card.bottom() - 54, card.width - 24, 42), tr(STR_GUIDE_KEEP_BEST), 0);
    gamepanel::row(s, fui::makeRect(right.x, right.bottom() - 2 * buttons - 12, right.width, buttons), tr(STR_2048_NEW),
                   ConfirmNew, 0, gamepanel::Mark::Restart, m.selected == 0, nullptr, false);
    gamepanel::row(s, fui::makeRect(right.x, right.bottom() - buttons, right.width, buttons), tr(STR_CANCEL), CancelNew,
                   0, gamepanel::Mark::Back, m.selected == 1, nullptr, false);
    return;
  }
  if (m.view == View::Menu) {
    const auto left = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 10, body.height) : body;
    const auto banner = fui::makeRect(left.x, left.y, left.width, 58);
    dashboard::frame(s, banner, 4);
    gamepanel::mark(s, fui::makeRect(banner.x + 14, banner.y + 12, 34, 34), gamepanel::Mark::Pause);
    s.target().fill(fui::makeRect(banner.x + 64, banner.y + 9, 2, banner.height - 18), dashboard::ink);
    dashboard::text(s, fui::makeRect(banner.x + 78, banner.y, banner.width - 86, banner.height),
                    game2048::canMove(state) ? tr(STR_REDESIGN_PAUSED) : tr(STR_2048_OVER), 0);
    const auto scores = fui::makeRect(left.x, left.y + 66, left.width, 48);
    dashboard::frame(s, scores, 3);
    s.target().fill(fui::makeRect(scores.x + scores.width / 2, scores.y, 2, scores.height), dashboard::ink);
    char value[24];
    for (int n = 0; n < 2; ++n) {
      const int x = scores.x + n * scores.width / 2;
      dashboard::text(s, fui::makeRect(x + 6, scores.y, scores.width / 3, 48),
                      n ? tr(STR_2048_BEST) : tr(STR_2048_SCORE), 3, fui::TextAlign::Left);
      std::snprintf(value, sizeof(value), "%llu", static_cast<unsigned long long>(n ? m.best : state.score));
      dashboard::digits(s, fui::makeRect(x + scores.width / 3, scores.y + 10, scores.width / 6 - 8, 28), value);
    }
    const int menuH = wide ? 64 : 62, footer = 3 * menuH + 48;
    const int room = wide ? left.height - 128 : body.height - 140 - footer;
    const int size = std::max(4, std::min(int(left.width) - 24, room)) / 4 * 4;
    const auto mini = fui::makeRect(left.x + (left.width - size) / 2, left.y + 126, size, size);
    board(s, mini, state);
    const auto right = wide ? fui::makeRect(left.right() + 20, body.y, body.right() - left.right() - 20, body.height)
                            : fui::makeRect(body.x, body.bottom() - footer, body.width, footer);
    const char* labels[] = {tr(STR_2048_RESUME), tr(STR_2048_NEW), tr(STR_2048_HELP)};
    const fui::ActionId actions[] = {Play, New, Help};
    const gamepanel::Mark marks[] = {gamepanel::Mark::Play, gamepanel::Mark::New, gamepanel::Mark::Help};
    const int y = wide ? right.y + (right.height - 3 * (menuH + 10) - 38) / 2 : right.y;
    for (int n = 0; n < 3; ++n)
      gamepanel::row(s, fui::makeRect(right.x, y + n * (menuH + 10), right.width, menuH), labels[n], actions[n], 0,
                     marks[n], n == m.selected);
    dashboard::text(s, fui::makeRect(right.x, right.bottom() - 28, right.width, 28), tr(STR_REDESIGN_2048_HINT), 3);
    return;
  }
  const auto r = boardRect(s.device());
  const auto info = wide ? fui::makeRect(r.right() + 16, body.y, body.right() - r.right() - 16, body.height)
                         : fui::makeRect(body.x, body.y, body.width, 76);
  const int half = (info.width - 10) / 2;
  score(s, fui::makeRect(info.x, info.y, half, 76), tr(STR_2048_SCORE), state.score);
  score(s, fui::makeRect(info.x + half + 10, info.y, info.width - half - 10, 76), tr(STR_2048_BEST), m.best);
  board(s, r, state);
  const auto controls = wide ? fui::makeRect(info.x, info.y + 112, info.width, info.height - 112)
                             : fui::makeRect(body.x, r.bottom() + 38, body.width, body.bottom() - r.bottom() - 38);
  dashboard::text(
      s,
      wide ? fui::makeRect(info.x, info.y + 82, info.width, 30) : fui::makeRect(body.x, r.bottom() + 8, body.width, 28),
      !game2048::canMove(state)        ? tr(STR_2048_OVER)
      : game2048::highest(state) >= 11 ? tr(STR_2048_WON)
                                       : tr(STR_REDESIGN_SWIPE),
      3);
  dpad::build(s, controls, Move);
  if (m.saveFailed)
    dashboard::text(s, fui::makeRect(body.x, body.bottom() - 28, body.width, 28), tr(STR_2048_SAVE_FAILED), 3);
}
}  // namespace game2048ui

#include "ConnectFourScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/CardPanels.h"
#include "../ui/FitText.h"
#include "../ui/PuzzleArt.h"

namespace c4ui {
namespace {
namespace c4 = connectfour;
constexpr auto black = dashboard::ink;
constexpr auto white = dashboard::paper;
fui::Rect rect(int x, int y, int w, int h) { return fui::makeRect(x, y, w, h); }
fui::Rect begin(toybox::Screen& s) {
  if (s.device().width > s.device().height) return cardpanel::begin(s, tr(STR_UI_CONNECT_FOUR_A81733));
  pixelui::header(s, {tr(STR_UI_CONNECT_FOUR_A81733), nullptr, nullptr, {}, -1, false, true, true});
  pixelui::begin(s);
  return s.body();
}
void copy(toybox::Screen& s, fui::Rect r, const char* text, int font = 4, int lines = 2) {
  fittext::draw(s.target(), r, text, font, lines, fui::TextAlign::Center);
}
void title(toybox::Screen& s, fui::Rect r, const char* text, int font = 2) {
  pixelui::text(s.target(), r, text, font, fui::Color::Black, fui::TextAlign::Center);
}
void disc(toybox::Screen& s, int x, int y, int radius, uint8_t side) {
  toybox::ring(s, x, y, radius, std::max(2, radius / 12), fui::Color::Black,
               side == c4::kDark ? fui::Color::Black : fui::Color::White);
}
void legend(toybox::Screen& s, fui::Rect r, uint8_t seat, bool explain = false) {
  const int half = r.width / 2, radius = std::min(15, r.height / 3);
  for (int n = 0; n < 2; ++n) {
    const int x = r.x + n * half;
    disc(s, x + half / 4, r.y + r.height / 2, radius, n ? c4::other(seat) : seat);
    pixelui::text(
        s.target(), rect(x + half / 4 + radius + 10, r.y, half * 3 / 4 - radius - 12, r.height),
        n ? (explain ? tr(STR_C4_THEIR_DISC) : tr(STR_C4_RIVAL)) : (explain ? tr(STR_C4_YOUR_DISC) : tr(STR_C4_YOU)),
        0);
  }
}
struct BoardLayout {
  fui::Rect grid, slots, status, legend, prompt, menu, again;
  int cell;
};
BoardLayout layout(const fui::DeviceContext& d, bool review = false) {
  const auto safe = d.safeRect();
  const bool wide = d.width > d.height;
  const int top = wide ? safe.y + 68 : pixelui::geometry(d).body.y;
  const int bottom = safe.bottom() - 12, width = safe.width - 24, x = safe.x + 12;
  BoardLayout g{};
  if (wide) {
    g.cell = std::min((width * 64 / 100) / 7, (bottom - top - (review ? 8 : 48)) / 6);
    g.grid = rect(x, top + (review ? 0 : 48), g.cell * 7, g.cell * 6);
    g.slots = rect(x, top, g.cell * 7, 42);
    const int right = g.grid.right() + 18, rw = safe.right() - 12 - right;
    g.status = rect(right, top, rw, 80);
    g.legend = rect(right, top + 90, rw, 48);
    g.prompt = rect(right, top + 146, rw, 76);
    g.menu = rect(right, bottom - 58, rw, 58);
    g.again = rect(right, bottom - 126, rw, 58);
  } else {
    g.status = rect(x, top, width, review ? 80 : 54);
    const int slotTop = g.status.bottom() + 20;
    g.slots = rect(x, slotTop, width, 52);
    const int gridTop = review ? g.status.bottom() + 18 : g.slots.bottom() + 24;
    g.cell = std::min(width / 7, (bottom - gridTop - (review ? 230 : 154)) / 6);
    g.grid = rect(x + (width - g.cell * 7) / 2, gridTop, g.cell * 7, g.cell * 6);
    g.slots.x = g.grid.x;
    g.slots.width = g.grid.width;
    g.legend = rect(x, g.grid.bottom() + 8, width, 46);
    g.prompt = rect(x, g.legend.bottom() + 4, width, 32);
    g.menu = rect(x, bottom - 60, width, 60);
    g.again = rect(x, bottom - 126, width, 58);
  }
  return g;
}
void grid(toybox::Screen& s, fui::Rect area, const uint8_t* cells, const int* line = nullptr, int last = -1) {
  const int cell = std::min(int(area.width) / 7, int(area.height) / 6);
  const int left = area.x + (area.width - cell * 7) / 2, top = area.y + (area.height - cell * 6) / 2;
  const auto board = rect(left, top, cell * 7, cell * 6);
  for (int col = 0; col < 7; ++col)
    for (int row = 0; row < 6; ++row) {
      const int at = col * 6 + row, x = left + col * cell + cell / 2, y = top + (5 - row) * cell + cell / 2;
      if (!cells || cells[at] == c4::kEmpty)
        toybox::disc(s, x, y, std::max(2, cell / 20), fui::Color::Black);
      else {
        disc(s, x, y, cell * 35 / 100, cells[at]);
        if (at == last)
          toybox::disc(s, x, y, std::max(2, cell / 12), cells[at] == c4::kDark ? fui::Color::White : fui::Color::Black);
      }
    }
  for (int n = 1; n < 7; ++n)
    s.target().line({int16_t(left + n * cell), int16_t(top)}, {int16_t(left + n * cell), int16_t(board.bottom())}, 2,
                    black);
  for (int n = 1; n < 6; ++n)
    s.target().line({int16_t(left), int16_t(top + n * cell)}, {int16_t(board.right()), int16_t(top + n * cell)}, 2,
                    black);
  dashboard::frame(s, board, 4);
  if (line && line[0] >= 0 && line[3] >= 0) {
    const int a = line[0], b = line[3];
    if (cells[a] == c4::kDark)
      s.target().line({int16_t(left + (a / 6) * cell + cell / 2), int16_t(top + (5 - a % 6) * cell + cell / 2)},
                      {int16_t(left + (b / 6) * cell + cell / 2), int16_t(top + (5 - b % 6) * cell + cell / 2)},
                      std::max(5, cell / 12 + 4), black);
    s.target().line({int16_t(left + (a / 6) * cell + cell / 2), int16_t(top + (5 - a % 6) * cell + cell / 2)},
                    {int16_t(left + (b / 6) * cell + cell / 2), int16_t(top + (5 - b % 6) * cell + cell / 2)},
                    std::max(3, cell / 12), cells[a] == c4::kDark ? white : black);
  }
}
void four(toybox::Screen& s, fui::Rect area, uint8_t side, int direction = 0, bool frames = false) {
  const int cell = std::min(int(area.width) / (direction == 1 ? 1 : 4), int(area.height) / (direction == 0 ? 1 : 4));
  const int w = direction == 1 ? cell : 4 * cell, h = direction == 0 ? cell : 4 * cell;
  const int x = area.x + (area.width - w) / 2, y = area.y + (area.height - h) / 2;
  for (int n = 0; n < 4; ++n) {
    if (frames) dashboard::frame(s, rect(x + n * cell, y, cell, cell), 2);
    disc(s, x + (direction == 1 ? 0 : n) * cell + cell / 2,
         y +
             (direction == 0   ? 0
              : direction == 1 ? n
                               : 3 - n) *
                 cell +
             cell / 2,
         direction == 0 ? cell / 3 : cell * 45 / 100, side);
  }
  if (side == c4::kDark)
    s.target().line({int16_t(x + cell / 2), int16_t(y + (direction == 2 ? 3 * cell : 0) + cell / 2)},
                    {int16_t(x + (direction == 1 ? 0 : 3 * cell) + cell / 2),
                     int16_t(y + (direction == 1 ? 3 * cell : 0) + cell / 2)},
                    std::max(5, cell / 12 + 4), black);
  s.target().line({int16_t(x + cell / 2), int16_t(y + (direction == 2 ? 3 * cell : 0) + cell / 2)},
                  {int16_t(x + (direction == 1 ? 0 : 3 * cell) + cell / 2),
                   int16_t(y + (direction == 1 ? 3 * cell : 0) + cell / 2)},
                  std::max(3, cell / 12), side == c4::kDark ? white : black);
}
void equal(toybox::Screen& s, fui::Rect area) {
  const int radius = std::min(area.width / 7, area.height / 3), cy = area.y + area.height / 2,
            cx = area.x + area.width / 2;
  disc(s, cx - radius * 2, cy, radius, c4::kLight);
  disc(s, cx + radius * 2, cy, radius, c4::kDark);
  for (int n = -1; n <= 1; n += 2) s.target().fill(rect(cx - radius / 2, cy + n * radius / 4 - 2, radius, 4), black);
}
}  // namespace
fui::Rect cellRect(const fui::DeviceContext& d, int column, int row) {
  const auto g = layout(d);
  return rect(g.grid.x + column * g.cell, g.grid.y + (5 - row) * g.cell, g.cell, g.cell);
}
fui::Rect slotRect(const fui::DeviceContext& d, int column) {
  const auto g = layout(d);
  return rect(g.slots.x + column * g.cell + 5, g.slots.y, g.cell - 10, g.slots.height);
}
int columnAt(const fui::DeviceContext& d, int x, int y) {
  const auto g = layout(d);
  if (x < g.grid.x || x >= g.grid.right()) return c4::kNoColumn;
  if (y >= g.grid.y && y < g.grid.bottom()) return (x - g.grid.x) / g.cell;
  for (int n = 0; n < 7; ++n) {
    const auto r = slotRect(d, n);
    if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom()) return n;
  }
  return c4::kNoColumn;
}
int howToPages() { return 3; }
void buildMenu(toybox::Screen& s, const MenuModel& m) {
  auto a = begin(s);
  const bool wide = s.device().width > s.device().height;
  auto panel = a;
  if (wide) panel.width = a.width * 55 / 100;
  title(s, rect(panel.x, panel.y, panel.width, 40), tr(STR_C4_HISTORY), 1);
  const char* labels[] = {tr(STR_C4_PLAYED), tr(STR_C4_VICTORIES), tr(STR_C4_STREAK)};
  const int values[] = {m.wins + m.losses + m.draws, m.wins, m.streak};
  cardpanel::stats(s, rect(panel.x, panel.y + 42, panel.width, 70), labels, values);
  const int actionY = a.bottom() - 196;
  const auto preview = wide ? rect(a.x + a.width * 57 / 100, a.y, a.width * 43 / 100, a.height - 36)
                            : rect(a.x + 12, a.y + 130, a.width - 24, actionY - a.y - 210);
  static constexpr uint8_t sample[42] = {2, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 2, 1, 1, 0, 0, 0, 1, 2, 2,
                                         2, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  grid(s, preview, m.hasHistory && m.lastCells ? m.lastCells : sample);
  const auto legendBox =
      wide ? rect(preview.x, preview.bottom() - 4, preview.width, 36) : rect(a.x, preview.bottom() + 4, a.width, 42);
  legend(s, legendBox, c4::kLight);
  const auto caption =
      wide ? rect(panel.x, panel.y + 122, panel.width, 64) : rect(a.x, legendBox.bottom(), a.width, 36);
  copy(s, caption, tr(STR_C4_CONNECT_RULE), 4, 2);
  gamepanel::row(s, rect(panel.x, actionY, panel.width, 58), tr(STR_WORDLE_PLAY), ActionMenuRow, int(MenuRow::Play),
                 gamepanel::Mark::Play, m.selected <= 0);
  gamepanel::row(s, rect(panel.x, actionY + 68, panel.width, 58), tr(STR_WORDLE_HELP), ActionMenuRow,
                 int(MenuRow::HowTo), gamepanel::Mark::Help, m.selected == 1);
  gamepanel::row(s, rect(panel.x, actionY + 136, panel.width, 58), tr(STR_MATCH_SETTINGS), ActionMenuRow,
                 int(MenuRow::Settings), gamepanel::Mark::Settings, m.selected == 2);
}
void buildBoard(toybox::Screen& s, const BoardModel& m) {
  begin(s);
  const auto g = layout(s.device(), m.review);
  if (m.review) {
    title(s, rect(g.status.x, g.status.y, g.status.width, 32), tr(STR_C4_FINISHED), 1);
    title(s, rect(g.status.x, g.status.y + 36, g.status.width, 40),
          m.game.outcome == c4::Outcome::Draw ? tr(STR_C4_DRAW)
          : m.local ? (m.game.outcome == c4::Outcome::LightWins ? tr(STR_MATCH_WHITE_WINS) : tr(STR_MATCH_BLACK_WINS))
          : (m.game.outcome == c4::Outcome::LightWins) == (m.seat == c4::kLight) ? tr(STR_C4_YOU_WON)
                                                                                 : tr(STR_C4_RIVAL_WON),
          1);
  } else {
    dashboard::frame(s, g.status);
    disc(s, g.status.x + 30, g.status.y + g.status.height / 2, 17, m.yourTurn ? m.seat : c4::other(m.seat));
    const int split = g.status.x + g.status.width * 69 / 100;
    pixelui::text(s.target(), rect(g.status.x + 58, g.status.y, split - g.status.x - 66, g.status.height),
                  m.local      ? (m.game.turn == c4::kLight ? tr(STR_MATCH_WHITE_TURN) : tr(STR_MATCH_BLACK_TURN))
                  : m.yourTurn ? tr(STR_C4_YOUR_TURN)
                               : tr(STR_C4_RIVAL_TURN),
                  1);
    int moves = 0;
    for (const auto& col : m.game.cell)
      for (auto v : col)
        if (v) ++moves;
    char label[32];
    std::snprintf(label, sizeof(label), tr(STR_C4_MOVE), moves + 1);
    s.target().line({int16_t(split), int16_t(g.status.y + 8)}, {int16_t(split), int16_t(g.status.bottom() - 8)}, 2,
                    black);
    copy(s, rect(split + 8, g.status.y, g.status.right() - split - 16, g.status.height), label, 4, 2);
    for (int n = 0; n < 7; ++n) {
      const auto r = slotRect(s.device(), n);
      const bool chosen = n == m.selectedColumn;
      dashboard::frame(s, r);
      if (chosen) s.target().fill(r, black);
      char number[2] = {char('1' + n), 0};
      pixelui::text(s.target(), r, number, 1, chosen ? fui::Color::White : fui::Color::Black, fui::TextAlign::Center);
      if (!(m.open & (1 << n)))
        s.target().line({int16_t(r.x + 6), int16_t(r.y + 6)}, {int16_t(r.right() - 6), int16_t(r.bottom() - 6)}, 3,
                        chosen ? white : black);
      if (chosen)
        s.target().triangle({int16_t(r.x + r.width / 2 - 10), int16_t(r.bottom() + 5)},
                            {int16_t(r.x + r.width / 2 + 10), int16_t(r.bottom() + 5)},
                            {int16_t(r.x + r.width / 2), int16_t(r.bottom() + 16)}, black);
    }
  }
  int line[4] = {-1, -1, -1, -1};
  if (m.review && m.game.outcome != c4::Outcome::Draw)
    c4::winningLine(m.game, m.game.outcome == c4::Outcome::LightWins ? c4::kLight : c4::kDark, line);
  grid(s, g.grid, &m.game.cell[0][0], m.review ? line : nullptr,
       !m.review && m.game.lastColumn >= 0 ? m.game.lastColumn * 6 + m.game.lastRow : -1);
  if (m.local) {
    for (int n = 0; n < 2; ++n) {
      const auto r = rect(g.legend.x + n * g.legend.width / 2, g.legend.y, g.legend.width / 2, g.legend.height);
      disc(s, r.x + 22, r.y + r.height / 2, 14, n ? c4::kDark : c4::kLight);
      pixelui::text(s.target(), rect(r.x + 46, r.y, r.width - 50, r.height),
                    n ? tr(STR_UI_BLACK_A9414A) : tr(STR_UI_WHITE_40D23F), 0);
    }
  } else
    legend(s, g.legend, m.seat);
  copy(s, g.prompt,
       m.review ? (m.game.outcome == c4::Outcome::Draw ? tr(STR_C4_DRAW_DETAIL) : tr(STR_C4_WIN_LINE))
                : tr(STR_C4_DROP_PROMPT),
       4, 2);
  if (m.review) {
    dashboard::button(s, g.again, tr(STR_C4_AGAIN), ActionAgain, 0, true, nullptr, 1);
    dashboard::button(s, g.menu, tr(STR_C4_BACK_RESULT), ActionReturnResult, 0, false, nullptr, 1);
  } else {
    dashboard::button(s, g.menu, tr(STR_C4_MENU), ActionDone, 0, true, nullptr, 1);
    gamepanel::mark(s, rect(g.menu.x + g.menu.width / 2 - 92, g.menu.y + 15, 30, 30), gamepanel::Mark::Menu, true);
  }
}
void buildResult(toybox::Screen& s, const ResultModel& m) {
  auto a = begin(s);
  const bool wide = s.device().width > s.device().height;
  const bool draw = m.outcome == c4::Outcome::Draw,
             won = !draw && ((m.outcome == c4::Outcome::LightWins) == (m.seat == c4::kLight));
  const int footer = wide ? 0 : 190, topH = wide ? 110 : 190;
  auto upper = wide ? rect(a.x, a.y, a.width * 55 / 100, a.height) : rect(a.x, a.y, a.width, topH);
  const auto emblem = rect(upper.x, upper.y, upper.width, wide ? 58 : 88);
  if (draw)
    equal(s, emblem);
  else
    puzzleart::draw(s, emblem, won ? puzzleart::FourTrophy : puzzleart::FourFlags);
  title(s, rect(upper.x, emblem.bottom() + 8, upper.width, wide ? 42 : 54),
        draw      ? tr(STR_C4_DRAW)
        : m.local ? (m.outcome == c4::Outcome::LightWins ? tr(STR_MATCH_WHITE_WINS) : tr(STR_MATCH_BLACK_WINS))
        : won     ? tr(STR_C4_WIN)
                  : tr(STR_C4_LOSS),
        2);
  copy(s, rect(upper.x, emblem.bottom() + (wide ? 52 : 68), upper.width, 44),
       draw      ? tr(STR_C4_DRAW_DETAIL)
       : m.local ? tr(STR_C4_FOUR_DETAIL)
       : won     ? tr(STR_C4_WIN_DETAIL)
                 : tr(STR_C4_LOSS_DETAIL));
  auto middle = wide ? rect(a.x, a.y + topH + 25, a.width * 55 / 100, a.height - topH - 35)
                     : rect(a.x, a.y + topH + 12, a.width, a.height - topH - footer - 24);
  dashboard::frame(s, middle, 4);
  if (draw) {
    equal(s, rect(middle.x, middle.y, middle.width, middle.height - 42));
    title(s, rect(middle.x, middle.bottom() - 40, middle.width, 36), tr(STR_C4_NO_WINNER), 1);
  } else {
    four(s, rect(middle.x + 20, middle.y + 20, middle.width - 40, middle.height - 66), won ? m.seat : c4::other(m.seat),
         0, true);
    title(s, rect(middle.x + 10, middle.bottom() - 40, middle.width - 20, 32),
          won ? tr(STR_C4_FOUR) : tr(STR_C4_FINISHED), 1);
  }
  auto controls = wide ? rect(a.x + a.width * 58 / 100, a.y, a.width * 42 / 100, a.height)
                       : rect(a.x, a.bottom() - footer, a.width, footer);
  if (won && !m.local) {
    const char* labels[] = {tr(STR_C4_VICTORIES), tr(STR_C4_STREAK)};
    dashboard::frame(s, rect(controls.x, controls.y, controls.width, 66));
    for (int n = 0; n < 2; ++n) {
      const int x = controls.x + n * controls.width / 2;
      if (n) s.target().line({int16_t(x), int16_t(controls.y)}, {int16_t(x), int16_t(controls.y + 66)}, 2, black);
      title(s, rect(x, controls.y + 4, controls.width / 2, 26), labels[n], 0);
      char count[12];
      std::snprintf(count, sizeof(count), "%d", n ? m.streak : m.wins);
      dashboard::digits(s, rect(x + 8, controls.y + 32, controls.width / 2 - 16, 28), count);
    }
  } else
    copy(s, rect(controls.x, controls.y, controls.width, 62),
         m.local || draw ? tr(STR_C4_TRY_STRATEGY) : tr(STR_C4_TRY_AGAIN));
  const int gap = 8, h = wide ? 58 : 36, y = wide ? controls.bottom() - 3 * h - 2 * gap : controls.y + 72;
  const char* labels[] = {tr(STR_C4_AGAIN), tr(STR_C4_VIEW_BOARD), tr(STR_C4_BACK_MENU)};
  const fui::ActionId actions[] = {ActionAgain, ActionViewBoard, ActionDone};
  for (int n = 0; n < 3; ++n)
    dashboard::button(s, rect(controls.x, y + n * (h + gap), controls.width, h), labels[n], actions[n], 0, n == 0,
                      nullptr, 1);
}
void buildHowTo(toybox::Screen& s, const HowToModel& m) {
  auto a = begin(s);
  const int page = std::clamp(m.page, 0, 2);
  const bool wide = s.device().width > s.device().height;
  auto pager = rect(a.x, a.bottom() - 48, a.width, 48);
  pageui::build(s, pager, page + 1, 3, ActionHowToPrevious, 0, ActionHowToNext, 0);
  a.height -= 60;
  title(s, rect(a.x, a.y, a.width, 28), tr(STR_WORDLE_HELP), 0);
  static constexpr StrId headings[] = {StrId::STR_C4_CHOOSE_COLUMN, StrId::STR_C4_CONNECT_FOUR,
                                       StrId::STR_C4_FULL_COLUMNS};
  static constexpr StrId bodies[] = {StrId::STR_C4_COLUMN_DETAIL, StrId::STR_C4_FOUR_DETAIL, StrId::STR_C4_FULL_DETAIL};
  title(s, rect(a.x, a.y + 32, a.width, 48), I18N.get(headings[page]), 2);
  copy(s, rect(a.x + 8, a.y + 86, a.width - 16, wide ? 44 : 62), I18N.get(bodies[page]), 4, 3);
  auto room = rect(a.x, a.y + (wide ? 138 : 160), a.width, a.height - (wide ? 138 : 160));
  if (page == 1) {
    const int gap = 8, h = (room.height - 60 - 2 * gap) / 3;
    static constexpr StrId names[] = {StrId::STR_C4_HORIZONTAL, StrId::STR_C4_VERTICAL, StrId::STR_C4_DIAGONAL};
    static constexpr StrId details[] = {StrId::STR_C4_ROW_DETAIL, StrId::STR_C4_VERTICAL_DETAIL,
                                        StrId::STR_C4_DIAGONAL_DETAIL};
    for (int n = 0; n < 3; ++n) {
      auto card = wide ? rect(room.x + n * (room.width / 3 + 3), room.y, room.width / 3 - 6, room.height - 60)
                       : rect(room.x, room.y + n * (h + gap), room.width, h);
      dashboard::frame(s, card);
      pixelui::text(s.target(), rect(card.x + 8, card.y + 4, card.width - 16, 28), I18N.get(names[n]), 1,
                    fui::Color::Black, wide ? fui::TextAlign::Center : fui::TextAlign::Left);
      auto art = wide ? rect(card.x + 10, card.y + 34, card.width - 20, card.height - 86)
                      : rect(card.x + 10, card.y + 36, card.width * 45 / 100, card.height - 44);
      four(s, art, c4::kLight, n);
      copy(s,
           wide ? rect(card.x + 10, card.bottom() - 48, card.width - 20, 42)
                : rect(art.right() + 10, card.y + 34, card.right() - art.right() - 20, card.height - 42),
           I18N.get(details[n]), 4, 3);
    }
    legend(s, rect(room.x, room.bottom() - 56, room.width, 30), c4::kLight);
    copy(s, rect(room.x, room.bottom() - 26, room.width, 26), tr(STR_C4_WIN_LINE), 4, 1);
  } else {
    const int footer = page == 0 ? 126 : 158;
    auto art = wide ? rect(room.x, room.y, room.width / 2, room.height)
                    : rect(room.x, room.y, room.width, room.height - footer);
    const int headroom = page == 0 ? 100 : 66;
    const int cell = std::min(art.width / 4, (art.height - headroom) / 4), left = art.x + (art.width - cell * 4) / 2,
              top = art.y + headroom;
    for (int n = 0; n < 4; ++n) {
      char digit[2] = {char('1' + n), 0};
      auto label = rect(left + n * cell, art.y + (page == 0 ? 18 : 0), cell, 38);
      if ((page == 0 && n == 1) || (page == 2 && n == 0)) s.target().fill(label, black);
      if (page == 2 && n > 0) {
        const int cx = label.x + label.width / 2;
        s.target().line({int16_t(cx), int16_t(label.y + 5)}, {int16_t(cx), int16_t(label.bottom() - 10)}, 3, black);
        s.target().triangle({int16_t(cx - 8), int16_t(label.bottom() - 16)},
                            {int16_t(cx + 8), int16_t(label.bottom() - 16)}, {int16_t(cx), int16_t(label.bottom() - 5)},
                            black);
      } else
        pixelui::text(s.target(), label, page == 2 ? "X" : digit, 1,
                      ((page == 0 && n == 1) || (page == 2 && n == 0)) ? fui::Color::White : fui::Color::Black,
                      fui::TextAlign::Center);
    }
    for (int row = 0; row < 4; ++row)
      for (int col = 0; col < 4; ++col) {
        auto box = rect(left + col * cell, top + row * cell, cell, cell);
        dashboard::frame(s, box, 2);
        if ((page == 0 && col == 1 && row >= 2) || (page == 2 && col == 0))
          disc(s, box.x + cell / 2, box.y + cell / 2, cell / 3,
               page == 0 ? (row == 2 ? c4::kLight : c4::kDark) : (row % 2 ? c4::kLight : c4::kDark));
        else
          toybox::disc(s, box.x + cell / 2, box.y + cell / 2, std::max(1, cell / 24), fui::Color::Black);
      }
    dashboard::frame(s, rect(left, top, cell * 4, cell * 4), 4);
    if (page == 0) {
      disc(s, left + cell + cell / 2, top - 26, 16, c4::kLight);
      s.target().line({int16_t(left + cell + cell / 2), int16_t(top - 8)},
                      {int16_t(left + cell + cell / 2), int16_t(top + 20)}, 3, black);
      s.target().triangle({int16_t(left + cell + cell / 2 - 8), int16_t(top + 10)},
                          {int16_t(left + cell + cell / 2 + 8), int16_t(top + 10)},
                          {int16_t(left + cell + cell / 2), int16_t(top + 24)}, black);
    }
    auto detail = wide ? rect(room.x + room.width / 2 + 12, room.y, room.width / 2 - 12, room.height)
                       : rect(room.x, room.bottom() - footer, room.width, footer);
    if (page == 0) {
      legend(s, rect(detail.x, detail.y, detail.width, 44), c4::kLight, true);
      copy(s, rect(detail.x, detail.y + 50, detail.width, detail.height - 50), tr(STR_C4_TURNS), 4, 3);
    } else {
      dashboard::frame(s, rect(detail.x, detail.y, detail.width, detail.height - 48));
      title(s, rect(detail.x + 8, detail.y + 16, detail.width - 16, 32), tr(STR_C4_NOBODY_WINS), 1);
      copy(s, rect(detail.x + 10, detail.y + 50, detail.width - 20, detail.height - 62), tr(STR_C4_DRAW_RULE), 4, 3);
    }
  }
}
}  // namespace c4ui

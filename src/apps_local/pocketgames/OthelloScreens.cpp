#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/CardPanels.h"
#include "../ui/FitText.h"
#include "../ui/GameGuide.h"
#include "../ui/MatchSettings.h"
#include "../ui/OthelloArt.h"
#include "PocketGamesScreens.h"

namespace pocketui {
namespace pg = pocketgames;
namespace {
fui::Rect rect(int x, int y, int w, int h) { return fui::makeRect(x, y, w, h); }
void text(toybox::Screen& s, fui::Rect r, const char* copy, int font = 4, int lines = 3,
          fui::TextAlign align = fui::TextAlign::Center) {
  fittext::draw(s.target(), r, copy, font, lines, align);
}
void title(toybox::Screen& s, fui::Rect r, const char* copy, int font = 2) {
  pixelui::text(s.target(), r, copy, font, fui::Color::Black, fui::TextAlign::Center);
}
void disk(toybox::Screen& s, int x, int y, int radius, int colour, bool inverse = false) {
  toybox::ring(s, x, y, radius, std::max(2, radius / 12), inverse ? fui::Color::White : fui::Color::Black,
               colour == 1 ? fui::Color::Black : fui::Color::White);
}
void grid(toybox::Screen& s, fui::Rect area, const pg::Othello& g, bool legal = false, bool coordinates = false,
          int botSquare = -1) {
  const int margin = coordinates ? 22 : 0;
  const int cell = std::min((int(area.width) - margin) / 8, (int(area.height) - margin) / 8);
  const auto b = rect(area.x + (area.width - margin - cell * 8) / 2 + margin, area.y, cell * 8, cell * 8);
  for (int n = 1; n < 8; ++n) {
    s.target().line({int16_t(b.x + n * cell), b.y}, {int16_t(b.x + n * cell), int16_t(b.bottom())}, 1, dashboard::ink);
    s.target().line({b.x, int16_t(b.y + n * cell)}, {int16_t(b.right()), int16_t(b.y + n * cell)}, 1, dashboard::ink);
  }
  for (int n = 0; n < 64; ++n) {
    const int x = b.x + n % 8 * cell + cell / 2, y = b.y + n / 8 * cell + cell / 2;
    if (n == botSquare && !g.board[n]) {
      dashboard::frame(s, rect(b.x + n % 8 * cell + 3, b.y + n / 8 * cell + 3, cell - 6, cell - 6), 3);
      disk(s, x, y, cell * 38 / 100, g.turn);
      toybox::disc(s, x, y, std::max(2, cell / 12), g.turn == 1 ? fui::Color::White : fui::Color::Black);
    } else if (g.board[n])
      disk(s, x, y, cell * 38 / 100, g.board[n]);
    else if (legal && !g.over && pg::flips(g, n, g.turn))
      toybox::disc(s, x, y, std::max(2, cell / 12), fui::Color::Black);
  }
  dashboard::frame(s, b, 4);
  if (!coordinates) return;
  for (int n = 0; n < 8; ++n) {
    char row[2] = {char('1' + n), 0}, col[2] = {char('A' + n), 0};
    title(s, rect(b.x - margin, b.y + n * cell, margin - 4, cell), row, 0);
    title(s, rect(b.x + n * cell, b.bottom() + 2, cell, 20), col, 0);
  }
}
void scores(toybox::Screen& s, fui::Rect r, const pg::Othello& g, bool compact = false, int human = 1) {
  const int gap = compact ? 0 : 10, half = (r.width - gap) / 2;
  for (int n = 0; n < 2; ++n) {
    const auto box = rect(r.x + n * (half + gap), r.y, half, r.height);
    dashboard::frame(s, box, compact ? 2 : 3);
    const int radius = std::min(compact ? 12 : 19, int(box.height) / 3);
    disk(s, box.x + radius + 10, box.y + box.height / 2, radius, n + 1);
    pixelui::text(s.target(), rect(box.x + 2 * radius + 18, box.y, box.width - 2 * radius - 52, box.height),
                  (n + 1) == human ? tr(STR_OT_YOU) : tr(STR_OT_MACHINE), 0);
    char value[8];
    std::snprintf(value, sizeof(value), "%d", pg::disks(g, n + 1));
    const int valueH = std::min(32, int(box.height) - 16);
    dashboard::digits(s, rect(box.right() - 36, box.y + (box.height - valueH) / 2, 28, valueH), value);
  }
}
struct Layout {
  fui::Rect board, score, status, prompt, menu, restart;
};
Layout layout(const fui::DeviceContext& d, bool review = false) {
  const auto safe = d.safeRect();
  const bool wide = d.width > d.height;
  const int x = safe.x + 12, w = safe.width - 24, top = wide ? safe.y + 68 : pixelui::geometry(d).body.y,
            bottom = safe.bottom() - 12;
  Layout l{};
  if (wide) {
    const int cell = std::min((w * 60 / 100) / 8, (bottom - top) / 8);
    l.board = rect(x, top, cell * 8, cell * 8);
    const int right = l.board.right() + 18, rw = safe.right() - 12 - right;
    const int groupY = top + (bottom - top - 300) / 2;
    l.prompt = rect(right, groupY, rw, 60);
    l.score = rect(right, groupY + 72, rw, 60);
    l.status = rect(right, groupY + 140, rw, 44);
    l.restart = rect(right, groupY + 192, rw, 50);
    l.menu = rect(right, groupY + 250, rw, 50);
  } else {
    l.score = rect(x, top, w, 64);
    l.status = rect(x, top + 74, w, 34);
    const int gridTop = top + 118, cell = std::min(w / 8, (bottom - gridTop - (review ? 176 : 112)) / 8);
    l.board = rect(x + (w - cell * 8) / 2, gridTop, cell * 8, cell * 8);
    l.prompt = rect(x, l.board.bottom() + 10, w, 38);
    l.menu = rect(x, bottom - 58, (w - 10) / 2, 58);
    l.restart = rect(x + (w + 10) / 2, bottom - 58, (w - 10) / 2, 58);
    if (review) {
      l.menu = rect(x, bottom - 58, w, 58);
      l.restart = rect(x, bottom - 126, w, 58);
    }
  }
  return l;
}
const char* status(const pg::Othello& g, int human) {
  if (g.over)
    return pg::disks(g, 1) == pg::disks(g, 2)              ? tr(STR_UI_DRAW_7AAB1B)
           : pg::disks(g, human) > pg::disks(g, 3 - human) ? tr(STR_OT_VICTORY)
                                                           : tr(STR_OT_MACHINE_WINS);
  return g.turn != human ? tr(STR_OT_THINKING) : g.passed ? tr(STR_OT_PASSED) : tr(STR_OT_YOUR_TURN);
}
void board(toybox::Screen& s, const Model& m) {
  const auto& g = m.state->othello;
  const bool review = m.view == View::Review;
  const auto l = layout(s.device(), review);
  scores(s, l.score, g, s.device().width > s.device().height, m.humanColour);
  s.target().fill(l.status, dashboard::ink);
  pixelui::text(s.target(), l.status, m.botSquare >= 0 ? tr(STR_OT_PLACING) : status(g, m.humanColour), 1,
                fui::Color::White, fui::TextAlign::Center);
  grid(s, l.board, g, !review && g.turn == m.humanColour, false, review ? -1 : m.botSquare);
  text(s, l.prompt,
       m.saveFailed              ? tr(STR_UI_COULD_NOT_SAVE_GAME_615E7A)
       : m.invalid               ? tr(STR_OT_INVALID)
       : review || g.over        ? tr(STR_OT_FINAL_BOARD)
       : m.botSquare >= 0        ? tr(STR_OT_PLACING_HINT)
       : g.turn != m.humanColour ? tr(STR_OT_THINKING)
                                 : tr(STR_OT_TAP),
       4, 2);
  gamepanel::row(s, l.menu, review ? tr(STR_OT_BACK_RESULT) : tr(STR_WORDLE_MENU), review ? ViewResult : Menu, 0,
                 gamepanel::Mark::Menu, false, nullptr, false);
  gamepanel::row(s, l.restart, review ? tr(STR_UI_NEW_GAME_F6F041) : tr(STR_UI_RESTART_69ED00), review ? New : Restart,
                 0, gamepanel::Mark::Restart, false, nullptr, false);
}
void menu(toybox::Screen& s, const Model& m) {
  const auto b = s.body();
  const bool wide = b.width > b.height;
  const auto left = wide ? rect(b.x, b.y, b.width / 2 - 8, b.height) : b;
  title(s, rect(left.x, left.y, left.width, 36), m.state->othello.over ? tr(STR_OT_FINISHED) : tr(STR_OT_PAUSED), 1);
  scores(s, rect(left.x + 20, left.y + 44, left.width - 40, 50), m.state->othello, true, m.humanColour);
  const int preview = std::min(int(left.width) * 72 / 100, wide ? int(left.height) - 124 : int(left.height) - 388);
  const auto art = rect(left.x + (left.width - preview) / 2, left.y + 112, preview, preview);
  grid(s, art, m.state->othello, false, true);
  const int x = wide ? left.right() + 16 : b.x, w = wide ? b.right() - x : b.width,
            top = wide ? b.y + 40 : art.bottom() + 22;
  const int row = std::min(62, (b.bottom() - top - 44 - 16) / 4);
  static constexpr fui::ActionId actions[] = {Play, New, Settings, Help};
  static constexpr gamepanel::Mark marks[] = {gamepanel::Mark::Play, gamepanel::Mark::Restart,
                                              gamepanel::Mark::Settings, gamepanel::Mark::Help};
  const char* labels[] = {m.state->othello.over ? tr(STR_OT_VIEW_RESULT) : tr(STR_OT_CONTINUE),
                          tr(STR_UI_NEW_GAME_F6F041), tr(STR_MATCH_SETTINGS), tr(STR_WORDLE_HELP)};
  for (int n = 0; n < 4; ++n)
    gamepanel::row(s, rect(x, top + n * (row + 8), w, row), labels[n], actions[n], 0, marks[n], n == 0);
  text(s, rect(x, b.bottom() - 30, w, 28), m.saveFailed ? tr(STR_UI_COULD_NOT_SAVE_GAME_615E7A) : tr(STR_OT_MENU_HINT),
       4, 2);
}
void confirm(toybox::Screen& s, const Model& m) {
  const auto b = s.body();
  const bool wide = b.width > b.height;
  const int top = b.y;
  const int leftW = wide ? b.width * 43 / 100 : b.width;
  othelloart::draw(s, rect(b.x, top + 8, leftW, wide ? 60 : 86), othelloart::Restart);
  title(s, rect(b.x, top + (wide ? 78 : 108), leftW, 54), tr(STR_OT_NEW_QUESTION));
  text(s, rect(b.x + 8, top + (wide ? 142 : 170), leftW - 16, wide ? 82 : 68), tr(STR_OT_REPLACE), 4, 3);
  const int x = wide ? b.x + leftW + 16 : b.x, y = wide ? b.y : top + 260, w = wide ? b.right() - x : b.width;
  const auto panel = rect(x, y, w, wide ? b.height - 174 : std::max(140, int(b.height) - 260 - 202));
  dashboard::frame(s, panel);
  title(s, rect(x + 8, y + 10, w - 16, 36), tr(STR_OT_CURRENT), 1);
  const int size = std::min(panel.height - 64, w * 45 / 100);
  grid(s, rect(x + 16, y + 52, size, size), m.state->othello);
  const int sx = x + size + 28, sw = panel.right() - sx - 12;
  for (int n = 0; n < 2; ++n) {
    const auto r = rect(sx, y + 64 + n * size / 2, sw, size / 2);
    disk(s, r.x + 15, r.y + r.height / 2, 15, n ? 3 - m.humanColour : m.humanColour);
    char label[48];
    std::snprintf(label, sizeof(label), "%s  %d", n ? tr(STR_OT_MACHINE) : tr(STR_OT_YOU),
                  pg::disks(m.state->othello, n ? 3 - m.humanColour : m.humanColour));
    pixelui::text(s.target(), rect(r.x + 38, r.y, r.width - 38, r.height), label, 1);
  }
  text(s, rect(x, panel.bottom() + 8, w, 32), tr(STR_OT_IRREVERSIBLE), 4, 2);
  dashboard::button(s, rect(x, b.bottom() - 126, w, 58), tr(STR_UI_NEW_GAME_F6F041), ConfirmNew, 0, true, nullptr, 1);
  dashboard::button(s, rect(x, b.bottom() - 58, w, 58), tr(STR_WORDLE_CANCEL), Cancel, 0, false, nullptr, 1);
}
void equal(toybox::Screen& s, fui::Rect r) {
  const int radius = std::min(r.width / 7, r.height * 2 / 5), cy = r.y + r.height / 2;
  disk(s, r.x + r.width / 3 - radius / 2, cy, radius, 1);
  disk(s, r.x + r.width * 2 / 3 + radius / 2, cy, radius, 2);
  title(s, rect(r.x + r.width / 2 - 30, r.y, 60, r.height), "=", 2);
}
void result(toybox::Screen& s, const Model& m) {
  const auto b = s.body();
  const bool wide = b.width > b.height;
  const auto& g = m.state->othello;
  const int black = pg::disks(g, 1), white = pg::disks(g, 2);
  const bool won = pg::disks(g, m.humanColour) > pg::disks(g, 3 - m.humanColour), draw = black == white;
  const auto left = wide ? rect(b.x, b.y, b.width * 45 / 100, b.height) : b;
  const int artH = wide ? 82 : 100;
  const int leftTop = left.y + (wide ? (left.height - 244) / 2 : 0);
  if (draw)
    equal(s, rect(left.x, leftTop + 12, left.width, artH));
  else
    othelloart::draw(s, rect(left.x, leftTop + 12, left.width, artH), won ? othelloart::Trophy : othelloart::Flag);
  title(s, rect(left.x, leftTop + artH + 28, left.width, 60),
        draw  ? tr(STR_UI_DRAW_7AAB1B)
        : won ? tr(STR_OT_WON)
              : tr(STR_OT_MACHINE_WINS));
  text(s, rect(left.x + 6, leftTop + artH + 96, left.width - 12, 54),
       draw  ? tr(STR_OT_DRAW_COPY)
       : won ? tr(STR_OT_WIN_COPY)
             : tr(STR_OT_LOSS_COPY),
       4, 2);
  const int row = wide ? 48 : 56, controlsH = 3 * row + 20, groupH = std::min(384, int(b.height));
  const int x = wide ? left.right() + 18 : b.x, w = wide ? b.right() - x : b.width,
            y = wide ? b.y + (b.height - groupH) / 2 : b.y + 260;
  const auto panel = rect(x, y, w, wide ? groupH - controlsH - 80 : 180);
  dashboard::frame(s, panel);
  const auto heading = rect(x + 6, y + 6, w - 12, 44);
  if (!won && !draw) s.target().fill(heading, dashboard::ink);
  pixelui::text(s.target(), heading, tr(STR_OT_FINAL), 1, !won && !draw ? fui::Color::White : fui::Color::Black,
                fui::TextAlign::Center);
  const int half = (w - 12) / 2;
  const auto score = rect(x + 6, y + 52, w - 12, panel.height - 58);
  for (int n = 0; n < 2; ++n) {
    const auto r = rect(score.x + n * half, score.y, half, score.height);
    const bool inverse = won && n == 0;
    if (inverse) s.target().fill(r, dashboard::ink);
    if (n) s.target().line({r.x, r.y}, {r.x, int16_t(r.bottom())}, 2, dashboard::ink);
    pixelui::text(s.target(), rect(r.x + 4, r.y + 6, r.width - 8, 32), n ? tr(STR_OT_MACHINE) : tr(STR_OT_YOU), 1,
                  inverse ? fui::Color::White : fui::Color::Black, fui::TextAlign::Center);
    const int radius = std::min(30, r.height / 5);
    disk(s, r.x + radius + 12, r.y + r.height * 60 / 100, radius, n ? 3 - m.humanColour : m.humanColour, inverse);
    char value[8];
    std::snprintf(value, sizeof(value), "%d", pg::disks(g, n ? 3 - m.humanColour : m.humanColour));
    dashboard::digits(s, rect(r.x + 2 * radius + 24, r.y + 44, r.width - 2 * radius - 34, r.height - 50), value,
                      inverse);
  }
  if (won && !m.saveFailed) {
    const auto badge = rect(x + w / 8, panel.bottom() + 10, w * 3 / 4, 38);
    s.target().fill(badge, dashboard::ink);
    s.target().stroke(badge.inset({3, 3, 3, 3}), dashboard::paper, 1);
    pixelui::text(s.target(), badge, tr(STR_OT_VICTORY), 1, fui::Color::White, fui::TextAlign::Center);
  } else {
    text(s, rect(x, panel.bottom() + 10, w, wide ? 56 : 38),
         m.saveFailed ? tr(STR_UI_COULD_NOT_SAVE_GAME_615E7A)
         : draw       ? tr(STR_OT_NO_WINNER)
         : won        ? tr(STR_OT_VICTORY)
                      : tr(STR_OT_TRY_AGAIN),
         4, 2);
  }
  const int first = wide ? y + groupH - controlsH : b.bottom() - controlsH;
  const char* labels[] = {tr(STR_UI_NEW_GAME_F6F041), tr(STR_OT_VIEW_BOARD), tr(STR_UI_BACK_TO_MENU_398BBB)};
  static constexpr fui::ActionId actions[] = {New, ViewBoard, Menu};
  for (int n = 0; n < 3; ++n)
    dashboard::button(s, rect(x, first + n * (row + 10), w, row), labels[n], actions[n], 0, n == 0, nullptr, 1);
}
void strip(toybox::Screen& s, fui::Rect r, bool after) {
  const int cell = std::min(r.width / 4, int(r.height)), x = r.x + (r.width - cell * 4) / 2,
            y = r.y + (r.height - cell) / 2;
  for (int n = 0; n < 4; ++n) {
    dashboard::frame(s, rect(x + n * cell, y, cell, cell), 2);
    if (!after && n == 3)
      toybox::disc(s, x + n * cell + cell / 2, y + cell / 2, std::max(3, cell / 12), fui::Color::Black);
    else
      disk(s, x + n * cell + cell / 2, y + cell / 2, cell / 3, after || !n ? 1 : 2);
  }
}
void lesson(toybox::Screen& s, fui::Rect r, int n, const char* heading, const char* copy, bool wide) {
  dashboard::frame(s, r);
  const int aw = wide ? r.width : r.width * 32 / 100;
  const auto art = wide ? rect(r.x + 12, r.y + 12, r.width - 24, 44) : rect(r.x + 12, r.y + 12, aw - 24, r.height - 24);
  if (n < 2)
    othelloart::draw(s, art, n ? othelloart::End : othelloart::Pass);
  else {
    const int radius = std::min(wide ? 12 : 24, art.width / 5);
    disk(s, art.x + radius, art.y + art.height / 2 - (wide ? 6 : 10), radius, 1);
    disk(s, art.right() - radius, art.y + art.height / 2 - (wide ? 6 : 10), radius, 2);
    title(s, rect(art.x + art.width / 2 - 10, art.y + art.height / 2 - (wide ? 18 : 26), 20, 32), ">", 1);
    title(s, rect(art.x, art.y + art.height / 2 + (wide ? 6 : 20), art.width / 2, 32), "38", 1);
    title(s, rect(art.x + art.width / 2, art.y + art.height / 2 + (wide ? 6 : 20), art.width / 2, 32), "26", 1);
  }
  const int tx = wide ? r.x + 10 : r.x + aw, ty = wide ? r.y + 62 : r.y + 10,
            tw = wide ? r.width - 20 : r.width - aw - 12;
  pixelui::text(s.target(), rect(tx, ty, tw, 32), heading, 1);
  text(s, rect(tx, ty + 36, tw, r.bottom() - ty - 44), copy, 4, 8, fui::TextAlign::Left);
}
void help(toybox::Screen& s, const Model& m) {
  const auto b = s.body();
  const bool wide = b.width > b.height;
  const int page = std::clamp(m.guidePage, 0, 2);
  title(s, rect(b.x, b.y, b.width, 28), tr(STR_WORDLE_HELP), 0);
  const char* headings[] = {tr(STR_OT_YOUR_DISKS), tr(STR_OT_FLIP_TITLE), tr(STR_OT_TURNS_TITLE)};
  title(s, rect(b.x, b.y + 36, b.width, 48), headings[page]);
  const int top = b.y + (wide ? 80 : 94), bottom = b.bottom() - 60;
  switch (page) {
    case 0: {
      text(s, rect(b.x, top, b.width, 42), tr(STR_OT_COLOURS), 4, 2);
      const auto content = wide ? rect(b.x, top + 50, b.width * 55 / 100, bottom - top - 50)
                                : rect(b.x, top + 96, b.width, bottom - top - 218);
      const int legendY = wide ? top + 42 : top + 48, lx = wide ? content.right() + 20 : b.x,
                lw = wide ? b.right() - lx : b.width;
      for (int n = 0; n < 2; ++n) {
        const int x = wide ? lx : lx + n * lw / 2, y = legendY + (wide ? n * 36 : 0);
        disk(s, x + 24, y + 18, 16, n ? 3 - m.humanColour : m.humanColour);
        pixelui::text(s.target(), rect(x + 48, y, wide ? lw - 52 : lw / 2 - 52, 38),
                      n ? tr(STR_OT_MACHINE) : tr(STR_OT_YOU), 1);
      }
      static constexpr pg::Othello initial = [] {
        pg::Othello g{};
        g.board[27] = g.board[36] = 2;
        g.board[28] = g.board[35] = 1;
        return g;
      }();
      grid(s, content, initial, true, true);
      const auto callout = wide ? rect(lx, top + 124, lw, bottom - top - 124) : rect(b.x, bottom - 118, b.width, 118);
      dashboard::frame(s, callout);
      title(s, rect(callout.x + 6, callout.y + 6, callout.width - 12, 26), tr(STR_OT_WHERE), 1);
      text(s, rect(callout.x + 12, callout.y + 34, callout.width - 24, callout.height - 40), tr(STR_OT_WHERE_COPY), 4,
           6);
      break;
    }
    case 1: {
      text(s, rect(b.x, top, b.width, 62), tr(STR_OT_FLIP_COPY), 4, 3, fui::TextAlign::Left);
      const int leftW = wide ? b.width * 55 / 100 : b.width, stripH = wide ? 40 : 70;
      const int first = top + (wide ? 66 : 76);
      pixelui::text(s.target(), rect(b.x + 20, first, leftW - 40, 24), tr(STR_OT_PLACE), 0, fui::Color::Black,
                    fui::TextAlign::Right);
      title(s, rect(b.x + 20, first + 26, leftW - 40, 28), tr(STR_OT_BEFORE), 1);
      strip(s, rect(b.x + 20, first + 58, leftW - 40, stripH), false);
      title(s, rect(b.x + 20, first + stripH + (wide ? 68 : 78), leftW - 40, 28), tr(STR_OT_AFTER), 1);
      strip(s, rect(b.x + 20, first + stripH + (wide ? 100 : 110), leftW - 40, stripH), true);
      if (!wide) {
        const int cell = stripH, left = b.x + 20 + (leftW - 40 - 4 * cell) / 2;
        const int cx = left + 7 * cell / 2, tip = first + 56;
        s.target().line({int16_t(cx), int16_t(first + 26)}, {int16_t(cx), int16_t(tip - 8)}, 4, dashboard::ink);
        s.target().triangle({int16_t(cx - 8), int16_t(tip - 10)}, {int16_t(cx + 8), int16_t(tip - 10)},
                            {int16_t(cx), int16_t(tip)}, dashboard::ink);
        const int y = first + 2 * stripH + 116;
        for (int n = 1; n < 3; ++n) {
          const int x = left + n * cell + cell / 2;
          s.target().line({int16_t(x), int16_t(y + 18)}, {int16_t(x), int16_t(y + 4)}, 3, dashboard::ink);
          s.target().triangle({int16_t(x), int16_t(y)}, {int16_t(x - 6), int16_t(y + 8)},
                              {int16_t(x + 6), int16_t(y + 8)}, dashboard::ink);
        }
        s.target().line({int16_t(left + cell + cell / 2), int16_t(y + 18)},
                        {int16_t(left + 2 * cell + cell / 2), int16_t(y + 18)}, 3, dashboard::ink);
        title(s, rect(b.x, y + 22, b.width, 24), tr(STR_OT_FLIPPED), 0);
      }
      const int x = wide ? b.x + leftW + 16 : b.x, y = wide ? top + 76 : bottom - 128,
                w = wide ? b.right() - x : b.width;
      const auto panel = rect(x, y, w, wide ? bottom - y : 128);
      dashboard::frame(s, panel);
      pixelui::text(s.target(), rect(x + 10, y + 10, w - 20, 32), tr(STR_OT_ALL_DIRECTIONS), 1);
      text(s, rect(x + 12, y + 50, w * 74 / 100 - 18, panel.height - 58), tr(STR_OT_DIRECTIONS_COPY), 4, 8,
           fui::TextAlign::Left);
      othelloart::draw(s, rect(x + w * 75 / 100, y + 48, w / 4 - 12, panel.height - 58), othelloart::Directions);
      break;
    }
    case 2: {
      const int row = wide ? (bottom - top - 56) : (bottom - top - 68) / 3 - 8;
      const char* labels[] = {tr(STR_OT_NO_MOVES), tr(STR_OT_END), tr(STR_OT_WHO)};
      const char* copies[] = {tr(STR_OT_PASS_COPY), tr(STR_OT_END_COPY), tr(STR_OT_WHO_COPY)};
      for (int n = 0; n < 3; ++n)
        lesson(s,
               wide ? rect(b.x + n * (b.width + 12) / 3, top, (b.width - 24) / 3, row)
                    : rect(b.x, top + n * (row + 10), b.width, row),
               n, labels[n], copies[n], wide);
      break;
    }
  }
  pageui::build(s, rect(b.x, b.bottom() - 50, b.width, 48), page + 1, 3, GuidePage, page - 1, GuidePage, page + 1);
}
}  // namespace
fui::Rect othelloBoardRect(const fui::DeviceContext& device) { return layout(device).board; }
void buildOthello(toybox::Screen& s, const Model& m) {
  if (s.device().width > s.device().height)
    cardpanel::begin(s, name(pg::Kind::Othello));
  else {
    pixelui::begin(s);
    pixelui::Header h;
    h.title = name(pg::Kind::Othello);
    h.back = s.device().hasTouch;
    pixelui::header(s, h);
    backui::publish(h.back, pixelui::geometry(s.device()).back);
  }
  switch (m.view) {
    case View::Menu:
      menu(s, m);
      return;
    case View::Board:
    case View::Review:
      board(s, m);
      return;
    case View::Help:
      help(s, m);
      return;
    case View::ConfirmNew:
      confirm(s, m);
      return;
    case View::Result:
      result(s, m);
      return;
    case View::Level:
      matchsettings::build(s, {matchsettings::Game::Othello, false, m.preferredColour == 1, m.level});
      return;
  }
}
}  // namespace pocketui

#include "MinesweeperScreens.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <cstdio>

#include "../ui/Dashboard.h"
#include "../ui/GameGuide.h"
#include "../ui/GamePanel.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxIcons.h"

namespace mineui {

namespace {

namespace ms = minesweeper;

// The mark on a flagged cell and beside the counter.
//
// Lucide's, not ours. Hand-drawn three times and wrong three times: every
// version put the pennant and the pole at the same height, so the pennant was
// coextensive with the staff and no flag could emerge from the geometry at any
// size. The first two attempts tuned the SIZE, which was never the problem.
//
// The fork already vendors Lucide and generates from it, and the design
// language says to draw our own only where no good licensed option exists. This
// is the same conclusion Solitaire reached about its pips after three rounds of
// hand-drawing.
void drawFlag(toybox::Screen& screen, const fui::Rect& where, const bool paper) {
  gamepanel::mark(screen, where.inset(fui::Insets{3, 3, 3, 3}), gamepanel::Mark::Flag, paper);
}
void covered(toybox::Screen& s, fui::Rect box) {
  const int bevel = std::max(1, std::min(3, int(box.width) / 9));
  s.target().fill(box, fui::Paint::dither(fui::Color::LightGray));
  s.target().stroke(box, dashboard::ink, 1);
  s.target().line({int16_t(box.x + bevel), int16_t(box.bottom() - bevel)},
                  {int16_t(box.x + bevel), int16_t(box.y + bevel)}, bevel, dashboard::paper);
  s.target().line({int16_t(box.x + bevel), int16_t(box.y + bevel)},
                  {int16_t(box.right() - bevel), int16_t(box.y + bevel)}, bevel, dashboard::paper);
  s.target().line({int16_t(box.right() - bevel), int16_t(box.y + bevel)},
                  {int16_t(box.right() - bevel), int16_t(box.bottom() - bevel)}, bevel, dashboard::ink);
  s.target().line({int16_t(box.x + bevel), int16_t(box.bottom() - bevel)},
                  {int16_t(box.right() - bevel), int16_t(box.bottom() - bevel)}, bevel, dashboard::ink);
}
void drawMine(toybox::Screen& s, const fui::Rect& box, const bool paper) {
  const int cx = box.x + box.width / 2, cy = box.y + box.height / 2,
            r = std::max(2, std::min(int(box.width), int(box.height)) * 23 / 100);
  const auto color = paper ? fui::Color::White : fui::Color::Black;
  const auto ink = fui::Paint::solid(color), highlight = paper ? dashboard::ink : dashboard::paper;
  const int ray = r + std::max(2, r / 2), weight = std::max(1, r / 4);
  for (int n = 0; n < 4; ++n) {
    static constexpr int dx[] = {1, 0, 1, 1}, dy[] = {0, 1, 1, -1};
    const int reach = n < 2 ? ray : ray * 7 / 10;
    s.target().line({int16_t(cx - dx[n] * reach), int16_t(cy - dy[n] * reach)},
                    {int16_t(cx + dx[n] * reach), int16_t(cy + dy[n] * reach)}, weight, ink);
  }
  toybox::disc(s, cx, cy, r, color);
  const int gleam = std::max(1, r / 3);
  s.target().fill(fui::makeRect(cx - r / 2, cy - r / 2, gleam, gleam), highlight);
}
// The header band with the offset rule under it, as jaipur and the dungeon
// wear it. A local copy rather than a shared helper, for the reason
// LinkScreens gives: a copy is cheaper than a header dependency between apps.
void toyboxChrome(toybox::Screen& screen, const char* title, const char* = nullptr) {
  pixelui::begin(screen);
  pixelui::Header h;
  h.title = title;
  h.back = true;
  pixelui::header(screen, h);
  backui::publish(true, pixelui::geometry(screen.device()).back);
}

// A fragment of minefield from a picture-string: '.' covered, 'o' open,
// '*' a covered mine, 'F' a flag on a mine, 'M' a mine laid bare for teaching.
// The numbers are computed from the fragment's own mines, so a diagram cannot
// disagree with the rule it illustrates -- the previous page one showed a 3
// with no mines in its picture at all.
void lessonField(toybox::Screen& screen, const int16_t left, const int16_t top, const int16_t cell,
                 const char* const* rows, const int columns, const int rowCount) {
  const auto mineAt = [&](const int c, const int r) {
    if (c < 0 || c >= columns || r < 0 || r >= rowCount) return false;
    const char mark = rows[r][c];
    return mark == '*' || mark == 'F' || mark == 'M' || mark == 'm';
  };
  for (int r = 0; r < rowCount; ++r) {
    for (int c = 0; c < columns; ++c) {
      const fui::Rect box =
          fui::makeRect(static_cast<int16_t>(left + c * cell), static_cast<int16_t>(top + r * cell), cell, cell);
      const char mark = rows[r][c];
      if (mark != 'o' && mark != 'M' && mark != 'm')
        covered(screen, box);
      else
        screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), 2);
      if (mark == 'F') {
        drawFlag(screen, box, false);
      } else if (mark == 'M') {
        screen.target().fill(box, fui::Paint::solid(fui::Color::Black));
        drawMine(screen, box, true);
      } else if (mark == 'm') {
        drawMine(screen, box, false);
      } else if (mark == 'o') {
        int touching = 0;
        for (int dc = -1; dc <= 1; ++dc) {
          for (int dr = -1; dr <= 1; ++dr) {
            if ((dc != 0 || dr != 0) && mineAt(c + dc, r + dr)) ++touching;
          }
        }
        if (touching > 0) {
          char digit[2] = {static_cast<char>('0' + touching), '\0'};
          fui::TextStyle number;
          number.font = toybox::kDisplayFont;
          number.align = fui::TextAlign::Center;
          // A cell is 60px and the display cut's line box is 63, so the box
          // itself would sit the digit low against the cell's own frame.
          screen.target().text(toybox::inkCentred(screen.target(), box, number), digit, number);
        }
      }
    }
  }
}

// The bottom strip: what is left to find on the left, what a tap will do on
// the right.
//
// The counter is labelled. A bare numeral and a mark could not say what it
// counted, and with no total on screen the player could not recover the
// denominator -- so it names both, and the flag beside it says which unit.
//
// The mode is a control rather than a caption: rowStyles() so it is outlined
// while it is DIG and inverted once it is FLAG. The button default paints
// solid in every state, so the two modes came out identical black pills
// differing by one word, which is not a difference a glance carries on a
// one-bit panel.
void boardStrip(toybox::Screen& screen, const BoardModel& model) {
  const auto body = pixelui::geometry(screen.device()).body;
  const bool wide = body.width > body.height;
  const auto last = cellRect(screen.device(), model.game.columns - 1, 0, model.game.columns, model.game.rows);
  const auto area =
      wide ? fui::makeRect(last.right() + 20, body.y + 70, body.right() - last.right() - 20, body.height - 70)
           : fui::makeRect(body.x, body.bottom() - 164, body.width, 164);
  const auto counter = fui::makeRect(area.x, area.y, area.width, 54);
  dashboard::frame(screen, counter, pixelui::kBorder);
  drawMine(screen, fui::makeRect(counter.x + 12, counter.y + 9, 36, 36), false);
  char value[48];
  std::snprintf(value, sizeof(value), tr(STR_GUIDE_MINES_COUNTER), ms::minesRemaining(model.game), model.game.mines);
  dashboard::text(screen, fui::makeRect(counter.x + 62, counter.y, counter.width - 74, counter.height), value, 0,
                  fui::TextAlign::Left);
  dashboard::text(screen, fui::makeRect(area.x, area.y + 62, area.width, 28),
                  model.flagMode ? tr(STR_GUIDE_MINES_TAP_FLAG) : tr(STR_GUIDE_MINES_TAP_DIG), 3);
  for (int n = 0; n < 2; ++n) {
    const bool active = n == 0 ? model.flagMode : !model.flagMode;
    const int gap = chrome::lightHeader ? 12 : 0;
    const auto band = chrome::lightHeader ? fui::makeRect(area.x + n * ((area.width - gap) / 2 + gap), area.y + 100,
                                                          (area.width - gap) / 2, 64)
                                          : fui::makeRect(area.x, area.y + 100, area.width, 64);
    gamepanel::segment(screen, band, chrome::lightHeader ? 0 : n, chrome::lightHeader ? 1 : 2,
                       n == 0 ? tr(STR_UI_FLAG_AAABC0) : tr(STR_UI_DIG_109AF0), ActionToggleMode, n,
                       n == 0 ? gamepanel::Mark::Flag : gamepanel::Mark::Dig, active);
  }
}

}  // namespace

fui::Rect cellRect(const fui::DeviceContext& device, const int column, const int row, int columns, int rows) {
  const auto body = pixelui::geometry(device).body;
  const bool wide = body.width > body.height;
  const int top = body.y + (wide ? 4 : 62);
  const int cell = std::max(1, std::min((int(body.width) - (wide ? 260 : 0)) / columns,
                                        (int(body.bottom()) - top - (wide ? 8 : 190)) / rows));
  const int left = body.x + (wide ? 4 : (body.width - columns * cell) / 2);
  return fui::makeRect(left + column * cell, top + row * cell, cell, cell);
}
bool cellAt(const fui::DeviceContext& device, const int x, const int y, int& column, int& row, int columns, int rows) {
  const auto first = cellRect(device, 0, 0, columns, rows);
  if (first.width <= 0 || x < first.x || y < first.y || x >= first.x + columns * first.width ||
      y >= first.y + rows * first.height)
    return false;
  column = (x - first.x) / first.width;
  row = (y - first.y) / first.height;
  return true;
}

int howToPages() { return 5; }

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toyboxChrome(screen, tr(STR_UI_MINESWEEPER_A2F226));
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  if (model.settings) {
    dashboard::text(screen, screen.takeTop(32, 0), tr(STR_SETTINGS_TITLE), 0, fui::TextAlign::Left);
    dashboard::text(screen, screen.takeTop(48, 18), tr(STR_MINES_BOARD_SIZE), 1, fui::TextAlign::Left);
    dashboard::button(screen, screen.takeBottom(64, 0), tr(STR_GUIDE_SAVE), ActionSaveSize, 0, true, &extra_check_32,
                      pixelui::kBody);
    dashboard::text(screen, screen.takeBottom(34, 18), tr(STR_GUIDE_NEXT_GAME), 0);
    const auto area = screen.body();
    const int sizes[] = {10, 15, 20};
    const int gap = 16, w = wide ? (area.width - 2 * gap) / 3 : area.width,
              h = wide ? area.height : std::min(116, (int(area.height) - 2 * gap) / 3);
    for (int n = 0; n < 3; ++n) {
      const auto r = fui::makeRect(area.x + (wide ? n * (w + gap) : 0), area.y + (wide ? 0 : n * (h + gap)), w, h);
      dashboard::frame(screen, r, pixelui::kBorder);
      const int icon = wide ? 68 : 80;
      guideui::grid(screen, fui::makeRect(r.x + 16, r.y + 16, icon, icon), sizes[n]);
      char label[40];
      std::snprintf(label, sizeof(label), "%d x %d", sizes[n], sizes[n]);
      const auto name = wide ? fui::makeRect(r.x + 8, r.y + icon + 24, r.width - 16, 34)
                             : fui::makeRect(r.x + 112, r.y + 18, r.width - 178, 42);
      dashboard::text(screen, name, label, 1, fui::TextAlign::Left);
      std::snprintf(label, sizeof(label), tr(STR_GUIDE_CELLS), sizes[n] * sizes[n]);
      dashboard::text(screen,
                      wide ? fui::makeRect(r.x + 8, name.bottom() + 4, r.width - 16, 28)
                           : fui::makeRect(r.x + 112, r.y + 68, r.width - 178, 30),
                      label, 3, fui::TextAlign::Left);
      const auto check = fui::makeRect(r.right() - 46, r.y + 24, 28, 28);
      const bool selected = screen.frame().buttonSelected(ActionBoardSize, sizes[n], model.size == sizes[n]);
      if (chrome::lightHeader)
        dashboard::check(screen, check, selected);
      else {
        dashboard::frame(screen, check, pixelui::kBorder);
        if (selected) screen.target().fill(check, dashboard::ink);
      }
      screen.frame().hit(r, ActionBoardSize, sizes[n]);
    }
    return;
  }
  const auto left = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 10, body.height) : body;
  const int statsH = wide ? 88 : std::max(112, 64 + 2 * screen.target().lineHeight(0));
  const auto stats = fui::makeRect(left.x, left.y + 8, left.width, statsH);
  dashboard::frame(screen, stats, pixelui::kBorder);
  screen.target().fill(fui::makeRect(stats.x + stats.width / 2, stats.y + 10, 2, stats.height - 20), dashboard::ink);
  for (int n = 0; n < 2; ++n) {
    const auto box = fui::makeRect(stats.x + n * stats.width / 2, stats.y, stats.width / 2, stats.height);
    if (n)
      guideui::finishFlag(screen, fui::makeRect(box.x + box.width / 2 - 60, box.y + 16, 34, 34));
    else
      gamepanel::mark(screen, fui::makeRect(box.x + box.width / 2 - 60, box.y + 16, 34, 34), gamepanel::Mark::Play);
    char number[16];
    std::snprintf(number, sizeof(number), "%d", n ? model.wins : model.wins + model.losses);
    dashboard::digits(screen, fui::makeRect(box.x + box.width / 2 - 12, box.y + 14, 60, 40), number);
    char copy[64];
    pixelui::copyCaps(copy, sizeof(copy), n ? tr(STR_GUIDE_MINES_WINS) : tr(STR_GUIDE_MINES_PLAYED));
    fui::TextStyle style;
    style.font = wide ? 3 : 0;
    style.align = fui::TextAlign::Center;
    style.maxLines = 2;
    screen.target().text(fui::makeRect(box.x + 10, box.y + 60, box.width - 20, statsH - 64), copy, style);
  }
  const auto right =
      wide ? fui::makeRect(left.right() + 20, body.y, body.right() - left.right() - 20, body.height) : body;
  const int rowH = wide ? 64 : 66,
            start = wide ? right.y + (right.height - 3 * rowH - 20) / 2 : body.bottom() - 3 * rowH - 20;
  const char* labels[] = {tr(STR_WORDLE_PLAY), tr(STR_SETTINGS_TITLE), tr(STR_WORDLE_HELP)};
  const int values[] = {int(MenuRow::Play), int(MenuRow::Settings), int(MenuRow::HowTo)};
  const gamepanel::Mark marks[] = {gamepanel::Mark::Play, gamepanel::Mark::Settings, gamepanel::Mark::Help};
  for (int n = 0; n < 3; ++n)
    gamepanel::row(screen, fui::makeRect(right.x, start + n * (rowH + 10), right.width, rowH), labels[n], ActionMenuRow,
                   values[n], marks[n], n == 0);
  const int top = stats.bottom() + 16, end = wide ? left.bottom() - 30 : start - 38;
  const int cell = std::max(1, std::min(int(left.width) - 28, end - top) / 5), x = left.x + (left.width - 5 * cell) / 2,
            y = top + (end - top - 5 * cell) / 2;
  static constexpr const char* sample[] = {"ooo..", "ooo..", "omo..", ".....", "....."};
  lessonField(screen, x, y, cell, sample, 5, 5);
  dashboard::frame(screen, fui::makeRect(x, y, 5 * cell, 5 * cell), pixelui::kBorder);
  dashboard::text(screen, fui::makeRect(left.x, end + 4, left.width, 30), tr(STR_REDESIGN_FIND_MINES), 0);
}

void buildHowTo(toybox::Screen& screen, const HowToModel& model) {
  const int page = std::clamp(model.page, 0, 4);
  toyboxChrome(screen, tr(STR_UI_MINESWEEPER_A2F226));
  const bool wide = screen.body().width > screen.body().height;
  dashboard::text(screen, screen.takeTop(wide ? 32 : 48, 0), tr(STR_WORDLE_HELP), 1);
  static constexpr StrId titles[] = {StrId::STR_GUIDE_MINES_NUMBERS, StrId::STR_GUIDE_MINES_TOOLS,
                                     StrId::STR_GUIDE_MINES_FIRST, StrId::STR_GUIDE_MINES_WIN,
                                     StrId::STR_GUIDE_MINES_CHORD};
  dashboard::text(screen, screen.takeTop(wide ? 28 : 36, 12), I18N.get(titles[page]), 0);
  pageui::build(screen, screen.takeBottom(48, 12), page + 1, 5, ActionHowToPage, -1, ActionHowToPage, 1);
  const auto area = screen.body();
  static constexpr StrId copy[] = {StrId::STR_GUIDE_MINES_NUMBERS_BODY, StrId::STR_GUIDE_MINES_TAP_BODY,
                                   StrId::STR_GUIDE_MINES_FIRST_BODY, StrId::STR_GUIDE_MINES_WIN_BODY,
                                   StrId::STR_GUIDE_MINES_CHORD_BODY};
  static constexpr StrId foot[] = {StrId::STR_GUIDE_MINES_THREE, StrId::STR_GUIDE_MINES_TAP_BODY,
                                   StrId::STR_GUIDE_MINES_OBSERVE, StrId::STR_GUIDE_MINES_FLAGS,
                                   StrId::STR_GUIDE_MINES_WARNING};
  if (page == 1) {
    const int w = area.width, h = wide ? area.height / 3 : 108;
    static constexpr StrId names[] = {StrId::STR_GUIDE_MINES_TAP, StrId::STR_GUIDE_MINES_HOLD,
                                      StrId::STR_GUIDE_MINES_SWITCH};
    static constexpr StrId notes[] = {StrId::STR_GUIDE_MINES_TAP_BODY, StrId::STR_GUIDE_MINES_HOLD_BODY,
                                      StrId::STR_GUIDE_MINES_SWITCH_BODY};
    for (int n = 0; n < 3; ++n) {
      const auto r = fui::makeRect(area.x, area.y + n * h, w, h);
      const auto art = fui::makeRect(r.x + 4, r.y + 4, h - 12, h - 12);
      dashboard::frame(screen, art, 2);
      if (n < 2)
        guideui::touchHand(screen, art.inset(fui::Insets{4, 4, 4, 4}), n == 1);
      else {
        guideui::arrow(screen, fui::makeRect(art.x + 8, art.y + 8, art.width - 16, art.height / 2 - 8));
        screen.target().line({int16_t(art.right() - 12), int16_t(art.bottom() - 14)},
                             {int16_t(art.x + 12), int16_t(art.bottom() - 14)}, 4, dashboard::ink);
        screen.target().triangle({int16_t(art.x + 8), int16_t(art.bottom() - 14)},
                                 {int16_t(art.x + 20), int16_t(art.bottom() - 24)},
                                 {int16_t(art.x + 20), int16_t(art.bottom() - 4)}, dashboard::ink);
      }
      dashboard::text(screen, fui::makeRect(art.right() + 14, r.y + 4, wide ? 140 : r.right() - art.right() - 18, 28),
                      I18N.get(names[n]), wide ? 0 : 1, fui::TextAlign::Left);
      guideui::text(screen,
                    fui::makeRect(art.right() + (wide ? 166 : 14), r.y + (wide ? 0 : 34),
                                  r.right() - art.right() - (wide ? 170 : 18), wide ? h : h - 38),
                    I18N.get(notes[n]), 2, fui::TextAlign::Left, 4);
    }
    static constexpr const char* rows[] = {"oF."};
    const int cell = wide ? std::min(72, int(area.height)) : std::min(76, (int(area.height) - 260));
    const int x = wide ? area.x + area.width * 3 / 4 - 3 * cell / 2 : area.x + (area.width - 3 * cell) / 2,
              y = wide ? area.y + (area.height - cell) / 2 : area.bottom() - cell - 8;
    if (!wide && cell > 0) lessonField(screen, x, y, cell, rows, 3, 1);
    return;
  }
  const int descH = wide ? area.height : page == 4 ? 152 : 112;
  const auto description = wide ? fui::makeRect(area.x, area.y, area.width * 2 / 5 - 14, area.height)
                                : fui::makeRect(area.x, area.y, area.width, descH);
  guideui::text(screen, description, I18N.get(copy[page]), wide ? 7 : 4, fui::TextAlign::Center, 4);
  const auto art = wide ? fui::makeRect(area.x + area.width * 2 / 5, area.y, area.width * 3 / 5, area.height - 68)
                        : fui::makeRect(area.x, area.y + descH + 8, area.width, area.height - descH - 84);
  static constexpr const char* numbers[] = {"M..", ".oM", "..M"};
  static constexpr const char* first[] = {"oo...", "oo**.", "ooo..", "ooo*."};
  static constexpr const char* win[] = {"ooooo", "ooo*o", "ooooo", "ooFoo"};
  static constexpr const char* chord[] = {".F.", ".o.", "..."};
  const char* const* rows = page == 0 ? numbers : page == 2 ? first : page == 3 ? win : chord;
  const int cols = page == 2 || page == 3 ? 5 : 3, rowCount = cols == 5 ? 4 : 3;
  const int cell = std::max(1, std::min({84, int(art.width) / cols, int(art.height) / rowCount}));
  lessonField(screen, art.x + (art.width - cols * cell) / 2, art.y + (art.height - rowCount * cell) / 2, cell, rows,
              cols, rowCount);
  const auto notice = wide ? fui::makeRect(art.x, area.bottom() - 64, art.width, 64)
                           : fui::makeRect(area.x, area.bottom() - 64, area.width, 64);
  if (page >= 3) dashboard::frame(screen, notice, pixelui::kBorder);
  const char* caption = I18N.get(foot[page]);
  guideui::text(screen, notice.inset(fui::Insets{4, 6, 4, 6}), caption, 2, fui::TextAlign::Center, 4);
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  toyboxChrome(screen, tr(STR_UI_MINESWEEPER_A2F226));
  const auto body = screen.body();
  const bool wide = body.width > body.height, over = ms::over(model.game);
  const auto last = cellRect(screen.device(), model.game.columns - 1, 0, model.game.columns, model.game.rows);
  const auto info =
      wide ? fui::makeRect(last.right() + 20, body.y, body.right() - last.right() - 20, body.height) : body;
  const auto settings = fui::makeRect(info.right() - 48, info.y, 48, 48);
  dashboard::frame(screen, settings, pixelui::kBorder);
  gamepanel::mark(screen, settings.inset(fui::Insets{8, 8, 8, 8}), gamepanel::Mark::Settings);
  dashboard::hit(screen, settings, ActionSettings);
  const auto status = fui::makeRect(info.x, info.y, info.width - 60, 48);
  dashboard::frame(screen, status, pixelui::kBorder);
  dashboard::text(screen, status.inset(fui::Insets{0, 8, 0, 8}),
                  over ? tr(STR_UI_GAME_OVER_B1DE5C) : tr(STR_REDESIGN_IN_PROGRESS), 0, fui::TextAlign::Left);
  for (int x = 0; x < model.game.columns; ++x)
    for (int y = 0; y < model.game.rows; ++y) {
      const auto box = cellRect(screen.device(), x, y, model.game.columns, model.game.rows);
      const uint8_t value = model.game.cell[x][y];
      const bool revealed = value & ms::kRevealed;
      if (!revealed)
        covered(screen, box);
      else
        screen.target().stroke(box, dashboard::ink, 1);
      if (value & ms::kFlagged)
        drawFlag(screen, box, false);
      else if (revealed && (value & ms::kMine)) {
        screen.target().fill(box, dashboard::ink);
        drawMine(screen, box, true);
      } else if (model.showMines && (value & ms::kMine))
        drawMine(screen, box, false);
      else if (revealed) {
        const int count = ms::neighbouringMines(model.game, x, y);
        if (count) {
          char digit[2] = {char('0' + count), 0};
          const int pad = std::max(3, int(box.width) / 4);
          numerals::draw(screen.target(),
                         box.inset(fui::Insets{int16_t(pad), int16_t(pad), int16_t(pad), int16_t(pad)}), digit);
        }
      }
    }
  const auto firstCell = cellRect(screen.device(), 0, 0, model.game.columns, model.game.rows);
  dashboard::frame(
      screen,
      fui::makeRect(firstCell.x, firstCell.y, firstCell.width * model.game.columns, firstCell.height * model.game.rows),
      3);
  if (over) {
    const auto banner = wide ? fui::makeRect(info.x, info.y + 76, info.width, 62)
                             : fui::makeRect(body.x, body.bottom() - 188, body.width, 52);
    dashboard::frame(screen, banner, pixelui::kBorder);
    drawMine(screen, fui::makeRect(banner.x + 12, banner.y + 8, 36, 36), false);
    dashboard::text(screen, fui::makeRect(banner.x + 60, banner.y, banner.width - 68, banner.height),
                    model.game.status == ms::Status::Won ? tr(STR_UI_YOU_CLEARED_THE_FIELD_1C9789)
                                                         : tr(STR_UI_YOU_HIT_A_MINE_D1ABAC),
                    0);
    gamepanel::row(screen,
                   wide ? fui::makeRect(info.x, info.y + 158, info.width, 60)
                        : fui::makeRect(body.x, body.bottom() - 122, body.width, 54),
                   tr(STR_UI_NEW_GAME_F6F041), ActionAgain, 0, gamepanel::Mark::Restart, true, nullptr, false);
    gamepanel::row(screen,
                   wide ? fui::makeRect(info.x, info.y + 232, info.width, 60)
                        : fui::makeRect(body.x, body.bottom() - 56, body.width, 54),
                   tr(STR_UI_BACK_TO_MENU_398BBB), ActionDone, 0, gamepanel::Mark::Back, false, nullptr, false);
  } else
    boardStrip(screen, model);
}

void buildResult(toybox::Screen& screen, const ResultModel& model) {
  fui::HeaderProps header;
  header.title = model.won ? tr(STR_UI_CLEARED_D5EA61) : tr(STR_UI_BOOM_2DEB1B);
  header.borderEdges = fui::EdgesNone;
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});

  fui::ButtonProps done;
  if (chrome::lightHeader) done.styles = pixelui::buttonStyles();
  done.label = tr(STR_UI_DONE_E782E7);
  done.action = ActionDone;
  screen.button(done, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  fui::ButtonProps again;
  if (chrome::lightHeader) again.styles = pixelui::buttonStyles();
  again.label = tr(STR_WORDLE_PLAY_AGAIN);
  again.action = ActionAgain;
  screen.button(again, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  const fui::Rect area = screen.body();

  // The verdict as a sentence, before the stats: BOOM in the band names the
  // event, this names what it means for you.
  fui::TextStyle body;
  body.font = toybox::kBodyFont;
  body.align = fui::TextAlign::Center;
  body.maxLines = 2;
  screen.target().text(fui::makeRect(area.x, static_cast<int16_t>(area.y + 30), area.width, 60),
                       model.won ? tr(STR_UI_YOU_CLEARED_THE_FIELD_1C9789) : tr(STR_UI_YOU_HIT_A_MINE_D1ABAC), body);

  char line[48];
  std::snprintf(line, sizeof(line), tr(STR_UI_D_OF_D_CELLS_OPENED_A449D0), model.revealed, model.cells - model.mines);
  screen.target().text(fui::makeRect(area.x, static_cast<int16_t>(area.y + 110), area.width, 60), line, body);

  char flags[48];
  std::snprintf(flags, sizeof(flags), tr(STR_UI_D_OF_D_MINES_FLAGGED_9D890F), model.flagsRight, model.mines);
  screen.target().text(fui::makeRect(area.x, static_cast<int16_t>(area.y + 180), area.width, 60), flags, body);
}

}  // namespace mineui

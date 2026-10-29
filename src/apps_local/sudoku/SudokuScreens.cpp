#include "SudokuScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/CardPanels.h"
#include "../ui/FitText.h"
#include "../ui/PuzzleArt.h"
#include "SudokuLabels.h"

namespace sudokuui {
namespace {
namespace sk = sudoku;
constexpr auto black = dashboard::ink, white = dashboard::paper;
fui::Rect rect(int x, int y, int w, int h) { return fui::makeRect(x, y, w, h); }
fui::Rect begin(toybox::Screen& s, const char* clock = nullptr) {
  if (s.device().width > s.device().height) return cardpanel::begin(s, tr(STR_GAME_SUDOKU));
  pixelui::Header header;
  header.title = tr(STR_GAME_SUDOKU);
  header.back = true;
  header.trailingLabel = clock;
  if (clock) header.mark = fui::bitmapFromIcon(extra_clock_32);
  pixelui::header(s, header);
  pixelui::begin(s);
  return s.body();
}
void copy(toybox::Screen& s, fui::Rect r, const char* text, int font = 4, int lines = 3,
          fui::TextAlign align = fui::TextAlign::Center) {
  fittext::draw(s.target(), r, text, font, lines, align);
}
void title(toybox::Screen& s, fui::Rect r, const char* text, int font = 2) {
  pixelui::text(s.target(), r, text, font, fui::Color::Black, fui::TextAlign::Center);
}
void digit(toybox::Screen& s, fui::Rect r, int value, bool inverse = false, int font = 1) {
  char text[2] = {char('0' + value), 0};
  pixelui::text(s.target(), r, text, font, inverse ? fui::Color::White : fui::Color::Black, fui::TextAlign::Center);
}
struct Layout {
  fui::Rect grid, pad, tools, prompt, menu, level;
  int cell;
  bool wide;
};
Layout layout(const fui::DeviceContext& d) {
  const auto safe = d.safeRect();
  const bool wide = d.width > d.height;
  const int x = safe.x + 12, top = wide ? safe.y + 68 : pixelui::geometry(d).body.y, bottom = safe.bottom() - 8,
            w = safe.width - 24;
  Layout g{};
  g.wide = wide;
  if (wide) {
    g.cell = std::min((w * 49 / 100) / 9, (bottom - top - 30) / 9);
    g.grid = rect(x, top + 30, g.cell * 9, g.cell * 9);
    const int right = g.grid.right() + 18, rw = safe.right() - 12 - right;
    g.level = rect(x, top, g.grid.width, 26);
    g.pad = rect(right, top, rw, 150);
    g.tools = rect(right, top + 162, rw, 56);
    g.prompt = rect(right, top + 230, rw, bottom - top - 298);
    g.menu = rect(right, bottom - 58, rw, 58);
  } else {
    g.cell = std::min(w / 9, (bottom - top - 28 - 208) / 9);
    g.level = rect(x, top, w, 28);
    g.grid = rect(x + (w - g.cell * 9) / 2, top + 30, g.cell * 9, g.cell * 9);
    g.pad = rect(x, g.grid.bottom() + 12, w, 48);
    g.tools = rect(x, g.pad.bottom() + 12, w, 52);
    g.menu = rect(x, bottom - 52, w, 52);
    g.prompt = rect(x, g.tools.bottom() + 6, w, g.menu.y - g.tools.bottom() - 12);
  }
  return g;
}
void brackets(toybox::Screen& s, fui::Rect r, bool inverse = false) {
  const int arm = std::max(5, r.width / 5);
  const auto ink = inverse ? white : black;
  r = r.inset({3, 3, 3, 3});
  for (int n = 0; n < 4; ++n) {
    const int x = n % 2 ? r.right() - arm : r.x, y = n / 2 ? r.bottom() - 3 : r.y;
    s.target().fill(rect(x, y, arm, 3), ink);
    s.target().fill(rect(n % 2 ? r.right() - 3 : r.x, n / 2 ? r.bottom() - arm : r.y, 3, arm), ink);
  }
}
void grid(toybox::Screen& s, fui::Rect area, const sk::Game& game, int selected = sk::kNoCell, int held = sk::kNoCell,
          bool active = false, sk::EntryTool tool = sk::EntryTool::Digit, bool showMatches = true) {
  const int cell = std::min(int(area.width), int(area.height)) / 9;
  const int x = area.x + (area.width - cell * 9) / 2, y = area.y + (area.height - cell * 9) / 2;
  for (int n = 0; n < 81; ++n) {
    const auto r = rect(x + n % 9 * cell, y + n / 9 * cell, cell, cell);
    const int value = sk::valueAt(game, n);
    const bool inverse = (tool == sk::EntryTool::Erase && value != 0 && !sk::isGiven(game, n)) ||
                         (tool == sk::EntryTool::Digit && n == selected && value != 0);
    if (inverse) s.target().fill(r, black);
    const auto checked = tool == sk::EntryTool::Check ? sk::checkEntry(game, n) : sk::CellCheck::None;
    const bool feedback = checked != sk::CellCheck::None;
    const int markSize = std::clamp(cell / 3, 10, 18);
    const auto number = feedback ? rect(r.x + 1, r.y, cell * 3 / 5, cell) : r;
    if (value) digit(s, number, value, inverse, cell >= 34 ? 1 : 0);
    if (feedback) {
      const int mx = r.right() - markSize - 5, my = r.y + 7;
      if (checked == sk::CellCheck::Correct) {
        s.target().line({int16_t(mx), int16_t(my + markSize / 2)}, {int16_t(mx + markSize / 3), int16_t(my + markSize)},
                        3, black);
        s.target().line({int16_t(mx + markSize / 3), int16_t(my + markSize)}, {int16_t(mx + markSize), int16_t(my)}, 3,
                        black);
      } else {
        s.target().line({int16_t(mx), int16_t(my)}, {int16_t(mx + markSize), int16_t(my + markSize)}, 3, black);
        s.target().line({int16_t(mx + markSize), int16_t(my)}, {int16_t(mx), int16_t(my + markSize)}, 3, black);
      }
    }
    if (tool == sk::EntryTool::Digit && (n == held || (active && showMatches && value == game.armed && n != selected)))
      brackets(s, r, inverse);
  }
  for (int n = 0; n <= 9; ++n) {
    const int weight = n % 3 == 0 ? 4 : 1;
    s.target().fill(rect(x + n * cell - (n == 9 ? weight : 0), y, weight, cell * 9), black);
    s.target().fill(rect(x, y + n * cell - (n == 9 ? weight : 0), cell * 9, weight), black);
  }
}
constexpr sk::Game sample() {
  sk::Game game{};
  constexpr char clues[] = "530070000600195000098000060800060003400803001700020006060000280000419005000080079";
  for (int n = 0; n < 81; ++n) game.puzzle.given[n] = clues[n] - '0';
  return game;
}
void miniature(toybox::Screen& s, fui::Rect r, const sk::Game& game, bool hasGame) {
  if (hasGame) {
    grid(s, r, game);
    return;
  }
  static constexpr sk::Game preview = sample();
  grid(s, r, preview);
}
void littleGrid(toybox::Screen& s, fui::Rect r, const char* values, int selected = -1) {
  const int cell = std::min(int(r.width), int(r.height)) / 3, x = r.x + (r.width - cell * 3) / 2,
            y = r.y + (r.height - cell * 3) / 2;
  for (int n = 0; n < 9; ++n) {
    auto b = rect(x + n % 3 * cell, y + n / 3 * cell, cell, cell);
    if (n == selected) s.target().fill(b, black);
    if (values && values[n] >= '1' && values[n] <= '9')
      digit(s, b, values[n] - '0', n == selected, cell > 50 ? 2 : cell >= 34 ? 1 : 0);
    dashboard::frame(s, b, 2);
  }
  dashboard::frame(s, rect(x, y, cell * 3, cell * 3), pixelui::kBorder);
}
void arrow(toybox::Screen& s, fui::Rect r) {
  const int x = r.x + r.width / 2, y = r.y + r.height / 2;
  s.target().line({int16_t(x), int16_t(r.y + 2)}, {int16_t(x), int16_t(r.bottom() - 8)}, 4, black);
  s.target().triangle({int16_t(x - 10), int16_t(y)}, {int16_t(x + 10), int16_t(y)},
                      {int16_t(x), int16_t(r.bottom() - 1)}, black);
}
void action(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId id, gamepanel::Mark mark,
            bool primary = false, int value = 0) {
  gamepanel::row(s, r, label, id, value, mark, primary);
}
}  // namespace
fui::Rect cellRect(const fui::DeviceContext& d, int n) {
  const auto g = layout(d);
  return rect(g.grid.x + n % 9 * g.cell, g.grid.y + n / 9 * g.cell, g.cell, g.cell);
}
bool cellAt(const fui::DeviceContext& d, int x, int y, int& n) {
  const auto g = layout(d);
  if (x < g.grid.x || x >= g.grid.right() || y < g.grid.y || y >= g.grid.bottom()) return false;
  n = (y - g.grid.y) / g.cell * 9 + (x - g.grid.x) / g.cell;
  return true;
}
fui::Rect padKeyRect(const fui::DeviceContext& d, int value) {
  const auto g = layout(d);
  const int n = value - 1;
  if (g.wide) {
    const int w = (g.pad.width - 12) / 3, h = (g.pad.height - 12) / 3;
    return rect(g.pad.x + n % 3 * (w + 6), g.pad.y + n / 3 * (h + 6), w, h);
  }
  const int w = (g.pad.width - 8 * 4) / 9;
  return rect(g.pad.x + n * (w + 4), g.pad.y, w, g.pad.height);
}
bool padKeyAt(const fui::DeviceContext& d, int x, int y, int& value) {
  for (int n = 1; n <= 9; ++n) {
    const auto r = padKeyRect(d, n);
    if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom()) {
      value = n;
      return true;
    }
  }
  return false;
}
void formatClock(uint32_t ms, char* out, int size) {
  const auto seconds = ms / 1000, hours = seconds / 3600;
  if (hours)
    std::snprintf(out, size, "%u:%02u:%02u", hours, (seconds / 60) % 60, seconds % 60);
  else
    std::snprintf(out, size, "%02u:%02u", seconds / 60, seconds % 60);
}
int howToPages() { return 4; }
void buildMenu(toybox::Screen& s, const MenuModel& m) {
  auto a = begin(s);
  const bool wide = s.device().width > s.device().height;
  auto left = wide ? rect(a.x, a.y, a.width / 2 - 10, a.height) : a;
  const auto stats = rect(left.x, left.y, left.width, 82);
  dashboard::frame(s, stats);
  s.target().line({int16_t(stats.x + stats.width / 2), int16_t(stats.y)},
                  {int16_t(stats.x + stats.width / 2), int16_t(stats.bottom())}, 2, black);
  for (int n = 0; n < 2; ++n) {
    auto b = rect(stats.x + n * stats.width / 2, stats.y, stats.width / 2, stats.height);
    if (n)
      puzzleart::draw(s, rect(b.x + 14, b.y + 20, 38, 44), puzzleart::Stopwatch);
    else
      dashboard::icon(s, rect(b.x + 14, b.y + 20, 38, 44), extra_trophy_32);
    title(s, rect(b.x + 62, b.y + 8, b.width - 70, 28), n ? tr(STR_SDK_BEST_TIME) : tr(STR_SDK_SOLVED_COUNT), 0);
    char value[20];
    if (n) {
      const auto best = m.record.bestMs[static_cast<int>(m.level)];
      if (best)
        formatClock(best, value, sizeof(value));
      else
        std::snprintf(value, sizeof(value), "--");
    } else
      std::snprintf(value, sizeof(value), "%d", sk::totalSolved(m.record));
    dashboard::digits(s, rect(b.x + 62, b.y + 38, b.width - 70, 34), value);
  }
  const int foot = wide ? 0 : 294;
  miniature(s, rect(left.x + 12, left.y + 100, left.width - 24, left.height - 100 - foot), m.game, m.hasGame);
  auto controls = wide ? rect(a.x + a.width / 2 + 10, a.y, a.width / 2 - 10, a.height)
                       : rect(a.x, a.bottom() - foot, a.width, foot);
  const bool resume = sk::canResume(m.game, m.hasGame, m.level);
  const int count = resume ? 4 : 3, gap = 8;
  const int h = std::min(62, (controls.height - 34 - gap * (count - 1)) / count);
  int y = wide ? gamepanel::centeredBlock(controls, count * h + gap * (count - 1)).y
               : controls.bottom() - count * h - gap * (count - 1);
  if (resume) {
    action(s, rect(controls.x, y, controls.width, h), tr(STR_UI_RESUME_A26A6E), ActionPlay, gamepanel::Mark::Play,
           true);
    y += h + gap;
  }
  action(s, rect(controls.x, y, controls.width, h), tr(STR_SDK_NEW), ActionNew, gamepanel::Mark::Restart, !resume);
  y += h + gap;
  gamepanel::row(s, rect(controls.x, y, controls.width, h), tr(STR_MATCH_SETTINGS), ActionMenuRow,
                 int(MenuRow::Settings), gamepanel::Mark::Settings, false, sudokulabels::levelName(m.level));
  y += h + gap;
  action(s, rect(controls.x, y, controls.width, h), tr(STR_WORDLE_HELP), ActionMenuRow, gamepanel::Mark::Help, false,
         int(MenuRow::HowTo));
}
void buildConfirmNew(toybox::Screen& s) {
  const auto b = begin(s);
  title(s, rect(b.x, b.y + 24, b.width, 56), tr(STR_SDK_NEW), 1);
  copy(s, rect(b.x + 12, b.y + 94, b.width - 24, 86), tr(STR_SDK_REPLACE_GAME), 4, 3);
  action(s, rect(b.x, b.bottom() - 126, b.width, 58), tr(STR_SDK_NEW), ActionAgain, gamepanel::Mark::Restart, true);
  action(s, rect(b.x, b.bottom() - 58, b.width, 58), tr(STR_WORDLE_CANCEL), ActionMenu, gamepanel::Mark::Back);
}
void buildSettings(toybox::Screen& s, const SettingsModel& m) {
  const auto a = begin(s);
  const bool wide = s.device().width > s.device().height;
  title(s, rect(a.x, a.y, a.width, 52), tr(STR_MATCH_SETTINGS), 2);
  const int footer = wide ? a.bottom() - 52 : a.bottom() - 110;
  const auto room = rect(a.x, a.y + 72, a.width, footer - a.y - 116);
  constexpr int columns = 2;
  const int rows = sk::kLevelCount / columns;
  const auto levels = wide ? rect(room.x, room.y, room.width * 42 / 100, room.height)
                           : rect(room.x, room.y, room.width, 36 + rows * 62 + (rows - 1) * 8);
  title(s, rect(levels.x, levels.y, levels.width, 28), tr(STR_UI_DIFFICULTY_9106E9), 1);
  const int cellW = (levels.width - (columns - 1) * 8) / columns;
  const int cellH = (levels.height - 36 - (rows - 1) * 8) / rows;
  for (int n = 0; n < sk::kLevelCount; ++n) {
    const auto r = rect(levels.x + n % columns * (cellW + 8), levels.y + 36 + n / columns * (cellH + 8), cellW, cellH);
    const bool selected = s.frame().buttonSelected(ActionPickLevel, n, n == int(m.level));
    if (selected) s.target().fill(r, black);
    dashboard::frame(s, r);
    pixelui::text(s.target(), r.inset({4, 4, 4, 4}), sudokulabels::levelName(static_cast<sk::Level>(n)), 1,
                  selected ? fui::Color::White : fui::Color::Black, fui::TextAlign::Center);
    dashboard::hit(s, r, ActionPickLevel, n);
  }
  const auto help = wide ? rect(levels.right() + 18, room.y, room.right() - levels.right() - 18, room.height)
                         : rect(room.x, levels.bottom() + 16, room.width, room.bottom() - levels.bottom() - 16);
  dashboard::frame(s, help);
  dashboard::hit(s, help, ActionToggleMatches);
  pixelui::text(s.target(), rect(help.x + 12, help.y + 10, help.width - 68, 32), tr(STR_SDK_MATCHES), 0);
  dashboard::check(s, rect(help.right() - 44, help.y + 11, 30, 30), m.showMatches);
  copy(s, rect(help.x + 12, help.y + 50, help.width - 24, std::min(60, int(help.height) - 58)),
       tr(STR_SDK_MATCHES_COPY), 4, help.height < 116 ? 1 : 2);
  if (help.height >= 194) {
    const auto art = rect(help.x + 12, help.y + 116, help.width - 24, help.height - 128);
    littleGrid(s, art, "1.3.5...9");
    if (m.showMatches) {
      const int cell = std::min(int(art.width), int(art.height)) / 3;
      brackets(s, rect(art.x + (art.width - cell * 3) / 2, art.y + (art.height - cell * 3) / 2, cell, cell));
    }
  }
  if (m.saveFailed) copy(s, rect(a.x, footer - 36, a.width, 32), tr(STR_SDK_SETTINGS_SAVE_FAILED), 4, 1);
}
void buildBoard(toybox::Screen& s, const BoardModel& m) {
  char clock[20];
  formatClock(m.game.elapsedMs, clock, sizeof(clock));
  begin(s, clock);
  const auto g = layout(s.device());
  pixelui::text(s.target(), g.level,
                m.generating ? tr(STR_SDK_GENERATING) : sudokulabels::levelName(m.game.puzzle.level), 0);
  if (!m.generating) {
    grid(s, g.grid, m.game, m.selectedCell, m.holdCell, true, m.entryTool, m.showMatches);
  } else {
    dashboard::frame(s, g.grid, pixelui::kBorder);
    copy(s, g.grid, tr(STR_SDK_GENERATING), 4, 2);
  }
  for (int n = 1; n <= 9; ++n) {
    const auto r = padKeyRect(s.device(), n);
    if (!m.generating) s.frame().hit(r, ActionDigit, n);
    const bool selected =
        s.frame().buttonSelected(ActionDigit, n, m.entryTool == sk::EntryTool::Digit && n == m.game.armed);
    if (selected)
      s.target().fill(r, black);
    else
      dashboard::frame(s, r);
    digit(s, r, n, selected, 1);
  }
  const int gap = 8, w = (g.tools.width - gap) / 2;
  const char* labels[] = {tr(STR_SDK_ERASE), tr(STR_SDK_HINT)};
  const fui::ActionId actions[] = {ActionErase, ActionHint};
  for (int n = 0; n < 2; ++n) {
    const auto r = rect(g.tools.x + n * (w + gap), g.tools.y, w, g.tools.height);
    const bool selected =
        s.frame().buttonSelected(actions[n], 0, m.entryTool == (n ? sk::EntryTool::Check : sk::EntryTool::Erase));
    if (selected)
      s.target().fill(r, black);
    else
      dashboard::frame(s, r);
    dashboard::hit(s, r, actions[n]);
    s.target().bitmap(rect(r.x + 10, r.y + (r.height - 28) / 2, 28, 28), n ? puzzleart::Hint : puzzleart::Eraser,
                      fui::BitmapMode::Contain, selected ? white : black);
    pixelui::text(s.target(), rect(r.x + 44, r.y, r.width - 50, r.height), labels[n], 0,
                  selected ? fui::Color::White : fui::Color::Black, fui::TextAlign::Center);
  }
  copy(s, g.prompt,
       m.generating                          ? tr(STR_SDK_GENERATING)
       : m.entryTool == sk::EntryTool::Erase ? tr(STR_SDK_ERASE_ACTIVE)
       : m.entryTool == sk::EntryTool::Check ? tr(STR_SDK_CHECK_ACTIVE)
       : m.notice                            ? m.notice
       : m.game.solvedFlag                   ? tr(STR_SDK_SOLVED_COPY)
                                             : tr(STR_SDK_PLACE_COPY),
       4, 2);
  const bool finished = !m.generating && m.game.solvedFlag != 0;
  action(s, g.menu, finished ? tr(STR_SDK_SEE_RESULT) : tr(STR_C4_MENU), finished ? ActionSeeResult : ActionMenu,
         gamepanel::Mark::Menu, true);
}
void buildResult(toybox::Screen& s, const ResultModel& m) {
  auto a = begin(s);
  const bool wide = s.device().width > s.device().height;
  auto left = wide ? rect(a.x, a.y, a.width * 56 / 100, a.height) : a;
  puzzleart::draw(s, rect(left.x, left.y, left.width, wide ? 70 : 94), puzzleart::SudokuTrophy);
  title(s, rect(left.x, left.y + (wide ? 76 : 108), left.width, 52), tr(STR_SDK_COMPLETED), 2);
  copy(s, rect(left.x, left.y + (wide ? 134 : 168), left.width, 42), tr(STR_SDK_SOLVED_COPY), 4, 2);
  const auto stats =
      wide ? rect(left.x, left.y + 190, left.width, left.height - 190) : rect(left.x, left.y + 226, left.width, 258);
  dashboard::frame(s, stats, pixelui::kBorder);
  const int split = stats.x + stats.width / 2, h = stats.height * 65 / 100;
  guideui::grid(s, rect(stats.x + 20, stats.y + 20, stats.width / 2 - 40, h - 38), 3);
  dashboard::icon(s, rect(stats.x + stats.width / 4 - 18, stats.y + h / 2 - 18, 36, 36), extra_check_32);
  s.target().line({int16_t(split), int16_t(stats.y + 16)}, {int16_t(split), int16_t(stats.y + h - 16)}, 2, black);
  title(s, rect(split + 8, stats.y + 24, stats.width / 2 - 16, 32), tr(STR_UI_DIFFICULTY_9106E9), 0);
  dashboard::frame(s, rect(split + 16, stats.y + 66, stats.width / 2 - 32, 58));
  title(s, rect(split + 18, stats.y + 66, stats.width / 2 - 36, 58), sudokulabels::levelName(m.level), 1);
  s.target().line({int16_t(stats.x), int16_t(stats.y + h)}, {int16_t(stats.right()), int16_t(stats.y + h)}, 2, black);
  for (int n = 0; n < 2; ++n) {
    auto r = rect(stats.x + n * stats.width / 2, stats.y + h, stats.width / 2, stats.height - h);
    dashboard::icon(s, rect(r.x + 12, r.y + 18, 36, 36), n ? extra_trophy_32 : extra_clock_32);
    copy(s, rect(r.x + 56, r.y + 6, r.width - 64, 28), n ? tr(STR_SDK_SOLVED_COUNT) : tr(STR_SDK_TIME), 4, 1);
    char value[20];
    if (n)
      std::snprintf(value, sizeof(value), "%d", m.solvedTotal);
    else
      formatClock(m.elapsedMs, value, sizeof(value));
    dashboard::digits(s, rect(r.x + 56, r.y + 38, r.width - 64, r.height - 44), value);
  }
  auto controls = wide ? rect(a.x + a.width * 59 / 100, a.y, a.width * 41 / 100, a.height)
                       : rect(a.x, a.bottom() - 194, a.width, 194);
  action(s, rect(controls.x, controls.y, controls.width, 58), tr(STR_SDK_NEW), ActionAgain, gamepanel::Mark::Play,
         true);
  action(s, rect(controls.x, controls.y + 68, controls.width, 58), tr(STR_C4_VIEW_BOARD), ActionDone,
         gamepanel::Mark::Grid);
  action(s, rect(controls.x, controls.y + 136, controls.width, 58), tr(STR_C4_BACK_MENU), ActionMenu,
         gamepanel::Mark::Menu);
}
void buildHowTo(toybox::Screen& s, const HowToModel& m) {
  auto a = begin(s);
  const bool wide = s.device().width > s.device().height;
  const int page = std::clamp(m.page, 0, 3);
  pageui::build(s, rect(a.x, a.bottom() - 48, a.width, 48), page + 1, 4, ActionHowToPrevious, 0, ActionHowToNext, 0);
  a.height -= 60;
  title(s, rect(a.x, a.y, a.width, 28), tr(STR_WORDLE_HELP), 0);
  static constexpr StrId headings[] = {StrId::STR_SDK_RULE, StrId::STR_SDK_PLACE, StrId::STR_SDK_ERASE_TITLE,
                                       StrId::STR_SDK_CONFLICTS};
  title(s, rect(a.x, a.y + 34, a.width, 52), I18N.get(headings[page]), 2);
  auto room = rect(a.x, a.y + 96, a.width, a.height - 96);
  if (page == 0) {
    copy(s, rect(room.x, room.y, room.width, wide ? 48 : 72), tr(STR_SDK_RULE_COPY), 4, 3);
    const int gap = 10, top = room.y + (wide ? 56 : 82), h = (room.bottom() - top - 2 * gap) / 3;
    static constexpr StrId names[] = {StrId::STR_SDK_ROW, StrId::STR_SDK_COLUMN, StrId::STR_SDK_BOX};
    static constexpr StrId details[] = {StrId::STR_SDK_ROW_COPY, StrId::STR_SDK_COLUMN_COPY, StrId::STR_SDK_BOX_COPY};
    for (int n = 0; n < 3; ++n) {
      auto r = wide ? rect(room.x + n * (room.width / 3 + 3), top, room.width / 3 - 6, room.bottom() - top)
                    : rect(room.x, top + n * (h + gap), room.width, h);
      dashboard::frame(s, r);
      pixelui::text(s.target(), rect(r.x + 10, r.y + 4, r.width - 20, 32), I18N.get(names[n]), 1);
      auto diagram = n == 0 ? rect(r.x + 14, r.bottom() - 40, r.width - 28, 30)
                            : rect(r.x + r.width * 62 / 100, r.y + 38, r.width * 34 / 100, r.height - 46);
      copy(s,
           rect(r.x + 12, r.y + 38, n == 0 ? r.width - 24 : r.width * 56 / 100, n == 0 ? r.height - 84 : r.height - 46),
           I18N.get(details[n]), 4, wide ? 7 : 4, fui::TextAlign::Left);
      if (n == 2)
        littleGrid(s, diagram, "123456789");
      else {
        const int count = n == 0 ? 9 : 5, w = n == 0 ? diagram.width / 9 : std::min(42, int(diagram.width)),
                  hh = n == 0 ? diagram.height : diagram.height / 5;
        const int x = n == 0 ? diagram.x : diagram.x + (diagram.width - w) / 2;
        for (int k = 0; k < count; ++k) {
          auto b = rect(x + (n == 0 ? k * w : 0), diagram.y + (n == 0 ? 0 : k * hh), w, hh);
          dashboard::frame(s, b, 2);
          const int font = hh >= s.target().lineHeight(0) ? 0 : 3;
          if (n == 1 && k == 3)
            title(s, b, "...", font);
          else
            digit(s, b, n == 0 ? k + 1 : k == 4 ? 9 : k + 1, false, font);
        }
      }
    }
  } else if (page == 1) {
    copy(s, rect(room.x, room.y, room.width, 48), tr(STR_SDK_PLACE_COPY), 4, 2);
    const int keyW = (room.width - 8 * 4) / 9;
    for (int n = 0; n < 9; ++n) {
      auto r = rect(room.x + n * (keyW + 4), room.y + 58, keyW, 44);
      dashboard::frame(s, r);
      if (n == 4) s.target().fill(r, black);
      digit(s, r, n + 1, n == 4, 1);
    }
    auto lower = rect(room.x, room.y + 110, room.width, room.height - 110);
    const int bottom = wide ? 0 : 140;
    if (!wide) arrow(s, rect(lower.x, lower.y, lower.width, 28));
    littleGrid(s,
               wide ? rect(lower.x, lower.y, lower.width / 2, lower.height)
                    : rect(lower.x, lower.y + 32, lower.width, lower.height - bottom - 32),
               "1.3.5...9", 4);
    auto detail = wide ? rect(lower.x + lower.width / 2 + 10, lower.y, lower.width / 2 - 10, lower.height)
                       : rect(lower.x, lower.bottom() - bottom, lower.width, bottom);
    title(s, rect(detail.x, detail.y, detail.width, 40), tr(STR_SDK_QUICK), 1);
    copy(s, rect(detail.x, detail.y + 44, detail.width, detail.height - 44), tr(STR_SDK_QUICK_COPY), 4, 4);
  } else if (page == 2) {
    copy(s, rect(room.x, room.y, room.width, 58), tr(STR_SDK_ERASE_COPY), 4, 2);
    auto main = rect(room.x, room.y + 70, room.width, room.height - 150);
    const int gap = 32, side = wide ? std::min(int(main.height), main.width / 2 - 30) : (main.height - gap) / 2;
    littleGrid(s, rect(main.x, main.y, wide ? main.width / 2 - 20 : main.width, side), "1.3.5...9", 4);
    if (!wide) arrow(s, rect(main.x, main.y + side, main.width, gap));
    littleGrid(s,
               rect(wide ? main.x + main.width / 2 + 20 : main.x, wide ? main.y : main.y + side + gap,
                    wide ? main.width / 2 - 20 : main.width, side),
               "1.3.....9");
    auto label = rect(room.x, room.bottom() - 76, room.width, 44);
    puzzleart::draw(s, rect(label.x + label.width / 4 - 40, label.y, 36, 36), puzzleart::Eraser);
    title(s, label, tr(STR_SDK_ERASE), 1);
    copy(s, rect(room.x, room.bottom() - 28, room.width, 28), tr(STR_SDK_GIVENS), 4, 1);
  } else {
    const int gap = 10, bottom = wide ? 46 : 48, h = (room.height - bottom - gap) / 2;
    for (int n = 0; n < 2; ++n) {
      auto r = wide ? rect(room.x + n * (room.width / 2 + 5), room.y, room.width / 2 - 5, room.height - bottom)
                    : rect(room.x, room.y + n * (h + gap), room.width, h);
      dashboard::frame(s, r);
      title(s, rect(r.x + 8, r.y + 4, r.width - 16, 32), n ? tr(STR_SDK_HINT) : tr(STR_SDK_CONFLICT_NUMBERS), 1);
      copy(s, rect(r.x + 12, r.y + 40, r.width - 24, 60), n ? tr(STR_SDK_HINT_COPY) : tr(STR_SDK_CONFLICT_COPY), 4, 3);
      auto art = rect(r.x + 12, r.y + 106, r.width - 24, r.height - 114);
      littleGrid(s, art, "134565789");
      if (n) {
        const int cell = std::min(int(art.width), int(art.height)) / 3;
        const int x = art.x + (art.width - cell * 3) / 2, y = art.y + (art.height - cell * 3) / 2;
        s.target().line({int16_t(x + 4), int16_t(y + 8)}, {int16_t(x + 8), int16_t(y + 13)}, 2, black);
        s.target().line({int16_t(x + 8), int16_t(y + 13)}, {int16_t(x + 16), int16_t(y + 3)}, 2, black);
        s.target().line({int16_t(x + cell + 4), int16_t(y + 4)}, {int16_t(x + cell + 14), int16_t(y + 14)}, 2, black);
        s.target().line({int16_t(x + cell + 14), int16_t(y + 4)}, {int16_t(x + cell + 4), int16_t(y + 14)}, 2, black);
      }
    }
  }
}
}  // namespace sudokuui

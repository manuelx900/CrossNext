#pragma once
#include <I18n.h>

#include "GameFrontDoor.h"
namespace matchsettings {
namespace fui = freeink::ui;
enum : fui::ActionId { Opponent = 600, Difficulty, Colour, Done };
enum class Game : uint8_t { ConnectFour, Othello };
struct Model {
  Game game = Game::ConnectFour;
  bool local = false, black = false;
  int level = 1;
};

inline void disc(toybox::Screen& s, fui::Rect r, bool black, bool inverse = false) {
  const auto ink = inverse ? dashboard::paper : dashboard::ink;
  const auto fill = black ? dashboard::ink : dashboard::paper;
  s.target().fill(r, fill, r.width / 2);
  s.target().stroke(r, ink, std::max(2, int(r.width) / 12), r.width / 2);
}

inline void preview(toybox::Screen& s, fui::Rect r, Game game) {
  const auto ink = dashboard::ink;
  const int cols = game == Game::ConnectFour ? 5 : 6, rows = game == Game::ConnectFour ? 4 : 5;
  const int cell = std::min((r.width - 12) / cols, (r.height - 20) / rows);
  const auto board = fui::makeRect(r.x + (r.width - cols * cell - 12) / 2, r.y, cols * cell + 12, rows * cell + 12);
  s.target().stroke(board, ink, 6);
  for (int side = 0; side < 2; ++side)
    s.target().fill(fui::makeRect(board.x + side * (board.width - 5) - 4, board.bottom(), 12, 9), ink);
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const auto square = fui::makeRect(board.x + 6 + col * cell, board.y + 6 + row * cell, cell, cell);
      if (game == Game::ConnectFour) {
        const bool filled = (col == 2 && row == 1) || (col == 3 && row > 1) || (col == 1 && row == 3);
        disc(s, square.inset({4, 4, 4, 4}), filled);
      } else {
        s.target().stroke(square, ink, 1);
        if ((col == 2 || col == 3) && (row == 1 || row == 2)) disc(s, square.inset({2, 2, 2, 2}), col != row + 1);
      }
    }
  }
}

inline void rivalIcon(toybox::Screen& s, fui::Rect r, bool person, bool inverse) {
  const auto ink = inverse ? dashboard::paper : dashboard::ink;
  const int unit = std::max(1, int(r.width) / 10);
  if (person) {
    s.target().fill(fui::makeRect(r.x + 3 * unit, r.y, 4 * unit, 4 * unit), ink, 2 * unit);
    s.target().fill(fui::makeRect(r.x + unit, r.y + 5 * unit, 8 * unit, 5 * unit), ink, 2 * unit);
  } else {
    s.target().stroke(r.inset({int16_t(2 * unit), int16_t(2 * unit), int16_t(2 * unit), int16_t(2 * unit)}), ink, unit);
    for (int n = 0; n < 4; ++n) {
      const int pos = (3 + n) * unit;
      s.target().line({int16_t(r.x + pos), r.y}, {int16_t(r.x + pos), int16_t(r.y + 2 * unit)}, unit, ink);
      s.target().line({int16_t(r.x + pos), int16_t(r.bottom() - 2 * unit)}, {int16_t(r.x + pos), r.bottom()}, unit,
                      ink);
      s.target().line({r.x, int16_t(r.y + pos)}, {int16_t(r.x + 2 * unit), int16_t(r.y + pos)}, unit, ink);
      s.target().line({int16_t(r.right() - 2 * unit), int16_t(r.y + pos)}, {r.right(), int16_t(r.y + pos)}, unit, ink);
    }
  }
}

inline void choice(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, int value, bool chosen,
                   bool icon = false, int index = 0, int count = 1, bool enabled = true) {
  const auto band = r;
  r = lightui::segmentRect(band, index, count);
  chosen = enabled && s.frame().buttonSelected(action, value, chosen);
  if (chrome::lightHeader)
    lightui::segment(s.target(), band, index, count, chosen);
  else {
    if (chosen) s.target().fill(r, dashboard::ink);
    dashboard::frame(s, r, pixelui::kBorder);
  }
  if (enabled) dashboard::hit(s, r, action, value);
  if (icon) {
    const int side = std::min(32, int(r.height) - 16);
    const auto image = fui::makeRect(r.x + 24, r.y + (r.height - side) / 2, side, side);
    if (action == Colour)
      disc(s, image, value == 1, chosen);
    else
      rivalIcon(s, image, value == 1, chosen);
    r.x += side + 36;
    r.width -= side + 48;
  }
  dashboard::text(s, r, label, 0, fui::TextAlign::Center, chosen);
}

inline void build(toybox::Screen& s, const Model& m) {
  frontdoor::begin(s, tr(STR_SETTINGS_TITLE));
  const auto b = s.body();
  const bool wide = b.width > b.height;
  const int leftWidth = wide ? b.width * 34 / 100 : b.width;
  const auto art = fui::makeRect(b.x + 4, b.y + 4, 120, 96);
  preview(s, art, m.game);
  const auto name = wide ? fui::makeRect(b.x, art.bottom() + 12, leftWidth, 54)
                         : fui::makeRect(art.right() + 14, b.y + 4, b.width - art.width - 18, 64);
  pixelui::text(s.target(), name, m.game == Game::ConnectFour ? tr(STR_MATCH_FOUR_TITLE) : tr(STR_REDESIGN_OTHELLO), 2);
  pixelui::text(s.target(),
                wide ? fui::makeRect(b.x, name.bottom() + 4, leftWidth, 48)
                     : fui::makeRect(name.x, name.bottom(), name.width, 34),
                tr(STR_MATCH_CONFIGURE), 0);

  const int x = wide ? b.x + leftWidth + 20 : b.x, width = wide ? b.width - leftWidth - 20 : b.width;
  const int groups = m.game == Game::ConnectFour ? 3 : 2;
  const int step = wide ? std::min(100, (b.height - 82) / groups) : 110;
  int top = wide ? b.y : b.y + 116;
  const int titleHeight = wide ? 28 : 34, optionHeight = std::min(52, step - titleHeight - 10);
  auto heading = [&](const char* label) {
    s.target().fill(fui::makeRect(x, top - 12, width, 2), dashboard::ink);
    pixelui::text(s.target(), fui::makeRect(x, top, width, titleHeight), label, 1);
  };
  auto pair = [&](const char* title, const char* first, const char* second, fui::ActionId action, bool chosen) {
    heading(title);
    for (int n = 0; n < 2; ++n) {
      choice(s, fui::makeRect(x, top + titleHeight + 4, width, optionHeight), n ? second : first, action, n,
             chosen == bool(n), true, n, 2);
    }
    top += step;
  };
  if (m.game == Game::ConnectFour)
    pair(tr(STR_MATCH_RIVAL), tr(STR_UI_COMPUTER_9E47DA), tr(STR_CHESS_PERSON), Opponent, m.local);
  {
    heading(tr(STR_UI_LEVEL_F481CC));
    static constexpr StrId labels[] = {StrId::STR_UI_EASY_493F11, StrId::STR_CHESS_NORMAL, StrId::STR_UI_HARD_A06A58};
    for (int n = 0; n < 3; ++n) {
      choice(s, fui::makeRect(x, top + titleHeight + 4, width, optionHeight), I18N.get(labels[n]), Difficulty, n,
             n == m.level, false, n, 3, m.game != Game::ConnectFour || !m.local);
    }
    top += step;
  }
  pair(tr(STR_CHESS_YOUR_COLOUR), tr(STR_UI_WHITE_40D23F), tr(STR_UI_BLACK_A9414A), Colour, m.black);
  if (m.game == Game::Othello)
    pixelui::text(s.target(), fui::makeRect(b.x, b.bottom() - 106, b.width, 30), tr(STR_MATCH_NEXT_GAME), 3);
  const auto done = fui::makeRect(b.x, b.bottom() - 68, b.width, 68);
  dashboard::button(s, done, tr(STR_DONE), Done, 0, true, nullptr, 2);
  if (m.game == Game::ConnectFour)
    gamepanel::mark(s, fui::makeRect(done.x + 24, done.y + 18, 32, 32), gamepanel::Mark::Play, true);
}
}  // namespace matchsettings

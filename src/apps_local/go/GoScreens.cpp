#include "GoScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../ui/FitText.h"
#include "../ui/GamePanel.h"
#include "../ui/MatchArt.h"
#include "../ui/PageIndicator.h"

namespace goui {
namespace {
using namespace dashboard;

void begin(toybox::Screen& s) {
  pixelui::begin(s);
  pixelui::Header header;
  header.title = tr(STR_GAME_GO);
  header.back = true;
  pixelui::header(s, header);
  backui::publish(true, pixelui::geometry(s.device()).back);
}
void prose(toybox::Screen& s, fui::Rect r, const char* value, int lines = 3, int = 4,
           fui::TextAlign align = fui::TextAlign::Left) {
  fittext::draw(s.target(), r, value, 4, lines, align);
}
void stone(toybox::Screen& s, int x, int y, int radius, uint8_t colour) {
  toybox::disc(s, x, y, radius, fui::Color::Black);
  if (colour == go::kWhite) toybox::disc(s, x, y, std::max(1, radius - 2), fui::Color::White);
}
struct Grid {
  fui::Rect box;
  int pitch, left, top;
};
Grid grid(fui::Rect r, int size) {
  // Edge stones need the same clearance as interior stones.
  const int pitch = std::max(1, (std::min(r.width, r.height) - 8) / size);
  const int span = pitch * (size - 1);
  return {r, pitch, r.x + (r.width - span) / 2, r.y + (r.height - span) / 2};
}
void lines(toybox::Screen& s, const Grid& g, int size) {
  frame(s, g.box, pixelui::kBorder);
  const int span = (size - 1) * g.pitch;
  for (int n = 0; n < size; ++n) {
    s.target().line({int16_t(g.left + n * g.pitch), int16_t(g.top)},
                    {int16_t(g.left + n * g.pitch), int16_t(g.top + span)}, 1, ink);
    s.target().line({int16_t(g.left), int16_t(g.top + n * g.pitch)},
                    {int16_t(g.left + span), int16_t(g.top + n * g.pitch)}, 1, ink);
  }
  const int lo = go::starLine(size), hi = size - 1 - lo, mid = size / 2;
  for (int row : {lo, mid, hi})
    for (int col : {lo, mid, hi}) {
      if (size != go::kFullSize && (row == mid) != (col == mid)) continue;
      toybox::disc(s, g.left + col * g.pitch, g.top + row * g.pitch, std::max(2, g.pitch / 11), fui::Color::Black);
    }
}
void position(toybox::Screen& s, const Grid& g, int size, const uint8_t* points) {
  lines(s, g, size);
  if (!points) return;
  for (int p = 0; p < size * size; ++p)
    if (go::isStone(points[p]))
      stone(s, g.left + go::colOf(size, p) * g.pitch, g.top + go::rowOf(size, p) * g.pitch,
            std::max(2, g.pitch * 35 / 100), points[p]);
}
void gameBoard(toybox::Screen& s, const go::Game& game, bool count, const uint8_t* owner = nullptr) {
  const auto g = grid(boardRect(s.device()), game.size);
  lines(s, g, game.size);
  for (int p = 0; p < game.points(); ++p) {
    const int x = g.left + go::colOf(game.size, p) * g.pitch, y = g.top + go::rowOf(game.size, p) * g.pitch;
    const auto here = game.at(p);
    const int radius = std::max(2, g.pitch * 35 / 100);
    if (go::isStone(here)) {
      stone(s, x, y, radius, here);
      if (count && go::marked(game.dead, p)) {
        s.target().line({int16_t(x - radius / 2), int16_t(y - radius / 2)},
                        {int16_t(x + radius / 2), int16_t(y + radius / 2)}, 2, here == go::kBlack ? paper : ink);
        s.target().line({int16_t(x - radius / 2), int16_t(y + radius / 2)},
                        {int16_t(x + radius / 2), int16_t(y - radius / 2)}, 2, here == go::kBlack ? paper : ink);
      }
    } else if (count && owner && owner[p] != go::kEmpty) {
      const int half = std::max(2, g.pitch / 6);
      const auto box = fui::makeRect(x - half, y - half, 2 * half, 2 * half);
      s.target().fill(box, owner[p] == go::kBlack ? ink : paper);
      frame(s, box, 1);
    }
  }
  if (!count && game.lastMove >= 0 && game.lastMove < game.points() && go::isStone(game.at(game.lastMove))) {
    const int p = game.lastMove;
    const auto colour = game.at(p) == go::kBlack ? fui::Color::White : fui::Color::Black;
    const int x = g.left + go::colOf(game.size, p) * g.pitch, y = g.top + go::rowOf(game.size, p) * g.pitch;
    toybox::ring(s, x, y, std::max(2, g.pitch / 7), 1, colour,
                 game.at(p) == go::kBlack ? fui::Color::Black : fui::Color::White);
  }
}
fui::Rect controls(const fui::DeviceContext& d) {
  const auto b = pixelui::geometry(d).body;
  const auto r = boardRect(d);
  return b.width > b.height ? fui::makeRect(r.right() + 20, b.y, b.right() - r.right() - 20, b.height) : b;
}
const char* colour(uint8_t side) { return side == go::kBlack ? tr(STR_UI_BLACK_A9414A) : tr(STR_UI_WHITE_40D23F); }
void seat(toybox::Screen& s, fui::Rect r, uint8_t side, const char* who, int captures) {
  const bool compact = r.width < 220;
  const int inset = compact ? 40 : 51;
  stone(s, r.x + (compact ? 18 : 24), r.y + r.height / 2, compact ? 12 : 18, side);
  char text[96];
  std::snprintf(text, sizeof(text), tr(STR_GO_SEAT), who, colour(side));
  fittext::draw(s.target(), fui::makeRect(r.x + inset, r.y + 2, r.width - inset - 6, 28), text, compact ? 3 : 0, 1);
  std::snprintf(text, sizeof(text), tr(STR_GO_CAPTURES), captures);
  fittext::draw(s.target(), fui::makeRect(r.x + inset, r.y + 30, r.width - inset - 6, 24), text, 3, 1);
}
void number(char* text, size_t capacity, int halves) {
  if (halves % 2)
    std::snprintf(text, capacity, I18N.getLanguage() == Language::ES ? "%d,5" : "%d.5", halves / 2);
  else
    std::snprintf(text, capacity, "%d", halves / 2);
}
const char* boardNote(const BoardModel& m) {
  if (m.review) return tr(STR_MATCH_BOARD);
  if (m.disagreed) return tr(STR_UI_IT_DISAGREES_KEEP_PLAYING_9DCA7F);
  if (m.thinking || (!m.yourTurn && !m.sharedDevice)) return tr(STR_UI_THINKING_B6D843);
  if (m.caution == go::Caution::FillsOwnEye) return tr(STR_UI_THAT_FILLS_YOUR_OWN_EYE_8309A5);
  if (m.caution == go::Caution::SelfAtari) return tr(STR_UI_THAT_STONE_WOULD_BE_IN_ATARI_916AD0);
  if (m.theyPassed) return tr(STR_UI_THEY_PASSED_1902F0);
  if (m.nothingLeft) return tr(STR_UI_NOTHING_LEFT_PASS_A415A7);
  return m.aimed >= 0 ? tr(STR_GO_TAP_AGAIN) : tr(STR_GO_TOUCH);
}
}  // namespace

fui::Rect boardRect(const fui::DeviceContext& device) {
  const auto b = pixelui::geometry(device).body;
  if (b.width > b.height) {
    const int side = std::min(int(b.height), int(b.width) * 52 / 100);
    return fui::makeRect(b.x, b.y + (b.height - side) / 2, side, side);
  }
  const int side = std::min(int(b.width), int(b.height) - 230);
  return fui::makeRect(b.x + (b.width - side) / 2, b.y + 110, side, side);
}
void stoneCentre(const fui::DeviceContext& d, int size, int p, int16_t& x, int16_t& y) {
  const auto g = grid(boardRect(d), size);
  x = g.left + go::colOf(size, p) * g.pitch;
  y = g.top + go::rowOf(size, p) * g.pitch;
}
bool pointAt(const fui::DeviceContext& d, int size, int x, int y, int& point) {
  if (!go::isBoardSize(size)) return false;
  const auto g = grid(boardRect(d), size);
  if (x < g.box.x || y < g.box.y || x >= g.box.right() || y >= g.box.bottom()) return false;
  const int col = std::clamp((x - g.left + g.pitch / 2) / g.pitch, 0, size - 1);
  const int row = std::clamp((y - g.top + g.pitch / 2) / g.pitch, 0, size - 1);
  point = go::pointAt(size, row, col);
  return true;
}
int16_t stoneRadius(int size) { return size == 9 ? 17 : size == 13 ? 11 : 8; }
void formatResult(char* out, int capacity, int black, int white) {
  const int margin = std::abs(black - white);
  std::snprintf(out, capacity, "%c+%d.%d", black > white ? 'B' : 'W', margin / 2, margin % 2 * 5);
}

void buildMenu(toybox::Screen& s, const MenuModel& m) {
  begin(s);
  const auto b = s.body();
  const bool wide = b.width > b.height;
  const int rows = m.inProgress ? 4 : 3, rowH = wide ? 54 : 46;
  const auto side = wide ? fui::makeRect(b.x + b.width / 2 + 12, b.y, b.width / 2 - 12, b.height) : b;
  dashboard::text(s, fui::makeRect(b.x, b.y, wide ? b.width / 2 - 8 : b.width, 40),
                  m.inProgress ? tr(STR_GAME_PAUSED) : tr(STR_WORDLE_PLAY), 1);
  char label[96];
  std::snprintf(label, sizeof(label), m.sharedDevice ? tr(STR_GO_LOCAL_DETAIL) : tr(STR_GO_MENU_DETAIL), m.boardSize,
                m.boardSize, colour(m.seat));
  prose(s, fui::makeRect(b.x, b.y + 40, wide ? b.width / 2 - 8 : b.width, 28), label, 1, 0, fui::TextAlign::Center);
  const int actionHeight = rows * (rowH + 6) - 6;
  const int actionsY = wide ? gamepanel::centeredBlock(side, actionHeight + 60).y : b.bottom() - 46 - rows * (rowH + 6);
  const auto area = wide ? fui::makeRect(b.x, b.y + 76, b.width / 2 - 8, b.height - 78)
                         : fui::makeRect(b.x, b.y + 76, b.width, actionsY - b.y - 88);
  const int edge = std::min(int(area.width), int(area.height));
  const auto preview = fui::makeRect(area.x + (area.width - edge) / 2, area.y + (area.height - edge) / 2, edge, edge);
  static constexpr uint8_t demo[81] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 2, 1, 0, 0, 0, 0, 0, 0, 1, 2, 0};
  const int size = go::isBoardSize(m.boardSize) ? m.boardSize : 9;
  position(s, grid(preview, size), size, m.boardPoints ? m.boardPoints : size == 9 ? demo : nullptr);
  int y = actionsY;
  if (m.inProgress) {
    gamepanel::row(s, fui::makeRect(side.x, y, side.width, rowH), tr(STR_MATCH_CONTINUE), ActionMenuRow,
                   int(MenuRow::Play), gamepanel::Mark::Play, true);
    y += rowH + 6;
  }
  gamepanel::row(s, fui::makeRect(side.x, y, side.width, rowH), tr(STR_UI_NEW_GAME_F6F041), ActionDiscard, 0,
                 gamepanel::Mark::Restart, !m.inProgress);
  y += rowH + 6;
  gamepanel::row(s, fui::makeRect(side.x, y, side.width, rowH), tr(STR_WORDLE_HELP), ActionHelp, 0,
                 gamepanel::Mark::Book, false, nullptr, true, &matchart::Book);
  y += rowH + 6;
  gamepanel::row(s, fui::makeRect(side.x, y, side.width, rowH), tr(STR_SETTINGS_TITLE), ActionMenuRow,
                 int(MenuRow::Settings), gamepanel::Mark::Settings, false, nullptr, true, &matchart::Gear);
  prose(s, fui::makeRect(side.x, wide ? actionsY + actionHeight + 16 : side.bottom() - 44, side.width, 44),
        tr(STR_FRONT_GO_RULE), 2, 3, fui::TextAlign::Center);
}

void buildSettings(toybox::Screen& s, const SettingsModel& m) {
  begin(s);
  const auto b = s.body();
  const bool wide = b.width > b.height;
  pixelui::text(s.target(), fui::makeRect(b.x, b.y, b.width, 44), tr(STR_SETTINGS_TITLE), 2);
  const int width = wide ? (b.width - 20) / 2 : b.width, top = b.y + 58;
  const auto heading = [&](int x, int y, const char* label) {
    s.target().fill(fui::makeRect(x, y, width, 38), ink);
    dashboard::text(s, fui::makeRect(x + 12, y, width - 24, 38), label, 1, fui::TextAlign::Left, true);
  };
  heading(b.x, top, tr(STR_GO_GAME));
  const auto row = [&](int x, int y, int height, SettingsRow id, const char* label, const char* value,
                       const fui::BitmapRef* art, bool enabled = true) {
    const auto r = fui::makeRect(x, y, width, height);
    const bool selected = enabled && s.frame().buttonSelected(ActionSettingsRow, int(id));
    if (selected) s.target().fill(r, ink);
    frame(s, r, 2);
    if (enabled) hit(s, r, ActionSettingsRow, int(id));
    if (art)
      s.target().bitmap(fui::makeRect(x + 14, y + 12, 38, 38), *art, fui::BitmapMode::Contain, selected ? paper : ink);
    else if (id == SettingsRow::PlayAs) {
      stone(s, x + 24, y + height / 2, 12, go::kBlack);
      stone(s, x + 48, y + height / 2, 12, go::kWhite);
    } else
      gamepanel::mark(s, fui::makeRect(x + 14, y + 14, 34, 34),
                      id == SettingsRow::Board ? gamepanel::Mark::Grid : gamepanel::Mark::New, selected);
    dashboard::text(s, fui::makeRect(x + 70, y, width / 2 - 65, 62), label, 0, fui::TextAlign::Left, selected);
    dashboard::text(s, fui::makeRect(x + width / 2, y, width / 2 - 48, 62), value, 0, fui::TextAlign::Right, selected);
    if (enabled) chevron(s.target(), fui::makeRect(x + width - 38, y, 28, 62), selected);
  };
  const bool computer = m.opponent == go::Opponent::Computer;
  row(b.x, top + 38, 62, SettingsRow::Opponent, tr(STR_UI_OPPONENT_974974),
      computer ? tr(STR_UI_COMPUTER_9E47DA) : tr(STR_CHESS_PERSON), &matchart::Robot);
  row(b.x, top + 100, 94, SettingsRow::Level, tr(STR_UI_LEVEL_F481CC),
      computer
          ? (m.level == go::Level::Medium ? tr(STR_CHESS_NORMAL) : static_cast<const char*>(go::levelName(m.level)))
          : "--",
      &matchart::Level, computer);
  if (computer)
    prose(s, fui::makeRect(b.x + 14, top + 160, width - 28, 30),
          m.level == go::Level::Easy   ? tr(STR_GO_LEVEL_EASY)
          : m.level == go::Level::Hard ? tr(STR_GO_LEVEL_HARD)
                                       : tr(STR_GO_LEVEL_NORMAL),
          2, 3);
  row(b.x, top + 194, 62, SettingsRow::PlayAs, tr(STR_UI_YOU_PLAY_E18634),
      computer ? colour(m.handicap ? go::kBlack : m.playAs) : "--", nullptr, computer && !m.handicap);
  const int x = wide ? b.x + width + 20 : b.x, y = wide ? top : top + 278;
  heading(x, y, tr(STR_UI_BOARD_3E91AD));
  char value[48];
  std::snprintf(value, sizeof(value), "%d × %d", m.boardSize, m.boardSize);
  row(x, y + 38, 62, SettingsRow::Board, tr(STR_GO_SIZE), value, nullptr);
  if (m.handicap)
    std::snprintf(value, sizeof(value), tr(STR_UI_D_STONES_76819C), m.handicap);
  else
    std::snprintf(value, sizeof(value), "%s", tr(STR_UI_NONE_0654A0));
  row(x, y + 100, 62, SettingsRow::Handicap, tr(STR_UI_HANDICAP_66447A), computer ? value : "--", nullptr, computer);
  button(s, fui::makeRect(b.x, b.bottom() - 54, b.width, 54), tr(STR_UI_BACK_TO_MENU_398BBB), ActionDone);
}

void buildBoard(toybox::Screen& s, const BoardModel& m) {
  begin(s);
  const auto b = s.body(), c = controls(s.device()), r = boardRect(s.device());
  const bool wide = b.width > b.height;
  const int groupY = c.y + (c.height - 204) / 2;
  const auto stats = fui::makeRect(c.x, wide ? groupY + 72 : c.y, c.width, 60);
  frame(s, stats, 2);
  s.target().fill(fui::makeRect(stats.x + stats.width / 2, stats.y, 2, stats.height), ink);
  const int human = m.seat, other = go::other(m.seat);
  seat(s, fui::makeRect(stats.x, stats.y, stats.width / 2, stats.height), human,
       m.sharedDevice ? tr(STR_UI_THIS_SIDE_F6B3AD) : tr(STR_UI_YOU_AF319E), m.game.capturedBy[human]);
  seat(s, fui::makeRect(stats.x + stats.width / 2, stats.y, stats.width / 2, stats.height), other,
       m.sharedDevice ? tr(STR_UI_THAT_SIDE_745984) : tr(STR_UI_THEM_78C509), m.game.capturedBy[other]);
  char label[96];
  std::snprintf(label, sizeof(label),
                m.sharedDevice ? tr(STR_GO_TURN)
                : m.yourTurn   ? tr(STR_GO_YOUR_TURN)
                               : tr(STR_GO_OTHER_TURN),
                colour(m.game.toMove));
  const int turnW = wide ? c.width * 46 / 100 : c.width;
  const auto turn = fui::makeRect(c.x, wide ? groupY + 148 : c.y + 68, turnW, wide ? 56 : 36);
  s.target().fill(turn, ink);
  fittext::draw(s.target(), turn.inset({4, 6, 4, 6}), m.review ? tr(STR_MATCH_BOARD) : label, wide ? 0 : 1,
                wide ? 2 : 1, fui::TextAlign::Center, fui::Color::White);
  gameBoard(s, m.game, false);
  if (!m.review && m.yourTurn && m.aimed >= 0 && m.aimed < m.game.points() && m.game.at(m.aimed) == go::kEmpty) {
    int16_t x, y;
    stoneCentre(s.device(), m.game.size, m.aimed, x, y);
    const auto g = grid(r, m.game.size);
    const int radius = std::max(3, g.pitch * 35 / 100), reach = std::min(g.pitch / 2, radius + 4);
    toybox::disc(s, x, y, radius, fui::Color::Black);
    toybox::disc(s, x, y, radius - 2, fui::Paint::dither(fui::Color::DarkGray));
    toybox::bracket(s, fui::makeRect(x - reach, y - reach, 2 * reach + 1, 2 * reach + 1), std::max(3, g.pitch / 6), 2);
  }
  char played[80];
  const char* note = boardNote(m);
  if (!m.review && go::explainsPlayedOn(m.itPlayedOn, m.freePoints, m.disagreed, m.thinking, m.caution)) {
    std::snprintf(played, sizeof(played), tr(STR_UI_NOT_OVER_U_FREE_POINTS_B1035D), unsigned(m.freePoints));
    note = played;
  }
  prose(s, fui::makeRect(c.x, wide ? groupY : r.bottom() + 12, c.width, wide ? 56 : 44), note, 3, 0,
        fui::TextAlign::Center);
  const int actionX = wide ? turn.right() + 8 : c.x;
  const int actionW = wide ? c.right() - actionX : c.width;
  const int half = (actionW - 8) / 2, actionY = wide ? turn.y : c.bottom() - 60;
  button(s, fui::makeRect(actionX, actionY, m.review && wide ? actionW : half, wide ? 56 : 60),
         m.review ? tr(STR_MATCH_RESULTS) : tr(STR_WORDLE_MENU), m.review ? ActionResult : ActionDone);
  if (!m.review && m.yourTurn)
    button(s, fui::makeRect(actionX + half + 8, actionY, actionW - half - 8, wide ? 56 : 60), tr(STR_UI_PASS_BD564D),
           ActionPass, 0, true);
}

void buildCount(toybox::Screen& s, const CountModel& m) {
  begin(s);
  const auto c = controls(s.device());
  dashboard::text(s, fui::makeRect(c.x, c.y, c.width, 38), tr(STR_UI_COUNTING_EE6CC5), 1);
  char value[32];
  formatResult(value, sizeof(value), m.blackHalves, m.whiteHalves);
  dashboard::text(s, fui::makeRect(c.x, c.y + 40, c.width, 36), value, 1);
  gameBoard(s, m.game, true, m.owner);
  const auto r = boardRect(s.device());
  const int noteTop = s.device().width > s.device().height ? c.y + 90 : r.bottom() + 10;
  prose(s, fui::makeRect(c.x, noteTop, c.width, c.bottom() - 70 - noteTop), tr(STR_GO_REVIEW_HELP), 3, 3,
        fui::TextAlign::Center);
  const int half = (c.width - 12) / 2;
  const bool can = m.yourTurn && !m.youAccepted;
  button(s, fui::makeRect(c.x, c.bottom() - 58, half, 58),
         m.youAccepted ? tr(STR_UI_WAITING_C271B6) : tr(STR_UI_ACCEPT_514158), can ? ActionAccept : fui::NO_ACTION, 0,
         true);
  button(s, fui::makeRect(c.x + half + 12, c.bottom() - 58, c.width - half - 12, 58), tr(STR_UI_PLAY_ON_D09F74),
         m.yourTurn ? ActionResume : fui::NO_ACTION);
}

void buildResult(toybox::Screen& s, const ResultModel& m) {
  begin(s);
  const auto b = s.body();
  const bool wide = b.width > b.height;
  const bool draw = m.blackHalves == m.whiteHalves, black = m.blackHalves > m.whiteHalves,
             you = (black ? go::kBlack : go::kWhite) == m.seat;
  const auto left = wide ? fui::makeRect(b.x, b.y, b.width / 2 - 12, b.height) : b;
  const auto right = wide ? fui::makeRect(left.right() + 24, b.y, b.right() - left.right() - 24, b.height) : b;
  matchart::draw(s, fui::makeRect(left.x + (left.width - 140) / 2, left.y, 140, wide ? 70 : 104),
                 you ? matchart::GoTrophy : matchart::GoFlag);
  dashboard::text(s, fui::makeRect(left.x, left.y + (wide ? 78 : 114), left.width, 48),
                  draw             ? tr(STR_UI_A_DRAW_4D5967)
                  : m.sharedDevice ? (black ? tr(STR_DRAUGHTS_BLACK_WINS) : tr(STR_DRAUGHTS_WHITE_WINS))
                  : you            ? tr(STR_MATCH_WON)
                                   : tr(STR_UI_THEY_WIN_2A4FCA),
                  2);
  char value[24], label[112];
  number(value, sizeof(value), std::abs(m.blackHalves - m.whiteHalves));
  std::snprintf(label, sizeof(label), tr(STR_GO_MARGIN), colour(black ? go::kBlack : go::kWhite), value);
  prose(s, fui::makeRect(left.x, left.y + (wide ? 132 : 166), left.width, 40), draw ? tr(STR_UI_A_DRAW_4D5967) : label,
        2, 0, fui::TextAlign::Center);
  const auto box =
      wide ? fui::makeRect(right.x, right.y, right.width, 190) : fui::makeRect(b.x, b.y + 218, b.width, 224);
  frame(s, box, pixelui::kBorder);
  dashboard::text(s, fui::makeRect(box.x, box.y + 10, box.width, 38), tr(STR_MATCH_FINAL), 1);
  s.target().fill(fui::makeRect(box.x + box.width / 2, box.y + 56, 2, box.height - 68), ink);
  for (int n = 0; n < 2; ++n) {
    const uint8_t side = n ? go::other(m.seat) : m.seat;
    const int x = box.x + box.width * n / 2, w = box.width / 2;
    stone(s, x + w / 2, box.y + 78, 22, side);
    std::snprintf(label, sizeof(label), tr(STR_GO_SEAT),
                  m.sharedDevice ? colour(side)
                  : n            ? tr(STR_UI_THEM_78C509)
                                 : tr(STR_UI_YOU_AF319E),
                  colour(side));
    dashboard::text(s, fui::makeRect(x + 6, box.y + 106, w - 12, 28), m.sharedDevice ? colour(side) : label, 0);
    number(value, sizeof(value), side == go::kBlack ? m.blackHalves : m.whiteHalves);
    digits(s, fui::makeRect(x + 12, box.y + 140, w - 24, box.height - 154), value);
  }
  prose(s,
        fui::makeRect(wide ? left.x : right.x, wide ? left.y + 202 : box.bottom() + 8, wide ? left.width : right.width,
                      40),
        tr(STR_GO_KOMI), 2, 3, fui::TextAlign::Center);
  const int y = right.bottom() - 3 * 54 - 16;
  button(s, fui::makeRect(right.x, y, right.width, 54), tr(STR_UI_NEW_GAME_F6F041), ActionAgain, 0, true, nullptr, 1);
  button(s, fui::makeRect(right.x, y + 62, right.width, 54), tr(STR_C4_VIEW_BOARD), ActionReview, 0, false, nullptr, 1);
  button(s, fui::makeRect(right.x, y + 124, right.width, 54), tr(STR_UI_BACK_TO_MENU_398BBB), ActionDone, 0, false,
         nullptr, 1);
}

void buildHelp(toybox::Screen& s, int page) {
  begin(s);
  page = std::clamp(page, 0, 2);
  const auto b = s.body();
  const bool wide = b.width > b.height;
  dashboard::text(s, fui::makeRect(b.x, b.y, b.width, 26), tr(STR_WORDLE_HELP), 0, fui::TextAlign::Left);
  static constexpr StrId titles[] = {StrId::STR_GO_STONES_LIBERTIES, StrId::STR_GO_SURROUND, StrId::STR_GO_PASS_COUNT};
  dashboard::text(s, fui::makeRect(b.x, b.y + 28, b.width, wide ? 30 : 48), I18N.get(titles[page]), wide ? 1 : 2,
                  fui::TextAlign::Left);
  const int bottom = b.bottom() - (page == 2 ? 92 : 48);
  pageui::build(s, fui::makeRect(b.x, b.bottom() - 42, b.width, 42), page + 1, 3, ActionHelpPage, -1, ActionHelpPage,
                1);
  if (page == 0) {
    prose(s, fui::makeRect(b.x, b.y + 82, b.width, wide ? 40 : 66), tr(STR_GO_PLACE_HELP), 3, 0);
    const auto art = wide ? fui::makeRect(b.x, b.y + 154, b.width / 2 - 12, bottom - b.y - 158)
                          : fui::makeRect(b.x + 50, b.y + 178, b.width - 100, bottom - b.y - 371);
    matchart::draw(s, art, matchart::GoLiberties);
    const auto note = wide ? fui::makeRect(b.x + b.width / 2 + 12, b.y + 130, b.width / 2 - 12, bottom - b.y - 134)
                           : fui::makeRect(b.x, bottom - 172, b.width, 164);
    frame(s, note, pixelui::kBorder);
    dashboard::text(s, fui::makeRect(note.x + 12, note.y + 6, note.width - 24, 30), tr(STR_GO_BREATHING), 1,
                    fui::TextAlign::Left);
    prose(s, fui::makeRect(note.x + 12, note.y + 42, note.width - 24, note.height - 48), tr(STR_GO_LIBERTIES_HELP), 6,
          0);
    dashboard::text(s, fui::makeRect(art.x, art.y - 24, art.width, 24), tr(STR_GO_FOUR_LIBERTIES), 0);
  } else if (page == 1) {
    prose(s, fui::makeRect(b.x, b.y + 82, wide ? b.width / 2 - 12 : b.width, 40), tr(STR_GO_CAPTURE_HELP), 2, 0);
    const int diagramsW = wide ? b.width / 2 - 12 : b.width, edge = std::min(diagramsW / 2 - 10, wide ? 140 : 210);
    const int top = b.y + 126;
    for (int n = 0; n < 2; ++n) {
      const auto r = fui::makeRect(b.x + n * (diagramsW / 2 + 4), top, diagramsW / 2 - 4, edge + 22);
      frame(s, r, pixelui::kBorder);
      matchart::draw(s, r.inset(fui::Insets{10, 10, 26, 10}), n ? matchart::GoAfter : matchart::GoBefore);
      dashboard::text(s, fui::makeRect(r.x, r.bottom() + 4, r.width, 30), n ? tr(STR_OT_AFTER) : tr(STR_OT_BEFORE), 1);
      if (!n) dashboard::text(s, fui::makeRect(r.x, r.bottom() - 24, r.width, 20), tr(STR_GO_PLACE_HERE), 3);
    }
    const int x = wide ? b.x + b.width / 2 + 12 : b.x, y = wide ? b.y + 82 : top + edge + 74,
              w = wide ? b.width / 2 - 12 : b.width;
    const auto ko = fui::makeRect(x, y, w, 108);
    frame(s, ko, pixelui::kBorder);
    dashboard::text(s, fui::makeRect(x + 12, y + 6, w - 24, 30), tr(STR_GO_KO), 1, fui::TextAlign::Left);
    prose(s, fui::makeRect(x + 12, y + 42, w - 24, ko.height - 48), tr(STR_GO_KO_HELP), 4, 3);
    const auto suicide = fui::makeRect(x, ko.bottom() + 8, w, bottom - ko.bottom() - 16);
    frame(s, suicide, pixelui::kBorder);
    prose(s, suicide.inset(fui::Insets{4, 12, 4, 12}), tr(STR_GO_SUICIDE_HELP), 4, 3);
  } else {
    const int top = b.y + (wide ? 62 : 84), available = bottom - top;
    const int leftW = wide ? b.width / 2 - 10 : b.width;
    const int reviewY = top + (wide ? 92 : 98);
    dashboard::text(s, fui::makeRect(b.x, top, leftW, 30), tr(STR_UI_PASS_BD564D), 1, fui::TextAlign::Left);
    prose(s, fui::makeRect(b.x, top + 34, leftW, 58), tr(STR_GO_PASS_HELP), 2);
    dashboard::text(s, fui::makeRect(b.x, reviewY, leftW, 30), tr(STR_GO_REVIEW), 1, fui::TextAlign::Left);
    prose(s, fui::makeRect(b.x, reviewY + 34, leftW, wide ? available - 132 : 94), tr(STR_GO_REVIEW_HELP), 4);
    const auto score = wide ? fui::makeRect(b.x + leftW + 20, top, b.width - leftW - 20, available)
                            : fui::makeRect(b.x, top + 240, b.width, available - 240);
    frame(s, score, 2);
    dashboard::text(s, fui::makeRect(score.x + 8, score.y + 4, score.width - 16, 30), tr(STR_GO_RESULT), 1,
                    fui::TextAlign::Left);
    const auto diagram = fui::makeRect(score.x + 8, score.y + 40, 136, std::min(136, int(score.height) - 104));
    const auto g = grid(diagram, 5);
    for (int n = 0; n < 5; ++n) {
      s.target().line({int16_t(g.left + n * g.pitch), int16_t(g.top)},
                      {int16_t(g.left + n * g.pitch), int16_t(g.top + 4 * g.pitch)}, 1, ink);
      s.target().line({int16_t(g.left), int16_t(g.top + n * g.pitch)},
                      {int16_t(g.left + 4 * g.pitch), int16_t(g.top + n * g.pitch)}, 1, ink);
    }
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < 5; ++col) {
        const int x = g.left + col * g.pitch, y = g.top + row * g.pitch;
        if (row == 0 || row == 4 || col == 0 || col == 4)
          stone(s, x, y, 8, go::kBlack);
        else
          s.target().stroke(fui::makeRect(x - 3, y - 3, 6, 6), ink, 2);
      }
    pixelui::text(s.target(), fui::makeRect(diagram.x, diagram.bottom() + 4, diagram.width, 26), "16 + 9 = 25", 0,
                  fui::Color::Black, fui::TextAlign::Center);
    pixelui::text(s.target(), fui::makeRect(diagram.x, diagram.bottom() + 32, diagram.width, 24),
                  tr(STR_GO_GUIDE_AREA_EXAMPLE), 3, fui::Color::Black, fui::TextAlign::Center);
    const int tx = diagram.right() + 12, tw = score.right() - tx - 10;
    prose(s, fui::makeRect(tx, score.y + 40, tw, 116), tr(STR_GO_RESULT_HELP), 4);
    fittext::draw(s.target(), fui::makeRect(tx, score.y + 160, tw, score.height - 168), tr(STR_GO_GUIDE_KOMI), 0, 3);
  }
}
}  // namespace goui

#include "DraughtsScreens.h"

#include <I18n.h>

#include <cstdio>

#include "../ui/CompactArt.h"
#include "../ui/Dashboard.h"
#include "../ui/GameFrontDoor.h"
#include "../ui/GamePanel.h"
#include "../ui/MatchArt.h"
#include "../ui/MatchResult.h"
#include "../ui/PageIndicator.h"
#include "../ui/ResultArt.h"
namespace draughtsui {
namespace fui = freeink::ui;
namespace {
const char* ruleName(draughts::Rules rule) {
  static constexpr StrId names[] = {StrId::STR_DRAUGHTS_INTERNATIONAL, StrId::STR_DRAUGHTS_BRAZILIAN,
                                    StrId::STR_DRAUGHTS_SPANISH, StrId::STR_DRAUGHTS_ENGLISH};
  return I18N.get(names[int(rule)]);
}

void token(toybox::Screen& s, int cx, int cy, int radius, bool black, bool king = false) {
  const auto fill = black ? fui::Color::Black : fui::Color::White;
  const auto detail = black ? fui::Color::White : fui::Color::Black;
  toybox::ring(s, cx, cy, radius, 2, fui::Color::Black, fill);
  toybox::ring(s, cx, cy, std::max(1, radius * 3 / 4), 1, detail, fill);
  if (king) toybox::ring(s, cx, cy, std::max(1, radius / 3), 2, detail, fill);
}

void board(toybox::Screen& s, fui::Rect r, int size, const uint8_t* cells, int selected = -1,
           const draughts::Generator* legal = nullptr, bool humanBlack = false) {
  const int cell = r.width / size;
  for (int display = 0; display < size * size; ++display) {
    const int n = humanBlack ? size * size - 1 - display : display;
    const auto box = fui::makeRect(r.x + display % size * cell, r.y + display / size * cell, cell, cell);
    const bool dark = (n % size + n / size) % 2;
    s.target().fill(box, dark ? fui::Paint::solid(fui::Color::LightGray) : dashboard::paper);
    const int row = n / size;
    const uint8_t piece = cells ? cells[n] : !dark ? 0 : row < size / 2 - 1 ? 2 : row >= size / 2 + 1 ? 1 : 0;
    if (piece) {
      const bool black = draughts::owner(piece) == 1;
      const int radius = std::max(3, cell * 36 / 100), cx = box.x + cell / 2, cy = box.y + cell / 2;
      token(s, cx, cy, radius, black, draughts::king(piece));
      if (piece & draughts::kTaken)
        s.target().line({int16_t(box.x + 6), int16_t(box.y + 6)}, {int16_t(box.right() - 6), int16_t(box.bottom() - 6)},
                        2, black ? dashboard::paper : dashboard::ink);
    }
    if (n == selected) {
      s.target().stroke(box.inset(fui::Insets{2, 2, 2, 2}), dashboard::paper, 4);
      s.target().stroke(box.inset(fui::Insets{3, 3, 3, 3}), dashboard::ink, 2);
    }
    if (legal && selected >= 0 && legal->permits(selected, n))
      toybox::ring(s, box.x + cell / 2, box.y + cell / 2, std::max(3, cell / 8), 1, fui::Color::Black,
                   fui::Color::White);
  }
  dashboard::frame(s, r, pixelui::kBorder);
}
}  // namespace
fui::Rect boardRect(const fui::DeviceContext& device, int size) {
  const auto b = pixelui::geometry(device).body;
  const bool wide = b.width > b.height;
  const int cell = std::max(
      1, (wide ? std::min(int(b.height), int(b.width) * 9 / 20) : std::min(int(b.width), int(b.height) - 200)) / size);
  return fui::makeRect(wide ? b.x : b.x + (b.width - cell * size) / 2,
                       wide ? b.y + (b.height - cell * size) / 2 : b.y + 76, cell * size, cell * size);
}
int squareAt(const fui::DeviceContext& device, int size, int x, int y, bool humanBlack) {
  const auto r = boardRect(device, size);
  if (x < r.x || y < r.y || x >= r.right() || y >= r.bottom()) return -1;
  const int index = (y - r.y) / (r.height / size) * size + (x - r.x) / (r.width / size);
  return humanBlack ? size * size - 1 - index : index;
}
void build(toybox::Screen& s, const Model& m) {
  pixelui::begin(s);
  pixelui::Header header;
  header.back = true;
  header.title = tr(STR_UI_CHECKERS_3296AA);
  pixelui::header(s, header);
  backui::publish(true, pixelui::geometry(s.device()).back);
  auto body = s.body();
  if (m.saveFailed && (m.view == View::Settings || m.view == View::Help || m.view == View::ConfirmNew)) {
    dashboard::text(s, fui::makeRect(body.x, body.y, body.width, 30), tr(STR_WORDLE_SAVE_FAILED), 3);
    body.y += 34;
    body.height -= 34;
  }
  switch (m.view) {
    case View::Menu: {
      const auto rule = m.active && m.game ? m.game->rules : m.rules;
      const bool wide = body.width > body.height;
      const auto left = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 10, body.height) : body;
      const auto right =
          wide ? fui::makeRect(left.right() + 20, body.y, body.right() - left.right() - 20, body.height) : body;
      const int size = rule == draughts::Rules::International ? 10 : 8;
      const auto badge = fui::makeRect(left.x, left.y, left.width, 72);
      dashboard::frame(s, badge, pixelui::kBorder);
      const int reserve = wide ? 76 : 90;
      s.target().fill(fui::makeRect(badge.right() - reserve, badge.y, 2, 44), dashboard::ink);
      s.target().fill(fui::makeRect(badge.x, badge.y + 44, badge.width, 2), dashboard::ink);
      dashboard::text(s, fui::makeRect(badge.x + 8, badge.y, badge.width - reserve - 16, 44), ruleName(rule), 1);
      char label[112];
      std::snprintf(label, sizeof(label), "%d × %d", size, size);
      dashboard::text(s, fui::makeRect(badge.right() - reserve, badge.y, reserve, 44), label, 0);
      static constexpr StrId levels[] = {StrId::STR_DRAUGHTS_EASY, StrId::STR_DRAUGHTS_MEDIUM,
                                         StrId::STR_DRAUGHTS_HARD};
      std::snprintf(label, sizeof(label), tr(STR_DRAUGHTS_SUMMARY),
                    m.local ? tr(STR_CHESS_PERSON) : tr(STR_UI_COMPUTER_9E47DA),
                    m.local ? "--" : I18N.get(levels[std::clamp(m.difficulty, 0, 2)]));
      dashboard::text(s, fui::makeRect(badge.x + 6, badge.y + 46, badge.width - 12, 24),
                      m.saveFailed ? tr(STR_WORDLE_SAVE_FAILED) : label, 3);
      const bool continuing = m.active && m.result == Outcome::Playing;
      const int rows = continuing ? 4 : 3;
      const int actionY = wide ? gamepanel::centeredBlock(right, rows * 70 - 8).y : body.bottom() - rows * 70 + 8;
      const auto area =
          fui::makeRect(left.x, left.y + 86, left.width, (wide ? left.bottom() : actionY - 16) - left.y - 86);
      const int cell = std::max(1, std::min(int(area.width), int(area.height)) / size);
      const auto preview = fui::makeRect(area.x + (area.width - cell * size) / 2,
                                         area.y + (area.height - cell * size) / 2, cell * size, cell * size);
      board(s, preview, size, m.active && m.game ? m.game->cells : nullptr);
      gamepanel::row(s, fui::makeRect(right.x, actionY, right.width, 62),
                     continuing ? tr(STR_MATCH_CONTINUE) : tr(STR_WORDLE_PLAY), Play, 0, gamepanel::Mark::Play, true);
      if (continuing)
        gamepanel::row(s, fui::makeRect(right.x, actionY + 70, right.width, 62), tr(STR_UI_NEW_GAME_F6F041), NewGame, 0,
                       gamepanel::Mark::New);
      const int settingsY = actionY + (continuing ? 140 : 70);
      gamepanel::row(s, fui::makeRect(right.x, settingsY, right.width, 62), tr(STR_SETTINGS_TITLE), Settings, 0,
                     gamepanel::Mark::Settings, false, nullptr, true, &matchart::Gear);
      gamepanel::row(s, fui::makeRect(right.x, settingsY + 70, right.width, 62), tr(STR_WORDLE_HELP), Help, 0,
                     gamepanel::Mark::Help);
      break;
    }
    case View::Settings: {
      const bool wide = body.width > body.height;
      const int width = wide ? (body.width - 20) / 2 : body.width;
      const auto heading = [&](int x, int y, const char* label) {
        dashboard::text(s, fui::makeRect(x, y, width, 36), label, 1, fui::TextAlign::Left);
      };
      const auto radio = [&](fui::Rect r, const char* label, bool active, fui::ActionId action, int value,
                             const char* tail = nullptr) {
        active = s.frame().buttonSelected(action, value, active);
        dashboard::button(s, r, "", action, value, active);
        const auto fg = active ? fui::Color::White : fui::Color::Black;
        toybox::ring(s, r.x + 23, r.y + r.height / 2, 11, 2, fg, active ? fui::Color::Black : fui::Color::White);
        if (active) toybox::disc(s, r.x + 23, r.y + r.height / 2, 6, fg);
        dashboard::text(s, fui::makeRect(r.x + 48, r.y, r.width - 56 - (tail ? 76 : 0), r.height), label, 0,
                        fui::TextAlign::Left, active);
        if (tail)
          dashboard::text(s, fui::makeRect(r.right() - 78, r.y, 70, r.height), tail, 0, fui::TextAlign::Right, active);
      };
      heading(body.x, body.y, tr(STR_DRAUGHTS_SETTINGS_VARIANT));
      const int rowH = wide ? 44 : 48, step = rowH + 6;
      for (int n = 0; n < 4; ++n)
        radio(fui::makeRect(body.x, body.y + 46 + n * step, width, rowH), ruleName(draughts::Rules(n)),
              int(m.rules) == n, Rule, n, n == 0 ? "10 × 10" : "8 × 8");
      const int x = wide ? body.x + width + 20 : body.x;
      const int y = wide ? body.y : body.y + 280 - std::max(0, 660 - int(body.height));
      heading(x, y, tr(STR_UI_OPPONENT_974974));
      radio(fui::makeRect(x, y + 38, width / 2, 50), tr(STR_UI_COMPUTER_9E47DA), !m.local, Local, 0);
      radio(fui::makeRect(x + width / 2, y + 38, width - width / 2, 50), tr(STR_CHESS_PERSON), m.local, Local, 1);
      heading(x, y + 110, tr(STR_UI_LEVEL_F481CC));
      static constexpr StrId levels[] = {StrId::STR_DRAUGHTS_EASY, StrId::STR_DRAUGHTS_MEDIUM,
                                         StrId::STR_DRAUGHTS_HARD};
      for (int n = 0; n < 3; ++n)
        dashboard::button(s, fui::makeRect(x + width * n / 3 + 3, y + 148, width * (n + 1) / 3 - width * n / 3 - 6, 48),
                          I18N.get(levels[n]), m.local ? fui::NO_ACTION : Difficulty, n, !m.local && m.difficulty == n);
      heading(x, y + (wide ? 200 : 220), tr(STR_DRAUGHTS_PLAY_COLOUR));
      for (int n = 0; n < 2; ++n) {
        const auto r = fui::makeRect(x + n * width / 2, y + (wide ? 238 : 258), width / 2, 56);
        dashboard::button(s, r, "", Colour, n, m.humanBlack == bool(n));
        toybox::ring(s, r.x + 32, r.y + 28, 17, 2, n ? fui::Color::White : fui::Color::Black,
                     n ? fui::Color::Black : fui::Color::White);
        dashboard::text(s, fui::makeRect(r.x + 60, r.y, r.width - 66, r.height),
                        n ? tr(STR_UI_BLACK_A9414A) : tr(STR_UI_WHITE_40D23F), 0, fui::TextAlign::Left,
                        m.humanBlack == bool(n));
      }
      dashboard::button(s, fui::makeRect(body.x, body.bottom() - 54, body.width, 54), tr(STR_UI_BACK_TO_MENU_398BBB),
                        Menu, 0, false, nullptr, 1);
      break;
    }
    case View::Help: {
      static constexpr draughts::Rules order[] = {draughts::Rules::Spanish, draughts::Rules::Brazilian,
                                                  draughts::Rules::International, draughts::Rules::English};
      const bool wide = body.width > body.height;
      const int page = std::clamp(m.helpPage, 0, wide ? 7 : 3);
      const auto summary = fui::makeRect(body.x, body.y, wide ? body.width * 32 / 100 : body.width, body.height);
      const auto lessons =
          wide ? fui::makeRect(summary.right() + 16, body.y, body.width - summary.width - 16, body.height) : body;
      const auto rule = order[wide ? page / 2 : page];
      static constexpr StrId helpNames[] = {StrId::STR_DRAUGHTS_HELP_SPANISH, StrId::STR_DRAUGHTS_HELP_BRAZILIAN,
                                            StrId::STR_DRAUGHTS_HELP_INTERNATIONAL, StrId::STR_DRAUGHTS_HELP_ENGLISH};
      const bool international = rule == draughts::Rules::International, english = rule == draughts::Rules::English;
      const bool forward = english || rule == draughts::Rules::Spanish;
      char label[160];
      auto title = fui::makeRect(summary.x, summary.y, summary.width, wide ? 36 : 44);
      fittext::draw(s.target(), title, tr(STR_WORDLE_HELP), wide ? 0 : pixelui::kTitle, 1);
      pixelui::copyCaps(label, sizeof(label), I18N.get(helpNames[wide ? page / 2 : page]));
      fittext::draw(s.target(), fui::makeRect(summary.x, title.bottom(), summary.width, wide ? 58 : 28), label,
                    pixelui::kBody, wide ? 2 : 1);
      const int size = international ? 10 : 8;
      std::snprintf(label, sizeof(label), tr(STR_DRAUGHTS_PIECES), size, size, international ? 20 : 12);
      const auto badge = fui::makeRect(summary.x, title.bottom() + (wide ? 66 : 28), summary.width, wide ? 68 : 28);
      dashboard::frame(s, badge, 2);
      fittext::draw(s.target(), badge.inset({4, 8, 4, 8}), label, 3, wide ? 2 : 1, fui::TextAlign::Center);
      const auto done = fui::makeRect(summary.x, body.bottom() - 44, summary.width, 44);
      pageui::build(s, fui::makeRect(summary.x, done.y, summary.width, 38), page + 1, wide ? 8 : 4, HelpPage, -1,
                    HelpPage, 1);
      const auto mandatory = fui::makeRect(summary.x, done.y - (wide ? 86 : 70), summary.width, wide ? 72 : 24);
      fittext::draw(s.target(), mandatory, tr(STR_DRAUGHTS_MANDATORY), 3, wide ? 3 : 1, fui::TextAlign::Center);
      const int top = wide ? lessons.y : badge.bottom() + 10, available = wide ? lessons.height : mandatory.y - top - 4;
      static constexpr int weights[] = {88, 128, 110, 158};
      static constexpr const fui::BitmapRef* illustrations[][4] = {
          {&compactart::SpanishMove, &compactart::SpanishCapture, &compactart::SpanishCrown, &compactart::SpanishKing},
          {&compactart::BrazilMove, &compactart::BrazilCapture, &compactart::BrazilCrown, &compactart::BrazilKing},
          {&compactart::InternationalMove, &compactart::InternationalCapture, &compactart::InternationalCrown,
           &compactart::InternationalKing},
          {&compactart::EnglishMove, &compactart::EnglishCapture, &compactart::EnglishCrown, &compactart::EnglishKing}};
      int y = top;
      const int first = wide ? (page % 2) * 2 : 0, last = wide ? first + 2 : 4;
      for (int n = first; n < last; ++n) {
        const int w = lessons.width;
        const int h = wide ? available / 2 : available * weights[n] / 484;
        const auto r = fui::makeRect(lessons.x, wide ? top + (n - first) * available / 2 : y, w, h);
        const auto number = fui::makeRect(r.x, r.y, 32, 32);
        s.target().fill(number, dashboard::ink);
        std::snprintf(label, sizeof(label), "%d", n + 1);
        dashboard::text(s, number, label, 0, fui::TextAlign::Center, true);
        static constexpr StrId headings[] = {StrId::STR_DRAUGHTS_MOVEMENT, StrId::STR_DRAUGHTS_CAPTURE,
                                             StrId::STR_DRAUGHTS_PROMOTION, StrId::STR_DRAUGHTS_FLYING_KING};
        pixelui::copyCaps(label, sizeof(label),
                          I18N.get(n == 3 && english ? StrId::STR_DRAUGHTS_SHORT_KING : headings[n]));
        fittext::draw(s.target(), fui::makeRect(r.x + 46, r.y, (wide ? r.width : r.width * 2 / 5) - 46, 32), label,
                      pixelui::kSmall, 1);
        const int artW = wide ? std::min(138, int(r.width) / 3) : r.width * 2 / 5;
        compactart::draw(s, fui::makeRect(r.x + 42, r.y + 34, artW - 42, r.height - 42),
                         *illustrations[wide ? page / 2 : page][n]);
        const char* text = n == 0    ? tr(STR_DRAUGHTS_MOVE_HELP)
                           : n == 1  ? (forward ? tr(STR_DRAUGHTS_CAPTURE_FORWARD) : tr(STR_DRAUGHTS_CAPTURE_BOTH))
                           : n == 2  ? ((forward) ? tr(STR_DRAUGHTS_CROWN_END) : tr(STR_DRAUGHTS_CROWN_FINAL))
                           : english ? tr(STR_DRAUGHTS_SHORT_HELP)
                                     : tr(STR_DRAUGHTS_LONG_HELP);
        fui::TextStyle prose;
        prose.font = 4;
        prose.maxLines = 8;
        auto copy =
            fui::makeRect(r.x + artW + 6, wide ? r.y + 38 : r.y + 4, r.width - artW - 10, r.height - (wide ? 50 : 8));
        if (n == 1) {
          std::snprintf(label, sizeof(label), "%s %s", text,
                        english                            ? tr(STR_DRAUGHTS_ANY_CHAIN)
                        : rule == draughts::Rules::Spanish ? tr(STR_DRAUGHTS_MOST_KINGS)
                                                           : tr(STR_DRAUGHTS_MOST));
          text = label;
        }
        if (wide)
          fittext::draw(s.target(), copy, text, 4, 6);
        else
          s.target().text(copy, text, prose);
        s.target().fill(fui::makeRect(r.x, r.bottom() - 4, r.width, 4), dashboard::ink);
        y += h;
      }
    } break;
    case View::ConfirmNew:
      dashboard::text(s, fui::makeRect(body.x, body.y + 20, body.width, 70), tr(STR_DRAUGHTS_REPLACE), 0);
      dashboard::button(s, fui::makeRect(body.x, body.bottom() - 136, body.width, 58), tr(STR_WORDLE_START_NEW),
                        NewGame, 0, true);
      dashboard::button(s, fui::makeRect(body.x, body.bottom() - 66, body.width, 58), tr(STR_CANCEL), Cancel);
      break;
    case View::Result: {
      if (!m.game || m.result == Outcome::Playing) return;
      const bool draw = m.result == Outcome::Draw;
      const bool winnerBlack = m.result == Outcome::Black;
      const bool won = !draw && winnerBlack == m.humanBlack;
      char badge[96];
      std::snprintf(badge, sizeof(badge), tr(STR_DRAUGHTS_BADGE), ruleName(m.game->rules), m.game->size, m.game->size);
      const auto art = draw  ? fui::BitmapRef{}
                       : won ? (m.humanBlack ? resultart::CheckersWinBlack : matchart::CheckersWin)
                             : (m.humanBlack ? resultart::CheckersLossBlack : matchart::CheckersLoss);
      matchresult::draw(s, body,
                        draw  ? tr(STR_UI_A_DRAW_4D5967)
                        : won ? tr(STR_MATCH_WON)
                              : tr(STR_MATCH_LOST),
                        draw          ? ""
                        : winnerBlack ? tr(STR_RESULT_BLACK_WINS)
                                      : tr(STR_RESULT_WHITE_WINS),
                        badge,
                        m.saveFailed ? tr(STR_WORDLE_SAVE_FAILED)
                        : won        ? tr(STR_DRAUGHTS_WON)
                                     : tr(STR_DRAUGHTS_RETRY),
                        art, NewGame, Review, Menu,
                        won ? matchresult::BadgePosition::AfterArt : matchresult::BadgePosition::BeforeArt);
      break;
    }
    case View::Review:
    case View::Board: {
      if (!m.game || !m.legal) return;
      const auto& g = *m.game;
      const bool review = m.view == View::Review, wide = body.width > body.height;
      const auto r = boardRect(s.device(), g.size);
      const auto side =
          wide ? gamepanel::centeredBlock(
                     fui::makeRect(r.right() + 20, body.y, body.right() - r.right() - 20, body.height), 208)
               : body;
      const char* status = review              ? tr(STR_MATCH_BOARD)
                           : !m.legal->ready() ? tr(STR_DRAUGHTS_CALCULATING)
                           : m.local           ? (g.turn ? tr(STR_DRAUGHTS_BLACK_YOURS) : tr(STR_DRAUGHTS_WHITE_YOURS))
                           : g.turn == int(m.humanBlack)
                               ? (g.turn ? tr(STR_DRAUGHTS_BLACK_YOURS) : tr(STR_DRAUGHTS_WHITE_YOURS))
                           : g.turn ? tr(STR_DRAUGHTS_BLACK_THEIRS)
                                    : tr(STR_DRAUGHTS_WHITE_THEIRS);
      const auto turn = fui::makeRect(side.x, side.y, side.width, 42);
      s.target().fill(turn, dashboard::ink);
      dashboard::text(s, turn, m.saveFailed ? tr(STR_WORDLE_SAVE_FAILED) : status, 1, fui::TextAlign::Center, true);
      char label[96];
      std::snprintf(label, sizeof(label), tr(STR_DRAUGHTS_BADGE), ruleName(g.rules), g.size, g.size);
      dashboard::text(s, fui::makeRect(side.x, side.y + 44, side.width, 28), label, 0);
      board(s, r, g.size, g.cells, review ? -1 : m.selected, review ? nullptr : m.legal, m.humanBlack);
      const auto totals = fui::makeRect(side.x, wide ? side.y + 88 : r.bottom() + 14, side.width, 46);
      dashboard::frame(s, totals, pixelui::kBorder);
      s.target().fill(fui::makeRect(totals.x + totals.width / 2, totals.y, 2, totals.height), dashboard::ink);
      for (int colour = 0; colour < 2; ++colour) {
        int count = 0;
        for (int n = 0; n < g.size * g.size; ++n)
          if (g.cells[n] && !(g.cells[n] & draughts::kTaken) && draughts::owner(g.cells[n]) == colour) ++count;
        const int x = totals.x + colour * totals.width / 2;
        token(s, x + 23, totals.y + 23, 16, colour != 0);
        std::snprintf(label, sizeof(label), colour ? tr(STR_FRONT_BLACK_COUNT) : tr(STR_FRONT_WHITE_COUNT), count);
        dashboard::text(s, fui::makeRect(x + 46, totals.y, totals.width / 2 - 50, totals.height), label, 0);
      }
      const int half = (side.width - 12) / 2;
      dashboard::button(s, fui::makeRect(side.x, side.bottom() - 58, half, 58),
                        review ? tr(STR_MATCH_RESULTS) : tr(STR_WORDLE_MENU), review ? Result : Menu);
      dashboard::button(s, fui::makeRect(side.x + half + 12, side.bottom() - 58, side.width - half - 12, 58),
                        tr(STR_UI_NEW_GAME_F6F041), NewGame);
      break;
    }
  }
}
}  // namespace draughtsui

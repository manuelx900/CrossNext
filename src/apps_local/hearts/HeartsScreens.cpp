#include "HeartsScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../cards/CardsLabels.h"
#include "../ui/CardPanels.h"
namespace heartsui {
using namespace hearts;
namespace {
fui::Rect begin(toybox::Screen& s, bool menu = false) {
  auto b = cardpanel::begin(s, tr(STR_UI_HEARTS_29FBCB));
  if (menu) {
    const auto r = fui::makeRect(s.device().safeRect().right() - 214, b.y - 60, 100, 40);
    dashboard::button(s, r, tr(STR_WORDLE_MENU), ActionButton, ButtonMenu, true, nullptr, 0);
    s.target().stroke(r, dashboard::paper, 2);
  }
  return b;
}
const char* passText(Pass pass) {
  switch (pass) {
    case Pass::Left:
      return tr(STR_HEARTS_PASS_LEFT);
    case Pass::Right:
      return tr(STR_HEARTS_PASS_RIGHT);
    case Pass::Across:
      return tr(STR_HEARTS_PASS_ACROSS);
    case Pass::Hold:
      return tr(STR_HEARTS_TAP_PLAY);
  }
  return "";
}
void difficultyRow(toybox::Screen& s, fui::Rect r, const Options& o, bool setup) {
  const char* values[] = {tr(STR_HEARTS_EASY), tr(STR_REDESIGN_NORMAL), tr(STR_HEARTS_EXPERT)};
  cardpanel::setting(s, r, setup ? tr(STR_HEARTS_OPPONENTS) : tr(STR_HEARTS_DIFFICULTY), values, 3, o.difficulty,
                     ActionOption, 0);
}
void limitRow(toybox::Screen& s, fui::Rect r, const Options& o) {
  const char* values[] = {"50", "100"};
  cardpanel::setting(s, r, tr(STR_HEARTS_LIMIT), values, 2, !o.shortGame, ActionOption, 3);
}
}  // namespace
void buildMenu(toybox::Screen& s, const MenuModel& m) {
  auto b = begin(s);
  cardpanel::title(s, fui::makeRect(b.x, b.y, b.width, 44),
                   m.hasSave ? tr(STR_CARDS_IN_PROGRESS) : tr(STR_CARDS_READY));
  char label[100];
  std::snprintf(label, sizeof(label), tr(STR_HEARTS_ROUND_LOW), m.savedHand);
  dashboard::text(s, fui::makeRect(b.x, b.y + 46, b.width, 34), label, 1, fui::TextAlign::Left);
  const int top = b.y + 94, leftW = b.width * 44 / 100, gap = 12;
  auto art = fui::makeRect(b.x, top, leftW, b.bottom() - top - 94);
  dashboard::frame(s, art);
  referenceart::draw(s, art.inset({14, 14, 14, 14}), referenceart::HeartsFan);
  const char* stats[] = {tr(STR_CARDS_PLAYED), tr(STR_CARDS_WINS), tr(STR_CARDS_BEST)};
  const int values[] = {m.gamesPlayed, m.gamesWon, m.bestScore};
  cardpanel::stats(s, fui::makeRect(b.x, art.bottom() + 8, leftW, 86), stats, values);
  const char* labels[] = {m.hasSave ? tr(STR_QUEST_CONTINUE) : tr(STR_UI_START_7196E7), tr(STR_UI_NEW_GAME_F6F041),
                          tr(STR_WORDLE_HELP), tr(STR_CARDS_SETTINGS)};
  static constexpr int ids[] = {ButtonConfirm, ButtonMenu, ButtonHowTo, ButtonSettings};
  static constexpr gamepanel::Mark marks[] = {gamepanel::Mark::Play, gamepanel::Mark::New, gamepanel::Mark::Help,
                                              gamepanel::Mark::Settings};
  const int rowH = (b.bottom() - top - 24) / 4;
  for (int n = 0; n < 4; ++n)
    gamepanel::row(s, fui::makeRect(b.x + leftW + gap, top + n * (rowH + 8), b.width - leftW - gap, rowH), labels[n],
                   ActionButton, ids[n], marks[n], n == 0, nullptr, true,
                   n == 1   ? &referenceart::Page
                   : n == 3 ? &referenceart::Gear
                            : nullptr);
}
void buildSetup(toybox::Screen& s, const Options& o, bool replacing) {
  const auto b = begin(s);
  cardpanel::title(s, fui::makeRect(b.x, b.y, b.width, 48), tr(STR_UI_NEW_GAME_F6F041));
  const int row = 70;
  difficultyRow(s, fui::makeRect(b.x, b.y + 54, b.width, row), o, true);
  limitRow(s, fui::makeRect(b.x, b.y + 132, b.width, row), o);
  const auto info = fui::makeRect(b.x, b.y + 210, b.width, b.height - 300);
  dashboard::frame(s, info);
  referenceart::draw(s, fui::makeRect(info.x + 20, info.y + 8, 160, info.height - 16), referenceart::Rivals);
  guideui::text(s, fui::makeRect(info.x + 210, info.y + 8, info.width - 224, info.height - 16), tr(STR_HEARTS_RIVALS),
                3, fui::TextAlign::Left, 0);
  if (replacing)
    dashboard::text(s, fui::makeRect(b.x, b.bottom() - 82, b.width, 28), tr(STR_CARDS_REPLACE), 0,
                    fui::TextAlign::Left);
  const int w = (b.width - 12) / 2;
  dashboard::button(s, fui::makeRect(b.x, b.bottom() - 50, w, 50), tr(STR_WORDLE_CANCEL), ActionButton, ButtonCancel,
                    false, nullptr, 1);
  dashboard::button(s, fui::makeRect(b.x + w + 12, b.bottom() - 50, w, 50), tr(STR_CARDS_DEAL), ActionButton,
                    ButtonDeal, true, nullptr, 1);
}
void buildSettings(toybox::Screen& s, const Options& o, bool failed) {
  const auto b = begin(s);
  cardpanel::title(s, fui::makeRect(b.x, b.y, b.width, 48), tr(STR_CARDS_SETTINGS));
  const int h = (b.height - 158) / 4;
  difficultyRow(s, fui::makeRect(b.x, b.y + 54, b.width, h), o, false);
  limitRow(s, fui::makeRect(b.x, b.y + 62 + h, b.width, h), o);
  const char* yes[] = {tr(STR_CARDS_YES), tr(STR_CARDS_NO)};
  cardpanel::setting(s, fui::makeRect(b.x, b.y + 70 + 2 * h, b.width, h), tr(STR_CARDS_CONFIRM_NEW), yes, 2,
                     !o.confirmNew, ActionOption, 5);
  cardpanel::setting(s, fui::makeRect(b.x, b.y + 78 + 3 * h, b.width, h), tr(STR_HEARTS_HIGHLIGHT), yes, 2,
                     !o.highlight, ActionOption, 7);
  dashboard::text(s, fui::makeRect(b.x, b.bottom() - 78, b.width, 26),
                  failed ? tr(STR_CHESS_PUZZLE_SAVE_ERROR) : tr(STR_HEARTS_NEXT), 0, fui::TextAlign::Left);
  dashboard::button(s, fui::makeRect(b.x, b.bottom() - 48, b.width, 48), tr(STR_CARDS_SAVE), ActionButton, ButtonSave,
                    true, nullptr, 1);
}
void buildBoard(toybox::Screen& s, const BoardModel& m, Layout& l) {
  if (!m.game) return;
  const auto& g = *m.game;
  auto b = begin(s, true);
  auto& t = s.target();
  const bool passing = g.phase == Phase::Passing;
  const auto status = fui::makeRect(b.x - 6, b.y - 6, b.width + 12, 34);
  dashboard::frame(s, status, 2);
  char label[100];
  std::snprintf(label, sizeof(label), tr(STR_HEARTS_ROUND), int(g.handNumber) + 1);
  dashboard::text(s, fui::makeRect(status.x + 10, status.y, 146, status.height), label, 0, fui::TextAlign::Left);
  dashboard::text(s, fui::makeRect(status.x + 158, status.y, status.width - (passing ? 320 : 420), status.height),
                  passing ? passText(g.passDirection())
                          : (g.phase == Phase::Playing && g.turn == Seat::South ? tr(STR_HEARTS_TURN) : m.status),
                  1);
  if (passing) {
    std::snprintf(label, sizeof(label), "%d/3", m.pickedCount);
  } else
    std::snprintf(label, sizeof(label), "%s", g.heartsBroken ? tr(STR_HEARTS_OPEN) : tr(STR_HEARTS_CLOSED));
  if (!passing)
    cardart::drawSuit(s, fui::makeRect(status.right() - 248, status.y + 5, 24, 24), cards::Suit::Hearts, true);
  dashboard::text(s,
                  fui::makeRect(status.right() - (passing ? 164 : 218), status.y, passing ? 154 : 208, status.height),
                  label, 0, fui::TextAlign::Right);
  const int handH = passing ? 168 : 158;
  const auto upper = fui::makeRect(b.x, status.bottom() + 8, b.width, b.bottom() - handH - status.bottom() - 16);
  const int scoreW = b.width / 4;
  const int scoreH = (upper.height - 18) / 4;
  for (int n = 0; n < kSeats; ++n) {
    const auto r = fui::makeRect(upper.x, upper.y + n * (scoreH + 6), scoreW, scoreH);
    const bool active = !passing && m.seats[n].isTurn;
    if (active)
      t.fill(r, dashboard::ink);
    else
      dashboard::frame(s, r, 2);
    dashboard::text(s, fui::makeRect(r.x + 10, r.y, r.width - 60, r.height), m.seats[n].name, 1, fui::TextAlign::Left,
                    active);
    std::snprintf(label, sizeof(label), "%d", m.seats[n].total);
    dashboard::text(s, fui::makeRect(r.right() - 52, r.y, 42, r.height), label, 1, fui::TextAlign::Right, active);
  }
  const auto felt = fui::makeRect(upper.x + scoreW + 10, upper.y, upper.width - scoreW - 10, upper.height);
  l.felt = felt;
  dashboard::frame(s, felt, 2);
  const auto& hand = g.hands[seatIndex(Seat::South)];
  if (passing) {
    dashboard::text(s, fui::makeRect(felt.x + 12, felt.y + 2, felt.width - 24, 34),
                    m.pickedCount == 3 ? tr(STR_HEARTS_SELECTED) : tr(STR_HEARTS_PICK), 1, fui::TextAlign::Left);
    const int ch = std::min(112, int(felt.height) - 48), cw = ch * 3 / 4;
    int shown = 0;
    for (int n = 0; n < hand.count; ++n)
      if (m.picked[n]) {
        cardart::drawCardFace(s, fui::makeRect(felt.x + 22 + shown * (cw + 14), felt.y + 42, cw, ch), hand.at(n), ch,
                              cardart::Fan::None, cardart::Ink::Picked);
        ++shown;
      }
    const auto pass = fui::makeRect(felt.x + felt.width * 62 / 100, felt.y + 60, felt.width * 38 / 100 - 12,
                                    std::min(80, int(felt.height) - 72));
    gamepanel::row(s, pass, tr(STR_UI_PASS_BD564D), m.confirmEnabled ? ActionButton : fui::ActionId(0), ButtonConfirm,
                   gamepanel::Mark::Play, m.confirmEnabled, nullptr, false);
  } else {
    const int ch = std::min(80, (int(felt.height) - 60) / 2), cw = ch * 3 / 4, cx = felt.x + felt.width / 2,
              cy = felt.y + felt.height / 2;
    const int x[] = {cx - cw / 2, cx - cw * 2, cx - cw / 2, cx + cw},
              y[] = {felt.bottom() - 24 - ch, cy - ch / 2, felt.y + 24, cy - ch / 2};
    for (int n = 0; n < kSeats; ++n) {
      const auto r = fui::makeRect(x[n], y[n], cw, ch);
      l.trickCard[n] = r;
      if (g.trick.played[n] == kNoCard) {
        t.stroke(r, dashboard::ink, 2, 5);
      } else
        cardart::drawCardFace(s, r, g.trick.played[n], ch);
      const auto name = fui::makeRect(n == 1   ? r.x - 90
                                      : n == 3 ? r.right() + 8
                                               : r.x - 30,
                                      n == 2   ? felt.y
                                      : n == 0 ? felt.bottom() - 24
                                               : r.y + ch / 2 - 12,
                                      n == 1 || n == 3 ? 82 : cw + 60, 24);
      dashboard::text(s, name, m.seats[n].name, 3);
    }
  }
  const auto handBox = fui::makeRect(b.x, b.bottom() - handH + (passing ? 0 : 0), b.width, handH);
  if (!passing) dashboard::frame(s, handBox, 2);
  dashboard::text(s, fui::makeRect(handBox.x + 8, handBox.y + 2, handBox.width - 16, 30),
                  passing ? tr(STR_HEARTS_HAND) : m.status, 0, fui::TextAlign::Left);
  const int ch = passing ? 108 : 102, cw = 74;
  const int pitch = hand.count > 1 ? std::min(84, (handBox.width - 24 - cw) / (hand.count - 1)) : cw;
  const int start = handBox.x + (handBox.width - (cw + std::max(0, int(hand.count) - 1) * pitch)) / 2;
  l.handCount = hand.count;
  for (int n = 0; n < hand.count; ++n) {
    const bool picked = passing && m.picked[n], highlight = !passing && m.highlight && m.legal[n];
    const auto r = fui::makeRect(start + n * pitch, handBox.y + 38 - (picked ? 6 : 0), cw, ch);
    l.handCard[n] = r;
    cardart::drawCardFace(s, r, hand.at(n), ch, cardart::Fan::Sideways,
                          picked || highlight ? cardart::Ink::Picked : cardart::Ink::Normal);
    if (picked || highlight) t.stroke(r.inset({4, 4, 4, 4}), dashboard::ink, 2, 5);
    dashboard::hit(s, fui::makeRect(r.x, r.y, n + 1 < hand.count ? pitch : cw, ch), ActionHandCard, n);
  }
  if (passing) {
    auto notice = fui::makeRect(b.x, b.bottom() - 22, b.width, 26);
    dashboard::frame(s, notice, 2);
    dashboard::text(s, notice, m.status && *m.status && !m.confirmEnabled ? m.status : tr(STR_HEARTS_PASS_NOTICE), 0);
  }
}
void buildScore(toybox::Screen& s, const ScoreModel& m) {
  if (!m.game) return;
  const auto& g = *m.game;
  const auto b = begin(s);
  cardpanel::title(s, fui::makeRect(b.x, b.y, b.width, 44),
                   m.gameOver ? tr(STR_UI_GAME_OVER_B1DE5C) : tr(STR_HEARTS_ROUND_END));
  char label[100];
  std::snprintf(label, sizeof(label), tr(STR_HEARTS_ROUND_POINTS), g.lastHand.scored[0]);
  dashboard::text(s, fui::makeRect(b.x, b.y + 44, b.width, 30), label, 1, fui::TextAlign::Left);
  const int top = b.y + 88, height = b.height - 150, leftW = b.width * 35 / 100;
  const auto left = fui::makeRect(b.x, top, leftW, height);
  dashboard::frame(s, left);
  cardart::drawSuit(s, fui::makeRect(left.x + (left.width - 90) / 2, left.y + 12, 90, 90), cards::Suit::Hearts, true);
  dashboard::text(s, fui::makeRect(left.x + 8, left.y + 108, left.width - 16, 30), tr(STR_HEARTS_YOUR_POINTS), 1);
  std::snprintf(label, sizeof(label), "%d", g.lastHand.scored[0]);
  dashboard::digits(s, fui::makeRect(left.x + 16, left.y + 142, left.width - 32, std::max(36, int(left.height) - 192)),
                    label);
  std::snprintf(label, sizeof(label), "%s  %d", tr(STR_HEARTS_TOTAL), g.total[0]);
  dashboard::text(s, fui::makeRect(left.x + 8, left.bottom() - 40, left.width - 16, 32), label, 1);
  const auto table = fui::makeRect(left.right() + 10, top, b.width - left.width - 10, height);
  dashboard::frame(s, table);
  const int nameW = table.width * 45 / 100, col = (table.width - nameW) / 2, rowH = (table.height - 40) / 4;
  dashboard::text(s, fui::makeRect(table.x + 10, table.y, nameW - 20, 40), tr(STR_HEARTS_PLAYER), 1,
                  fui::TextAlign::Left);
  dashboard::text(s, fui::makeRect(table.x + nameW, table.y, col, 40), tr(STR_HEARTS_ROUND_LABEL), 0);
  dashboard::text(s, fui::makeRect(table.x + nameW + col, table.y, col, 40), tr(STR_HEARTS_TOTAL), 0);
  int order[] = {0, 1, 2, 3};
  std::sort(order, order + 4,
            [&](int a, int c) { return g.total[a] < g.total[c] || (g.total[a] == g.total[c] && a < c); });
  for (int n = 0; n < 4; ++n) {
    const int seat = order[n];
    const auto row = fui::makeRect(table.x, table.y + 40 + n * rowH, table.width, rowH);
    dashboard::frame(s, row, seat == 0 ? 4 : 1);
    dashboard::text(s, fui::makeRect(row.x + 14, row.y, nameW - 22, rowH), m.seats[seat].name, 1, fui::TextAlign::Left);
    if (g.total[seat] == g.total[order[0]])
      cardpanel::crown(s, fui::makeRect(row.x + nameW - 34, row.y + (rowH - 24) / 2, 24, 24));
    std::snprintf(label, sizeof(label), "+%d", g.lastHand.scored[seat]);
    dashboard::text(s, fui::makeRect(row.x + nameW, row.y, col, rowH), label, 1);
    std::snprintf(label, sizeof(label), "%d", g.total[seat]);
    dashboard::text(s, fui::makeRect(row.x + nameW + col, row.y, col, rowH), label, 1);
  }
  const int w = (b.width - 12) / 2;
  dashboard::button(s, fui::makeRect(b.x, b.bottom() - 50, w, 50),
                    m.gameOver ? tr(STR_UI_NEW_GAME_F6F041) : tr(STR_HEARTS_NEXT_ROUND), ActionButton, ButtonConfirm,
                    true, nullptr, 1);
  dashboard::button(s, fui::makeRect(b.x + w + 12, b.bottom() - 50, w, 50), tr(STR_UI_BACK_TO_MENU_398BBB),
                    ActionButton, ButtonMenu, false, nullptr, 1);
}
namespace {
struct GuideCard {
  Suit suit;
  uint8_t rank;
};
enum class LessonArt { Points, Follow, Pass, Score, Open, Moon };
void helpFan(toybox::Screen& s, fui::Rect r, const GuideCard* values, int count) {
  const int h = std::min(110, int(r.height) - 36 * (count - 1)), w = h * 92 / 122;
  const int x = r.x + (r.width - w) / 2, y = r.y + (r.height - h - 36 * (count - 1)) / 2;
  for (int n = 0; n < count; ++n)
    cardart::drawCardFace(s, fui::makeRect(x, y + n * 36, w, h),
                          cards::faceUp(cards::makeCard(values[n].suit, values[n].rank)), n == count - 1 ? h : 36,
                          cardart::Fan::Down);
}
void lessonArt(toybox::Screen& s, fui::Rect r, LessonArt example) {
  static constexpr GuideCard points[] = {{Suit::Hearts, 0}, {Suit::Spades, 11}};
  static constexpr GuideCard follow[] = {{Suit::Clubs, 2}, {Suit::Clubs, 5}, {Suit::Clubs, 12}};
  static constexpr GuideCard pass[] = {{Suit::Hearts, 1}, {Suit::Spades, 11}, {Suit::Diamonds, 9}};
  static constexpr GuideCard opening[] = {{Suit::Clubs, 1}};
  switch (example) {
    case LessonArt::Points:
      helpFan(s, r, points, 2);
      return;
    case LessonArt::Follow:
      helpFan(s, r, follow, 3);
      return;
    case LessonArt::Pass:
      helpFan(s, r, pass, 3);
      return;
    case LessonArt::Score:
      referenceart::draw(s, r, referenceart::Podium);
      return;
    case LessonArt::Open:
      helpFan(s, r, opening, 1);
      return;
    case LessonArt::Moon: {
      const int y = r.y + (r.height - 184) / 2;
      cardart::drawSuit(s, fui::makeRect(r.x + (r.width - 50) / 2, y, 50, 50), Suit::Hearts, true);
      pixelui::text(s.target(), fui::makeRect(r.x, y + 54, r.width, 26), "13 x 1", 0, fui::Color::Black,
                    fui::TextAlign::Center);
      const int h = 94, w = h * 92 / 122;
      cardart::drawCardFace(s, fui::makeRect(r.x + (r.width - w) / 2, y + 86, w, h),
                            cards::faceUp(cards::makeCard(points[1].suit, points[1].rank)), h);
      return;
    }
  }
}
}  // namespace
int howToPages() { return 4; }
int howToLines(int) { return 4; }
const char* howToTitle(int) { return tr(STR_WORDLE_HELP); }
const char* howToLine(int, int line) {
  switch (line) {
    case 0:
      return tr(STR_HEARTS_PENALTIES);
    case 1:
      return tr(STR_HEARTS_FOLLOW_RULE);
    case 2:
      return tr(STR_HEARTS_PASS_RULE);
    case 3:
      return tr(STR_HEARTS_LOW_RULE);
    default:
      return "";
  }
}
void buildHowTo(toybox::Screen& s, const HowToModel& m) {
  const auto b = begin(s);
  const int page = std::clamp(m.page, 0, howToPages() - 1);
  const bool wide = b.width > b.height;
  static constexpr StrId titles[] = {StrId::STR_CARDS_GUIDE_CARDS, StrId::STR_HEARTS_GUIDE_TRICKS,
                                     StrId::STR_HEARTS_GUIDE_ROUND, StrId::STR_HEARTS_GUIDE_SPECIAL};
  pixelui::text(s.target(), fui::makeRect(b.x, b.y, b.width, 44), I18N.get(titles[page]), 1);
  const auto area = fui::makeRect(b.x, b.y + 54, b.width, b.height - 118);
  if (page == 0)
    cardpanel::deckLesson(s, area, true);
  else {
    static constexpr StrId heads[][2] = {{StrId::STR_HEARTS_AVOID, StrId::STR_HEARTS_FOLLOW},
                                         {StrId::STR_HEARTS_PASS_THREE, StrId::STR_HEARTS_LOW},
                                         {StrId::STR_HEARTS_GUIDE_OPEN, StrId::STR_HEARTS_GUIDE_MOON}};
    static constexpr StrId copy[][2] = {{StrId::STR_HEARTS_PENALTIES, StrId::STR_HEARTS_FOLLOW_RULE},
                                        {StrId::STR_HEARTS_PASS_RULE, StrId::STR_HEARTS_LOW_RULE},
                                        {StrId::STR_HEARTS_GUIDE_OPEN_COPY, StrId::STR_HEARTS_GUIDE_MOON_COPY}};
    static constexpr LessonArt arts[][2] = {{LessonArt::Points, LessonArt::Follow},
                                            {LessonArt::Pass, LessonArt::Score},
                                            {LessonArt::Open, LessonArt::Moon}};
    const int w = wide ? (area.width - 12) / 2 : area.width, h = wide ? area.height : (area.height - 12) / 2;
    for (int n = 0; n < 2; ++n) {
      const auto r = fui::makeRect(area.x + (wide ? n * (w + 12) : 0), area.y + (wide ? 0 : n * (h + 12)), w, h);
      dashboard::frame(s, r, 2);
      pixelui::text(s.target(), fui::makeRect(r.x + 10, r.y + 8, r.width - 20, 34), I18N.get(heads[page - 1][n]), 1);
      const int artW = wide ? 90 : 108;
      lessonArt(s, fui::makeRect(r.x + 10, r.y + 50, artW, r.height - 62), arts[page - 1][n]);
      guideui::text(s, fui::makeRect(r.x + artW + 22, r.y + 52, r.width - artW - 34, r.height - 64),
                    I18N.get(copy[page - 1][n]), 10);
    }
  }
  pageui::build(s, fui::makeRect(b.x, b.bottom() - 48, b.width, 48), page + 1, howToPages(), ActionHelpPage, -1,
                ActionHelpPage, 1);
}
const char* refusalText(const Refusal why) {
  switch (why) {
    case Refusal::NotYourTurn:
      return tr(STR_UI_NOT_YOUR_TURN_YET_637511);
    case Refusal::TwoOfClubsOpens:
      return tr(STR_UI_THE_TWO_OF_CLUBS_OPENS_30E95D);
    case Refusal::MustFollowSuit:
      return tr(STR_UI_YOU_STILL_HOLD_THAT_SUIT_4CFEA2);
    case Refusal::NoPointsFirstTrick:
      return tr(STR_UI_NO_POINTS_ON_TRICK_ONE_4AA5D8);
    case Refusal::HeartsNotBroken:
      return tr(STR_UI_HEARTS_ARE_NOT_BROKEN_2F3EBE);
    case Refusal::JustNo:
      return tr(STR_UI_THE_RULES_REFUSE_THAT_E4FECE);
    case Refusal::Count:
      break;
  }
  return "";
}

// SEEN and UNSEEN rather than OUT and GONE. "QUEEN OUT" meant she had not been
// played while "QUEEN GONE" meant she had -- opposite meanings, one word apart,
// in the smallest type on the screen. These are also short enough to survive
// their box, which the pair before them was not: it was shrunk a cut and then
// truncated anyway, to "QUEEN STILL...".
const char* statusTemplate(const Status which) {
  switch (which) {
    case Status::PickToPass:
      return tr(STR_UI_PICK_THREE_TO_PASS_S_754050);
    case Status::Thinking:
      return tr(STR_UI_S_IS_THINKING_DF70D7);
    case Status::YourLeadFirst:
      return tr(STR_UI_LEAD_THE_TWO_OF_CLUBS_E8406A);
    case Status::YourLead:
      return tr(STR_UI_YOUR_LEAD_47EC69);
    case Status::FollowSuit:
      return tr(STR_UI_FOLLOW_S_3D59EA);
    case Status::TakesIt:
      return tr(STR_HEARTS_TRICK_WINNER);
    case Status::TakesItPoints:
      return tr(STR_HEARTS_TRICK_POINTS);
    case Status::HandOver:
      return tr(STR_UI_HAND_OVER_70413F);
    case Status::GameOver:
      return tr(STR_UI_GAME_OVER_B1DE5C);
    case Status::Count:
      break;
  }
  return "";
}

void longestStatus(const Status which, char* out, const int size) {
  // The widest real substitutions: the longest seat name, the longest suit, the
  // longest pass direction, and a two-digit point count that takes the plural.
  switch (which) {
    case Status::PickToPass:
      std::snprintf(out, size, statusTemplate(which), "%s", tr(STR_UI_ACROSS_A8E6CA));
      return;
    case Status::Thinking:
      std::snprintf(out, size, statusTemplate(which), "%s", tr(STR_UI_NORTH_CF42DF));
      return;
    case Status::FollowSuit:
      std::snprintf(out, size, statusTemplate(which), "%s", tr(STR_UI_DIAMONDS_6CB5FD));
      return;
    case Status::TakesIt:
      std::snprintf(out, size, statusTemplate(which), tr(STR_UI_NORTH_CF42DF));
      return;
    case Status::TakesItPoints:
      std::snprintf(out, size, statusTemplate(which), tr(STR_UI_NORTH_CF42DF), kMoonPoints);
      return;
    default:
      std::snprintf(out, size, "%s", statusTemplate(which));
      return;
  }
}

const char* heartsStateText(const bool broken) {
  return broken ? tr(STR_UI_HEARTS_BROKEN_4F5C5D) : tr(STR_UI_HEARTS_UNBROKEN_E5D772);
}
const char* queenStateText(const bool played) {
  return played ? tr(STR_UI_QUEEN_SEEN_C26E0F) : tr(STR_UI_QUEEN_UNSEEN_45F332);
}

}  // namespace heartsui

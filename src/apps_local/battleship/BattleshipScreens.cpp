#include "BattleshipScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/CardPanels.h"
#include "../ui/FitText.h"
#include "../ui/MatchResult.h"
#include "../ui/MysteryArt.h"
#include "BattleshipLabels.h"
namespace bshipui {
namespace {
fui::Rect begin(toybox::Screen& s) {
  if (s.device().width > s.device().height) return cardpanel::begin(s, tr(STR_UI_BATTLESHIP_4046EE));
  pixelui::begin(s);
  pixelui::Header header;
  header.title = tr(STR_UI_BATTLESHIP_4046EE);
  header.back = true;
  pixelui::header(s, header);
  const auto geometry = pixelui::geometry(s.device(), 0);
  backui::publish(s.device().hasTouch, geometry.back);
  const auto body = geometry.body;
  s.setContentMarginAbsolute(
      {body.y, int16_t(s.device().width - body.right()), int16_t(s.device().height - body.bottom()), body.x});
  return body;
}
int landscapeBoardWidth(fui::Rect body) { return std::min(int(body.height) + 28, int(body.width) * 55 / 100); }
fui::Rect gridRect(fui::Rect r) {
  const int cell = std::max(2, std::min(int(r.width) - 28, int(r.height) - 24) / 10);
  return fui::makeRect(r.x + 28 + (r.width - 28 - cell * 10) / 2, r.y + 24, cell * 10, cell * 10);
}
fui::Rect cellRect(fui::Rect g, int cell) {
  const int side = g.width / 10;
  return fui::makeRect(g.x + cell % 10 * side, g.y + cell / 10 * side, side, side);
}
void lattice(toybox::Screen& s, fui::Rect g, bool labels = true) {
  const int cell = g.width / 10;
  const int font = cell >= 24 ? 0 : 3;
  for (int n = 0; n <= 10; ++n) {
    s.target().line({int16_t(g.x + n * cell), g.y}, {int16_t(g.x + n * cell), int16_t(g.bottom())},
                    n == 0 || n == 10 ? 3 : 1, dashboard::ink);
    s.target().line({g.x, int16_t(g.y + n * cell)}, {int16_t(g.right()), int16_t(g.y + n * cell)},
                    n == 0 || n == 10 ? 3 : 1, dashboard::ink);
  }
  if (!labels) return;
  for (int n = 0; n < 10; ++n) {
    char label[8];
    std::snprintf(label, sizeof(label), "%c", 'A' + n);
    dashboard::text(s, fui::makeRect(g.x + n * cell, g.y - 24, cell, 22), label, font);
    std::snprintf(label, sizeof(label), "%d", n + 1);
    dashboard::text(s, fui::makeRect(g.x - 28, g.y + n * cell, 24, cell), label, font);
  }
}
void ring(toybox::Screen& s, fui::Rect box) {
  const int side = std::max(4, box.width * 35 / 100);
  s.target().stroke(fui::makeRect(box.x + (box.width - side) / 2, box.y + (box.height - side) / 2, side, side),
                    dashboard::ink, 2, side / 2);
}
void grid(toybox::Screen& s, fui::Rect g, const bship::Side* side, bool own, bool reveal, int aim = -1,
          bool labels = true) {
  const int cell = g.width / 10;
  for (int n = 0; n < bship::kCells; ++n) {
    const auto box = cellRect(g, n);
    const bool shot = side && bship::shotAt(*side, n);
    if (!side) continue;
    if (!shot) {
      if ((own || reveal) && bship::shipAt(side->fleet, n) >= 0)
        s.target().fill(box.inset({2, 2, 2, 2}), fui::Paint::dither(fui::Color::LightGray));
      continue;
    }
    const int ship = bship::shipAt(side->fleet, n);
    if (ship < 0)
      ring(s, box);
    else {
      const int pad = own || bship::sunk(*side, ship) ? 2 : std::max(3, cell / 4);
      s.target().fill(box.inset({int16_t(pad), int16_t(pad), int16_t(pad), int16_t(pad)}), dashboard::ink);
    }
  }
  lattice(s, g, labels);
  if (aim >= 0) {
    const auto r = cellRect(g, aim).inset({3, 3, 3, 3});
    const int u = std::max(4, cell / 4);
    for (int n = 0; n < 4; ++n) {
      const int x = n % 2 ? r.right() : r.x, y = n / 2 ? r.bottom() : r.y, dx = n % 2 ? -1 : 1, dy = n / 2 ? -1 : 1;
      s.target().line({int16_t(x), int16_t(y)}, {int16_t(x + dx * u), int16_t(y)}, 3, dashboard::ink);
      s.target().line({int16_t(x), int16_t(y)}, {int16_t(x), int16_t(y + dy * u)}, 3, dashboard::ink);
    }
  }
}
void hull(toybox::Screen& s, fui::Rect g, const bship::Fleet& fleet, int chosen) {
  lattice(s, g);
  const int cell = g.width / 10;
  for (int n = 0; n < 5; ++n) {
    const auto& ship = fleet.ships[n];
    const auto bow = cellRect(g, ship.bow);
    const auto r = fui::makeRect(bow.x + 2, bow.y + 2, ship.horizontal ? cell * bship::kShipLength[n] - 4 : cell - 4,
                                 ship.horizontal ? cell - 4 : cell * bship::kShipLength[n] - 4);
    s.target().fill(r, fui::Paint::dither(fui::Color::LightGray));
    s.target().stroke(r, dashboard::ink, chosen == n ? 4 : 2);
  }
}
void legend(toybox::Screen& s, fui::Rect r) {
  ring(s, fui::makeRect(r.x, r.y, 24, 24));
  dashboard::text(s, fui::makeRect(r.x + 30, r.y, r.width / 2 - 30, 24), tr(STR_NAVAL_WATER), 3, fui::TextAlign::Left);
  s.target().fill(fui::makeRect(r.x + r.width / 2, r.y + 4, 16, 16), dashboard::ink);
  dashboard::text(s, fui::makeRect(r.x + r.width / 2 + 24, r.y, r.width / 2 - 24, 24), tr(STR_NAVAL_HIT), 3,
                  fui::TextAlign::Left);
}
}  // namespace
int startRows(const StartModel& m) { return m.hasSavedGame ? 3 : 2; }
StartRow startRowAt(const StartModel& m, int n) {
  return static_cast<StartRow>(std::clamp(n, 0, startRows(m) - 1) + (m.hasSavedGame ? 0 : 1));
}
const char* startRowLabel(StartRow r) {
  switch (r) {
    case StartRow::Continue:
      return tr(STR_UI_CONTINUE_FF3DBB);
    case StartRow::NewGame:
      return tr(STR_UI_NEW_GAME_F6F041);
    case StartRow::HowToPlay:
      return tr(STR_WORDLE_HELP);
    case StartRow::Count:
      return "";
  }
  return "";
}
fui::Rect buildStartMenu(toybox::Screen& s, const StartModel& m) {
  const auto b = begin(s);
  const bool wide = b.width > b.height;
  if (wide) {
    const int leftW = landscapeBoardWidth(b);
    const auto g = gridRect(fui::makeRect(b.x, b.y + 34, leftW, b.height - 34));
    dashboard::text(s, fui::makeRect(b.x, b.y, leftW, 28), tr(STR_NAVAL_PREVIEW), 0);
    static constexpr bship::Fleet demo{{{1, 1}, {18, 0}, {61, 0}, {76, 1}, {33, 0}}};
    const auto& fleet = m.fleet ? *m.fleet : demo;
    for (int n = 0; n < 100; ++n)
      if (bship::shipAt(fleet, n) >= 0) s.target().fill(cellRect(g, n).inset({3, 3, 3, 3}), dashboard::ink);
    lattice(s, g);
    const int count = startRows(m), x = b.x + leftW + 16, width = b.right() - x;
    const int groupH = 104 + count * 60;
    const int top = b.y + (b.height - groupH) / 2;
    dashboard::text(s, fui::makeRect(x, top, width, 30),
                    m.hasSavedGame ? tr(STR_CARDS_IN_PROGRESS) : tr(STR_NAVAL_READY), 0);
    const char* labels[] = {tr(STR_CARDS_PLAYED), tr(STR_CARDS_WINS), tr(STR_UI_STREAK_FC661A)};
    const int values[] = {m.played, m.won, m.streak};
    cardpanel::stats(s, fui::makeRect(x, top + 38, width, 58), labels, values);
    for (int n = 0; n < count; ++n) {
      const auto row = startRowAt(m, n);
      gamepanel::row(s, fui::makeRect(x, top + 112 + n * 60, width, 52), startRowLabel(row), ActionStartRow, n,
                     row == StartRow::NewGame     ? gamepanel::Mark::Restart
                     : row == StartRow::HowToPlay ? gamepanel::Mark::Help
                                                  : gamepanel::Mark::Play,
                     n == m.selected);
    }
    return g;
  }
  const int count = startRows(m), footer = count * 60 + (count - 1) * 8;
  const auto left = wide ? fui::makeRect(b.x, b.y, b.width * 53 / 100, b.height)
                         : fui::makeRect(b.x, b.y, b.width, b.height - footer - 12);
  dashboard::text(s, fui::makeRect(left.x, left.y, left.width, 34),
                  m.hasSavedGame ? tr(STR_CARDS_IN_PROGRESS) : tr(STR_NAVAL_READY), 1);
  const char* labels[] = {tr(STR_CARDS_PLAYED), tr(STR_CARDS_WINS), tr(STR_UI_STREAK_FC661A)};
  const int vals[] = {m.played, m.won, m.streak};
  cardpanel::stats(s, fui::makeRect(left.x, left.y + 42, left.width, 64), labels, vals);
  dashboard::text(s, fui::makeRect(left.x, left.y + 114, left.width, 28), tr(STR_NAVAL_PREVIEW), 0);
  const auto g = gridRect(fui::makeRect(left.x, left.y + 144, left.width, left.height - 144));
  static constexpr bship::Fleet demo{{{1, 1}, {18, 0}, {61, 0}, {76, 1}, {33, 0}}};
  const auto& fleet = m.fleet ? *m.fleet : demo;
  for (int n = 0; n < 100; ++n)
    if (bship::shipAt(fleet, n) >= 0) s.target().fill(cellRect(g, n).inset({3, 3, 3, 3}), dashboard::ink);
  lattice(s, g);
  const auto right =
      wide ? fui::makeRect(left.right() + 18, b.y + (b.height - footer) / 2, b.right() - left.right() - 18, footer)
           : fui::makeRect(b.x, b.bottom() - footer, b.width, footer);
  for (int n = 0; n < count; ++n) {
    const auto row = startRowAt(m, n);
    gamepanel::row(s, fui::makeRect(right.x, right.y + n * 68, right.width, 60), startRowLabel(row), ActionStartRow, n,
                   row == StartRow::NewGame     ? gamepanel::Mark::Restart
                   : row == StartRow::HowToPlay ? gamepanel::Mark::Help
                                                : gamepanel::Mark::Play,
                   n == m.selected);
  }
  return g;
}
fui::Rect buildPlaceChrome(toybox::Screen& s, const PlaceModel& m, Layout* out) {
  const auto b = begin(s);
  const bool wide = b.width > b.height;
  if (wide) {
    const int leftW = landscapeBoardWidth(b);
    const auto g = gridRect(fui::makeRect(b.x, b.y, leftW, b.height));
    if (out) out->place = g;
    if (m.fleet)
      hull(s, g, *m.fleet, m.selectedShip);
    else
      lattice(s, g);
    const int x = b.x + leftW + 16, width = b.right() - x;
    const int top = b.y + (b.height - 316) / 2;
    dashboard::text(s, fui::makeRect(x, top, width, 30), tr(STR_NAVAL_PLACE), 0, fui::TextAlign::Left);
    fittext::draw(s.target(), fui::makeRect(x, top + 36, width, 64),
                  m.status && *m.status ? m.status : tr(STR_NAVAL_PLACE_HELP), 4, 2);
    char label[100];
    const int chosen = std::clamp(m.selectedShip, 0, 4);
    std::snprintf(label, sizeof(label), tr(STR_NAVAL_SHIP_LENGTH), battleshiplabels::shipName(chosen),
                  bship::kShipLength[chosen]);
    dashboard::text(s, fui::makeRect(x, top + 112, width, 28), label, 0, fui::TextAlign::Left);
    const int cellW = (width - 24) / 5;
    for (int n = 0; n < 5; ++n) {
      const auto r = fui::makeRect(x + n * (cellW + 6), top + 150, cellW, 54);
      if (out) out->ships[n] = r;
      const bool selected = n == m.selectedShip;
      if (selected)
        s.target().fill(r, dashboard::ink);
      else
        dashboard::frame(s, r, 2);
      std::snprintf(label, sizeof(label), "%d", bship::kShipLength[n]);
      dashboard::text(s, fui::makeRect(r.x, r.y + 2, r.width, 26), label, 0, fui::TextAlign::Center, selected);
      const int pip = std::max(2, (cellW - 12) / 6);
      for (int j = 0; j < bship::kShipLength[n]; ++j)
        s.target().fill(fui::makeRect(r.x + 6 + j * (pip + 1), r.bottom() - pip - 8, pip, pip),
                        selected ? dashboard::paper : dashboard::ink);
      if (m.canEdit) dashboard::hit(s, r, ActionSelectShip, n);
    }
    dashboard::button(s, fui::makeRect(x, top + 216, width, 44), tr(STR_NAVAL_ROTATE),
                      m.canEdit ? ActionRotate : fui::NO_ACTION, 0, false, &extra_rotate_cw_32, 0);
    const int half = (width - 10) / 2;
    dashboard::button(s, fui::makeRect(x, top + 272, half, 44), tr(STR_UI_SHUFFLE_9E01C0),
                      m.canEdit ? ActionShuffle : fui::NO_ACTION, 0, false, nullptr, 0);
    dashboard::button(s, fui::makeRect(x + half + 10, top + 272, width - half - 10, 44), tr(STR_UI_READY_FC554D),
                      m.canEdit ? ActionReady : fui::NO_ACTION, 0, true, nullptr, 0);
    return g;
  }
  dashboard::text(s, fui::makeRect(b.x, b.y, b.width, 30), tr(STR_NAVAL_PLACE), 1, fui::TextAlign::Left);
  dashboard::text(s, fui::makeRect(b.x, b.y + 32, b.width, 28),
                  m.status && *m.status ? m.status : tr(STR_NAVAL_PLACE_HELP), 0, fui::TextAlign::Left);
  const auto area = wide ? fui::makeRect(b.x, b.y + 68, b.width * 55 / 100, b.height - 68)
                         : fui::makeRect(b.x, b.y + 68, b.width, b.height - 226);
  const auto g = gridRect(area);
  if (out) out->place = g;
  if (m.fleet)
    hull(s, g, *m.fleet, m.selectedShip);
  else
    lattice(s, g);
  const auto panel = wide ? fui::makeRect(area.right() + 16, b.y + 68, b.right() - area.right() - 16, b.height - 68)
                          : fui::makeRect(b.x, g.bottom() + 12, b.width, b.bottom() - g.bottom() - 12);
  char label[100];
  std::snprintf(label, sizeof(label), tr(STR_NAVAL_SHIP_LENGTH),
                battleshiplabels::shipName(std::clamp(m.selectedShip, 0, 4)),
                bship::kShipLength[std::clamp(m.selectedShip, 0, 4)]);
  const int rotateW = std::min(130, int(panel.width) / 3);
  s.target().fill(fui::makeRect(panel.x, panel.y, panel.width - rotateW - 8, 38), dashboard::ink);
  dashboard::text(s, fui::makeRect(panel.x + 6, panel.y, panel.width - rotateW - 20, 38), label, 0,
                  fui::TextAlign::Left, true);
  const auto rotate = fui::makeRect(panel.right() - rotateW, panel.y, rotateW, 38);
  dashboard::frame(s, rotate, 2);
  dashboard::icon(s, fui::makeRect(rotate.x + 6, rotate.y + 5, 28, 28), extra_rotate_cw_32);
  fittext::draw(s.target(), fui::makeRect(rotate.x + 42, rotate.y, rotate.width - 46, 38), tr(STR_NAVAL_ROTATE), 0, 1);
  if (m.canEdit) dashboard::hit(s, rotate, ActionRotate);
  const int w = (panel.width - 24) / 5;
  for (int n = 0; n < 5; ++n) {
    const auto r = fui::makeRect(panel.x + n * (w + 6), panel.y + 46, w, 58);
    if (out) out->ships[n] = r;
    const bool selected = n == m.selectedShip;
    if (selected)
      s.target().fill(r, dashboard::ink);
    else
      dashboard::frame(s, r, 2);
    std::snprintf(label, sizeof(label), "%d", bship::kShipLength[n]);
    dashboard::text(s, fui::makeRect(r.x, r.y + 2, r.width, 28), label, 1, fui::TextAlign::Center, selected);
    const int pip = std::min(12, (w - 10) / 5);
    for (int j = 0; j < bship::kShipLength[n]; ++j)
      s.target().fill(fui::makeRect(r.x + 5 + j * (pip + 1), r.y + 36, pip, pip),
                      selected ? dashboard::paper : dashboard::ink);
    if (m.canEdit) dashboard::hit(s, r, ActionSelectShip, n);
  }
  const int half = (panel.width - 10) / 2;
  dashboard::button(s, fui::makeRect(panel.x, panel.bottom() - 50, half, 50), tr(STR_UI_SHUFFLE_9E01C0),
                    m.canEdit ? ActionShuffle : fui::NO_ACTION, 0, false, half >= 200 ? &extra_rotate_cw_32 : nullptr,
                    1);
  dashboard::button(s, fui::makeRect(panel.x + half + 10, panel.bottom() - 50, half, 50), tr(STR_UI_READY_FC554D),
                    m.canEdit ? ActionReady : fui::NO_ACTION, 0, true, &extra_check_32, 1);
  return g;
}
fui::Rect buildBoardChrome(toybox::Screen& s, const BoardModel& m, Layout* out) {
  const auto b = begin(s);
  const bool wide = b.width > b.height;
  const int panelW = wide ? b.width - landscapeBoardWidth(b) - 16 : b.width;
  const auto panel =
      wide ? fui::makeRect(b.right() - panelW, b.y, panelW, b.height) : fui::makeRect(b.x, b.y, b.width, b.height);
  const int fleetH = wide ? 68 : 48;
  const auto notice = fui::makeRect(panel.x, panel.y, panel.width, wide ? 112 : 80);
  dashboard::frame(s, notice);
  dashboard::text(s, fui::makeRect(notice.x + 12, notice.y + 6, notice.width - 24, 28), tr(STR_NAVAL_YOUR_MOVE), 0,
                  fui::TextAlign::Left);
  char feedback[96];
  if (m.canFire && !m.ownFleet) {
    char where[4];
    bship::cellName(m.aim, where);
    std::snprintf(feedback, sizeof(feedback), tr(STR_NAVAL_TARGET_AT), where);
  } else {
    const bool hasReport = m.report && *m.report && std::strcmp(m.report, tr(STR_UI_YOUR_MOVE_AF6C56));
    std::snprintf(feedback, sizeof(feedback), "%s", hasReport ? m.report : tr(STR_NAVAL_SELECT_CELL));
  }
  fittext::draw(s.target(), fui::makeRect(notice.x + 12, notice.y + 38, notice.width - 24, notice.height - 44),
                feedback, 4, wide ? 3 : 2);
  const int fleetY = notice.bottom() + (wide ? 20 : 8);
  const int half = (panel.width - 8) / 2;
  for (int n = 0; n < 2; ++n) {
    const auto box = fui::makeRect(panel.x + n * (half + 8), fleetY, half, fleetH);
    const bool own = n == 0;
    const bool active = own == m.ownFleet;
    const bool inverse = s.frame().buttonSelected(ActionFleet, n, active);
    dashboard::button(s, box, "", ActionFleet, n, inverse, nullptr, 0);
    const int afloat = m.game ? bship::kShipCount - bship::sunkCount(m.game->side[own ? m.mySide : (m.mySide ^ 1)]) : 5;
    char label[64];
    std::snprintf(label, sizeof(label), own ? tr(STR_NAVAL_OWN_AFLOAT) : tr(STR_NAVAL_RIVAL_AFLOAT), afloat);
    fittext::draw(s.target(), box.inset({5, 8, 5, 8}), label, 0, 2, fui::TextAlign::Center,
                  inverse ? fui::Color::White : fui::Color::Black);
  }
  const auto action = fui::makeRect(panel.x, panel.bottom() - 52, panel.width, 52);
  const int gridTop = fleetY + fleetH + 8;
  const int gridBottom = action.y - 8;
  const auto area = wide ? fui::makeRect(b.x, b.y, b.width - panelW - 16, b.height)
                         : fui::makeRect(b.x, gridTop, b.width, gridBottom - gridTop);
  auto target = gridRect(area);
  const int spare = area.bottom() - target.bottom();
  target.y += std::max(0, spare / 2);
  if (out) {
    out->target = m.ownFleet ? fui::Rect{} : target;
    out->own = m.ownFleet ? target : fui::Rect{};
  }
  grid(s, target, m.game ? &m.game->side[m.ownFleet ? m.mySide : (m.mySide ^ 1)] : nullptr, m.ownFleet, m.gameOver,
       m.ownFleet ? -1 : m.aim);
  if (wide) {
    const int helpTop = fleetY + fleetH + 20;
    const int helpBottom = action.y - 16;
    const auto help = fui::makeRect(panel.x + 8, helpTop, panel.width - 16, helpBottom - helpTop);
    fittext::draw(s.target(), help, m.ownFleet ? tr(STR_NAVAL_OWN_HELP) : tr(STR_NAVAL_TARGET_HELP), 4, 4,
                  fui::TextAlign::Center);
  }
  const int actionHalf = (action.width - 8) / 2;
  const auto viewButton = fui::makeRect(action.x, action.y, actionHalf, action.height);
  const bool viewInverse = s.frame().buttonSelected(ActionOwnBoard, m.ownFleet ? 1 : 0, false);
  dashboard::button(s, viewButton, "", ActionOwnBoard, m.ownFleet ? 1 : 0, viewInverse, nullptr, 0);
  pixelui::copyCaps(feedback, sizeof(feedback), m.ownFleet ? tr(STR_NAVAL_RIVAL_BOARD) : tr(STR_NAVAL_MY_BOARD));
  fittext::draw(s.target(), viewButton.inset({2, 6, 2, 6}), feedback, 0, 2, fui::TextAlign::Center,
                viewInverse ? fui::Color::White : fui::Color::Black);
  const auto fireButton =
      fui::makeRect(action.x + actionHalf + 8, action.y, action.width - actionHalf - 8, action.height);
  const auto fireAction = m.ownFleet   ? fui::NO_ACTION
                          : m.gameOver ? ActionPlayAgain
                          : m.canFire  ? ActionFire
                                       : fui::NO_ACTION;
  const bool primary = fireAction != fui::NO_ACTION;
  const bool fireInverse = s.frame().buttonSelected(fireAction, 0, primary);
  dashboard::button(s, fireButton, "", fireAction, 0, primary, nullptr, 0);
  pixelui::copyCaps(feedback, sizeof(feedback), m.ownFleet ? tr(STR_NAVAL_READ_ONLY) : m.status);
  fittext::draw(s.target(), fireButton.inset({2, 6, 2, 6}), feedback, 0, 2, fui::TextAlign::Center,
                fireInverse ? fui::Color::White : fui::Color::Black);
  return m.ownFleet ? fui::Rect{} : target;
}

void buildResult(toybox::Screen& s, const BoardModel& m) {
  const auto b = begin(s);
  const bool won = m.game && bship::winner(*m.game) == m.mySide;
  matchresult::draw(s, b, won ? tr(STR_MATCH_WON) : tr(STR_MATCH_LOST),
                    won ? tr(STR_NAVAL_VICTORY) : tr(STR_NAVAL_DEFEAT), m.report, tr(STR_NAVAL_RESULT_HELP),
                    won ? mysteryart::NavalReady : mysteryart::NavalSunk, ActionPlayAgain, ActionResultReview,
                    ActionResultMenu);
}
void buildHowTo(toybox::Screen& s, int) {
  const auto b = begin(s);
  const bool wide = b.width > b.height;
  const int textW = b.width;
  const int row = wide ? 98 : 126;
  const StrId heads[] = {StrId::STR_NAVAL_PLACE, StrId::STR_NAVAL_PREPARE, StrId::STR_NAVAL_AIM};
  const StrId copy[] = {StrId::STR_NAVAL_HELP1, StrId::STR_NAVAL_HELP2, StrId::STR_NAVAL_HELP3};
  const fui::BitmapRef* arts[] = {&mysteryart::NavalRotate, &mysteryart::NavalReady, &mysteryart::NavalAim};
  for (int n = 0; n < 3; ++n) {
    const auto r = fui::makeRect(b.x, b.y + n * row, textW, row - 8);
    guideui::badge(s, fui::makeRect(r.x, r.y + 4, 38, 38), n + 1);
    dashboard::text(s, fui::makeRect(r.x + 52, r.y, r.width - 140, 30), I18N.get(heads[n]), 0, fui::TextAlign::Left);
    fittext::draw(s.target(), fui::makeRect(r.x + 52, r.y + 32, r.width - 140, r.height - 32), I18N.get(copy[n]), 4, 4);
    mysteryart::draw(s, fui::makeRect(r.right() - 80, r.y + 4, 76, r.height - 8), *arts[n]);
  }
  if (!wide) {
    const auto area = wide ? fui::makeRect(b.x + textW + 16, b.y, b.width - textW - 16, b.height - 86)
                           : fui::makeRect(b.x + 42, b.y + 3 * row, b.width - 64, b.height - 3 * row - 84);
    const auto g = gridRect(area);
    bship::Side demo;
    demo.fleet = {{{51, 1}, {9, 0}, {34, 0}, {76, 1}, {82, 1}}};
    bship::markShot(demo, 12);
    bship::markShot(demo, 34);
    bship::markShot(demo, 77);
    bship::markShot(demo, 83);
    grid(s, g, &demo, true, false);
    legend(s, fui::makeRect(area.x, area.bottom() + 8, area.width, 24));
  }
  gamepanel::row(s, fui::makeRect(b.x, b.bottom() - 48, b.width, 48), tr(STR_UI_GOT_IT_C3A894), ActionHelpNext, 0,
                 gamepanel::Mark::Play, true, nullptr, false);
}
}  // namespace bshipui

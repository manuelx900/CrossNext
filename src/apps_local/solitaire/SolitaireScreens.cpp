#include "SolitaireScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../cards/CardArt.h"
#include "../ui/CardPanels.h"
#include "../ui/NumberTypography.h"

namespace solitaireui {
using namespace solitaire;
namespace {
constexpr int kHeader = kHeaderBand;
constexpr int kBottomMargin = 16;
void drawPip(toybox::Screen& s, fui::Rect r, uint8_t c) { cardart::drawSuit(s, r, suitOf(c), isRed(c)); }
fui::Rect begin(toybox::Screen& s) { return cardpanel::begin(s, tr(STR_UI_SOLITAIRE_8C3ACB)); }
enum class GuideExample { Columns, Foundations, Stock, Empty };
void helpLesson(toybox::Screen& s, fui::Rect r, const char* heading, GuideExample example, const char* caption) {
  dashboard::frame(s, r, 2);
  pixelui::text(s.target(), fui::makeRect(r.x + 8, r.y + 4, r.width - 16, 34), heading, 1, fui::Color::Black,
                fui::TextAlign::Center);
  const int captionH = r.width < 250 ? 112 : 64;
  const auto art = fui::makeRect(r.x + 12, r.y + 42, r.width - 24, r.height - captionH - 52);
  guideui::text(s, fui::makeRect(r.x + 10, r.bottom() - captionH - 4, r.width - 20, captionH), caption, 4,
                fui::TextAlign::Center);
  switch (example) {
    case GuideExample::Columns:
    case GuideExample::Foundations: {
      const int h = std::min(int(art.height), (art.width - 16) / 3 * 122 / 92), w = h * 92 / 122;
      const int x = art.x + (art.width - 3 * w - 16) / 2, y = art.y + (art.height - h) / 2;
      static constexpr int down[] = {6, 5, 4}, up[] = {0, 1, 2};
      for (int n = 0; n < 3; ++n) {
        const auto suit = example == GuideExample::Columns && n == 1 ? Suit::Hearts : Suit::Spades;
        const int rank = example == GuideExample::Columns ? down[n] : up[n];
        cardart::drawCardFace(s, fui::makeRect(x + n * (w + 8), y, w, h), faceUp(makeCard(suit, rank)), h);
      }
      return;
    }
    case GuideExample::Stock:
    case GuideExample::Empty: {
      const int h = std::min(int(art.height), (art.width - 42) / 2 * 122 / 92), w = h * 92 / 122;
      const int x = art.x + (art.width - 2 * w - 34) / 2, y = art.y + (art.height - h) / 2;
      const auto left = fui::makeRect(x, y, w, h), right = fui::makeRect(x + w + 34, y, w, h);
      if (example == GuideExample::Stock)
        cardart::drawCardBack(s, left, h);
      else
        cardart::drawCardSlot(s, left);
      guideui::arrow(s, fui::makeRect(left.right() + 4, y + h / 2 - 14, 26, 28));
      cardart::drawCardFace(s, right, faceUp(makeCard(Suit::Spades, example == GuideExample::Stock ? 6 : 12)), h);
      return;
    }
  }
}
}  // namespace
cards::Suit foundationSuit(const Game& g, int pile) {
  static constexpr Suit order[] = {Suit::Clubs, Suit::Diamonds, Suit::Hearts, Suit::Spades};
  if (!g.pile(pile).empty()) return suitOf(g.pile(pile).top());
  bool occupied[4]{};
  for (int p = kFirstFoundation; p < kFirstTableau; ++p)
    if (!g.pile(p).empty()) occupied[int(suitOf(g.pile(p).top()))] = true;
  int emptyBefore = 0;
  for (int p = kFirstFoundation; p < pile; ++p)
    if (g.pile(p).empty()) ++emptyBefore;
  for (auto suit : order)
    if (!occupied[int(suit)] && emptyBefore-- == 0) return suit;
  return Suit::Clubs;
}
int Layout::cardAt(const Game& game, int pile, int ty) const {
  if (pile < 0 || pile >= kPileCount || game.pile(pile).empty()) return -1;
  const auto& p = game.pile(pile);
  if (pile < kFirstTableau) return p.count - 1;
  for (int n = p.count - 1; n >= 0; --n)
    if (ty - y[pile] >= cardOffset[pile - kFirstTableau][n]) return n;
  return -1;
}
void buildBoard(toybox::Screen& s, const BoardModel& m, Layout& l) {
  if (!m.game) return;
  const auto& g = *m.game;
  const auto b = begin(s);
  auto& t = s.target();
  const int h = b.y - 12, right = s.device().safeRect().right() - 112;
  const auto menu = fui::makeRect(right - 126, h - 48, 120, 40);
  const auto undo = fui::makeRect(menu.x - 170, menu.y, 160, 40);
  t.stroke(menu, dashboard::paper, 2);
  t.stroke(undo, dashboard::paper, 2);
  gamepanel::mark(s, fui::makeRect(menu.x + 8, menu.y + 8, 24, 24), gamepanel::Mark::Menu, true);
  dashboard::text(s, fui::makeRect(menu.x + 36, menu.y, menu.width - 40, menu.height), tr(STR_WORDLE_MENU), 0,
                  fui::TextAlign::Center, true);
  dashboard::icon(s, fui::makeRect(undo.x + 8, undo.y + 8, 24, 24), extra_dashboard_undo_32, true);
  dashboard::text(s, fui::makeRect(undo.x + 38, undo.y, undo.width - 42, undo.height), tr(STR_UI_UNDO_55AD59), 0,
                  fui::TextAlign::Center, true);
  dashboard::hit(s, menu, ActionButton, ButtonMenu);
  if (g.canUndo()) dashboard::hit(s, undo, ActionButton, ButtonUndo);
  const auto strip = fui::makeRect(0, h, s.device().width, 30);
  dashboard::frame(s, strip, 2);
  char label[64];
  if (m.showMoves) {
    std::snprintf(label, sizeof(label), tr(STR_CARDS_MOVE), g.moveCount());
    dashboard::text(s, fui::makeRect(b.x, strip.y, b.width / 2, strip.height), label, 0, fui::TextAlign::Left);
  }
  std::snprintf(label, sizeof(label), "%s", g.drawThree() ? tr(STR_CARDS_DRAW_3) : tr(STR_CARDS_DRAW_1));
  dashboard::text(s, fui::makeRect(b.x + b.width / 2, strip.y, b.width / 2, strip.height), label, 0,
                  fui::TextAlign::Right);
  const int pitch = b.width / 7, cw = std::min(94, pitch - 12), ch = cw * 4 / 3;
  const int top = strip.bottom() + 16, tableau = top + ch + 24;
  l.cardWidth = cw;
  l.cardHeight = ch;
  l.hasSelection = false;
  for (int p = 0; p < kPileCount; ++p) {
    int col = p < 2 ? p : p - kFirstTableau;
    if (p >= kFirstFoundation && p < kFirstTableau) {
      static constexpr int visual[] = {0, 1, 3, 2};
      col = 3 + visual[int(foundationSuit(g, p))];
    }
    const auto r = fui::makeRect(b.x + col * pitch + 2, p < kFirstTableau ? top : tableau, cw, ch);
    l.x[p] = r.x;
    l.y[p] = r.y;
    const auto& pile = g.pile(p);
    int bottom = r.bottom();
    if (pile.empty()) {
      cardart::drawCardSlot(s, r);
      if (p >= kFirstFoundation && p < kFirstTableau) {
        cardart::drawSuit(s, fui::makeRect(r.x + (cw - 46) / 2, r.y + (ch - 46) / 2, 46, 46), foundationSuit(g, p),
                          true);
      }
    } else if (p == kStockPile) {
      for (int n = 2; n >= 0; --n) {
        const auto card = fui::makeRect(r.x + n * 3, r.y + n * 3, cw, ch);
        t.fill(card, dashboard::paper, 7);
        t.stroke(card, dashboard::ink, 2, 7);
      }
      t.stroke(r.inset({7, 7, 7, 7}), dashboard::ink, 2, 5);
      cardart::drawSuit(s, fui::makeRect(r.x + (cw - 46) / 2, r.y + (ch - 46) / 2, 46, 46), Suit::Diamonds, true);
    } else if (p < kFirstTableau) {
      const int shown = p == kWastePile && g.drawThree() ? std::min(3, int(pile.count)) : 1;
      for (int n = 0; n < shown; ++n) {
        auto card = fui::makeRect(r.x + n * 28, r.y, cw, ch);
        cardart::drawCardFace(s, card, pile.cards[pile.count - shown + n], ch,
                              n + 1 < shown ? cardart::Fan::Sideways : cardart::Fan::None);
        if (m.selectedPile == p && n + 1 == shown) {
          l.selection = card;
          l.hasSelection = true;
        }
      }
      if (p == kWastePile) {
        dashboard::hit(s, fui::makeRect(r.x, r.y, cw + (shown - 1) * 28, ch), ActionPile, p);
        continue;
      }
    } else {
      const int room = std::max(0, b.bottom() - tableau - ch), colIndex = p - kFirstTableau;
      int nominal = 0;
      for (int n = 0; n + 1 < pile.count; ++n) nominal += isFaceUp(pile.cards[n]) ? 28 : 14;
      int offset = 0, wanted = 0;
      for (int n = 0; n < pile.count; ++n) {
        l.cardOffset[colIndex][n] = offset;
        wanted += isFaceUp(pile.cards[n]) ? 28 : 14;
        offset = nominal > room ? wanted * room / std::max(1, nominal) : wanted;
      }
      for (int n = 0; n < pile.count; ++n) {
        const auto card = fui::makeRect(r.x, r.y + l.cardOffset[colIndex][n], cw, ch);
        const int visible = n + 1 < pile.count ? l.cardOffset[colIndex][n + 1] - l.cardOffset[colIndex][n] : ch;
        if (isFaceUp(pile.cards[n]))
          cardart::drawCardFace(s, card, pile.cards[n], visible, cardart::Fan::Down);
        else
          cardart::drawCardBack(s, card, visible);
        if (m.selectedPile == p && m.selectedCard == n) {
          l.selection = fui::makeRect(card.x, card.y, cw, r.y + l.cardOffset[colIndex][pile.count - 1] + ch - card.y);
          l.hasSelection = true;
        }
      }
      bottom = r.y + l.cardOffset[colIndex][pile.count - 1] + ch;
    }
    const auto hit = fui::makeRect(r.x, r.y, cw, bottom - r.y);
    if (p >= kFirstTableau && !pile.empty()) {
      s.frame().hit(hit, ActionPile, p, fui::InputTouch | fui::InputPointerOnly);
      int first = 0;
      while (first + 1 < pile.count && !isFaceUp(pile.cards[first])) ++first;
      const int column = p - kFirstTableau, count = pile.count - first;
      // Compressed offsets distribute rounding across the entire face-up run.
      const int span = l.cardOffset[column][pile.count - 1] - l.cardOffset[column][first];
      const int height = count > 1 ? std::max(count, (span * count + count - 2) / (count - 1)) : ch;
      s.frame().navigationGrid(fui::makeRect(r.x, r.y + l.cardOffset[column][first], cw, height), 1, count);
    } else
      s.frame().hit(hit, ActionPile, p);
  }
  if (l.hasSelection) {
    t.stroke(l.selection.inset({-4, -4, -4, -4}), dashboard::ink, 2, 8);
    t.stroke(l.selection, dashboard::ink, 3, 6);
  }
  if (g.canAutoFinish())
    dashboard::button(s, fui::makeRect(b.x + 2 * pitch, top + 38, pitch - 8, 48), tr(STR_UI_FINISH_2D355F),
                      ActionButton, ButtonFinish);
}
void buildMenu(toybox::Screen& s, const MenuModel& m) {
  const auto b = begin(s);
  cardpanel::title(s, fui::makeRect(b.x, b.y, b.width, 44),
                   m.hasSave ? tr(STR_CARDS_IN_PROGRESS) : tr(STR_CARDS_READY));
  char label[80];
  std::snprintf(label, sizeof(label), tr(STR_CARDS_MOVES_DRAW), m.savedMoves,
                m.savedDrawThree ? tr(STR_CARDS_DRAW_3) : tr(STR_CARDS_DRAW_1));
  dashboard::text(s, fui::makeRect(b.x, b.y + 46, b.width, 34), label, 1, fui::TextAlign::Left);
  const int top = b.y + 94, leftW = b.width * 44 / 100, gap = 12;
  const auto art = fui::makeRect(b.x, top, leftW, b.bottom() - top - 94);
  dashboard::frame(s, art);
  referenceart::draw(s, art.inset({18, 18, 18, 18}), referenceart::SolitaireFan);
  const char* labels[] = {tr(STR_CARDS_PLAYED), tr(STR_CARDS_WINS), tr(STR_UI_STREAK_FC661A)};
  const int values[] = {m.played, m.wins, m.streak};
  cardpanel::stats(s, fui::makeRect(b.x, art.bottom() + 8, leftW, 86), labels, values);
  const char* actions[] = {m.hasSave ? tr(STR_QUEST_CONTINUE) : tr(STR_UI_START_7196E7), tr(STR_UI_NEW_GAME_F6F041),
                           tr(STR_WORDLE_HELP), tr(STR_CARDS_SETTINGS)};
  const int ids[] = {MenuResume, MenuNew, MenuHelp, MenuSettings};
  static constexpr gamepanel::Mark marks[] = {gamepanel::Mark::Play, gamepanel::Mark::New, gamepanel::Mark::Help,
                                              gamepanel::Mark::Settings};
  const int rowH = (b.bottom() - top - 24) / 4;
  for (int n = 0; n < 4; ++n)
    gamepanel::row(s, fui::makeRect(b.x + leftW + gap, top + n * (rowH + 8), b.width - leftW - gap, rowH), actions[n],
                   ActionButton, ids[n], marks[n], n == 0, nullptr, true,
                   n == 1   ? &referenceart::Page
                   : n == 3 ? &referenceart::Gear
                            : nullptr);
}
void buildSetup(toybox::Screen& s, bool drawThree, bool replacing) {
  const auto b = begin(s);
  cardpanel::title(s, fui::makeRect(b.x, b.y, b.width, 48), tr(STR_UI_NEW_GAME_F6F041));
  const int w = (b.width - 16) / 2, top = b.y + 60, height = b.height - 152;
  for (int n = 0; n < 2; ++n) {
    const auto r = fui::makeRect(b.x + n * (w + 16), top, w, height);
    dashboard::frame(s, r);
    if (drawThree == bool(n)) dashboard::frame(s, r.inset({5, 5, 5, 5}), 2);
    cardpanel::choice(s, fui::makeRect(r.x + 8, r.y + 8, r.width - 16, 54),
                      n ? tr(STR_CARDS_DRAW_THREE) : tr(STR_CARDS_DRAW_ONE), ActionOption, n, drawThree == bool(n));
    referenceart::draw(s, fui::makeRect(r.x + 20, r.y + 74, r.width - 40, r.height - 120),
                       n ? referenceart::DrawThree : referenceart::DrawOne);
    dashboard::text(s, fui::makeRect(r.x + 10, r.bottom() - 40, r.width - 20, 34),
                    n ? tr(STR_CARDS_THREE_TURN) : tr(STR_CARDS_ONE_TURN), 0);
    dashboard::hit(s, fui::makeRect(r.x, r.y + 64, r.width, r.height - 64), ActionOption, n);
  }
  if (replacing)
    dashboard::text(s, fui::makeRect(b.x, b.bottom() - 82, b.width, 26), tr(STR_CARDS_REPLACE), 0,
                    fui::TextAlign::Left);
  dashboard::button(s, fui::makeRect(b.x, b.bottom() - 50, w, 50), tr(STR_WORDLE_CANCEL), ActionButton, Cancel, false,
                    nullptr, 1);
  dashboard::button(s, fui::makeRect(b.x + w + 16, b.bottom() - 50, w, 50), tr(STR_CARDS_DEAL), ActionButton, Deal,
                    true, nullptr, 1);
}
void buildSettings(toybox::Screen& s, const Options& o, bool failed) {
  const auto b = begin(s);
  cardpanel::title(s, fui::makeRect(b.x, b.y, b.width, 52), tr(STR_CARDS_SETTINGS));
  const char *draw[] = {tr(STR_CARDS_ONE), tr(STR_CARDS_THREE)}, *yes[] = {tr(STR_CARDS_YES), tr(STR_CARDS_NO)};
  const int row = (b.height - 174) / 3;
  cardpanel::setting(s, fui::makeRect(b.x, b.y + 64, b.width, row), tr(STR_CARDS_DEFAULT_DRAW), draw, 2, o.drawThree,
                     ActionOption, 0);
  cardpanel::setting(s, fui::makeRect(b.x, b.y + 72 + row, b.width, row), tr(STR_CARDS_CONFIRM_NEW), yes, 2,
                     !o.confirmNew, ActionOption, 2);
  cardpanel::setting(s, fui::makeRect(b.x, b.y + 80 + 2 * row, b.width, row), tr(STR_CARDS_SHOW_MOVES), yes, 2,
                     !o.showMoves, ActionOption, 4);
  dashboard::text(s, fui::makeRect(b.x, b.bottom() - 80, b.width, 28),
                  failed ? tr(STR_CHESS_PUZZLE_SAVE_ERROR) : tr(STR_CARDS_DRAW_NEXT), 0, fui::TextAlign::Left);
  dashboard::button(s, fui::makeRect(b.x, b.bottom() - 50, b.width, 50), tr(STR_CARDS_SAVE), ActionButton, SaveOptions,
                    true, nullptr, 1);
}
void buildHelp(toybox::Screen& s, int page) {
  const auto b = begin(s);
  page = std::clamp(page, 0, kHelpPages - 1);
  const bool wide = b.width > b.height;
  const int titleH = wide ? 44 : 50;
  pixelui::text(s.target(), fui::makeRect(b.x, b.y, b.width, titleH),
                page == 0 ? tr(STR_CARDS_GUIDE_CARDS) : tr(STR_WORDLE_HELP), 1);
  const auto area = fui::makeRect(b.x, b.y + titleH + 10, b.width, b.height - titleH - 78);
  if (page == 0)
    cardpanel::deckLesson(s, area, false);
  else {
    const int lesson = page - 1;
    const int noticeH = wide ? 64 : 104;
    const int w = (area.width - 12) / 2, h = area.height - noticeH - 12;
    helpLesson(s, fui::makeRect(area.x, area.y, w, h), lesson ? tr(STR_CARDS_STOCK) : tr(STR_CARDS_COLUMNS),
               lesson ? GuideExample::Stock : GuideExample::Columns,
               lesson ? tr(STR_CARDS_TAP_STOCK) : tr(STR_CARDS_ALTERNATE));
    helpLesson(s, fui::makeRect(area.x + w + 12, area.y, w, h), lesson ? tr(STR_CARDS_EMPTY) : tr(STR_CARDS_BASES),
               lesson ? GuideExample::Empty : GuideExample::Foundations,
               lesson ? tr(STR_CARDS_KING_ONLY) : tr(STR_CARDS_ASCEND));
    const auto notice = fui::makeRect(area.x, area.bottom() - noticeH, area.width, noticeH);
    dashboard::frame(s, notice, 2);
    guideui::text(s, notice.inset({int16_t(wide ? 3 : 6), 10, int16_t(wide ? 3 : 6), 10}),
                  lesson ? tr(STR_CARDS_GUIDE_REVEAL) : tr(STR_CARDS_GUIDE_WIN), 4, fui::TextAlign::Center);
  }
  pageui::build(s, fui::makeRect(b.x, b.bottom() - 48, b.width, 48), page + 1, kHelpPages, ActionPage, page - 1,
                ActionPage, page + 1);
}
void buildWin(toybox::Screen& screen, const WinModel& model) {
  auto& target = screen.target();
  const fui::Rect band = screen.device().screen();
  const fui::Paint ink = fui::Paint::solid(fui::Color::Black);

  fui::HeaderProps header;
  header.title = tr(STR_UI_SOLITAIRE_8C3ACB);
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);

  // The cascade. Every physical solitaire ends with the deck coming off the
  // foundations and bouncing down the table, and it is the one image everyone
  // who has played the game recognises. We cannot animate it, so we draw where
  // it landed: fifty-two cards fanned across the foot of the screen, arcing.
  //
  // This is ornament made of the game's own material, which is the only kind
  // this design language allows -- and unlike a static mascot it is only ever
  // seen once per win, so it cannot become wallpaper.
  const int cascadeCount = 28;
  const int cardW = 58;
  const int cardH = 86;
  const int step = (band.width - cardW - 36) / (cascadeCount - 1);
  for (int i = 0; i < cascadeCount; ++i) {
    // A shallow parabola, lowest in the middle, so the run reads as a bounce
    // rather than as a row.
    const int fromCentre = i - cascadeCount / 2;
    const int lift = (fromCentre * fromCentre * 30) / ((cascadeCount / 2) * (cascadeCount / 2));
    const fui::Rect card = fui::makeRect(18 + i * step, band.height - kBottomMargin - cardH - lift, cardW, cardH);
    target.fill(card, fui::Paint::solid(fui::Color::White), 6);
    target.stroke(card, ink, toybox::kHairline, 6);
    // The pip goes in the strip the next card will not cover. Centring it
    // looked right in the code and came out as half a diamond on screen,
    // because every card but the last is overlapped by the one after it.
    if (i % 3 == 0) {
      drawPip(screen, fui::makeRect(card.x + 3, card.y + 24, 20, 24), makeCard(static_cast<Suit>((i / 3) % kSuits), 0));
    }
  }

  const int top = toybox::chromeBelow(kHeader) + 27;
  fui::TextStyle hero;
  hero.font = toybox::kDisplayFont;
  hero.align = fui::TextAlign::Center;
  target.text(fui::makeRect(0, top, band.width, 64), tr(STR_UI_ALL_FIFTY_TWO_HOME_DD606E), hero);

  // Varied off the move count so the second win does not read exactly like the
  // first. Cheap personality: it is a string table and a modulo.
  static constexpr UiLabel kBrags[4] = {
      trLabel(STR_UI_D_MOVES_THE_DECK_NEVER_STOOD_29208E),
      trLabel(STR_UI_D_MOVES_AND_NOT_ONE_OF_278B36),
      trLabel(STR_UI_D_MOVES_SOMEBODY_HAS_DONE_THIS_A44912),
      trLabel(STR_UI_D_MOVES_THE_STOCK_IS_STILL_79F800),
  };
  char line[96];
  std::snprintf(line, sizeof(line), kBrags[model.moves % 4].get(), model.moves);
  fui::TextStyle sub;
  sub.font = toybox::kUiFont;
  sub.align = fui::TextAlign::Center;
  target.text(toybox::inkCentred(fui::makeRect(0, top + 70, band.width, 26), toybox::kUiCut), line, sub);

  std::snprintf(line, sizeof(line), tr(STR_UI_D_CLEARED_D_IN_A_ROW_90558C), model.wins, model.streak);
  fui::TextStyle stats;
  stats.font = toybox::kTileFont;
  stats.align = fui::TextAlign::Center;
  target.text(fui::makeRect(0, top + 104, band.width, 24), line, stats);

  const int buttonW = 210;
  const int buttonY = top + 144;
  fui::ButtonProps again;
  again.label = tr(STR_UI_DEAL_AGAIN_146E1A);
  again.action = ActionButton;
  again.value = WinAgain;
  again.borderEdges = fui::EdgesNone;
  screen.button(again,
                fui::makeRect(band.width / 2 - buttonW - toybox::kGutter / 2, buttonY, buttonW, toybox::kPillHeight));

  fui::ButtonProps menu;
  menu.label = tr(STR_UI_BACK_TO_THE_TABLE_6590E8);
  menu.action = ActionButton;
  menu.value = WinMenu;
  menu.styles = toybox::rowStyles();
  screen.button(menu, fui::makeRect(band.width / 2 + toybox::kGutter / 2, buttonY, buttonW, toybox::kPillHeight));
}

}  // namespace solitaireui

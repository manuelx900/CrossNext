#include "ForeheadScreens.h"

#include <I18n.h>

#include <cstdio>
#include <cstring>

#include "../ui/PageIndicator.h"
#include "../ui/ToyboxFormat.h"
#include "ForeheadCategoryIcons.h"

namespace foreheadui {

namespace {

namespace fh = forehead;

fh::CategoryInfo localizedCategory(int index) {
  static constexpr StrId titles[] = {StrId::STR_FOREHEAD_CATEGORY_TITLE_0,  StrId::STR_FOREHEAD_CATEGORY_TITLE_1,
                                     StrId::STR_FOREHEAD_CATEGORY_TITLE_2,  StrId::STR_FOREHEAD_CATEGORY_TITLE_3,
                                     StrId::STR_FOREHEAD_CATEGORY_TITLE_4,  StrId::STR_FOREHEAD_CATEGORY_TITLE_5,
                                     StrId::STR_FOREHEAD_CATEGORY_TITLE_6,  StrId::STR_FOREHEAD_CATEGORY_TITLE_7,
                                     StrId::STR_FOREHEAD_CATEGORY_TITLE_8,  StrId::STR_FOREHEAD_CATEGORY_TITLE_9,
                                     StrId::STR_FOREHEAD_CATEGORY_TITLE_10, StrId::STR_FOREHEAD_CATEGORY_TITLE_11,
                                     StrId::STR_FOREHEAD_CATEGORY_TITLE_12, StrId::STR_FOREHEAD_CATEGORY_TITLE_13,
                                     StrId::STR_FOREHEAD_CATEGORY_TITLE_14, StrId::STR_FOREHEAD_CATEGORY_TITLE_15,
                                     StrId::STR_FOREHEAD_CATEGORY_TITLE_16};
  static constexpr StrId hints[] = {
      StrId::STR_FOREHEAD_CATEGORY_HINT_0,  StrId::STR_FOREHEAD_CATEGORY_HINT_1,  StrId::STR_FOREHEAD_CATEGORY_HINT_2,
      StrId::STR_FOREHEAD_CATEGORY_HINT_3,  StrId::STR_FOREHEAD_CATEGORY_HINT_4,  StrId::STR_FOREHEAD_CATEGORY_HINT_5,
      StrId::STR_FOREHEAD_CATEGORY_HINT_6,  StrId::STR_FOREHEAD_CATEGORY_HINT_7,  StrId::STR_FOREHEAD_CATEGORY_HINT_8,
      StrId::STR_FOREHEAD_CATEGORY_HINT_9,  StrId::STR_FOREHEAD_CATEGORY_HINT_10, StrId::STR_FOREHEAD_CATEGORY_HINT_11,
      StrId::STR_FOREHEAD_CATEGORY_HINT_12, StrId::STR_FOREHEAD_CATEGORY_HINT_13, StrId::STR_FOREHEAD_CATEGORY_HINT_14,
      StrId::STR_FOREHEAD_CATEGORY_HINT_15, StrId::STR_FOREHEAD_CATEGORY_HINT_16};
  auto category = fh::kCategories[index];
  category.title = I18N.get(titles[index]);
  category.hint = I18N.get(hints[index]);
  return category;
}

// ---------------------------------------------------------------------------
// Shared chrome

fui::TextStyle textStyle(const fui::FontId font, const fui::TextAlign align,
                         const fui::Color colour = fui::Color::Black) {
  fui::TextStyle style;
  style.font = font;
  // Named even when it is the default the component would apply anyway.
  // FONT_SLOT_SMALL is 0, and textStyleUnset() calls a style unset when its
  // font is 0 and every other field is default -- so a small-slot label with
  // nothing else set comes back at the theme's size, looking exactly as though
  // the assignment never happened.
  style.align = align;
  style.color = colour;
  return style;
}

void drawText(toybox::Screen& screen, const fui::Rect& box, const char* text, const fui::FontId font,
              const fui::TextAlign align, const toybox::CutMetrics&, const fui::Color colour = fui::Color::Black) {
  screen.target().text(toybox::inkCentred(screen.target(), box, textStyle(font, align, colour)), text,
                       textStyle(font, align, colour));
}

// The header band with the offset rule under it, as jaipur, yahtzee and the
// dungeon wear it. A local copy rather than a shared helper, for the reason
// LinkScreens gives: a copy is cheaper than a header dependency between apps.
void toyboxChrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr) {
  fui::HeaderProps header;
  header.title = title;
  header.borderEdges = fui::EdgesNone;
  if (rightLabel && *rightLabel) {
    fui::TextStyle labelStyle;
    labelStyle.font = toybox::kUiFont;
    header.rightReserve = screen.target().measureText(labelStyle.font, rightLabel, labelStyle).width + 12;
  }
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);

  // The right label is drawn HERE rather than handed to header.rightLabel, and
  // that is a correctness fix rather than a preference.
  //
  // The component centres each run on its own LINE BOX. The title is the 63px
  // display cut and the label the 42px UI cut, and Jersey's leading differs
  // between them, so centring both on the same 76px band leaves their ink 11 to
  // 13 pixels apart: measured, the title lands dead centre and the label rides
  // low enough to see. Every app in this fork that passes rightLabel has it.
  // inkCentred solves for the rect that puts the LABEL's ink where the title's
  // is. (The colour matters too: subtitleText defaults to black on the black
  // band, which is a label indistinguishable from one never set -- jaipur paid
  // for that one.)
  if (rightLabel != nullptr && *rightLabel != '\0') {
    // The band's VISIBLE rows, asked for rather than rebuilt: ink placed over
    // the whole band centres partly under the bezel and rides above the title
    // beside it.
    const fui::Rect band = toybox::headerInkRect(screen).inset(fui::Insets{0, toybox::kMargin, 0, 0});
    drawText(screen, band, rightLabel, toybox::kUiFont, fui::TextAlign::Right, toybox::kUiCut, fui::Color::White);
  }

  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

// ---------------------------------------------------------------------------
// The paging pips, the shelf folder's shape.
//
// Pips rather than prev/next arrows: arrows are up to pageCount-1 taps to the
// far end and say nothing about where you are, and on this device a right
// chevron already means "opens". A small centred cluster with air round it, and
// the targets are contiguous WITHIN the cluster only, so the screen edges do
// nothing.
constexpr int16_t kPipSize = 10;
constexpr int16_t kPipStep = 34;
constexpr int16_t kPipBand = 44;

void drawPips(toybox::Screen& screen, const fui::Rect& band, const int pages, const int current,
              const fui::ActionId action) {
  if (pages > 1) pageui::build(screen, band, current + 1, pages, action, current - 1, action, current + 1);
}

// ---------------------------------------------------------------------------
// The two key bands.
//
// This is the only screen in the fork whose labels are placed by where the
// HARDWARE is rather than by where the layout wants them, and it has to be:
// the guesser is holding the panel against their forehead and cannot see any
// of it. The bands are for the room, which reads them out.
//
// Rotated into LandscapeCounterClockwise the device's two side keys land on the
// long edges of what you are looking at -- the key that is on the physical left
// in portrait is at the BOTTOM, and the one on the physical right is at the TOP
// (GfxRenderer.cpp rotateCoordinates: portrait maps logical y to panel x, so
// the portrait right edge becomes the landscape top). So a band that runs the
// whole width of an edge cannot be pointing at the wrong key: the entire edge
// is the label.
//
// Bottom is GOT IT and top is PASS, which is the phone game's tilt-down and
// tilt-up in the only vocabulary this hardware has.
constexpr int16_t kKeyBandHeight = 54;

void drawKeyBands(toybox::Screen& screen) {
  HeaderBackTapTarget::clear();
  const int16_t w = screen.device().screen().width;
  const int16_t h = screen.device().screen().height;
  const fui::Color ground = fui::Color::Black;
  const fui::Color ink = fui::Color::White;

  const fui::Rect top = fui::makeRect(0, 0, w, kKeyBandHeight);
  const fui::Rect bottom = fui::makeRect(0, static_cast<int16_t>(h - kKeyBandHeight), w, kKeyBandHeight);
  screen.target().fill(top, fui::Paint::solid(ground));
  screen.target().fill(bottom, fui::Paint::solid(ground));

  drawText(screen, top, tr(STR_UI_PASS_BD564D), toybox::kBodyFont, fui::TextAlign::Center, toybox::kDisplayCut, ink);
  drawText(screen, bottom, tr(STR_UI_GOT_IT_C3A894), toybox::kBodyFont, fui::TextAlign::Center, toybox::kDisplayCut,
           ink);
}

// Touch, standing in for the two keys.
//
// Every page-key action in this fork stays reachable by touch, and this one has
// to be as well -- but not as two full halves. The guesser's fingers curl over
// the long edges to reach the keys, which is exactly where a half-screen target
// would be, so the tappable region is a band across the MIDDLE of each half,
// clear of every grip. A player who wants to play with a thumb still can; a
// player holding it normally cannot answer their own card by accident.
constexpr int16_t kTouchInsetX = 140;
constexpr int16_t kTouchInsetY = 90;

void registerKeyTouch(toybox::Screen& screen) {
  const int16_t w = screen.device().screen().width;
  const int16_t h = screen.device().screen().height;
  const int16_t bandW = static_cast<int16_t>(w - kTouchInsetX * 2);
  const int16_t bandH = static_cast<int16_t>(h / 2 - kTouchInsetY);
  screen.frame().hit(fui::makeRect(kTouchInsetX, kTouchInsetY, bandW, bandH), ActionMissed);
  screen.frame().hit(fui::makeRect(kTouchInsetX, static_cast<int16_t>(h / 2), bandW, bandH), ActionGot);
}

}  // namespace

// ---------------------------------------------------------------------------
// The card's type ladder.

CardLayout layOutCard(toybox::Screen& screen, const fui::Rect& box, const char* text) {
  CardLayout out;
  if (text == nullptr || *text == '\0') return out;
  const int length = static_cast<int>(std::strlen(text));

  // Largest first. All three are bound at once (title = 64, small = 44,
  // body = 30) precisely so this can measure them without a rebind, which a
  // freestanding builder has no way to ask for.
  const fui::FontId ladder[] = {toybox::kDisplayFont, toybox::kSmallFont, toybox::kBodyFont};

  for (const fui::FontId font : ladder) {
    const fui::TextStyle style = textStyle(font, fui::TextAlign::Center);
    const int16_t lineHeight = screen.target().lineHeight(font);

    // Greedy wrap on spaces. A break inside a word is never taken: the design
    // language settled that the day a hyphenated tile was rejected on sight.
    int16_t start[kCardMaxLines] = {};
    int16_t end[kCardMaxLines] = {};
    int16_t width[kCardMaxLines] = {};
    int lines = 0;
    int cursor = 0;
    bool fits = true;

    while (cursor < length && fits) {
      if (lines == kCardMaxLines) {
        fits = false;
        break;
      }
      int lineEnd = length;
      int accepted = -1;
      // Walk break points right to left: the last space at which the run still
      // fits is the greedy choice.
      for (int at = length; at > cursor; --at) {
        if (at != length && text[at] != ' ') continue;
        char buffer[fh::kMaxEntryLen + 1];
        const int count = at - cursor;
        if (count > fh::kMaxEntryLen) continue;
        std::memcpy(buffer, text + cursor, static_cast<size_t>(count));
        buffer[count] = '\0';
        const int16_t measured = screen.target().measureText(font, buffer, style).width;
        if (measured <= box.width) {
          lineEnd = at;
          accepted = measured;
          break;
        }
      }
      if (accepted < 0) {
        // Not even the first word fits at this cut.
        fits = false;
        break;
      }
      start[lines] = static_cast<int16_t>(cursor);
      end[lines] = static_cast<int16_t>(lineEnd);
      width[lines] = static_cast<int16_t>(accepted);
      ++lines;
      cursor = lineEnd;
      while (cursor < length && text[cursor] == ' ') ++cursor;
    }

    if (!fits || lines == 0) continue;
    // Leading is tightened to the ink rather than the line box: Jersey carries
    // a 133px line box round an 82px capital, and three of those is more than
    // the panel is tall.
    const int16_t stacked = static_cast<int16_t>(lines * lineHeight - (lines - 1) * lineHeight / 4);
    if (stacked > box.height) continue;

    out.font = font;
    out.lines = lines;
    out.lineHeight = lineHeight;
    for (int i = 0; i < lines; ++i) {
      out.start[i] = start[i];
      out.end[i] = end[i];
      out.width[i] = width[i];
    }
    return out;
  }

  // Nothing fit. The generator's length cap makes this unreachable for shipped
  // content, so it means somebody raised the cap without re-checking the card:
  // draw the smallest cut on one line and let it clip rather than draw nothing.
  out.font = toybox::kBodyFont;
  out.lines = 1;
  out.lineHeight = screen.target().lineHeight(toybox::kBodyFont);
  out.start[0] = 0;
  out.end[0] = static_cast<int16_t>(length);
  out.width[0] = box.width;
  return out;
}

namespace {

// Draws a laid-out card centred in `box`.
void drawCard(toybox::Screen& screen, const fui::Rect& box, const char* text, const CardLayout& layout,
              const fui::Color colour) {
  const int16_t gap = static_cast<int16_t>(layout.lineHeight - layout.lineHeight / 4);
  const int16_t stacked = static_cast<int16_t>(layout.lineHeight + (layout.lines - 1) * gap);
  int16_t y = static_cast<int16_t>(box.y + (box.height - stacked) / 2);

  const toybox::CutMetrics cut = layout.font == toybox::kDisplayFont ? toybox::kHugeCut
                                 : layout.font == toybox::kSmallFont ? toybox::kLargeCut
                                                                     : toybox::kDisplayCut;
  char buffer[fh::kMaxEntryLen + 1];
  for (int line = 0; line < layout.lines; ++line) {
    const int count = layout.end[line] - layout.start[line];
    if (count <= 0 || count > fh::kMaxEntryLen) continue;
    std::memcpy(buffer, text + layout.start[line], static_cast<size_t>(count));
    buffer[count] = '\0';
    drawText(screen, fui::makeRect(box.x, y, box.width, layout.lineHeight), buffer, layout.font, fui::TextAlign::Center,
             cut, colour);
    y = static_cast<int16_t>(y + gap);
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// The front door (portrait)

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  const fh::CategoryInfo& category = localizedCategory(model.category);
  toyboxChrome(screen, tr(STR_UI_FOREHEAD_82AFB5));

  constexpr int16_t kDoorsHeight = 3 * toybox::kRowHeight + 2 * 4;

  // The play control is a BOX with a play mark in it, not a line of type that
  // happens to be tappable.
  //
  // It used to be the category name with "TAP TO PLAY" under it, on the
  // reasoning that the biggest thing on the screen needs no button. That is
  // true of a headline you already know is a button, and this one nobody did:
  // the three rows below it are bordered and look like controls, so the one
  // thing that actually starts the game was the only unbordered thing on the
  // screen. A border and a triangle cost two draws and remove the guess.
  const fui::Rect play = screen.takeTop(140);
  screen.target().stroke(play, fui::Paint::solid(fui::Color::Black), toybox::kRule);

  constexpr int16_t kMark = 44;
  const int16_t markLeft = static_cast<int16_t>(play.x + play.width - toybox::kMargin - kMark);
  const int16_t markMid = static_cast<int16_t>(play.y + play.height / 2);
  // A solid triangle, which is the one glyph every device on earth agrees means
  // start. Drawn rather than vendored: three points need no asset.
  screen.target().triangle(fui::Point{markLeft, static_cast<int16_t>(markMid - kMark / 2)},
                           fui::Point{markLeft, static_cast<int16_t>(markMid + kMark / 2)},
                           fui::Point{static_cast<int16_t>(markLeft + kMark), markMid},
                           fui::Paint::solid(fui::Color::Black));

  const int16_t textLeft = static_cast<int16_t>(play.x + toybox::kMargin);
  const int16_t textWidth = static_cast<int16_t>(markLeft - textLeft - toybox::kGutter);
  drawText(screen, fui::makeRect(textLeft, static_cast<int16_t>(play.y + 18), textWidth, 52), category.title,
           toybox::kDisplayFont, fui::TextAlign::Left, toybox::kDisplayCut);
  drawText(screen, fui::makeRect(textLeft, static_cast<int16_t>(play.y + 76), textWidth, 30), category.hint,
           toybox::kSmallFont, fui::TextAlign::Left, toybox::kButtonCut);

  char state[40];
  if (model.record != nullptr && model.record->everPlayed(model.category)) {
    std::snprintf(state, sizeof(state), tr(STR_UI_BEST_D_698683), model.record->bestIn[model.category]);
  } else {
    std::snprintf(state, sizeof(state), "%s", tr(STR_UI_NEW_66AABD));
  }
  drawText(screen, fui::makeRect(textLeft, static_cast<int16_t>(play.y + 104), textWidth, 28), state,
           toybox::kSmallFont, fui::TextAlign::Left, toybox::kButtonCut);

  // Derived from the rect that drew it, never recomputed.
  screen.frame().hit(play, ActionReady);

  screen.takeTop(toybox::kGutter);
  const fui::Rect ruleRow = screen.takeTop(toybox::kRule);
  screen.target().fill(ruleRow, fui::Paint::solid(fui::Color::Black));
  screen.takeTop(toybox::kGutter);

  char record[64];
  if (model.record != nullptr && model.record->rounds > 0) {
    // "TOTAL" because this line counts every category, while the card directly
    // above it shows one. A player read NEW on the card and "15 ROUNDS BEST 8"
    // underneath as a single block and concluded the card was wrong.
    //
    // The all-time BEST is deliberately NOT here any more. It was the specific
    // collision -- the card's per-category BEST and an unlabelled overall BEST,
    // one above the other -- and comparing a best in ANIMALS against a best in
    // TRICKY is meaningless anyway when the lists differ in difficulty. The
    // number that matters is on the card, for the list you are about to play.
    //
    // Widest this can ever draw is 422px of a 448px row, with both counters
    // saturated at 65535. Adding BEST back costs 121px and does not fit.
    //
    // And the singulars, because "1 ROUNDS" was the first thing anybody saw
    // after their first game. The results screen already got this right.
    std::snprintf(record, sizeof(record), tr(STR_UI_TOTAL_D_S_D_S_873CBD), model.record->rounds,
                  model.record->rounds == 1 ? tr(STR_UI_ROUND_8580CE) : tr(STR_UI_ROUNDS_695CCE), model.record->words,
                  model.record->words == 1 ? tr(STR_UI_WORD_986B96) : tr(STR_UI_WORDS_A1F4A5));
  } else {
    std::snprintf(record, sizeof(record), "%s", tr(STR_UI_NO_ROUNDS_PLAYED_YET_78936A));
  }
  drawText(screen, screen.takeTop(28), record, toybox::kSmallFont, fui::TextAlign::Left, toybox::kButtonCut);

  screen.takeTop(toybox::kGutter);
  const fui::Rect panel = screen.takeTop(static_cast<int16_t>(screen.body().height - kDoorsHeight - toybox::kGutter));
  screen.takeTop(toybox::kGutter);
  toybox::bracket(screen, panel, 22, toybox::kRule);

  if (model.record != nullptr && model.record->recentCount > 0) {
    const int peak = model.record->recentPeak();
    const int16_t inset = 26;
    const int16_t plotW = static_cast<int16_t>(panel.width - inset * 2);
    const int16_t plotH = static_cast<int16_t>(panel.height - inset * 2 - 6);
    const int16_t step = static_cast<int16_t>(plotW / fh::Record::kRecentCount);
    const int16_t slack = static_cast<int16_t>(plotW - step * fh::Record::kRecentCount);
    const int16_t left = static_cast<int16_t>(panel.x + inset + slack / 2);
    const int16_t base = static_cast<int16_t>(panel.y + inset + plotH);
    screen.target().fill(
        fui::makeRect(left, base, static_cast<int16_t>(step * fh::Record::kRecentCount), toybox::kHairline),
        fui::Paint::solid(fui::Color::Black));
    for (int i = 0; i < fh::Record::kRecentCount; ++i) {
      const int score = model.record->recentAt(i);
      if (score < 0) continue;
      const int16_t x = static_cast<int16_t>(left + i * step + 3);
      const int16_t barH = static_cast<int16_t>(score <= 0 ? 6 : 6 + (score * (plotH - 6)) / peak);
      const bool newest = i == fh::Record::kRecentCount - 1;
      const fui::Rect bar = fui::makeRect(x, static_cast<int16_t>(base - barH), static_cast<int16_t>(step - 6), barH);
      screen.target().fill(bar, fui::Paint::solid(newest ? fui::Color::Black : fui::Color::White));
      screen.target().stroke(bar, fui::Paint::solid(fui::Color::Black), toybox::kHairline + 1);
    }
  } else {
    // The 14px cut, not the 20px one. At 20 this line is 469px in a 448px
    // panel, so the first screen anybody ever saw had a sentence running off
    // the side of it -- and it is the ONE state that only appears before you
    // have played, so it never showed up again to be noticed.
    drawText(screen, panel, tr(STR_UI_YOUR_SCORES_SHOW_UP_HERE_7E90A8), toybox::kSmallFont, fui::TextAlign::Center,
             toybox::kButtonCut);
  }

  // "%d SECONDS"
  constexpr int kLengthChars = toybox::kIntChars + toybox::literalChars(" SECONDS") + 1;
  char length[kLengthChars];
  std::snprintf(length, sizeof(length), tr(STR_UI_D_SECONDS_B16645), model.roundSeconds);
  fui::ListItem rows[static_cast<int>(MenuRow::Count)];
  rows[static_cast<int>(MenuRow::Category)].label = tr(STR_UI_CATEGORY_762258);
  rows[static_cast<int>(MenuRow::Category)].value = category.title;
  rows[static_cast<int>(MenuRow::Settings)].label = tr(STR_UI_SETTINGS_C4B924);
  rows[static_cast<int>(MenuRow::Settings)].value = length;
  rows[static_cast<int>(MenuRow::HowTo)].label = tr(STR_WORDLE_HELP);
  for (int i = 0; i < static_cast<int>(MenuRow::Count); ++i) rows[i].actionValue = static_cast<int16_t>(i);

  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<int>(MenuRow::Count);
  list.selectedIndex = static_cast<int16_t>(model.selected);
  list.action = ActionMenuRow;
  screen.list(list);
}

// ---------------------------------------------------------------------------
// The category picker (portrait)

namespace {
// Nine, which is what the band holds without a footer, so seventeen lists are
// two pages rather than three with one lonely row on the last.
constexpr int kPickerRows = 9;
}  // namespace

int pickerRowsPerPage() { return kPickerRows; }
int pickerPages() { return (fh::kCategoryCount + kPickerRows - 1) / kPickerRows; }

void buildPicker(toybox::Screen& screen, const PickerModel& model) {
  // "%d LISTS"
  constexpr int kRightChars = toybox::kIntChars + toybox::literalChars(" LISTS") + 1;
  char right[kRightChars];
  std::snprintf(right, sizeof(right), tr(STR_UI_D_LISTS_BE86B4), fh::kCategoryCount);
  toyboxChrome(screen, tr(STR_UI_CATEGORY_762258), right);

  const fui::Rect pips = screen.takeBottom(kPipBand);
  const int pages = pickerPages();
  const int page = model.page < 0 ? 0 : (model.page >= pages ? pages - 1 : model.page);
  drawPips(screen, pips, pages, page, ActionPage);

  const int first = page * kPickerRows;
  const int count = fh::kCategoryCount - first < kPickerRows ? fh::kCategoryCount - first : kPickerRows;

  // The screen is handed a SLICE, never the whole list plus an offset. The list
  // component clamps topIndex so its last screen is always full, which is right
  // for scrolling and wrong for paging: page two would repeat half of page one.
  fui::ListItem rows[kPickerRows];
  char values[kPickerRows][16];
  for (int i = 0; i < count; ++i) {
    const int index = first + i;
    rows[i].label = localizedCategory(index).title;
    if (model.record != nullptr && model.record->everPlayed(index)) {
      std::snprintf(values[i], sizeof(values[i]), tr(STR_UI_BEST_D_698683), model.record->bestIn[index]);
    } else {
      std::snprintf(values[i], sizeof(values[i]), "%s", tr(STR_UI_NEW_66AABD));
    }
    rows[i].value = values[i];
    rows[i].icon = fui::bitmapFromIcon(*fh::categoryIcon(index));
    // The ABSOLUTE index, so a tap reports which category it is rather than
    // which row of which page.
    rows[i].actionValue = static_cast<int16_t>(index);
  }

  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<uint16_t>(count);
  // The list the player is on is shown selected, so opening the picker says
  // where you already are rather than making you remember.
  list.selectedIndex =
      model.current >= first && model.current < first + count ? static_cast<int16_t>(model.current - first) : -1;
  list.action = ActionCategoryRow;
  screen.list(list);
}

// ---------------------------------------------------------------------------
// The ready card (landscape, arriving sideways on purpose)

void buildReady(toybox::Screen& screen, const ReadyModel& model) {
  const fh::CategoryInfo& category = localizedCategory(model.category);
  const int16_t w = screen.device().screen().width;
  const int16_t h = screen.device().screen().height;

  // The key bands are drawn here, in the positions they keep for the whole
  // round, so the one screen anybody reads before playing is also the one that
  // teaches the controls -- and it teaches them by being in the right place
  // rather than by naming a button nobody can see.
  //
  // Under cardFaces the BODY slot is the 30px cut and SMALL is the 44px cut, so
  // every cut named below is the one actually bound. A slot is not a cut: pass
  // kUiCut for a 30px slot and the label is centred against a line box 21px
  // shorter than the real one, which reads as text that hangs.
  drawKeyBands(screen);

  const int16_t top = kKeyBandHeight;
  const int16_t bottom = static_cast<int16_t>(h - kKeyBandHeight);

  int16_t y = static_cast<int16_t>(top + 40);
  drawText(screen, fui::makeRect(0, y, w, 62), category.title, toybox::kSmallFont, fui::TextAlign::Center,
           toybox::kLargeCut);
  y = static_cast<int16_t>(y + 74);
  drawText(screen, fui::makeRect(0, y, w, 42), category.hint, toybox::kBodyFont, fui::TextAlign::Center,
           toybox::kDisplayCut);

  // The instructions are anchored to the bottom band rather than stacked under
  // the title, so the card reads as a page with a footer instead of a block of
  // type with a hole under it.
  drawText(screen, fui::makeRect(0, static_cast<int16_t>(bottom - 108), w, 44),
           tr(STR_UI_HOLD_IT_ON_YOUR_FOREHEAD_CA3075), toybox::kBodyFont, fui::TextAlign::Center, toybox::kDisplayCut);
  drawText(screen, fui::makeRect(0, static_cast<int16_t>(bottom - 58), w, 44),
           tr(STR_UI_PRESS_EITHER_KEY_TO_START_CAFC2C), toybox::kBodyFont, fui::TextAlign::Center, toybox::kDisplayCut);

  // Tapping the middle also starts it, for the same reason the round accepts
  // touch: never only a button.
  screen.frame().hit(fui::makeRect(kTouchInsetX, top, static_cast<int16_t>(w - kTouchInsetX * 2),
                                   static_cast<int16_t>(h - kKeyBandHeight * 2)),
                     ActionStart);
}

// ---------------------------------------------------------------------------
// The round (landscape). Three arrangements; see the macro at the top.

void buildPlay(toybox::Screen& screen, const PlayModel& model) {
  const int16_t w = screen.device().screen().width;
  const int16_t h = screen.device().screen().height;

  drawKeyBands(screen);
  const int16_t whiteTop = kKeyBandHeight;
  const int16_t whiteH = static_cast<int16_t>(h - kKeyBandHeight * 2);

  // The side inset the glass hides, in this orientation. The bands above and
  // below are paint and may run under the bezel; the bar is READ, so it may
  // not. Rotated into landscape the X4 Pro's 10px strip lands on a side, and a
  // bar starting at a flat 16px margin would show six visible pixels of its
  // first segment on one edge and twenty on the other -- which on a countdown
  // reads as time already gone.
  const int16_t safeX = screen.frame().safeRect().x > toybox::kMargin ? screen.frame().safeRect().x
                                                                      : static_cast<int16_t>(toybox::kMargin);
  const int16_t barLeft = safeX;
  const int16_t barRight = static_cast<int16_t>(w - safeX);

  // The clock is a bar, and no figures appear on this screen except the score.
  //
  // Chosen against a version with the seconds in figures and a version with no
  // clock at all, by rendering all three and looking. A bar costs FIVE forced
  // repaints across a sixty-second round where a ticking numeral costs eleven,
  // and on a panel that takes a third of a second to update, a screen that
  // blinks every five seconds while nobody has pressed anything is a screen
  // somebody is trying to read through. And the bar is legible from the sofa,
  // which is where the people who need the clock are sitting: the guesser
  // cannot see any of this, so every element here is for the room.
  //
  // A full kMargin of air under the band, not six pixels. At six the bar and
  // the solid band were the two heaviest masses on the screen a fifth of a
  // millimetre apart, and from three metres they close into one thick header
  // with a hairline through it -- the same merge the design language documents
  // for two parallel strokes at 1 bit.
  constexpr int16_t kBarH = 14;
  const int16_t barTop = static_cast<int16_t>(whiteTop + toybox::kMargin);
  const int16_t segments = static_cast<int16_t>(fh::barSegments(model.secondsLeft, model.lengthSeconds));
  const int16_t barW = static_cast<int16_t>(barRight - barLeft);
  for (int i = 0; i < fh::kBarSegments; ++i) {
    // Each segment's edges are derived from the FULL width rather than from a
    // rounded segment width, so the integer remainder is absorbed between them
    // and the last segment ends exactly on the right margin. Dividing first
    // left a full bar four pixels short of its own right edge.
    const int16_t x0 = static_cast<int16_t>(barLeft + static_cast<int32_t>(barW) * i / fh::kBarSegments);
    const int16_t x1 = static_cast<int16_t>(barLeft + static_cast<int32_t>(barW) * (i + 1) / fh::kBarSegments);
    const fui::Rect cell = fui::makeRect(x0, barTop, static_cast<int16_t>(x1 - x0 - 4), kBarH);
    // A spent segment keeps its outline rather than disappearing. A bar that
    // only shortens says how much is left; one that also shows where it started
    // says how fast it is going, which is the thing a room shouts about.
    if (i < segments) {
      screen.target().fill(cell, fui::Paint::solid(fui::Color::Black));
    } else {
      screen.target().stroke(cell, fui::Paint::solid(fui::Color::Black), toybox::kHairline + 1);
    }
  }

  // The score shares the bar's row and its right edge. It used to sit in a band
  // of its own below the bar, which cost the word forty-eight pixels of height
  // for something occupying one corner, and its right edge missed the bar's by
  // four pixels -- two near-aligned edges at 1 bit read as a mistake.
  char scoreText[toybox::kIntTextChars];
  std::snprintf(scoreText, sizeof(scoreText), "%d", model.score);
  const int16_t chromeH = static_cast<int16_t>(toybox::kMargin + kBarH + 38);
  drawText(screen,
           fui::makeRect(static_cast<int16_t>(barRight - 160), static_cast<int16_t>(barTop + kBarH + 10), 160, 42),
           scoreText, toybox::kBodyFont, fui::TextAlign::Right, toybox::kDisplayCut);

  // Two rects, and the difference between them is the whole fix for a word that
  // hung four millimetres low. The word is CENTRED in the full white area
  // between the two bands, which is the frame a reader sees -- but it is FITTED
  // to that area minus the chrome at both ends, so the tallest layout the
  // ladder can pick still cannot reach the bar. Centring in the leftover space
  // under the chrome is what made a four-letter word sag against its frame.
  const fui::Rect drawBox = fui::makeRect(barLeft, whiteTop, static_cast<int16_t>(barRight - barLeft), whiteH);
  const fui::Rect fitBox = fui::makeRect(drawBox.x, static_cast<int16_t>(whiteTop + chromeH), drawBox.width,
                                         static_cast<int16_t>(whiteH - chromeH * 2));
  drawCard(screen, drawBox, model.word, layOutCard(screen, fitBox, model.word), fui::Color::Black);

  registerKeyTouch(screen);
}

// ---------------------------------------------------------------------------
// The results (landscape)

namespace {
constexpr int kResultRows = 6;
constexpr int kResultCols = 2;
constexpr int kResultsPerPage = kResultRows * kResultCols;

// How the words on a page split between the two columns. Filling the left one
// to six before starting the right left eight words as 6 + 2, with a third of
// the panel empty beneath the short column and half the divider separating a
// column from nothing. Balanced, eight words are 4 + 4.
constexpr int leftColumnCount(const int onPage) { return (onPage + 1) / 2; }
}  // namespace

int resultsPerPage() { return kResultsPerPage; }
int resultPages(const int cards) {
  const int pages = (cards + kResultsPerPage - 1) / kResultsPerPage;
  return pages < 1 ? 1 : pages;
}

void buildResult(toybox::Screen& screen, const ResultModel& model) {
  const fh::Round* round = model.round;
  const int cards = round != nullptr ? round->cards() : 0;
  toyboxChrome(screen, localizedCategory(model.category).title);

  const fui::Rect footer = screen.takeBottom(toybox::kPillHeight);
  const fui::Rect pips = screen.takeBottom(kPipBand);

  // Under bigNumberFaces the BODY slot is the 64px cut and SMALL is the 20px
  // one, so the score reads from BODY and everything else from SMALL. The
  // TITLE slot stays the 30px display cut, which is what keeps the header band
  // from clipping its own title in half.
  // 140, not 180. The score is at most three digits and the column was sized
  // by eye; the 40px it gives back go to the word list, where they are the
  // difference between BEAUTY AND THE BEAST fitting and clipping.
  constexpr int16_t kScoreColumn = 140;
  const fui::Rect content = screen.body();
  char scoreText[toybox::kIntTextChars];
  std::snprintf(scoreText, sizeof(scoreText), "%d", model.score);
  drawText(screen, fui::makeRect(content.x, content.y, kScoreColumn, 140), scoreText, toybox::kBodyFont,
           fui::TextAlign::Center, toybox::kHugeCut);
  drawText(screen, fui::makeRect(content.x, static_cast<int16_t>(content.y + 146), kScoreColumn, 32),
           model.score == 1 ? tr(STR_UI_WORD_986B96) : tr(STR_UI_WORDS_A1F4A5), toybox::kSmallFont,
           fui::TextAlign::Center, toybox::kButtonCut);
  char outOf[24];
  std::snprintf(outOf, sizeof(outOf), tr(STR_UI_OUT_OF_D_40FC5A), cards);
  drawText(screen, fui::makeRect(content.x, static_cast<int16_t>(content.y + 180), kScoreColumn, 30), outOf,
           toybox::kSmallFont, fui::TextAlign::Center, toybox::kButtonCut);

  // A rule between the number and the list, so the two read as two things.
  // kRule, which is a Toybox weight; the first version was 2px, which is none
  // of them. This divides two zones of the screen, so it is a rule and not a
  // hairline -- and it stops at the taller column rather than running on past
  // the words into empty paper.

  // The list. This is the screen people argue over, and e-ink holds it for free
  // while they do, so it gets the space rather than a summary line.
  const fui::Rect band =
      fui::makeRect(static_cast<int16_t>(content.x + kScoreColumn + toybox::kGutter), content.y,
                    static_cast<int16_t>(content.width - kScoreColumn - toybox::kGutter), content.height);
  const int pages = resultPages(cards);
  const int page = model.page < 0 ? 0 : (model.page >= pages ? pages - 1 : model.page);
  drawPips(screen, pips, pages, page, ActionResultsPage);

  const int onPage =
      cards - page * kResultsPerPage < kResultsPerPage ? cards - page * kResultsPerPage : kResultsPerPage;
  const int leftCount = leftColumnCount(onPage);
  const int16_t colW = static_cast<int16_t>(band.width / kResultCols);
  const int16_t rowH = static_cast<int16_t>(band.height / kResultRows);
  // A short list is centred in the band rather than stacked against its top.
  // Four words in a six-row band left every one of them high with the slack
  // below, on the screen people sit and look at longest.
  const int16_t listTop = static_cast<int16_t>(band.y + (band.height - leftCount * rowH) / 2);
  screen.target().fill(fui::makeRect(static_cast<int16_t>(content.x + kScoreColumn), listTop, toybox::kRule,
                                     static_cast<int16_t>(leftCount * rowH)),
                       fui::Paint::solid(fui::Color::Black));
  for (int slot = 0; slot < onPage; ++slot) {
    const int index = page * kResultsPerPage + slot;
    const int column = slot < leftCount ? 0 : 1;
    const int row = slot < leftCount ? slot : slot - leftCount;
    const int16_t x = static_cast<int16_t>(band.x + column * colW);
    const int16_t y = static_cast<int16_t>(listTop + row * rowH);

    // The mark is not a tick and a cross: at 1 bit and this size a cross is two
    // diagonals that merge into a blob, so a word you got is a filled disc and
    // one you gave up on is the same disc hollow. Same shape, opposite weight,
    // no glyph to squint at.
    const fh::Mark mark = round->markAt(index);
    const int16_t cy = static_cast<int16_t>(y + rowH / 2);
    const int16_t cx = static_cast<int16_t>(x + 10);
    if (mark == fh::Mark::Got) {
      toybox::disc(screen, cx, cy, 8, fui::Color::Black);
    } else if (mark == fh::Mark::Missed) {
      toybox::ring(screen, cx, cy, 8, toybox::kHairline + 1, fui::Color::Black, fui::Color::White);
    } else {
      // The card in hand when the clock ran out. Neither, and it says so with a
      // bar rather than by looking like one you gave up on.
      screen.target().fill(fui::makeRect(static_cast<int16_t>(cx - 8), static_cast<int16_t>(cy - 2), 16, 4),
                           fui::Paint::solid(fui::Color::Black));
    }
    drawText(screen, fui::makeRect(static_cast<int16_t>(x + 26), y, static_cast<int16_t>(colW - 30), rowH),
             round->textAt(index), toybox::kSmallFont, fui::TextAlign::Left, toybox::kButtonCut);
  }

  // The button component takes its label style from the theme's bodyText, which
  // is the BODY slot -- and on this screen the BODY slot is the 64px card cut,
  // so both labels drew at display size in a 52px pill and ran off the bottom
  // of the panel. toybox::buttonText() moves them to the small slot, which is
  // exactly the case it exists for.
  const fui::TextStyle label = toybox::buttonText(screen.theme());
  fui::ButtonProps again;
  again.label = tr(STR_WORDLE_PLAY_AGAIN);
  again.text = label;
  again.action = ActionAgain;
  screen.button(again, fui::makeRect(footer.x, footer.y, static_cast<int16_t>(footer.width / 2 - 6), footer.height));
  fui::ButtonProps done;
  done.label = tr(STR_UI_DONE_E782E7);
  done.text = label;
  done.action = ActionDone;
  screen.button(done, fui::makeRect(static_cast<int16_t>(footer.x + footer.width / 2 + 6), footer.y,
                                    static_cast<int16_t>(footer.width / 2 - 6), footer.height));
}

// ---------------------------------------------------------------------------
// Paging

int pageAfter(const int page, const int step, const int pages) {
  if (pages <= 0) return 0;
  // Two modulos, because C++ gives the sign of the DIVIDEND: -1 % 5 is -1, so
  // the single-modulo version pages backwards off page one into a negative
  // index and draws whatever is at kCategories[-4].
  return ((page + step) % pages + pages) % pages;
}

// ---------------------------------------------------------------------------
// Settings (portrait)

void buildSettings(toybox::Screen& screen, const SettingsModel& model) {
  toyboxChrome(screen, tr(STR_UI_SETTINGS_C4B924));

  char length[20];
  std::snprintf(length, sizeof(length), tr(STR_UI_D_SECONDS_B16645), model.roundSeconds);

  const bool anything = model.anythingToClear;

  fui::ListItem rows[static_cast<int>(SettingRow::Count)];
  rows[static_cast<int>(SettingRow::Length)].label = tr(STR_UI_ROUND_8580CE);
  rows[static_cast<int>(SettingRow::Length)].value = length;
  // A cycle rather than four rows: the row shows what it IS and a tap shows the
  // next one. Four rows would spend the screen on a setting nobody changes
  // twice, and the subtitle is where the cycle is spelled out because a value
  // column reading "60 SECONDS >" is a promise about a control this list does
  // not have.
  rows[static_cast<int>(SettingRow::Length)].subtitle = tr(STR_UI_TAP_TO_CYCLE_30_60_90_C1CE4A);

  // The LABEL changes when armed, not just the subtitle. A destructive action
  // whose armed state is one small second line is a destructive action you can
  // arm without noticing -- on e-ink, at arm's length, across a room. The big
  // text is the only part of this row read at a glance.
  rows[static_cast<int>(SettingRow::Reset)].label =
      model.confirmingReset ? tr(STR_UI_TAP_AGAIN_TO_WIPE_1C9CE5) : tr(STR_UI_RESET_EVERYTHING_9FEAC7);
  // The cost goes in the SUBTITLE, which owns its own line, and never in the
  // value column beside a sixteen-character label: those two together are 474px
  // of an approximately 416px row, and the overflow does not look like an
  // overflow. It looked like a row reading "RE  NOTHING TO CLEAR YET", because
  // the 20px cut has no U+2026 bitmap (only toybox_10 does) and it drew as
  // nothing.
  //
  // Fixed sentences, with no count interpolated into them: "CLEARS 999 ROUNDS
  // AND WORDS SEEN" is 436px and would bring the same bug back on the day
  // somebody plays their thousandth round. The counts are on the front door.
  //
  // And it enumerates all three things, because the reset clears the chosen
  // category and the round length too. A row promising only scores that also
  // moves you back to the first list is a surprise found later, on a different
  // screen.
  rows[static_cast<int>(SettingRow::Reset)].subtitle =
      !anything ? tr(STR_UI_NOTHING_TO_CLEAR_YET_AD222D)
                : (model.confirmingReset ? tr(STR_UI_THIS_CANNOT_BE_UNDONE_C2DEC6)
                                         : tr(STR_UI_SCORES_WORDS_AND_SETTINGS_A329F5));
  rows[static_cast<int>(SettingRow::Reset)].enabled = anything;

  for (int i = 0; i < static_cast<int>(SettingRow::Count); ++i) rows[i].actionValue = static_cast<int16_t>(i);

  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<int>(SettingRow::Count);
  list.selectedIndex = -1;
  list.action = ActionSettingsRow;
  // The subtitle is set EXPLICITLY to the small slot, and maxLines carries the
  // setting: FONT_SLOT_SMALL is 0, so a style that names only the font is
  // indistinguishable from a default-constructed one and Screen::list quietly
  // substitutes the theme's, which here is the 20px body cut. That is what put
  // "CLEARS SCORES AND WOR" on this screen at label size -- a subtitle drawn as
  // big as the thing it was subtitling, and truncated because it did not fit.
  // maxLines is what makes the style caller-owned, and it is also a real
  // second line, so a sentence that outgrows the row wraps instead of stopping
  // mid-word with no ellipsis to show for it.
  list.subtitleText.font = toybox::kSmallFont;
  list.subtitleText.maxLines = 2;
  screen.list(list);
}

// ---------------------------------------------------------------------------
// How to play (portrait, paged by the side keys and by the pips)

namespace {

// What a page draws under its text. Every page has one, because a page of type
// with a third of the panel blank under it is a real defect on a screen that
// holds its image for hours, not merely untidy -- and the fix that page two got
// was written up in this file while pages one and three shipped without it.
enum class Figure : uint8_t { Card, Keys, Marks };

struct Page {
  UiLabel title;
  // Seven lines at the 14px cut, which is about 32 characters across this
  // panel. The 20px UI cut fits 24, and the component truncates an overrun with
  // U+2026 -- which this cut does not carry (only toybox_10 does), so it
  // draws as NOTHING and the
  // sentence just stops mid-word. Two pages shipped that way before anybody
  // rendered one.
  UiLabel body[7];
  Figure figure;
};

// Written for somebody who has never played, in the order they need it. The
// controls come SECOND, before the scoring, because they are the only part this
// device does differently from every other version of this game.
//
// No page names a number of seconds. The round length is a setting on the front
// door, and the first version said SIXTY here while the menu three taps away
// said THIRTY: two screens of the same app disagreeing, both of them wrong for
// somebody.
constexpr Page kPages[] = {
    {trLabel(STR_UI_THE_GAME_875FE0),
     {trLabel(STR_UI_ONE_PLAYER_HOLDS_THE_DEVICE_FLAT_3C5115), trLabel(STR_UI_AGAINST_THEIR_FOREHEAD_SCREEN_0F11D8),
      trLabel(STR_UI_FACING_OUT_THEY_CANNOT_SEE_IT_39B69A), "", trLabel(STR_UI_EVERYBODY_ELSE_CAN_AND_SHOUTS_E6A236),
      trLabel(STR_UI_CLUES_UNTIL_THEY_GET_IT_ONE_B27107), trLabel(STR_UI_POINT_FOR_EVERY_WORD_B6D22F)},
     Figure::Card},
    {trLabel(STR_UI_THE_TWO_KEYS_B57225),
     {trLabel(STR_UI_TURN_THE_DEVICE_SIDEWAYS_AND_THE_045DA9), trLabel(STR_UI_TWO_PAGE_KEYS_LAND_ON_THE_B00586),
      trLabel(STR_UI_AND_BOTTOM_EDGES_WHERE_YOUR_8F8D6C), trLabel(STR_UI_FINGERS_ALREADY_ARE_15E156), "",
      trLabel(STR_UI_THE_SCREEN_LABELS_BOTH_EDGES_SO_2CFF2D), trLabel(STR_UI_THE_ROOM_CAN_CALL_THEM_OUT_9B5DDA)},
     Figure::Keys},
    {trLabel(STR_UI_THE_SCOREBOARD_FBCA4F),
     {trLabel(STR_UI_GIVING_UP_COSTS_NOTHING_BUT_THE_65402A), trLabel(STR_UI_SECONDS_IT_TOOK_THE_WORD_IN_1AD8BE),
      trLabel(STR_UI_YOUR_HAND_WHEN_THE_CLOCK_RUNS_F4870E), trLabel(STR_UI_OUT_IS_NEITHER_AND_THE_RESULTS_46C71B),
      trLabel(STR_UI_SAY_SO_RATHER_THAN_QUIETLY_3E7518), trLabel(STR_UI_COUNTING_IT_AGAINST_YOU_THE_F5DFB7),
      trLabel(STR_UI_MARKS_THERE_MEAN_047AAF)},
     Figure::Marks},
};

constexpr int kPageCount = static_cast<int>(sizeof(kPages) / sizeof(kPages[0]));

// A little device, drawn portrait-shaped so it reads as the thing in your hand
// turned on its side. Its bands are the SAME FRACTION of its height as the real
// ones are of the panel: at a flat 40px they were 19% against the round
// screen's 11%, so the proportions a reader memorised here were twice as heavy
// as the ones they would meet.
void drawMiniDevice(toybox::Screen& screen, const fui::Rect& box, const char* word, const bool labelled) {
  screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), toybox::kFrame);
  const int16_t band = static_cast<int16_t>((box.height - toybox::kFrame * 2) * kKeyBandHeight / 480);
  const fui::Rect top =
      fui::makeRect(static_cast<int16_t>(box.x + toybox::kFrame), static_cast<int16_t>(box.y + toybox::kFrame),
                    static_cast<int16_t>(box.width - toybox::kFrame * 2), band);
  const fui::Rect bottom =
      fui::makeRect(top.x, static_cast<int16_t>(box.y + box.height - toybox::kFrame - band), top.width, band);
  screen.target().fill(top, fui::Paint::solid(fui::Color::Black));
  screen.target().fill(bottom, fui::Paint::solid(fui::Color::Black));
  if (labelled) {
    drawText(screen, top, tr(STR_UI_PASS_BD564D), toybox::kSmallFont, fui::TextAlign::Center, toybox::kButtonCut,
             fui::Color::White);
    drawText(screen, bottom, tr(STR_UI_GOT_IT_C3A894), toybox::kSmallFont, fui::TextAlign::Center, toybox::kButtonCut,
             fui::Color::White);
  }
  drawText(screen, fui::makeRect(box.x, static_cast<int16_t>(box.y + box.height / 2 - 22), box.width, 44), word,
           toybox::kUiFont, fui::TextAlign::Center, toybox::kUiCut);
}

// The three marks the results screen uses, with what they mean. Three shapes
// and no legend is a room asking the same question every game.
void drawMarkLegend(toybox::Screen& screen, const fui::Rect& box) {
  const int16_t rowH = static_cast<int16_t>(box.height / 3);
  const int16_t cx = static_cast<int16_t>(box.x + 20);
  // "PASSED", not "YOU GAVE UP". The key is labelled PASS on the round screen
  // and on the ready card, and a legend that renames the action makes the
  // reader check whether it means something else.
  const char* labels[3] = {tr(STR_UI_YOU_GOT_IT_78CC49), tr(STR_UI_YOU_PASSED_7D361B), tr(STR_UI_TIME_RAN_OUT_99A1B1)};
  for (int i = 0; i < 3; ++i) {
    const int16_t cy = static_cast<int16_t>(box.y + i * rowH + rowH / 2);
    if (i == 0) {
      toybox::disc(screen, cx, cy, 9, fui::Color::Black);
    } else if (i == 1) {
      toybox::ring(screen, cx, cy, 9, toybox::kHairline + 1, fui::Color::Black, fui::Color::White);
    } else {
      screen.target().fill(fui::makeRect(static_cast<int16_t>(cx - 9), static_cast<int16_t>(cy - 2), 18, 4),
                           fui::Paint::solid(fui::Color::Black));
    }
    drawText(screen,
             fui::makeRect(static_cast<int16_t>(cx + 26), static_cast<int16_t>(box.y + i * rowH),
                           static_cast<int16_t>(box.width - 46), rowH),
             labels[i], toybox::kSmallFont, fui::TextAlign::Left, toybox::kButtonCut);
  }
}

}  // namespace

int howToPages() { return kPageCount; }

void buildHowTo(toybox::Screen& screen, const HowToModel& model) {
  const int page = model.page < 0 ? 0 : (model.page >= kPageCount ? kPageCount - 1 : model.page);
  char right[toybox::kSlashCounterChars];
  std::snprintf(right, sizeof(right), "%d/%d", page + 1, kPageCount);
  toyboxChrome(screen, tr(STR_WORDLE_HELP), right);

  const fui::Rect pips = screen.takeBottom(kPipBand);
  drawPips(screen, pips, kPageCount, page, ActionPage);

  drawText(screen, screen.takeTop(56), kPages[page].title, toybox::kDisplayFont, fui::TextAlign::Left,
           toybox::kDisplayCut);
  screen.takeTop(toybox::kGutter);

  for (const char* line : kPages[page].body) {
    const fui::Rect row = screen.takeTop(38);
    if (line != nullptr && *line != '\0') {
      drawText(screen, row, line, toybox::kSmallFont, fui::TextAlign::Left, toybox::kButtonCut);
    }
  }

  screen.takeTop(toybox::kGutter);
  const fui::Rect figure = screen.body();
  switch (kPages[page].figure) {
    case Figure::Marks:
      drawMarkLegend(screen, figure);
      break;
    case Figure::Card:
    case Figure::Keys:
    default: {
      // Fitted to the figure rect in BOTH axes. Sized from the height alone it
      // came out wider than the page and was clipped at each edge, which on a
      // picture of a device reads as a device with no sides.
      int16_t deviceH = figure.height;
      int16_t deviceW = static_cast<int16_t>(deviceH * 5 / 3);
      if (deviceW > figure.width) {
        deviceW = figure.width;
        deviceH = static_cast<int16_t>(deviceW * 3 / 5);
      }
      drawMiniDevice(
          screen,
          fui::makeRect(static_cast<int16_t>(figure.x + (figure.width - deviceW) / 2), figure.y, deviceW, deviceH),
          tr(STR_UI_PENGUIN_008338), kPages[page].figure == Figure::Keys);
      break;
    }
  }

  // Anywhere below the header advances, matching the tutorial in Insider: this
  // is a deck of pages and a tap is how you turn one.
  screen.frame().hit(screen.body(), ActionHowToNext);
}

}  // namespace foreheadui

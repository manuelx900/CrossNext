#include "WordsScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/CompactArt.h"
#include "../ui/Dashboard.h"
#include "../ui/PageIndicator.h"
#include "../ui/ToyboxIcons.h"
#include "../ui/WordsArt.h"
#include "WordsText.h"

namespace wordsui {
namespace {
constexpr auto black = fui::Paint::solid(fui::Color::Black);
constexpr auto white = fui::Paint::solid(fui::Color::White);
void label(fui::DrawTarget& t, fui::Rect r, const char* value, int font = 0, bool center = false,
           bool inverse = false) {
  fui::TextStyle probe;
  probe.font = font;
  if (t.measureText(font, value, probe).width > r.width || t.lineHeight(font) > r.height) {
    if (font == 2) font = 1;
    probe.font = font;
    if (t.measureText(font, value, probe).width > r.width || t.lineHeight(font) > r.height) font = font == 4 ? 3 : 0;
    probe.font = font;
    if (t.measureText(font, value, probe).width > r.width || t.lineHeight(font) > r.height) font = 3;
  }
  fui::TextStyle style;
  style.font = font;
  style.color = inverse ? fui::Color::White : fui::Color::Black;
  style.align = center ? fui::TextAlign::Center : fui::TextAlign::Left;
  style.maxLines = 1;
  const auto* cut = toybox::cutForLineHeight(t.lineHeight(font));
  t.text(cut ? toybox::inkCentred(r, *cut) : r, value, style);
}
void caps(fui::DrawTarget& t, fui::Rect r, const char* value, int font = 0, bool center = false, bool inverse = false) {
  char copy[128];
  pixelui::copyCaps(copy, sizeof(copy), value);
  label(t, r, copy, font, center, inverse);
}
void languageLabel(fui::DrawTarget& t, fui::Rect r, const char* value) {
  const char* arrow = std::strstr(value, "\xE2\x86\x92");
  if (!arrow) {
    caps(t, r, value);
    return;
  }
  char left[64];
  std::snprintf(left, sizeof(left), "%.*s", int(arrow - value), value);
  fui::TextStyle style;
  style.font = 0;
  const int width = std::min(int(r.width) / 2 - 16, int(t.measureText(0, left, style).width));
  caps(t, fui::makeRect(r.x, r.y, width, r.height), left);
  const int x = r.x + width + 4, y = r.y + r.height / 2;
  t.line({int16_t(x), int16_t(y)}, {int16_t(x + 18), int16_t(y)}, 2, black);
  t.line({int16_t(x + 12), int16_t(y - 6)}, {int16_t(x + 18), int16_t(y)}, 2, black);
  t.line({int16_t(x + 12), int16_t(y + 6)}, {int16_t(x + 18), int16_t(y)}, 2, black);
  caps(t, fui::makeRect(x + 26, r.y, r.right() - x - 26, r.height), arrow + 3);
}
void prose(fui::DrawTarget& t, fui::Rect r, const char* value, int lines = 3, bool center = false, int font = 4,
           bool inverse = false) {
  fui::TextStyle st;
  st.font = font;
  st.color = inverse ? fui::Color::White : fui::Color::Black;
  st.maxLines = 32;
  st.align = center ? fui::TextAlign::Center : fui::TextAlign::Left;
  int needed = 0;
  fui::layoutText(t, fui::makeRect(r.x, r.y, r.width, 30000), value, st, [&](const char*, fui::Rect) { ++needed; });
  if (font == 4 && needed > std::min(lines, int(r.height) / std::max(1, int(t.lineHeight(font))))) st.font = 3;
  st.maxLines = std::max(1, std::min(lines, int(r.height) / std::max(1, int(t.lineHeight(st.font)))));
  t.text(r, value, st);
}
void art(fui::DrawTarget& t, fui::Rect r, fui::BitmapRef image, bool inverse = false) {
  t.bitmap(r, image, fui::BitmapMode::Contain, inverse ? white : black);
}
void button(fui::DrawTarget& t, fui::Rect box, const char* value, bool primary = false, bool focus = false) {
  primary |= focus;
  t.fill(box, primary ? black : white);
  t.stroke(box, black, 4);
  const auto text = box.inset(fui::Insets{4, 10, 4, 10});
  fui::TextStyle style;
  style.font = 3;
  char title[128];
  pixelui::copyCaps(title, sizeof(title), value);
  if (t.measureText(3, title, style).width > text.width) {
    style.maxLines = 2;
    style.align = fui::TextAlign::Center;
    style.color = primary ? fui::Color::White : fui::Color::Black;
    t.text(text, title, style);
  } else
    caps(t, text, value, 0, true, primary);
}
void action(fui::DrawTarget& t, fui::Rect r, const char* title, const char* help, fui::BitmapRef icon,
            bool focused = false) {
  t.fill(r, focused ? black : white);
  t.stroke(r, black, 4);
  const int side = std::min(68, int(r.height) - 22);
  art(t, fui::makeRect(r.x + 16, r.y + (r.height - side) / 2, side, side), icon, focused);
  const int x = r.x + side + 32, width = r.width - side - 72;
  caps(t, fui::makeRect(x, r.y + 8, width, 24), title, 0, false, focused);
  const int offset = 34;
  prose(t, fui::makeRect(x, r.y + offset, width, r.height - offset - 2), help, 3, false, 4, focused);
  dashboard::chevron(t, fui::makeRect(r.right() - 32, r.y, 24, r.height), focused);
}
void home(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g) {
  const bool wide = body.width > body.height;
  const int gap = 10, half = (body.width - gap) / 2, metricH = wide ? 66 : 80;
  for (int n = 0; n < 2; ++n) {
    const auto box = fui::makeRect(body.x + n * (half + gap), body.y, half, metricH);
    t.stroke(box, black, 4);
    art(t, fui::makeRect(box.x + 12, box.y + 14, 44, 44), n ? wordsart::Review : wordsart::Book);
    caps(t, fui::makeRect(box.x + 70, box.y + 7, box.width - 78, 26),
         n ? tr(STR_WORDS_PENDING) : tr(STR_WORDS_SAVED_LABEL), 0);
    char number[16];
    std::snprintf(number, sizeof(number), "%d", n ? m.pending : m.saved);
    numerals::draw(t, fui::makeRect(box.x + 70, box.y + 36, box.width - 78, metricH - 43), number);
  }
  const int menuW = wide ? body.width * 53 / 100 : body.width, top = body.y + metricH + 12, rowH = wide ? 88 : 94;
  const char* titles[] = {tr(STR_WORDS_COLLECTION), tr(STR_WORDS_TRANSLATIONS), tr(STR_WORDS_GUIDE)};
  const char* helps[] = {tr(STR_WORDS_VOCABULARY_SUBTITLE), tr(STR_WORDS_LANGUAGES_SUBTITLE),
                         tr(STR_WORDS_GUIDE_SUBTITLE)};
  const fui::BitmapRef icons[] = {wordsart::Book, wordsart::Languages, wordsart::GuideBook};
  constexpr Action ids[] = {Vocabulary, Languages, Help};
  for (int n = 0; n < 3; ++n) {
    const auto r = fui::makeRect(body.x, top + n * (rowH + gap), menuW, rowH);
    g.buttons[ids[n]] = r;
    action(t, r, titles[n], helps[n], icons[n], m.keyboardFocus && m.focus == n);
  }
  const auto tail =
      wide ? fui::makeRect(body.x + menuW + 18, top, body.width - menuW - 18, body.bottom() - top)
           : fui::makeRect(body.x, top + 3 * (rowH + gap) + 4, body.width, body.bottom() - top - 3 * (rowH + gap) - 4);
  const int artH = std::max(20, int(tail.height) - 88);
  art(t, fui::makeRect(tail.x, tail.y, tail.width, artH), wordsart::BookCards);
  label(t, fui::makeRect(tail.x, tail.y + artH + 4, tail.width, 32), tr(STR_WORDS_BOOK_TO_WORDS), 0, true);
  g.buttons[Settings] = fui::makeRect(tail.x, tail.bottom() - 48, tail.width, 48);
  button(t, g.buttons[Settings], tr(STR_CARDS_SETTINGS), false, m.keyboardFocus && m.focus == 3);
  art(t, fui::makeRect(tail.x + 10, tail.bottom() - 40, 32, 32), fui::bitmapFromIcon(icon_go_settings_32));
}
void empty(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g) {
  const bool wide = body.width > body.height;
  caps(t, fui::makeRect(body.x, body.y, body.width, 40), m.filter, 1);
  label(t, fui::makeRect(body.x, body.y + 42, body.width, 30), tr(STR_WORDS_NONE_SAVED), 4);
  auto copy = body;
  if (wide) {
    art(t, fui::makeRect(body.x, body.y + 76, body.width * 40 / 100, body.height - 90), wordsart::EmptyBook);
    copy = fui::makeRect(body.x + body.width * 44 / 100, body.y + 80, body.width * 56 / 100, body.height - 80);
  } else {
    const int height = std::min(220, int(body.height) * 32 / 100);
    art(t, fui::makeRect(body.x + 28, body.y + 112, body.width - 56, height), wordsart::EmptyBook);
    copy = fui::makeRect(body.x, body.y + height + 140, body.width, body.height - height - 140);
  }
  caps(t, fui::makeRect(copy.x, copy.y, copy.width, 44), tr(STR_WORDS_EMPTY_TITLE), 1, true);
  prose(t, fui::makeRect(copy.x + 12, copy.y + 56, copy.width - 24, 100), tr(STR_WORDS_EMPTY_GUIDE), 4, true);
  g.buttons[Help] = fui::makeRect(copy.x, body.bottom() - 126, copy.width, 58);
  g.buttons[Back] = fui::makeRect(copy.x, body.bottom() - 58, copy.width, 58);
  button(t, g.buttons[Help], tr(STR_WORDS_GUIDE), !m.keyboardFocus, m.keyboardFocus && m.focus == 0);
  button(t, g.buttons[Back], tr(STR_WORDS_RETURN), false, m.keyboardFocus && m.focus == 1);
}
void collection(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g) {
  if (!m.busy && !m.total && !*m.message) {
    empty(t, body, m, g);
    return;
  }
  const bool wide = body.width > body.height;
  const int actionsW = wide ? body.width * 44 / 100 : body.width;
  caps(t, fui::makeRect(body.x, body.y, actionsW * 3 / 5, 32), m.filter, 0);
  char count[48];
  std::snprintf(count, sizeof(count), tr(STR_WORDS_COLLECTION_COUNT), m.total);
  label(t, fui::makeRect(body.x + actionsW * 3 / 5, body.y, actionsW * 2 / 5, 32), count, 3);
  const int metricY = body.y + 42, metricH = wide ? 62 : 78, half = (actionsW - 10) / 2;
  for (int n = 0; n < 2; ++n) {
    const auto r = fui::makeRect(body.x + n * (half + 10), metricY, half, metricH);
    t.stroke(r, black, 4);
    art(t, fui::makeRect(r.x + 10, r.y + (metricH - 36) / 2, 36, 36), n ? wordsart::Review : wordsart::Book);
    caps(t, fui::makeRect(r.x + (wide ? 44 : 52), r.y + 5, r.width - (wide ? 50 : 58), 24),
         n ? tr(STR_WORDS_DIFFICULT) : tr(STR_WORDS_PENDING), 3);
    std::snprintf(count, sizeof(count), "%d", n ? m.difficult : m.due);
    numerals::draw(t, fui::makeRect(r.x + 52, r.y + 30, r.width - 58, metricH - 36), count);
  }
  const int top = metricY + metricH + 12, actionH = wide ? 54 : 48;
  g.buttons[Review] = fui::makeRect(body.x, top, half, actionH);
  g.buttons[ReviewHard] = fui::makeRect(body.x + half + 10, top, half, actionH);
  g.buttons[ReviewAll] = fui::makeRect(body.x, top + actionH + 8, actionsW, actionH);
  g.buttons[Export] = fui::makeRect(body.x, top + 2 * (actionH + 8), actionsW, actionH);
  button(t, g.buttons[Review], tr(STR_WORDS_REVIEW_WORDS), !m.keyboardFocus, m.keyboardFocus && m.focus == -5);
  button(t, g.buttons[ReviewHard], tr(STR_WORDS_REVIEW_HARD), false, m.keyboardFocus && m.focus == -4);
  button(t, g.buttons[ReviewAll], tr(STR_WORDS_REVIEW_ALL_SAVED), false, m.keyboardFocus && m.focus == -3);
  button(t, g.buttons[Export], tr(STR_WORDS_EXPORT_ANKI), false, m.keyboardFocus && m.focus == -2);
  const int listX = wide ? body.x + actionsW + 18 : body.x, listW = wide ? body.width - actionsW - 18 : body.width;
  const int headingY = wide ? body.y : top + 3 * (actionH + 8) + 4, listY = headingY + 38;
  const int listButtonW = wide ? 120 : 140;
  caps(t, fui::makeRect(listX, headingY, listW - listButtonW - 10, 32), tr(STR_WORDS_YOUR_VOCABULARY), 0);
  g.buttons[FullList] = fui::makeRect(listX + listW - listButtonW, headingY - 2, listButtonW, 34);
  button(t, g.buttons[FullList], tr(STR_WORDS_VIEW_LIST), false, m.keyboardFocus && m.focus == -1);
  const auto list = fui::makeRect(listX, listY, listW, body.bottom() - 52 - listY);
  if (m.busy || *m.message) {
    prose(t, list.inset(fui::Insets{12, 12, 60, 12}), m.busy ? tr(STR_READING_LOADING) : m.message, 8, true);
    if (!m.busy) {
      g.buttons[Dismiss] = fui::makeRect(list.x, list.bottom() - 48, list.width, 44);
      button(t, g.buttons[Dismiss], tr(STR_OK_BUTTON));
    }
  } else {
    t.stroke(list, black, 4);
    for (int n = 0; n < std::min(m.count, m.capacity); ++n) {
      const int y = list.y + n * list.height / m.capacity, end = list.y + (n + 1) * list.height / m.capacity;
      const auto row = fui::makeRect(list.x, y, list.width, end - y);
      g.rows[n] = row;
      if (n) t.line({row.x, row.y}, {row.right(), row.y}, 2, black);
      fui::TextStyle wordStyle;
      wordStyle.font = 1;
      t.text(fui::makeRect(row.x + 16, row.y + 4, row.width - 64, row.height - 8), m.rows[n].label, wordStyle);
      dashboard::chevron(t, fui::makeRect(row.right() - 34, row.y, 24, row.height));
      if (m.keyboardFocus && m.focus == n) t.stroke(row.inset(fui::Insets{3, 3, 3, 3}), black, 2);
    }
  }
  const int pages = std::max(1, (m.total + m.capacity - 1) / m.capacity);
  const auto nav = pageui::draw(t, fui::makeRect(listX, body.bottom() - 44, listW, 44), m.page + 1, pages);
  if (!m.busy && m.page > 0) g.before = nav.before;
  if (!m.busy && m.page + 1 < pages) g.after = nav.after;
}
void wordList(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g) {
  if (!m.busy && !m.total && !*m.message) {
    empty(t, body, m, g);
    return;
  }
  caps(t, fui::makeRect(body.x, body.y, body.width * 2 / 3, 34), tr(STR_WORDS_LIST_TITLE), 0);
  char count[48];
  std::snprintf(count, sizeof(count), tr(STR_WORDS_COLLECTION_COUNT), m.total);
  label(t, fui::makeRect(body.x + body.width * 2 / 3, body.y, body.width / 3, 34), count, 3, true);
  const auto list = fui::makeRect(body.x, body.y + 46, body.width, body.height - 100);
  if (m.busy || *m.message) {
    prose(t, list.inset(fui::Insets{12, 12, 60, 12}), m.busy ? tr(STR_READING_LOADING) : m.message, 8, true);
    if (!m.busy) {
      g.buttons[Dismiss] = fui::makeRect(list.x, list.bottom() - 48, list.width, 44);
      button(t, g.buttons[Dismiss], tr(STR_OK_BUTTON));
    }
  } else {
    t.stroke(list, black, 4);
    for (int n = 0; n < std::min(m.count, m.capacity); ++n) {
      const int y = list.y + n * list.height / m.capacity, end = list.y + (n + 1) * list.height / m.capacity;
      const auto row = fui::makeRect(list.x, y, list.width, end - y);
      g.rows[n] = row;
      if (n) t.line({row.x, row.y}, {row.right(), row.y}, 2, black);
      fui::TextStyle wordStyle;
      wordStyle.font = 1;
      t.text(fui::makeRect(row.x + 16, row.y + 4, row.width - 64, row.height - 8), m.rows[n].label, wordStyle);
      dashboard::chevron(t, fui::makeRect(row.right() - 34, row.y, 24, row.height));
      if (m.keyboardFocus && m.focus == n) t.stroke(row.inset(fui::Insets{3, 3, 3, 3}), black, 2);
    }
  }
  const int pages = std::max(1, (m.total + m.capacity - 1) / m.capacity);
  const auto nav = pageui::draw(t, fui::makeRect(body.x, body.bottom() - 44, body.width, 44), m.page + 1, pages);
  if (!m.busy && m.page > 0) g.before = nav.before;
  if (!m.busy && m.page + 1 < pages) g.after = nav.after;
}
void guide(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g) {
  const bool wide = body.width > body.height;
  caps(t, fui::makeRect(body.x, body.y, body.width, 32), tr(STR_WORDS_GUIDE));
  const char* titles[] = {tr(STR_WORDS_GUIDE_SELECT), tr(STR_WORDS_GUIDE_SAVE), tr(STR_WORDS_GUIDE_REVIEW),
                          tr(STR_WORDS_GUIDE_MANAGE)};
  caps(t, fui::makeRect(body.x, body.y + (wide ? 28 : 42), body.width, wide ? 42 : 52), titles[m.guidePage], 1);
  const int bottom = body.bottom() - 60, top = body.y + (wide ? 80 : 110);
  if (m.guidePage == 0) {
    prose(t, fui::makeRect(body.x, top, body.width, 58), tr(STR_WORDS_SELECT_HELP), 2);
    const int boxH = wide ? 124 : 190;
    const auto box = fui::makeRect(body.x, top + (wide ? 62 : 72), wide ? body.width / 2 - 10 : body.width, boxH);
    t.stroke(box, black, 4);
    label(t, fui::makeRect(box.x + 18, box.y + 22, box.width - 36, 40), tr(STR_WORDS_EXAMPLE_BEFORE), 1);
    const auto word = fui::makeRect(box.x + box.width * 42 / 100, box.y + 20, box.width * 45 / 100, 46);
    t.fill(word, black);
    label(t, word, tr(STR_WORDS_EXAMPLE_WORD), 1, true, true);
    label(t, fui::makeRect(box.x + 18, box.y + 76, box.width - 36, 40), tr(STR_WORDS_EXAMPLE_AFTER), 1);
    art(t, fui::makeRect(word.x + word.width / 3, word.bottom() - 4, 60, wide ? 50 : 82), wordsart::Hand);
    const int y = wide ? top + 30 : box.bottom() + 16, x = wide ? body.x + body.width / 2 + 8 : body.x,
              w = wide ? body.width / 2 - 8 : body.width;
    const int half = (w - 12) / 2;
    button(t, fui::makeRect(x, y, half, 64), tr(STR_WORDS_TRANSLATE_LABEL));
    button(t, fui::makeRect(x + half + 12, y, half, 64), tr(STR_WORDS_DEFINE_LABEL));
    caps(t, fui::makeRect(x, y + (wide ? 80 : 84), w, 34), tr(STR_WORDS_WHAT_LOOKUP));
    prose(t, fui::makeRect(x, y + (wide ? 122 : 128), w, bottom - y - (wide ? 122 : 128)), tr(STR_WORDS_LOOKUP_HELP),
          4);
  } else if (m.guidePage == 1) {
    const int cardW = wide ? body.width / 2 - 12 : body.width, cardH = wide ? bottom - top : 300;
    const auto card = fui::makeRect(body.x, top, cardW, cardH);
    t.stroke(card, black, 4);
    label(t, fui::makeRect(card.x + 16, card.y + (wide ? 8 : 12), card.width - 32, wide ? 40 : 54),
          tr(STR_WORDS_EXAMPLE_WORD), 2);
    languageLabel(t, fui::makeRect(card.x + 16, card.y + (wide ? 50 : 70), card.width - 32, wide ? 22 : 28),
                  tr(STR_WORDS_EXAMPLE_LANGUAGE));
    label(t, fui::makeRect(card.x + 16, card.y + (wide ? 76 : 108), card.width - 32, wide ? 32 : 44),
          tr(STR_WORDS_EXAMPLE_MEANING), 1);
    prose(t, fui::makeRect(card.x + 16, card.y + (wide ? 112 : 168), card.width - 32, wide ? 56 : 60),
          tr(STR_WORDS_EXAMPLE_SENTENCE), 2);
    const auto save = fui::makeRect(card.x + 14, card.bottom() - (wide ? 48 : 64), card.width - 28, wide ? 40 : 50);
    button(t, save, tr(STR_WORDS_SAVE_WORD), true);
    const auto help = wide ? fui::makeRect(body.x + cardW + 24, top, body.width - cardW - 24, bottom - top)
                           : fui::makeRect(body.x, card.bottom() + 22, body.width, bottom - card.bottom() - 22);
    prose(t, fui::makeRect(help.x, help.y, help.width, 56), tr(STR_WORDS_SAVE_HELP), 2);
    art(t, fui::makeRect(help.x, help.y + 68, 58, 54), wordsart::Book);
    caps(t, fui::makeRect(help.x + 72, help.y + 68, help.width - 72, 42), tr(STR_WORDS_MISSING_DICTIONARY));
    prose(t, fui::makeRect(help.x + 72, help.y + 108, help.width - 72, help.height - 108),
          tr(STR_WORDS_DICTIONARY_HELP), 4);
  } else {
    const bool manage = m.guidePage == 3;
    const char* labels[] = {manage ? tr(STR_WORDS_VIEW_LIST) : tr(STR_WORDS_REVIEW_WORDS),
                            manage ? tr(STR_WORDS_OPEN_WORD) : tr(STR_WORDS_REVIEW_HARD),
                            manage ? tr(STR_WORDS_EXPORT_ANKI) : tr(STR_WORDS_REVIEW_ALL_SAVED)};
    const char* helps[] = {manage ? tr(STR_WORDS_LIST_HELP) : tr(STR_WORDS_RECALL_HELP),
                           manage ? tr(STR_WORDS_DETAIL_HELP) : tr(STR_WORDS_HARD_HELP),
                           manage ? tr(STR_WORDS_TRANSFER_HELP) : tr(STR_WORDS_ALL_HELP)};
    const fui::BitmapRef icons[] = {manage ? wordsart::Book : wordsart::Review, wordsart::Review,
                                    manage ? wordsart::Export : wordsart::Review};
    const int gap = 12, h = wide ? 156 : 112, w = wide ? (body.width - 2 * gap) / 3 : body.width;
    for (int n = 0; n < 3; ++n) {
      const auto row = fui::makeRect(body.x + (wide ? n * (w + gap) : 0), top + (wide ? 0 : n * (h + gap)), w, h);
      if (wide) {
        t.stroke(row, black, 4);
        art(t, fui::makeRect(row.x + 8, row.y + 10, 52, 44), icons[n]);
        char title[80];
        pixelui::copyCaps(title, sizeof(title), labels[n]);
        prose(t, fui::makeRect(row.x + 68, row.y + 8, row.width - 76, 48), title, 2, false, 3);
        prose(t, fui::makeRect(row.x + 10, row.y + 64, row.width - 20, h - 72), helps[n], 3);
      } else
        action(t, row, labels[n], helps[n], icons[n]);
    }
    const int y = wide ? top + h + 8 : top + 3 * (h + gap);
    const int doneWidth = 200, stripGap = 12;
    prose(t, fui::makeRect(body.x, y, wide ? body.width - doneWidth - stripGap : body.width, wide ? 56 : 80),
          manage ? tr(STR_WORDS_LIST_SECTION_HELP) : tr(STR_WORDS_SPACING_HELP), 4, true, 3);
  }
  const auto nav =
      pageui::draw(t, fui::makeRect(body.x, body.bottom() - 44, body.width, 44), m.guidePage + 1, kGuidePages);
  if (m.guidePage > 0) g.before = nav.before;
  if (m.guidePage + 1 < kGuidePages) g.after = nav.after;
}
fui::Rect faceBox(fui::Rect body, const Model& m) {
  const bool wide = body.width > body.height;
  if (m.view == View::Detail)
    return fui::makeRect(body.x, body.y, wide ? body.width * 63 / 100 : body.width,
                         wide ? body.height : body.height - 3 * 68 - 8);
  const int footer = m.answer ? (wide ? 140 : 176) : (wide ? 128 : 190);
  return fui::makeRect(body.x, body.y + 52, wide ? body.width * 57 / 100 : body.width,
                       wide ? body.height - 52 : body.height - 52 - footer);
}
fui::Rect contentBox(fui::Rect face, const Model& m) {
  return face.inset(m.view == View::Review && !m.answer ? fui::Insets{60, 18, 50, 18} : fui::Insets{18, 16, 50, 16});
}
int layoutContent(fui::DrawTarget& t, fui::Rect text, Model& m, pagedtext::Work& work, int selected,
                  int* used = nullptr, uint8_t* wordBreaks = nullptr) {
  fui::TextStyle heading, body, example;
  heading.font = 2;
  body.font = 4;
  if (m.view == View::Review && !m.answer) {
    heading.align = fui::TextAlign::Center;
    const Paragraph prompt{m.term, heading};
    return paragraphs(t, text, &prompt, 1, selected, work, 12, used, &m.textCache, wordBreaks);
  }
  example = body;
  example.italic = false;
  const Paragraph parts[] = {{m.term, heading, tr(STR_WORDS_TERM_LABEL)},
                             {m.meaning, body, tr(STR_WORDS_MEANING_LABEL), false, true},
                             {m.sentence, example, tr(STR_WORDS_EXAMPLE_LABEL), false, true},
                             {m.book, example, tr(STR_WORDS_BOOK_LABEL), true, true}};
  return paragraphs(t, text, parts, m.view == View::Detail ? 4 : 3, selected, work, 12, used, &m.textCache, wordBreaks);
}
void contentPage(fui::DrawTarget& t, fui::Rect box, Model& m, Geometry& g, pagedtext::Work& work) {
  const auto text = contentBox(box, m);
  m.textPages = measureCard(t, text, m, work);
  m.textPage = std::clamp(m.textPage, 0, m.textPages - 1);
  layoutContent(t, text, m, work, m.textPage);
  g.text = text;
  if (m.textPages > 1) {
    const auto nav =
        pageui::draw(t, fui::makeRect(box.x + 8, box.bottom() - 44, box.width - 16, 36), m.textPage + 1, m.textPages);
    if (m.textPage > 0) g.before = nav.before;
    if (m.textPage + 1 < m.textPages) g.after = nav.after;
  }
}
void card(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g, pagedtext::Work& work, fui::DrawTarget& content) {
  const bool wide = body.width > body.height;
  const int footer = m.answer ? (wide ? 140 : 176) : (wide ? 128 : 190);
  const char* title = tr(STR_WORDS_REVIEW_WORDS);
  switch (m.review) {
    case ReviewKind::Difficult:
      title = tr(STR_WORDS_REVIEW_HARD);
      break;
    case ReviewKind::AllSaved:
      title = tr(STR_WORDS_REVIEW_ALL_SAVED);
      break;
    case ReviewKind::DueWords:
    case ReviewKind::SingleWord:
      break;
  }
  caps(t, fui::makeRect(body.x, body.y, body.width - 86, 40), title, 1);
  char progress[32];
  if (m.review == ReviewKind::AllSaved)
    std::snprintf(progress, sizeof(progress), "%d", m.reviewIndex + 1);
  else
    std::snprintf(progress, sizeof(progress), "%d / %d", m.reviewIndex + 1, std::max(m.reviewTotal, m.reviewIndex + 1));
  label(t, fui::makeRect(body.right() - 80, body.y, 80, 40), progress, 0, true);
  const auto box = faceBox(body, m);
  t.stroke(box, black, 4);
  if (m.answer)
    contentPage(content, box, m, g, work);
  else {
    languageLabel(t, fui::makeRect(box.x + 16, box.y + 16, box.width - 32, 28), m.language);
    auto area = contentBox(box, m);
    int used = 0;
    m.textPages = layoutContent(content, area, m, work, -1, &used);
    m.textPage = std::clamp(m.textPage, 0, m.textPages - 1);
    if (m.textPages == 1) {
      area.y += std::max(0, (int(area.height) - used) / 2);
      // Keep the measured height in the cache; only its origin moves.
    }
    layoutContent(content, area, m, work, m.textPage);
    if (m.textPages > 1) {
      const auto nav =
          pageui::draw(t, fui::makeRect(box.x + 8, box.bottom() - 44, box.width - 16, 36), m.textPage + 1, m.textPages);
      if (m.textPage > 0) g.before = nav.before;
      if (m.textPage + 1 < m.textPages) g.after = nav.after;
    }
  }
  const int x = wide ? box.right() + 18 : body.x, width = wide ? body.right() - x : body.width;
  if (!m.answer) {
    const int groupHeight = 80 + 16 + 58 * 2 + 12;
    const int y = wide ? box.y + std::max(0, (int(box.height) - groupHeight) / 2) : box.bottom() + 12;
    prose(t, fui::makeRect(x, y, width, wide ? 80 : 46), tr(STR_WORDS_THINK), wide ? 2 : 3, true, wide ? 4 : 3);
    g.buttons[Reveal] = fui::makeRect(x, wide ? y + 96 : y + 56, width, 58);
    button(t, g.buttons[Reveal], tr(STR_WORDS_SHOW_ANSWER), !m.keyboardFocus, m.keyboardFocus && m.focus == 0);
    g.buttons[Back] = fui::makeRect(x, wide ? g.buttons[Reveal].bottom() + 12 : body.bottom() - 58, width, 58);
    button(t, g.buttons[Back], tr(STR_WORDS_RETURN), false, m.keyboardFocus && m.focus == 1);
  } else {
    const int top = wide ? body.y + 52 : body.bottom() - footer + 8;
    static constexpr StrId ratings[] = {StrId::STR_UI_AGAIN_88825D, StrId::STR_UI_HARD_A06A58,
                                        StrId::STR_UI_GOOD_7763D3, StrId::STR_UI_EASY_493F11};
    const freeink::Icon* const icons[] = {&extra_rotate_cw_32, &extra_dashboard_bars_32, &extra_check_32,
                                          &extra_dashboard_easy_32};
    const int half = (width - 10) / 2, h = wide ? 56 : 78, y = top + 4;
    for (int n = 0; n < 4; ++n) {
      auto r = wide ? fui::makeRect(x, y + n * (h + 8), width, h)
                    : fui::makeRect(x + n % 2 * (half + 10), y + n / 2 * (h + 10), half, h);
      g.rows[n] = r;
      const bool selected = m.keyboardFocus && m.focus == n;
      t.fill(r, selected ? black : white);
      t.stroke(r, black, 4);
      t.bitmap(fui::makeRect(r.x + 12, r.y + (h - 32) / 2, 32, 32), fui::bitmapFromIcon(*icons[n]),
               fui::BitmapMode::Contain, selected ? white : black);
      caps(t, fui::makeRect(r.x + 54, r.y + (wide ? 10 : 4), r.width - (wide ? 170 : 62), 32), I18N.get(ratings[n]), 0,
           true, selected);
      label(t,
            wide ? fui::makeRect(r.right() - 110, r.y + 10, 104, 34)
                 : fui::makeRect(r.x + 54, r.y + 38, r.width - 62, 34),
            m.delays[n], 3, true, selected);
    }
  }
}
void detail(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g, pagedtext::Work& work,
            fui::DrawTarget& content) {
  const bool wide = body.width > body.height;
  const int h = 58, gap = 10;
  const auto box = faceBox(body, m);
  t.stroke(box, black, 4);
  contentPage(content, box, m, g, work);
  constexpr Action ids[] = {Passage, Delete, ReviewWord};
  const char* labels[] = {tr(STR_WORDS_VIEW_BOOK), tr(STR_WORDS_DELETE), tr(STR_WORDS_REVIEW_ONE)};
  const fui::BitmapRef icons[] = {wordsart::Book, compactart::Trash, wordsart::Review};
  const int x = wide ? box.right() + 18 : body.x, w = wide ? body.right() - x : body.width;
  for (int n = 0; n < 3; ++n) {
    auto r = fui::makeRect(x, (wide ? body.y + 14 : box.bottom() + 18) + n * (h + gap), w, h);
    g.buttons[ids[n]] = r;
    const bool selected = m.keyboardFocus && m.focus == n;
    t.fill(r, selected ? black : white);
    t.stroke(r, black, 4);
    art(t, fui::makeRect(r.x + 14, r.y + 10, 36, 36), icons[n], selected);
    caps(t, fui::makeRect(r.x + 64, r.y + 4, r.width - 100, r.height - 8), labels[n], 0, true, selected);
    dashboard::chevron(t, fui::makeRect(r.right() - 30, r.y, 24, r.height), selected);
  }
}
void waiting(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g) {
  g.buttons[Back] = fui::makeRect(body.x, body.bottom() - 58, body.width, 58);
  const int groupHeight = 246, contentHeight = g.buttons[Back].y - 16 - body.y;
  const int top = body.y + std::max(0, (contentHeight - groupHeight) / 2);
  caps(t, fui::makeRect(body.x, top, body.width, 60), tr(STR_WORDS_REVIEW_WORDS), 1, true);
  char message[128];
  std::snprintf(message, sizeof(message), tr(STR_WORDS_WAIT), std::max(1, m.waitMinutes));
  prose(t, fui::makeRect(body.x + 16, top + 72, body.width - 32, 72), message, 4, true);
  prose(t, fui::makeRect(body.x + 16, top + 156, body.width - 32, 90), tr(STR_WORDS_WAIT_HELP), 4, true);
  button(t, g.buttons[Back], tr(STR_WORDS_RETURN));
}
void deleteWord(fui::DrawTarget& t, fui::Rect body, Model& m, Geometry& g) {
  if (body.width > body.height) {
    const int width = (body.width - 24) / 2;
    const int right = body.x + width + 24;
    caps(t, fui::makeRect(body.x, body.y + 24, width, 64), tr(STR_WORDS_DELETE_TITLE), 1, true);
    prose(t, fui::makeRect(body.x + 12, body.y + 108, width - 24, body.height - 132), m.term, 2, true, 1);
    prose(t, fui::makeRect(right + 8, body.y + 24, width - 16, body.height - 174), tr(STR_WORDS_DELETE_HELP), 4, true);
    g.buttons[ConfirmDelete] = fui::makeRect(right, body.bottom() - 126, width, 58);
    g.buttons[Cancel] = fui::makeRect(right, body.bottom() - 58, width, 58);
    button(t, g.buttons[ConfirmDelete], tr(STR_WORDS_DELETE), false, m.keyboardFocus && m.focus == 0);
    button(t, g.buttons[Cancel], tr(STR_CANCEL), !m.keyboardFocus, m.keyboardFocus && m.focus == 1);
    return;
  }
  caps(t, fui::makeRect(body.x, body.y + 20, body.width, 50), tr(STR_WORDS_DELETE_TITLE), 1, true);
  prose(t, fui::makeRect(body.x + 16, body.y + 96, body.width - 32, 72), m.term, 2, true, 1);
  prose(t, fui::makeRect(body.x + 16, body.y + 184, body.width - 32, 100), tr(STR_WORDS_DELETE_HELP), 4, true);
  g.buttons[ConfirmDelete] = fui::makeRect(body.x, body.bottom() - 126, body.width, 58);
  g.buttons[Cancel] = fui::makeRect(body.x, body.bottom() - 58, body.width, 58);
  button(t, g.buttons[ConfirmDelete], tr(STR_WORDS_DELETE), false, m.keyboardFocus && m.focus == 0);
  button(t, g.buttons[Cancel], tr(STR_CANCEL), !m.keyboardFocus, m.keyboardFocus && m.focus == 1);
}
}  // namespace
fui::Rect textBox(const fui::DeviceContext& d, const Model& m) {
  return contentBox(faceBox(pixelui::geometry(d, 12).body, m), m);
}
int measureCard(fui::DrawTarget& t, fui::Rect box, Model& m, pagedtext::Work& work, int* used, uint8_t* wordBreaks,
                bool fitOnly) {
  return layoutContent(t, box, m, work, fitOnly ? -2 : -1, used, wordBreaks);
}
void draw(fui::DrawTarget& t, const fui::DeviceContext& d, Model& m, Geometry& g, pagedtext::Work& work,
          fui::DrawTarget* content) {
  g = {};
  const auto body = pixelui::geometry(d, 12).body;
  if (m.error) {
    prose(t, body.inset(fui::Insets{12, 12, 80, 12}), *m.message ? m.message : tr(STR_READING_STORAGE_ERROR), 16);
    g.buttons[Back] = fui::makeRect(body.x, body.bottom() - 58, body.width, 58);
    button(t, g.buttons[Back], tr(STR_WORDS_RETURN), false, m.keyboardFocus && m.focus == 1);
    return;
  }
  switch (m.view) {
    case View::Home:
      home(t, body, m, g);
      break;
    case View::List:
      collection(t, body, m, g);
      break;
    case View::WordList:
      wordList(t, body, m, g);
      break;
    case View::Guide:
      guide(t, body, m, g);
      break;
    case View::Review:
      card(t, body, m, g, work, content ? *content : t);
      break;
    case View::Detail:
      detail(t, body, m, g, work, content ? *content : t);
      break;
    case View::Waiting:
      waiting(t, body, m, g);
      break;
    case View::Delete:
      deleteWord(t, body, m, g);
      break;
  }
}
}  // namespace wordsui

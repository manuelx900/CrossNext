#include "FlashScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../../components/icons/listIcons.h"
#include "../study/StudyScreens.h"
#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "../ui/PageIndicator.h"
#include "../ui/ToyboxIcons.h"
#include "FlashArt.h"
namespace flashui {
namespace {
using fui::makeRect;
fui::Rect begin(toybox::Screen& s, const char* title) {
  pixelui::begin(s);
  flashui::header(s, title);
  return s.body();
}
void caps(toybox::Screen& s, fui::Rect r, const char* t, int font = 0, int lines = 1,
          fui::TextAlign align = fui::TextAlign::Left) {
  char text[192];
  pixelui::copyCaps(text, sizeof(text), t);
  fittext::draw(s.target(), r, text, font, lines, align);
}
void prose(toybox::Screen& s, fui::Rect r, const char* t, int lines = 1) { fittext::draw(s.target(), r, t, 4, lines); }
void section(toybox::Screen& s, fui::Rect r, const char* t, const char* count = nullptr) {
  caps(s, r, t, 1);
  fui::TextStyle style;
  style.font = 1;
  const int measured = std::min(int(r.width) - 30, int(s.target().measureText(1, t, style).width) + 18);
  const int right = r.right() - (count ? 34 : 0);
  if (right > r.x + measured)
    s.target().line({int16_t(r.x + measured), int16_t(r.bottom() - 5)}, {int16_t(right), int16_t(r.bottom() - 5)}, 2,
                    dashboard::ink);
  if (count) dashboard::text(s, makeRect(r.right() - 30, r.y, 30, r.height), count, 0, fui::TextAlign::Right);
}
void mark(toybox::Screen& s, fui::Rect r, const freeink::Icon& value, bool inverse = false) {
  if (&value != &extra_dashboard_cards_32) {
    dashboard::icon(s, r, value, inverse);
    return;
  }
  const int side = std::min(int(r.width), int(r.height)), offset = std::max(2, side / 8), square = side - 2 * offset;
  for (int n = 0; n < 3; ++n) {
    auto card = makeRect(r.x + n * offset, r.y + (2 - n) * offset, square, square);
    s.target().fill(card, inverse ? dashboard::ink : dashboard::paper, 4);
    s.target().stroke(card, inverse ? dashboard::paper : dashboard::ink, std::max(2, side / 16), 4);
  }
}
void row(toybox::Screen& s, fui::Rect r, const char* title, const char* sub, const freeink::Icon& icon,
         fui::ActionId action, int value = 0, bool primary = false) {
  primary = s.frame().buttonSelected(action, value, primary);
  if (primary)
    s.target().fill(r, dashboard::ink);
  else
    dashboard::frame(s, r, 2);
  mark(s,
       makeRect(r.x + 14, r.y + (sub && r.width < 400 ? 6 : (r.height - 44) / 2), sub && r.width < 400 ? 32 : 44,
                sub && r.width < 400 ? 32 : 44),
       icon, primary);
  char text[192];
  pixelui::copyCaps(text, sizeof(text), title);
  fittext::draw(s.target(), makeRect(r.x + 78, r.y + 8, r.width - 114, sub ? (r.width < 400 ? 28 : 40) : r.height - 16),
                text, 0, 2, fui::TextAlign::Left, primary ? fui::Color::White : fui::Color::Black);
  if (sub)
    prose(s,
          makeRect(r.x + (r.width < 400 ? 14 : 78), r.y + (r.width < 400 ? 38 : 50),
                   r.width - (r.width < 400 ? 28 : 110), r.height - (r.width < 400 ? 42 : 54)),
          sub, 1);
  dashboard::chevron(s.target(), makeRect(r.right() - 34, r.y, 26, r.height), primary);
  s.frame().hit(r, action, value, fui::InputTouch);
}
void metric(toybox::Screen& s, fui::Rect r, const char* value, const char* label, const freeink::Icon& icon,
            bool vertical = false) {
  dashboard::frame(s, r, 2);
  const int side = vertical ? 36 : 42, textX = vertical ? r.x + 6 : r.x + 70,
            textW = vertical ? r.width - 12 : r.width - 78;
  mark(s, makeRect(vertical ? r.x + (r.width - side) / 2 : r.x + 14, r.y + 16, side, side), icon);
  dashboard::text(s, makeRect(textX, r.y + (vertical ? 56 : 8), textW, vertical ? 40 : 48), value, 2,
                  vertical ? fui::TextAlign::Center : fui::TextAlign::Left);
  prose(s, makeRect(vertical ? textX : r.x + 8, r.y + (vertical ? 98 : 56), vertical ? textW : r.width - 16, 26), label,
        1);
}
void paging(toybox::Screen& s, fui::Rect band, int page, int pages) {
  pageui::build(s, band, page + 1, pages, Page, -1, Page, 1);
}
constexpr StrId ratings[] = {StrId::STR_UI_AGAIN_88825D, StrId::STR_UI_HARD_A06A58, StrId::STR_UI_GOOD_7763D3,
                             StrId::STR_UI_EASY_493F11};
const freeink::Icon* const ratingIcons[] = {&extra_rotate_cw_32, &extra_dashboard_bars_32, &extra_check_32,
                                            &extra_dashboard_easy_32};
}  // namespace
void header(toybox::Screen& screen, const char* title) {
  pixelui::Header model;
  model.title = title;
  model.back = screen.device().hasTouch;
  model.mark = fui::bitmapFromIcon(icon_go_settings_32);
  model.markAction = Settings;
  pixelui::header(screen, model);
}
int textPage(fui::DrawTarget& target, fui::Rect box, const char* text, int page, char* line, size_t bytes,
             TextPageCache* cache, int fontKey) {
  return cachedTextPage(target, box, text, page, line, bytes, cache, fontKey);
}

void interval(char* out, size_t bytes, int minutes, int days) {
  if (minutes > 0)
    std::snprintf(out, bytes, tr(STR_FLASH_INTERVAL_MIN), minutes);
  else
    std::snprintf(out, bytes, days <= 1 ? tr(STR_FLASH_INTERVAL_DAY) : tr(STR_FLASH_INTERVAL_DAYS), std::max(1, days));
}
void duration(char* out, size_t bytes, uint64_t ms, bool known) {
  if (!known) {
    std::snprintf(out, bytes, "%s", tr(STR_FLASH_DURATION_UNKNOWN));
    return;
  }
  std::snprintf(out, bytes, tr(STR_FLASH_TIME_MIN_SEC), static_cast<unsigned long long>(ms / 60000),
                static_cast<unsigned long long>(ms / 1000 % 60));
}
void importing(toybox::Screen& s, const ImportModel& m) {
  const auto b = s.body();
  const int available = std::max(1, int(b.height) - 80);
  const int height = std::min(248, available), y = b.y + (available - height) / 2;
  StrId label = StrId::STR_FLASH_IMPORT_CHECKING;
  switch (m.progress.phase) {
    case flashcards::ImportPhase::Checking:
      label = StrId::STR_FLASH_IMPORT_CHECKING;
      break;
    case flashcards::ImportPhase::Unpacking:
      label = StrId::STR_FLASH_IMPORT_UNPACKING;
      break;
    case flashcards::ImportPhase::Decompressing:
      label = StrId::STR_FLASH_IMPORT_DECOMPRESSING;
      break;
    case flashcards::ImportPhase::Cards:
      label = StrId::STR_FLASH_IMPORT_CARDS;
      break;
    case flashcards::ImportPhase::Images:
      label = StrId::STR_FLASH_IMPORT_IMAGES;
      break;
    case flashcards::ImportPhase::Saving:
      label = StrId::STR_FLASH_IMPORT_SAVING;
      break;
    case flashcards::ImportPhase::SavingImages:
      label = StrId::STR_FLASH_IMPORT_SAVING_IMAGES;
      break;
  }
  fittext::draw(s.target(), makeRect(b.x, y, b.width, 44), I18N.get(label), 0, 2, fui::TextAlign::Center);
  const bool known = m.progress.total > 0;
  const int barWidth = std::max(8, int(b.width) - (known ? 78 : 0));
  const auto bar = makeRect(b.x, y + 58, barWidth, 18);
  dashboard::frame(s, bar, 2);
  const uint32_t percent =
      known ? static_cast<uint32_t>(std::min<uint64_t>(100, m.progress.done * 100 / m.progress.total)) : 0;
  if (known) {
    const int width = (bar.width - 8) * percent / 100;
    if (width) s.target().fill(makeRect(bar.x + 4, bar.y + 4, width, bar.height - 8), dashboard::ink);
    char number[12];
    std::snprintf(number, sizeof(number), "%u%%", unsigned(percent));
    dashboard::text(s, makeRect(bar.right() + 8, bar.y - 8, b.right() - bar.right() - 8, 32), number, 0,
                    fui::TextAlign::Right);
  } else {
    const int width = std::max(2, (bar.width - 8) / 5);
    const int offset = (bar.width - 8 - width) * (m.tick % 7) / 6;
    s.target().fill(makeRect(bar.x + 4 + offset, bar.y + 4, width, bar.height - 8), dashboard::ink);
  }
  char text[128], elapsed[48];
  std::snprintf(text, sizeof(text), tr(STR_FLASHCARDS_IMPORT_COUNT), unsigned(m.cards), unsigned(m.skipped));
  fittext::draw(s.target(), makeRect(b.x, y + 94, b.width, 52), text, 4, 2, fui::TextAlign::Center);
  duration(elapsed, sizeof(elapsed), m.elapsedMs);
  std::snprintf(text, sizeof(text), "%s: %s", tr(STR_FLASH_TIME), elapsed);
  fittext::draw(s.target(), makeRect(b.x, y + 150, b.width, 28), text, 4, 1, fui::TextAlign::Center);
  fittext::draw(s.target(), makeRect(b.x, y + 192, b.width, 56), tr(STR_FLASH_IMPORT_KEEP_SD), 4, 2,
                fui::TextAlign::Center);
}
void library(toybox::Screen& s, const LibraryModel& m) {
  const auto b = begin(s, tr(STR_FLASHCARDS_APP));
  const bool wide = b.width > b.height;
  const int w = wide ? (b.width - 18) / 2 : b.width;
  char text[128];
  mark(s, makeRect(b.x + 8, b.y + 16, 44, 44), extra_calendar_32);
  std::snprintf(text, sizeof(text), m.today == 1 ? tr(STR_FLASH_TODAY_ONE) : tr(STR_FLASH_TODAY_TOTAL), m.today);
  fittext::draw(s.target(), makeRect(b.x + 74, b.y + 8, b.width - 82, 60), text, 1, 1);
  std::snprintf(text, sizeof(text), "%d", m.totalDecks);
  section(s, makeRect(b.x, b.y + (wide ? 60 : (m.pages > 1 ? 78 : 98)), w, 36), tr(STR_FLASH_MY_DECKS), text);
  if (m.error || !m.count)
    prose(s, makeRect(b.x + 8, b.y + 154, w - 16, 130),
          m.error ? tr(STR_READING_STORAGE_ERROR) : tr(STR_FLASH_EMPTY_LIBRARY), 4);
  for (int n = 0; n < m.count; ++n) {
    const auto& d = m.rows[n];
    std::snprintf(text, sizeof(text), d.total == 1 ? tr(STR_FLASH_DECK_ONE) : tr(STR_FLASH_DECK_ROW), d.total,
                  d.due + d.fresh);
    row(s,
        makeRect(b.x, b.y + (wide ? 100 : (m.pages > 1 ? 126 : 146)) + n * (wide ? 76 : (m.pages > 1 ? 90 : 98)), w,
                 wide ? 68 : (m.pages > 1 ? 80 : 86)),
        d.name, d.ready ? text : tr(STR_FLASH_IMPORT_FIRST), d.folder ? icon_folder_32 : extra_dashboard_cards_32,
        OpenDeck, n);
  }
  if (m.pages > 1) paging(s, makeRect(b.x, wide ? b.bottom() - 48 : b.y + 394, w, 44), m.page, m.pages);
  const int x = wide ? b.x + w + 18 : b.x, y = wide ? b.y + 98 : b.bottom() - 248;
  section(s, makeRect(x, y, w, 36), tr(STR_FLASH_NAVIGATION));
  row(s, makeRect(x, y + 44, w, 60), tr(STR_FLASH_HISTORY), nullptr, extra_calendar_32, History);
  row(s, makeRect(x, y + 116, w, 60), tr(STR_FLASH_HELP), nullptr, extra_reading_book_32, Help);
  mark(s, makeRect(x + 4, b.bottom() - 50, 36, 36), icon_folder_32);
  prose(s, makeRect(x + 54, b.bottom() - 54, w - 62, 28), tr(STR_FLASH_FILES_AT));
  prose(s, makeRect(x + 54, b.bottom() - 26, w - 62, 24), ".apkg · .csv");
}
void deck(toybox::Screen& s, const studyui::DeckModel& m) {
  const auto b = begin(s, tr(STR_FLASHCARDS_APP));
  const bool wide = b.width > b.height;
  const int w = wide ? (b.width - 18) / 2 : b.width;
  char text[96];
  const auto hero = makeRect(b.x, b.y, w, 92);
  dashboard::frame(s, hero, 2);
  mark(s, makeRect(hero.x + 16, hero.y + 20, 52, 52), extra_dashboard_cards_32);
  caps(s, makeRect(hero.x + 96, hero.y + 12, hero.width - 110, 36), m.name, 1);
  std::snprintf(text, sizeof(text), m.total == 1 ? tr(STR_FLASH_CARD_ONE) : tr(STR_FLASH_CARD_COUNT), m.total);
  prose(s, makeRect(hero.x + 96, hero.y + 52, hero.width - 110, 32), text);
  const auto today = makeRect(b.x, b.y + 108, w, wide ? 132 : 148);
  dashboard::frame(s, today, 2);
  caps(s, makeRect(today.x + 12, today.y + 8, w - 24, 36), tr(STR_FLASH_FOR_TODAY), 1);
  for (int n = 0; n < 2; ++n) {
    std::snprintf(text, sizeof(text), "%d", n ? m.fresh : m.due);
    metric(s, makeRect(today.x + 12 + n * (w - 12) / 2, today.y + 50, (w - 36) / 2, wide ? 76 : 86), text,
           n ? tr(STR_FLASH_NEW) : tr(STR_FLASH_DUE), n ? extra_dashboard_new_32 : extra_notes_32);
  }
  const int ax = wide ? b.x + w + 18 : b.x, ay = wide ? b.y : b.y + 272;
  const auto activity = makeRect(ax, ay, w, 164);
  dashboard::frame(s, activity, 2);
  caps(s, makeRect(ax + 12, ay + 8, w - 24, 32), tr(STR_FLASH_RECENT_ACTIVITY), 1);
  if (m.writeFailed || m.clockUnset)
    prose(s, makeRect(ax + 12, ay + 48, w - 24, 104),
          m.writeFailed ? tr(STR_UI_SOME_REVIEWS_DID_NOT_SAVE_BB1305) : tr(STR_FLASH_NO_CLOCK), 4);
  else {
    int peak = 1, days = 0;
    for (int n = 0; n <= m.weekday; ++n)
      if (m.history) {
        peak = std::max(peak, m.history[n]);
        days += m.history[n] > 0;
      }
    static constexpr StrId weekdays[] = {
        StrId::STR_STATS_WEEKDAY_0, StrId::STR_STATS_WEEKDAY_1, StrId::STR_STATS_WEEKDAY_2, StrId::STR_STATS_WEEKDAY_3,
        StrId::STR_STATS_WEEKDAY_4, StrId::STR_STATS_WEEKDAY_5, StrId::STR_STATS_WEEKDAY_6};
    for (int n = 0; n < 7; ++n) {
      const int cw = (w - 28) / 7;
      const auto r = makeRect(ax + 14 + n * cw, ay + 48, cw - 9, 50);
      dashboard::frame(s, r, 2);
      const int age = m.weekday - n;
      const int value = m.history && age >= 0 ? m.history[age] : 0;
      const int h = value * (r.height - 8) / peak;
      if (h) s.target().fill(makeRect(r.x + 8, r.bottom() - 4 - h, r.width - 16, h), dashboard::ink);
      dashboard::text(s, makeRect(r.x, r.bottom() + 2, r.width, 26), I18N.get(weekdays[n]), 0);
    }
    std::snprintf(text, sizeof(text), days == 1 ? tr(STR_FLASH_WEEK_ONE) : tr(STR_FLASH_WEEK_DAYS), days);
    prose(s, makeRect(ax + 12, ay + 126, w - 24, 30), text);
  }
  const int actions = wide ? b.y + 180 : b.bottom() - 232;
  row(s, makeRect(ax, actions, w, 52), tr(STR_FLASH_DECK_HISTORY), nullptr, extra_calendar_32, History);
  row(s, makeRect(ax, actions + 60, w, 52), tr(STR_FLASH_DECK_CARDS), nullptr, extra_dashboard_cards_32, Cards);
  row(s, makeRect(wide ? b.x : ax, wide ? b.bottom() - 112 : actions + 120, w, 52), tr(STR_FLASH_START_REVIEW), nullptr,
      extra_dashboard_play_32, studyui::ActionStudy, 1, true);
  row(s, makeRect(wide ? b.x : ax, wide ? b.bottom() - 52 : actions + 180, w, 52), tr(STR_FLASH_BACK_DECKS), nullptr,
      extra_dashboard_undo_32, Back);
}
void history(toybox::Screen& s, const HistoryModel& m) {
  const auto b = begin(s, tr(STR_FLASH_HISTORY));
  const bool wide = b.width > b.height;
  const int w = wide ? (b.width - 18) / 2 : b.width;
  row(s, makeRect(b.x, b.y + 14, w, 70), m.filter, nullptr, extra_dashboard_cards_32, Filter);
  const auto left = makeRect(b.x + 48, b.y + 108, 48, 48), right = makeRect(b.x + w - 96, b.y + 108, 48, 48);
  dashboard::frame(s, left, 2);
  dashboard::chevron(s.target(), left, false, true);
  s.frame().hit(left, Month, -1);
  dashboard::frame(s, right, 2);
  dashboard::chevron(s.target(), right);
  s.frame().hit(right, Month, 1);
  caps(s, makeRect(left.right() + 12, left.y, w - 216, 48), m.month, 1);
  char text[32];
  std::snprintf(text, sizeof(text), "%d", m.sessions);
  metric(s, makeRect(b.x, b.y + 180, w / 2, 92), text, tr(STR_FLASH_SESSIONS), extra_dashboard_bars_32);
  std::snprintf(text, sizeof(text), "%d", m.cards);
  metric(s, makeRect(b.x + w / 2, b.y + 180, w - w / 2, 92), text,
         wide ? tr(STR_FLASH_CARDS) : tr(STR_FLASH_CARDS_STUDIED), extra_dashboard_cards_32);
  const int x = wide ? b.x + w + 18 : b.x, y = wide ? b.y : b.y + 300;
  section(s, makeRect(x, y, w, 36), tr(STR_FLASH_SESSIONS));
  if (m.error || !m.count)
    prose(s, makeRect(x + 12, y + 60, w - 24, 120), m.error ? tr(STR_FLASH_HISTORY_ERROR) : tr(STR_FLASH_EMPTY_HISTORY),
          4);
  for (int n = 0; n < m.count; ++n) {
    const auto& item = m.rows[n];
    char detail[160], time[40];
    duration(time, sizeof(time), item.session.tookMs, item.session.hasTiming);
    std::snprintf(detail, sizeof(detail), "%s · %u · %s", item.deck, unsigned(item.session.cards), time);
    row(s, makeRect(x, y + 48 + n * 92, w, 82), item.date, detail, extra_calendar_32, OpenSession, n);
  }
  paging(s, makeRect(x, b.bottom() - 54, w, 48), m.page, m.pages);
}
void session(toybox::Screen& s, const SessionModel& m) {
  const auto b = begin(s, tr(STR_FLASH_SESSION));
  const bool wide = b.width > b.height;
  const int w = wide ? (b.width - 18) / 2 : b.width;
  caps(s, makeRect(b.x, b.y, w, 38), m.deck, 1);
  prose(s, makeRect(b.x, b.y + 42, w, 32), m.date);
  char value[48];
  const char* labels[] = {tr(STR_FLASH_CARDS), tr(STR_FLASH_RESPONSES),
                          wide ? tr(STR_FLASH_TIME) : tr(STR_FLASH_STUDY_TIME), tr(STR_FLASH_NEW)};
  const freeink::Icon* icons[] = {&extra_dashboard_cards_32, &extra_notes_32, &extra_clock_32, &extra_dashboard_new_32};
  for (int n = 0; n < 4; ++n) {
    if (n == 2)
      duration(value, sizeof(value), m.session.tookMs, m.session.hasTiming);
    else
      std::snprintf(value, sizeof(value), "%u",
                    unsigned(n == 0   ? m.session.cards
                             : n == 1 ? m.session.answers
                                      : m.session.fresh));
    metric(s, makeRect(b.x + n % 2 * (w + 12) / 2, b.y + 86 + n / 2 * 96, (w - 12) / 2, 84), value, labels[n],
           *icons[n]);
  }
  prose(s, makeRect(b.x, b.y + 276, w, wide ? 48 : 28), tr(STR_FLASH_REPEAT_NOTE), 2);
  const int x = wide ? b.x + w + 18 : b.x, y = wide ? b.y : b.y + 314;
  section(s, makeRect(x, y, w, 36), tr(STR_FLASH_RATINGS));
  for (int n = 0; n < 4; ++n) {
    const auto r = makeRect(x, y + 44 + n * 54, w, 46);
    dashboard::frame(s, r, 2);
    mark(s, makeRect(r.x + 14, r.y + 5, 36, 36), *ratingIcons[n]);
    caps(s, makeRect(r.x + 74, r.y + 6, r.width - 138, 40), I18N.get(ratings[n]), 0);
    std::snprintf(value, sizeof(value), "%u", unsigned(m.session.ratings[n]));
    dashboard::text(s, makeRect(r.right() - 54, r.y + 4, 44, 44), value, 1, fui::TextAlign::Right);
  }
  row(s, makeRect(x, b.bottom() - 112, w, 52), tr(STR_FLASH_VIEW_REVIEWED), nullptr, extra_dashboard_cards_32, Cards);
  row(s, makeRect(x, b.bottom() - 52, w, 52), tr(STR_FLASH_BACK_HISTORY), nullptr, extra_dashboard_undo_32, Back);
}
void completed(toybox::Screen& s, const SessionModel& m) {
  const auto b = begin(s, tr(STR_FLASHCARDS_APP));
  const bool wide = b.width > b.height;
  const int w = wide ? (b.width - 18) / 2 : b.width;
  char headline[192];
  pixelui::copyCaps(headline, sizeof(headline), m.error ? tr(STR_READING_STORAGE_ERROR) : tr(STR_FLASH_COMPLETE));
  fittext::draw(s.target(), makeRect(b.x, b.y, w, 126), headline, 2, 2, fui::TextAlign::Center);
  flashart::draw(s, makeRect(b.x + 30, b.y + 132, w - 60, 140), flashart::Completed);
  caps(s, makeRect(b.x, b.y + 278, w, 44), m.deck, 1, 1, fui::TextAlign::Center);
  const int x = wide ? b.x + w + 18 : b.x, y = wide ? b.y : b.y + 328;
  char value[48];
  for (int n = 0; n < 3; ++n) {
    if (n == 2)
      duration(value, sizeof(value), m.session.tookMs, m.session.hasTiming);
    else
      std::snprintf(value, sizeof(value), "%u", unsigned(n ? m.session.answers : m.session.cards));
    metric(s, makeRect(x + n * (w + 8) / 3, y, (w - 16) / 3, 124), value,
           n == 0   ? tr(STR_FLASH_CARDS)
           : n == 1 ? tr(STR_FLASH_RESPONSES)
                    : tr(STR_FLASH_TIME),
           n == 0   ? extra_dashboard_cards_32
           : n == 1 ? extra_check_32
                    : extra_clock_32,
           true);
  }
  fittext::draw(s.target(), makeRect(x, y + 136, w, 48),
                m.error ? tr(STR_UI_SOME_REVIEWS_DID_NOT_SAVE_BB1305) : tr(STR_FLASH_DONE_TODAY), 4, 2,
                fui::TextAlign::Center);
  fittext::draw(s.target(), makeRect(wide ? b.x : x, wide ? b.y + 322 : y + 188, w, 48), tr(STR_FLASH_DETAILS_HISTORY),
                4, 2, fui::TextAlign::Center);
  row(s, makeRect(x, b.bottom() - 110, w, 50), m.session.answers ? tr(STR_FLASH_VIEW_SESSION) : tr(STR_FLASH_HISTORY),
      nullptr, extra_calendar_32, m.session.answers ? ViewSession : History, 0, true);
  row(s, makeRect(x, b.bottom() - 56, w, 56), tr(STR_FLASH_BACK_DECKS), nullptr, extra_dashboard_cards_32, Back);
}
void cards(toybox::Screen& s, const CardsModel& m) {
  const auto b = begin(s, tr(STR_FLASHCARDS_APP));
  caps(s, makeRect(b.x, b.y, b.width, 44), m.deck, 1);
  if (m.error || !m.count)
    prose(s, makeRect(b.x + 12, b.y + 80, b.width - 24, 100),
          m.error ? tr(STR_FLASH_HISTORY_ERROR) : tr(STR_FLASH_EMPTY_CARDS), 4);
  const int columns = b.width > b.height ? 2 : 1;
  const int rows = (m.perPage + columns - 1) / columns;
  const int width = (b.width - (columns - 1) * 12) / columns;
  const int height = (int(b.height) - 120 - (rows - 1) * 4) / rows;
  for (int n = 0; n < m.count; ++n) {
    const auto r = makeRect(b.x + n % columns * (width + 12), b.y + 56 + n / columns * (height + 4), width, height);
    const bool selected = s.frame().buttonSelected(OpenCard, n);
    s.target().fill(r, selected ? dashboard::ink : dashboard::paper);
    s.target().stroke(r, dashboard::ink, 1);
    fui::TextStyle text;
    text.font = 4;
    text.maxLines = 1;
    text.color = selected ? fui::Color::White : fui::Color::Black;
    s.target().text(r.inset({0, 8, 0, 8}), m.labels[n], text);
    s.frame().hit(r, OpenCard, n, fui::InputTouch | fui::InputLongPress);
  }
  paging(s, makeRect(b.x, b.bottom() - 52, b.width, 48), m.page, m.pages);
}
void help(toybox::Screen& s, int page) {
  const auto b = begin(s, tr(STR_FLASH_HELP));
  const bool wide = b.width > b.height;
  const int w = wide ? b.width / 3 : b.width;
  const int lessonW = wide ? b.width - w - 18 : w;
  caps(s, makeRect(b.x, b.y, w, 52), page ? tr(STR_FLASH_REMEMBER_RATE) : tr(STR_FLASH_PREPARE_DECKS), 2);
  const int artY = b.y + 64, artH = wide ? 188 : (page ? 142 : 156);
  if (!page)
    flashart::draw(s, makeRect(b.x, artY, w, artH), flashart::Prepare);
  else {
    prose(s, makeRect(b.x, artY, w, wide ? 72 : 48), tr(STR_FLASH_REMEMBER_HELP), 2);
    constexpr int flowGap = 32;
    const int tileW = (w - flowGap) / 2;
    const int tileY = artY + (wide ? 72 : 56);
    char flowLabels[2][32];
    pixelui::copyCaps(flowLabels[0], sizeof(flowLabels[0]), tr(STR_FLASH_QUESTION));
    pixelui::copyCaps(flowLabels[1], sizeof(flowLabels[1]), tr(STR_FLASH_ANSWER));
    fui::TextStyle flowStyle;
    flowStyle.font = pixelui::kSmall;
    const int labelWidth = std::max(s.target().measureText(flowStyle.font, flowLabels[0], flowStyle).width,
                                    s.target().measureText(flowStyle.font, flowLabels[1], flowStyle).width);
    const fui::FontId labelFont = labelWidth <= tileW - 16 ? pixelui::kSmall : 3;
    for (int n = 0; n < 2; ++n) {
      const auto r = makeRect(b.x + n * (tileW + flowGap), tileY, tileW, 86);
      dashboard::frame(s, r, 3);
      if (n)
        mark(s, makeRect(r.x + (r.width - 32) / 2, r.y + 10, 32, 32), extra_check_32);
      else
        dashboard::text(s, makeRect(r.x + 8, r.y + 10, r.width - 16, 32), "?", 1);
      pixelui::text(s.target(), makeRect(r.x + 8, r.bottom() - 34, r.width - 16, 28), flowLabels[n], labelFont,
                    fui::Color::Black, fui::TextAlign::Center);
    }
    dashboard::chevron(s.target(), makeRect(b.x + tileW + (flowGap - 24) / 2, tileY, 24, 86));
  }
  const int x = wide ? b.x + w + 18 : b.x, y = wide ? b.y + (page ? 6 : 40) : artY + artH + 14;
  if (!page) {
    static constexpr StrId titles[] = {StrId::STR_FLASH_COPY_FILES, StrId::STR_FLASH_CHOOSE_DECK,
                                       StrId::STR_FLASH_START_REVIEW};
    static constexpr StrId copy[] = {StrId::STR_FLASH_COPY_HELP, StrId::STR_FLASH_CHOOSE_HELP,
                                     StrId::STR_FLASH_START_HELP};
    for (int n = 0; n < 3; ++n) {
      const int h = wide ? 96 : 104;
      auto r = makeRect(x, y + n * (h + 10), lessonW, h);
      dashboard::frame(s, r, 2);
      char number[4];
      std::snprintf(number, sizeof(number), "%d", n + 1);
      const auto nr = makeRect(r.x + 12, r.y + 14, 56, 56);
      s.target().stroke(nr, dashboard::ink, 3, 28);
      dashboard::text(s, nr, number, 2);
      caps(s, makeRect(r.x + 80, r.y + 8, r.width - 94, wide ? 34 : 40), I18N.get(titles[n]), 1);
      prose(s, makeRect(r.x + 80, r.y + (wide ? 42 : 48), r.width - 94, r.height - (wide ? 46 : 52)), I18N.get(copy[n]),
            3);
    }
  } else {
    static constexpr StrId copy[] = {StrId::STR_FLASH_AGAIN_HELP, StrId::STR_FLASH_HARD_HELP,
                                     StrId::STR_FLASH_GOOD_HELP, StrId::STR_FLASH_EASY_HELP};
    for (int n = 0; n < 4; ++n) {
      auto r = makeRect(x, y + n * (wide ? 92 : 76), lessonW, wide ? 84 : 68);
      dashboard::frame(s, r, 2);
      mark(s, makeRect(r.x + 14, r.y + 18, 38, 38), *ratingIcons[n]);
      caps(s, makeRect(r.x + 80, r.y + 6, r.width - 94, 30), I18N.get(ratings[n]), 0);
      prose(s, makeRect(r.x + 80, r.y + 36, r.width - 94, wide ? 48 : 32), I18N.get(copy[n]), 2);
    }
    prose(s, makeRect(wide ? b.x : x, wide ? b.y + 276 : y + 312, w, 48), tr(STR_FLASH_INTERVAL_HELP), 2);
    prose(s, makeRect(wide ? b.x : x, wide ? b.y + 224 : y + 360, w, 48), tr(STR_FLASH_HISTORY_HELP), 2);
  }
  const auto pager = makeRect(b.x, b.bottom() - 50, w, 48);
  dashboard::frame(s, pager, 2);
  const auto prev = makeRect(pager.x, pager.y, 56, 48), next = makeRect(pager.right() - 56, pager.y, 56, 48);
  dashboard::frame(s, prev, 2);
  dashboard::frame(s, next, 2);
  dashboard::chevron(s.target(), prev, false, true);
  dashboard::chevron(s.target(), next);
  char count[16];
  std::snprintf(count, sizeof(count), "%d / 2", page + 1);
  dashboard::text(s, makeRect(prev.right(), pager.y, w - 112, 48), count, 1);
  if (page) s.frame().hit(prev, Page, -1);
  if (!page) s.frame().hit(next, Page, 1);
}
}  // namespace flashui

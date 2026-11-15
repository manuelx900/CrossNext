#include "../ui/PageIndicator.h"
#include "PetActionSurface.h"
#include "PetAlbumWidgets.h"

namespace petui {
using namespace widget;
using namespace albumwidget;
namespace {
void statisticCard(toybox::Screen& s, fui::Rect r, const char* label, const char* value, const freeink::Icon& mark,
                   int side = 36) {
  actionsurface::panel(s, r);
  constexpr bool inverse = false;
  const int topHeight = std::min(2 * int(s.target().lineHeight(3)) + 4, int(r.height) - 30);
  const auto picture = rect(r.x + 12, r.y + (r.height - side) / 2, side, side);
  icon(s, picture, mark, inverse);
  const int left = picture.right() + 8, width = r.right() - left - 10;
  caption(s, rect(left, r.y + 6, width, topHeight - 4), label, 2, inverse, 3);
  caption(s, rect(left, r.y + topHeight, width, r.height - topHeight - 6), value, 1, inverse, 0, true);
}
void statistics(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  if (m.profile->readingSeconds >= 3600 && m.profile->readingSeconds / 60 % 60 == 0)
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_ALBUM_HOURS),
                  static_cast<unsigned long long>(m.profile->readingSeconds / 3600));
  else
    duration(w.secondary, sizeof(w.secondary), m.profile->readingSeconds);
  fui::Rect reading, days, streak;
  if (r.width > r.height * 3) {
    const int lead = r.width / 2;
    reading = rect(r.x, r.y, lead - UI_GAP / 2, r.height);
    days = rect(reading.right() + UI_GAP, r.y, (r.width - lead - UI_GAP) / 2 - UI_GAP / 2, r.height);
    streak = rect(days.right() + UI_GAP, r.y, r.right() - days.right() - UI_GAP, r.height);
  } else {
    reading = rect(r.x, r.y, r.width, 88);
    days = cell(rect(r.x, r.y + 88 + UI_GAP, r.width, r.height - 88 - UI_GAP), 0, 2, UI_GAP);
    streak = cell(rect(r.x, r.y + 88 + UI_GAP, r.width, r.height - 88 - UI_GAP), 1, 2, UI_GAP);
  }
  statisticCard(s, reading, tr(STR_PET_READING_ACCUMULATED), w.secondary, referenceicons::BOOK, 64);
  std::snprintf(w.label, sizeof(w.label), "%u", unsigned(m.profile->sharedDays));
  statisticCard(s, days, tr(STR_PET_READING_DAYS), w.label, extra_calendar_32);
  std::snprintf(w.label, sizeof(w.label), "%u", unsigned(m.profile->streak));
  statisticCard(s, streak, tr(STR_PET_ALBUM_BEST_STREAK), w.label, extra_reading_flame_64);
}
void memoriesRow(toybox::Screen& s, Work& w, fui::Rect r) {
  const bool inverse = button(s, w, r, "", OpenMemories, 0, true);
  icon(s, rect(r.x + 14, r.y + (r.height - 32) / 2, 40, 32), referenceicons::MEMORIES, inverse);
  text(s, rect(r.x + 64, r.y + 4, r.width - 104, r.height - 8), tr(STR_PET_VIEW_MEMORIES), 0, inverse);
  chevron(s, rect(r.right() - 27, r.y + (r.height - 18) / 2, 10, 18), inverse);
}
void livesRow(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_ALBUM_LIVES), unsigned(m.history ? m.history->count : 0));
  const bool inverse = button(s, w, r, "", OpenLives);
  icon(s, rect(r.x + 14, r.y + (r.height - 32) / 2, 40, 32), extra_clock_32, inverse);
  prose(s, rect(r.x + 64, r.y + 4, r.width - 104, r.height - 8), w.label, 2, fui::TextAlign::Left, inverse);
  chevron(s, rect(r.right() - 27, r.y + (r.height - 18) / 2, 10, 18), inverse);
}
void books(toybox::Screen& s, fui::Rect r) {
  const int width = r.width * 88 / 100, height = r.height * 88 / 100;
  const auto book = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  icon(s, book, referenceicons::BOOK_STACK);
}
}  // namespace
void album(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const int statsH = wide ? 104 : 192;
  statistics(s, m, w, rect(r.x, r.y, r.width, statsH));
  r.y += statsH + UI_GAP;
  r.height -= statsH + UI_GAP;
  if (wide) {
    const int left = r.width * 55 / 100;
    stages(s, m, rect(r.x, r.y, left, r.height));
    const auto side = rect(r.x + left + UI_GAP, r.y, r.width - left - UI_GAP, r.height);
    books(s, rect(side.x + 8, side.y, side.width - 16, side.height - 2 * (CONTROL_HEIGHT + UI_GAP)));
    memoriesRow(s, w, rect(side.x, side.bottom() - 2 * CONTROL_HEIGHT - UI_GAP, side.width, CONTROL_HEIGHT));
    livesRow(s, m, w, rect(side.x, side.bottom() - CONTROL_HEIGHT, side.width, CONTROL_HEIGHT));
    return;
  }
  const int stageH = 196;
  stages(s, m, rect(r.x, r.y, r.width, stageH));
  const int illustrationY = r.y + stageH + UI_GAP;
  books(s, rect(r.x + 30, illustrationY, r.width - 60, r.bottom() - 2 * (CONTROL_HEIGHT + UI_GAP) - illustrationY));
  memoriesRow(s, w, rect(r.x, r.bottom() - 2 * CONTROL_HEIGHT - UI_GAP, r.width, CONTROL_HEIGHT));
  livesRow(s, m, w, rect(r.x, r.bottom() - CONTROL_HEIGHT, r.width, CONTROL_HEIGHT));
}
void memoryDetails(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  if (m.view == pet::View::Memory) {
    const unsigned id = unsigned(std::clamp(m.memory, 0, int(MEMORY_COUNT - 1)));
    memoryHeading(w.label, sizeof(w.label), *m.profile, id, m.dateOrder);
    text(s, rect(r.x + 8, r.y, r.width - 16, 44), w.label, 0, false, fui::TextAlign::Center);
    const auto image =
        wide ? rect(r.x, r.y + 54, r.width * 47 / 100, r.height - 116) : rect(r.x, r.y + 60, r.width, 270);
    if (memoryOwned(*m.profile, id))
      vignette(s, m, image, id);
    else
      icon(s, image, memoryUnrecorded(*m.profile, id) ? extra_information_32 : referenceicons::LOCK);
    const auto copy =
        wide ? rect(image.right() + 18, image.y + 8, r.right() - image.right() - 26, image.height - 12)
             : rect(r.x, image.bottom() + UI_GAP, r.width, r.bottom() - image.bottom() - CONTROL_HEIGHT - 2 * UI_GAP);
    const bool owned = memoryOwned(*m.profile, id);
    caption(s, rect(copy.x, copy.y, copy.width, wide ? 28 : 36), memoryTitle(id), 1, false, 0, true);
    const int day = memoryDay(*m.profile, id);
    if (day > 0 && m.profile->bornDay > 0 && day >= m.profile->bornDay) {
      const auto days = unsigned(int64_t(day) - m.profile->bornDay);
      std::snprintf(w.secondary, sizeof(w.secondary), days == 1 ? tr(STR_PET_AGE_DAY) : tr(STR_PET_AGE_DAYS), days);
    } else
      std::snprintf(w.secondary, sizeof(w.secondary), "%s", tr(STR_PET_AGE_UNKNOWN));
    caption(s, rect(copy.x, copy.y + (wide ? 32 : 40), copy.width, wide ? 24 : 30), w.secondary, 1, false, 4);
    if (!owned) {
      caption(s, rect(copy.x, copy.y + 78, copy.width, 28),
              memoryUnrecorded(*m.profile, id) ? tr(STR_PET_MEMORY_TIME_UNKNOWN) : tr(STR_PET_MEMORY_PENDING_SHORT), 1,
              false, 3);
      prose(s, rect(copy.x, copy.y + 116, copy.width, std::max(0, int(copy.height) - 116)),
            memoryRequirement(*m.profile, id, w), 5, fui::TextAlign::Center);
    } else {
      const auto reading = rect(copy.x, copy.y + (wide ? 64 : 80), copy.width, wide ? 80 : 88);
      actionsurface::panel(s, reading);
      icon(s, rect(reading.x + 12, reading.y + 25, 36, 36), referenceicons::BOOK);
      caption(s, rect(reading.x + 58, reading.y + 6, reading.width - 70, 26), tr(STR_PET_READING_ACCUMULATED), 1, false,
              3);
      if (id < pet::MOMENT_COUNT && m.profile->momentDays[id] > 0 && day > 0)
        std::snprintf(w.label, sizeof(w.label), tr(STR_PET_MINUTES),
                      static_cast<unsigned long long>(m.profile->momentReading[id] / 60));
      else
        std::snprintf(w.label, sizeof(w.label), "%s",
                      owned ? tr(STR_PET_MEMORY_TIME_UNKNOWN) : tr(STR_PET_MEMORY_PENDING_SHORT));
      caption(s, rect(reading.x + 58, reading.y + 34, reading.width - 70, 44), w.label, 1, false,
              fui::HEADER_CLOCK_FONT, true);
      const int storyY = wide ? 160 : 184;
      prose(s, rect(copy.x, copy.y + storyY, copy.width, std::max(0, int(copy.height) - storyY)),
            owned ? memoryText(id) : memoryRequirement(*m.profile, id, w), 3, fui::TextAlign::Center);
    }
    button(s, w, rect(r.x, r.bottom() - CONTROL_HEIGHT, r.width, CONTROL_HEIGHT),
           m.memoryReturn == pet::View::Album ? tr(STR_PET_BACK_ALBUM) : tr(STR_PET_MEMORIES), Back, 0, true);
    return;
  }
  text(s, rect(r.x, r.y, r.width, 40), tr(STR_PET_MEMORIES), 0);
  const int count = wide ? 3 : 4, columns = wide ? 3 : 2, rows = count / columns;
  const auto discovered = memoryOwnedCount(*m.profile);
  w.pages = std::max(1, (int(discovered) + count - 1) / count);
  const int page = std::clamp(m.page, 0, w.pages - 1);
  const auto grid = rect(r.x, r.y + 54, r.width, r.height - 54 - CONTROL_HEIGHT - UI_GAP);
  if (!discovered) prose(s, grid.inset({20, 20, 20, 20}), tr(STR_PET_MEMORIES_EMPTY), 4, fui::TextAlign::Center);
  const int height = (grid.height - UI_GAP * (rows - 1)) / rows;
  for (int i = 0; i < count; ++i) {
    const unsigned at = unsigned(page * count + i);
    if (at >= discovered) break;
    memoryCard(s, m, w,
               cell(rect(grid.x, grid.y + (i / columns) * (height + UI_GAP), grid.width, height), i % columns, columns,
                    UI_GAP),
               memoryAt(*m.profile, at), wide);
  }
  const int back = std::min(200, int(r.width) / 2);
  const auto footer = rect(r.x, r.bottom() - CONTROL_HEIGHT, r.width, CONTROL_HEIGHT);
  button(s, w, rect(footer.x, footer.y, back, footer.height), tr(STR_PET_BACK_ALBUM), Back);
  std::snprintf(w.label, sizeof(w.label), "%d / %d", page + 1, w.pages);
  pager(s, w, rect(footer.x + back + UI_GAP, footer.y, footer.width - back - UI_GAP, footer.height), w.label, Page,
        page > 0, page + 1 < w.pages);
}
}  // namespace petui

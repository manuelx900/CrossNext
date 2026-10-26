#include "../ui/PageIndicator.h"
#include "PetAlbumWidgets.h"

namespace petui {
using namespace widget;
using namespace albumwidget;
namespace {
void statistics(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool inverse = button(s, w, r, "", OpenAccumulatedHelp);
  const int side = r.height < 80 ? 32 : 52;
  icon(s, rect(r.x + 12, r.y + (r.height - side) / 2, side, side), extra_reading_book_64, inverse);
  const auto copy = rect(r.x + side + 26, r.y + 10, r.width - side - 38, r.height - 20);
  if (m.profile->readingSeconds >= 3600 && m.profile->readingSeconds / 60 % 60 == 0)
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_ALBUM_HOURS),
                  static_cast<unsigned long long>(m.profile->readingSeconds / 3600));
  else
    duration(w.secondary, sizeof(w.secondary), m.profile->readingSeconds);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_ALBUM_READING), w.secondary);
  text(s, rect(copy.x, r.y + (r.height < 80 ? 4 : 10), copy.width, 28), w.label, 3, inverse);
  std::snprintf(w.label, sizeof(w.label), m.profile->sharedDays == 1 ? tr(STR_PET_ALBUM_DAY) : tr(STR_PET_ALBUM_DAYS),
                unsigned(m.profile->sharedDays), unsigned(m.profile->streak));
  if (r.height < 80)
    prose(s, rect(copy.x, r.y + 34, copy.width, 22), w.label, 1, fui::TextAlign::Left, inverse);
  else
    prose(s, rect(copy.x, copy.y + 32, copy.width, copy.height - 32), w.label, 2, fui::TextAlign::Left, inverse);
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
  pixelui::copyCaps(w.secondary, sizeof(w.secondary), w.label);
  prose(s, rect(r.x + 64, r.y + 4, r.width - 104, r.height - 8), w.secondary, 2, fui::TextAlign::Left, inverse);
  chevron(s, rect(r.right() - 27, r.y + (r.height - 18) / 2, 10, 18), inverse);
}
void books(toybox::Screen& s, fui::Rect r) {
  const int width = r.width * 88 / 100, height = r.height * 88 / 100;
  icon(s, rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height), referenceicons::BOOK_STACK);
}
}  // namespace
void album(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const int statsH = wide ? 64 : 100;
  statistics(s, m, w, rect(r.x, r.y, r.width, statsH));
  r.y += statsH + UI_GAP;
  r.height -= statsH + UI_GAP;
  if (wide) {
    const int left = r.width * 55 / 100;
    stages(s, m, rect(r.x, r.y, left, r.height));
    const auto side = rect(r.x + left + GAP, r.y, r.width - left - GAP, r.height);
    books(s, rect(side.x + 8, side.y, side.width - 16, side.height - 124));
    memoriesRow(s, w, rect(side.x, side.bottom() - 116, side.width, 54));
    livesRow(s, m, w, rect(side.x, side.bottom() - 54, side.width, 54));
    return;
  }
  const int stageH = 196;
  stages(s, m, rect(r.x, r.y, r.width, stageH));
  const int illustrationY = r.y + stageH + GAP;
  books(s, rect(r.x + 30, illustrationY, r.width - 60, r.bottom() - 124 - illustrationY));
  memoriesRow(s, w, rect(r.x, r.bottom() - 116, r.width, 54));
  livesRow(s, m, w, rect(r.x, r.bottom() - 54, r.width, 54));
}
void memoryDetails(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  if (m.view == pet::View::Memory) {
    const unsigned id = unsigned(std::clamp(m.memory, 0, int(MEMORY_COUNT - 1)));
    memoryHeading(w.label, sizeof(w.label), *m.profile, id, m.dateOrder);
    text(s, rect(r.x + 8, r.y, r.width - 16, 44), w.label, 0, false, fui::TextAlign::Center);
    const auto image =
        wide ? rect(r.x, r.y + 54, r.width * 47 / 100, r.height - 116) : rect(r.x + 12, r.y + 60, r.width - 24, 270);
    if (memoryOwned(*m.profile, id))
      vignette(s, m, image, id);
    else
      icon(s, image, memoryUnrecorded(*m.profile, id) ? extra_information_32 : referenceicons::LOCK);
    const auto copy = wide ? rect(image.right() + 18, image.y + 8, r.right() - image.right() - 26, image.height - 12)
                           : rect(r.x + 16, image.bottom() + 18, r.width - 32, r.bottom() - image.bottom() - 90);
    const bool owned = memoryOwned(*m.profile, id);
    caption(s, rect(copy.x, copy.y, copy.width, 56), memoryTitle(id));
    if (owned)
      memoryReading(w.label, sizeof(w.label), *m.profile, id);
    else
      std::snprintf(
          w.label, sizeof(w.label), "%s",
          memoryUnrecorded(*m.profile, id) ? tr(STR_PET_MEMORY_TIME_UNKNOWN) : tr(STR_PET_MEMORY_PENDING_SHORT));
    text(s, rect(copy.x, copy.y + 62, copy.width, 26), w.label, 3, false, fui::TextAlign::Center);
    prose(s, rect(copy.x, copy.y + 96, copy.width, std::min(92, std::max(0, int(copy.height) - 96))),
          owned ? memoryText(id) : memoryRequirement(*m.profile, id, w), 5, fui::TextAlign::Center);
    if (memoryOwned(*m.profile, id))
      prose(s, rect(copy.x, copy.y + 181, copy.width, std::max(0, int(copy.height) - 181)),
            memoryCaptured(*m.profile, id) ? tr(STR_PET_MEMORY_SNAPSHOT)
            : id >= pet::MOMENT_COUNT      ? tr(STR_PET_DISCOVERY_REFERENCE)
                                           : tr(STR_PET_MEMORY_LEGACY),
            4, fui::TextAlign::Center);
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54),
           m.memoryReturn == pet::View::Album ? tr(STR_PET_BACK_ALBUM) : tr(STR_PET_MEMORIES), Back, 0, true);
    return;
  }
  text(s, rect(r.x + 6, r.y + 6, r.width - 12, 36), tr(STR_PET_MEMORIES), 0);
  const int count = wide ? 3 : 4, columns = wide ? 3 : 2, rows = count / columns;
  const auto discovered = memoryOwnedCount(*m.profile);
  w.pages = std::max(1, (int(discovered) + count - 1) / count);
  const int page = std::clamp(m.page, 0, w.pages - 1);
  const auto grid = rect(r.x, r.y + 54, r.width, r.height - 116);
  if (!discovered) prose(s, grid.inset({20, 20, 20, 20}), tr(STR_PET_MEMORIES_EMPTY), 4, fui::TextAlign::Center);
  const int height = (grid.height - GAP * (rows - 1)) / rows;
  for (int i = 0; i < count; ++i) {
    const unsigned at = unsigned(page * count + i);
    if (at >= discovered) break;
    memoryCard(s, m, w,
               cell(rect(grid.x, grid.y + (i / columns) * (height + GAP), grid.width, height), i % columns, columns),
               memoryAt(*m.profile, at), wide);
  }
  const int back = 200;
  button(s, w, rect(r.x, r.bottom() - 48, back, 48), tr(STR_PET_BACK_ALBUM), Back);
  pageui::build(s, rect(r.x + back + GAP, r.bottom() - 48, r.width - back - GAP, 48), page + 1, w.pages, Page, -1, Page,
                1);
}
}  // namespace petui

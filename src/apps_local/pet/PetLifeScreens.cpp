#include <ctime>

#include "../reading/ReadingStatsCore.h"
#include "../ui/PageIndicator.h"
#include "PetWidgets.h"

namespace petui {
using namespace widget;
namespace {
void endedDate(char* out, size_t bytes, int64_t timestamp) {
  const auto value = time_t(timestamp);
  std::tm date{};
  if (timestamp < 1577836800 || timestamp > 7258118399LL || !localtime_r(&value, &date)) {
    std::snprintf(out, bytes, "%s", tr(STR_PET_MEMORY_DATE_UNKNOWN));
    return;
  }
  if (I18N.getLanguage() == Language::ES)
    std::snprintf(out, bytes, tr(STR_PET_MEMORY_DATE_SHORT), unsigned(date.tm_mday), unsigned(date.tm_mon + 1),
                  unsigned((date.tm_year + 1900) % 100));
  else
    std::snprintf(out, bytes, tr(STR_PET_MEMORY_DATE_SHORT), unsigned((date.tm_year + 1900) % 100),
                  unsigned(date.tm_mon + 1), unsigned(date.tm_mday));
}
}  // namespace
void lifeDetails(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool wide = r.width > r.height;
  text(s, rect(r.x + 8, r.y, r.width - 16, 44), tr(STR_PET_PREVIOUS_LIVES), 0);
  if (m.view == pet::View::Retire) {
    frame(s, rect(r.x, r.y + 60, r.width, wide ? 126 : 220));
    prose(s, rect(r.x + 16, r.y + 76, r.width - 32, wide ? 94 : 188), tr(STR_PET_RETIRE_HELP), 7);
    button(s, w, rect(r.x, r.bottom() - 116, r.width, 54), tr(STR_PET_RETIRE_CONFIRM), ConfirmRetire, 0, true, nullptr,
           m.writable && m.historyWritable);
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_PET_CANCEL), Back);
    return;
  }
  if (m.view == pet::View::LifeDetail && m.history && m.memory >= 0 && m.memory < m.history->count) {
    const auto& life = m.history->records[m.memory];
    const auto card = rect(r.x, r.y + 60, r.width, r.height - 122);
    frame(s, card);
    text(s, rect(card.x + 14, card.y + 10, card.width - (wide ? 150 : 28), wide ? 34 : 45),
         displayName(life.name, life.species), wide ? 1 : 2);
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GENERATION), static_cast<unsigned long>(life.generation));
    text(s, wide ? rect(card.right() - 130, card.y + 10, 116, 30) : rect(card.x + 14, card.y + 66, card.width - 28, 30),
         w.label, 1);
    const auto age = pet::stage(life.readingSeconds);
    const int bodyY = wide ? 56 : 102;
    const auto picture = wide ? rect(card.x + 12, card.y + bodyY, card.width * 32 / 100, card.height - bodyY - 14)
                              : rect(card.x + 12, card.y + 102, card.width - 24, 190);
    art(s, m, picture,
        age == pet::Stage::Egg ? unsigned(life.species) : 3 + unsigned(life.species) * 16 + (unsigned(age) - 1) * 4,
        speciesIcon(life.species), false);
    const auto copy =
        wide ? rect(picture.right() + 14, card.y + bodyY, card.width - picture.width - 40, card.height - bodyY - 12)
             : rect(card.x + 14, card.y + 302, card.width - 28, card.height - 314);
    text(s, rect(copy.x, copy.y, copy.width, 26), stageName(age), 0);
    text(s, rect(copy.x, copy.y + 28, copy.width, 26),
         life.reason == pet::LifeStatus::Lost ? tr(STR_PET_LIFE_REASON_LOST) : tr(STR_PET_LIFE_REASON_RETIRED), 0);
    if (m.lifeEndedDay > 0) {
      const auto date = readingstats::dateFromDay(m.lifeEndedDay);
      if (I18N.getLanguage() == Language::ES)
        std::snprintf(w.label, sizeof(w.label), tr(STR_PET_LIFE_ENDED_DATE), unsigned(date.day), unsigned(date.month),
                      unsigned(date.year));
      else
        std::snprintf(w.label, sizeof(w.label), tr(STR_PET_LIFE_ENDED_DATE), unsigned(date.year), unsigned(date.month),
                      unsigned(date.day));
      text(s, rect(copy.x, copy.y + 54, copy.width, 24), w.label, 3);
    }
    duration(w.label, sizeof(w.label), life.readingSeconds);
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_LIFE_SUMMARY), w.label, unsigned(life.days));
    prose(s, rect(copy.x, copy.y + 82, copy.width, copy.height - 82), w.secondary, 3);
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54),
           m.lifeReturn == pet::View::Album ? tr(STR_PET_BACK_ALBUM) : tr(STR_PET_BACK_HISTORY), Back, 0, true);
    return;
  }
  const bool retire = p.lifeStatus == pet::LifeStatus::Active && pet::stage(p.readingSeconds) == pet::Stage::Adult;
  const bool restart = p.lifeStatus != pet::LifeStatus::Active;
  const int footer = (retire || restart) ? 62 : 0;
  if (!m.historyWritable) {
    info(s, rect(r.x, r.y + 61, r.width, 110), tr(STR_PET_ARCHIVE_FAILED));
  } else if (!m.history || !m.history->count) {
    info(s, rect(r.x, r.y + 61, r.width, wide ? 126 : 200), tr(STR_PET_NO_LIVES_SHORT));
  } else {
    const int count = m.history->count;
    w.pages = (count + 3) / 4;
    const int page = std::clamp(m.page, 0, w.pages - 1);
    const auto grid = rect(r.x, r.y + 54, r.width, r.height - 113 - footer);
    const int rowH = wide ? (grid.height - 3 * GAP) / 4 : (grid.height - GAP) / 2;
    for (int i = 0; i < 4; ++i) {
      const int index = count - 1 - page * 4 - i;
      if (index < 0) break;
      const auto& life = m.history->records[index];
      const auto box = wide ? rect(grid.x, grid.y + i * (rowH + GAP), grid.width, rowH)
                            : cell(rect(grid.x, grid.y + (i / 2) * (rowH + GAP), grid.width, rowH), i % 2, 2);
      const bool inverse = button(s, w, box, "", OpenLife, index);
      if (wide) {
        text(s, rect(box.x + 8, box.y + 3, box.width * 30 / 100 - 16, box.height - 6),
             displayName(life.name, life.species), 0, inverse);
        std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GENERATION), static_cast<unsigned long>(life.generation));
        text(s, rect(box.x + box.width * 30 / 100, box.y + 3, box.width * 15 / 100, box.height - 6), w.label, 3,
             inverse, fui::TextAlign::Center);
        endedDate(w.label, sizeof(w.label), life.endedAt);
        text(s, rect(box.x + box.width * 45 / 100, box.y + 3, box.width * 25 / 100, box.height - 6), w.label, 3,
             inverse, fui::TextAlign::Center);
        duration(w.label, sizeof(w.label), life.readingSeconds);
        text(s, rect(box.x + box.width * 70 / 100, box.y + 3, box.width * 30 / 100 - 8, box.height - 6), w.label, 3,
             inverse, fui::TextAlign::Right);
        continue;
      }
      text(s, rect(box.x + 8, box.y + 8, box.width - 16, 31), displayName(life.name, life.species), 1, inverse);
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GENERATION), static_cast<unsigned long>(life.generation));
      text(s, rect(box.x + 8, box.y + 43, box.width - 16, 24), w.label, 3, inverse);
      endedDate(w.label, sizeof(w.label), life.endedAt);
      text(s, rect(box.x + 8, box.bottom() - 62, box.width - 16, 23), w.label, 3, inverse);
      duration(w.label, sizeof(w.label), life.readingSeconds);
      text(s, rect(box.x + 8, box.bottom() - 33, box.width - 16, 27), w.label, 0, inverse);
    }
    pageui::build(s, rect(r.x, r.bottom() - 48 - footer, r.width, 48), page + 1, w.pages, Page, -1, Page, 1);
  }
  if (retire || restart)
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), retire ? tr(STR_PET_RETIRE) : tr(STR_PET_NEW_COMPANION),
           retire ? OpenRetire : NewCompanion, 0, true, nullptr, m.writable && m.historyWritable);
}
}  // namespace petui

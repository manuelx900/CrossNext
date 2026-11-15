#include <ctime>

#include "../reading/ReadingStatsCore.h"
#include "../ui/PageIndicator.h"
#include "PetReferenceIcons.h"
#include "PetSimpleScreens.h"

namespace petui {
using namespace widget;
namespace {
void endedDate(char* out, size_t bytes, int64_t timestamp, dateformat::Order order, bool compact = true) {
  const auto value = time_t(timestamp);
  std::tm date{};
  if (timestamp < 1577836800 || timestamp > 7258118399LL || !localtime_r(&value, &date)) {
    std::snprintf(out, bytes, "%s", tr(STR_PET_MEMORY_DATE_UNKNOWN));
    return;
  }
  dateformat::format(out, bytes, unsigned(date.tm_mday), unsigned(date.tm_mon + 1), unsigned(date.tm_year + 1900),
                     order, compact);
}
}  // namespace

namespace {
void lifeAge(char* out, size_t bytes, const pet::LifeRecord& life) {
  if (!life.ageKnown) {
    std::snprintf(out, bytes, "%s", tr(STR_PET_LIFE_AGE_UNKNOWN));
  } else {
    std::snprintf(out, bytes, life.ageDays == 1 ? tr(STR_PET_AGE_DAY) : tr(STR_PET_AGE_DAYS), unsigned(life.ageDays));
  }
}
void lifeButton(toybox::Screen& s, Work& w, fui::Rect r, const char* label, fui::ActionId action, bool primary = false,
                bool enabled = true) {
  const bool inverse = actionsurface::button(s, w, r, action, 0, primary, enabled);
  simple::copy(s, r.inset({4, 10, 4, 10}), label, true, fui::TextAlign::Center, inverse, 1, 4);
}
void lifePicture(toybox::Screen& s, const Model& m, fui::Rect r, const pet::LifeRecord& life, bool inverse = false) {
  const auto stage = pet::stage(life.readingSeconds);
  const auto id =
      stage == pet::Stage::Egg ? unsigned(life.species) : 3 + unsigned(life.species) * 16 + (unsigned(stage) - 1) * 4;
  art(s, m, r, id, speciesIcon(life.species), false, inverse);
}
void lifeTile(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, int index, bool horizontal) {
  const auto& life = m.history->records[index];
  const bool inverse = actionsurface::button(s, w, box, OpenLife, index);
  const int artSide = std::min(128, int(box.height) - 24);
  const auto picture = horizontal ? rect(box.x + 10, box.y + (box.height - artSide) / 2, artSide, artSide)
                                  : rect(box.x + 14, box.y + 96, box.width - 28, box.height - 156);
  lifePicture(s, m, picture, life, inverse);
  const int left = horizontal ? picture.right() + 14 : box.x + 12;
  const int width = box.right() - left - 12;
  simple::copy(s, rect(left, box.y + 10, width, 30), displayName(life.name, life.species), true, fui::TextAlign::Left,
               inverse, 1, 0);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GENERATION), static_cast<unsigned long>(life.generation));
  std::snprintf(w.secondary, sizeof(w.secondary), "%s · %s", w.label, stageName(pet::stage(life.readingSeconds)));
  simple::copy(s, rect(left, box.y + 42, width, 24), w.secondary, false, fui::TextAlign::Left, inverse, 1, 3);
  lifeAge(w.label, sizeof(w.label), life);
  simple::copy(s, rect(left, box.y + 68, width, 24), w.label, true, fui::TextAlign::Left, inverse, 1, 4);
  endedDate(w.label, sizeof(w.label), life.endedAt, m.dateOrder);
  simple::copy(s, rect(left, box.bottom() - 56, width, 22), w.label, false, fui::TextAlign::Left, inverse, 1, 3);
  duration(w.label, sizeof(w.label), life.readingSeconds);
  icon(s, rect(left, box.bottom() - 30, 22, 22), referenceicons::BOOK, inverse);
  simple::copy(s, rect(left + 30, box.bottom() - 32, width - 30, 26), w.label, true, fui::TextAlign::Left, inverse, 1,
               4);
}
void lifeMetric(toybox::Screen& s, fui::Rect box, const char* title, const char* value, const freeink::Icon& mark) {
  actionsurface::panel(s, box);
  icon(s, rect(box.x + 12, box.y + 12, 30, 30), mark);
  simple::copy(s, rect(box.x + 50, box.y + 10, box.width - 62, 40), title, false, fui::TextAlign::Center, false, 2, 3);
  simple::copy(s, rect(box.x + 10, box.bottom() - 40, box.width - 20, 30), value, true, fui::TextAlign::Center, false,
               1, 0);
}
void lifeDetailCard(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& life = m.history->records[m.memory];
  const bool wide = r.width > r.height;
  const auto content = rect(r.x, r.y + 54, r.width, r.height - 54 - CONTROL_HEIGHT - UI_GAP);
  const auto hero = wide ? rect(content.x, content.y, content.width * 43 / 100, content.height)
                         : rect(content.x, content.y, content.width, content.height - 216);
  actionsurface::panel(s, hero);
  simple::copy(s, rect(hero.x + 12, hero.y + 12, hero.width - 24, 34), displayName(life.name, life.species), true,
               fui::TextAlign::Center, false, 1, 0);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GENERATION), static_cast<unsigned long>(life.generation));
  std::snprintf(w.secondary, sizeof(w.secondary), "%s · %s", stageName(pet::stage(life.readingSeconds)), w.label);
  simple::copy(s, rect(hero.x + 12, hero.y + 48, hero.width - 24, 24), w.secondary, false, fui::TextAlign::Center,
               false, 1, 4);
  lifeAge(w.label, sizeof(w.label), life);
  simple::copy(s, rect(hero.x + 12, hero.y + 76, hero.width - 24, 26), w.label, true, fui::TextAlign::Center, false, 1,
               4);
  lifePicture(s, m, rect(hero.x + 20, hero.y + 112, hero.width - 40, hero.height - 124), life);
  const auto facts = wide ? rect(hero.right() + 16, content.y, content.right() - hero.right() - 16, content.height)
                          : rect(content.x, hero.bottom() + 16, content.width, content.bottom() - hero.bottom() - 16);
  const int metricH = wide ? std::min(96, int(facts.height) / 2 - 8) : 94;
  const auto metrics = rect(facts.x, facts.y, facts.width, metricH);
  duration(w.label, sizeof(w.label), life.readingSeconds);
  lifeMetric(s, cell(metrics, 0, 2, UI_GAP), tr(STR_PET_READING), w.label, referenceicons::BOOK);
  std::snprintf(w.label, sizeof(w.label), "%u", unsigned(life.days));
  lifeMetric(s, cell(metrics, 1, 2, UI_GAP), tr(STR_PET_READING_DAYS), w.label, extra_calendar_32);
  const auto status = rect(facts.x, metrics.bottom() + 16, facts.width, facts.bottom() - metrics.bottom() - 16);
  actionsurface::panel(s, status);
  const int lineH = 24;
  const int y = status.y + std::max(6, (int(status.height) - lineH * 2 - 4) / 2);
  simple::copy(s, rect(status.x + 12, y, status.width - 24, lineH),
               life.reason == pet::LifeStatus::Lost ? tr(STR_PET_LIFE_REASON_LOST) : tr(STR_PET_LIFE_REASON_RETIRED),
               true, fui::TextAlign::Center, false, 1, 4);
  endedDate(w.secondary, sizeof(w.secondary), life.endedAt, m.dateOrder, false);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_LIFE_ENDED_ON), w.secondary);
  simple::copy(s, rect(status.x + 12, y + lineH + 4, status.width - 24, lineH), w.label, false, fui::TextAlign::Center,
               false, 1, 3);
  lifeButton(s, w, rect(r.x, r.bottom() - CONTROL_HEIGHT, r.width, CONTROL_HEIGHT),
             m.lifeReturn == pet::View::Album ? tr(STR_PET_BACK_ALBUM) : tr(STR_PET_BACK_HISTORY), Back, true);
}
}  // namespace
void lifeDetails(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool wide = r.width > r.height;
  simple::copy(s, rect(r.x, r.y, r.width, 40), tr(STR_PET_PREVIOUS_LIVES), true, fui::TextAlign::Left, false, 1, 0);
  if (m.view == pet::View::Retire) {
    actionsurface::panel(s, rect(r.x, r.y + 60, r.width, wide ? 126 : 220));
    prose(s, rect(r.x + 16, r.y + 76, r.width - 32, wide ? 94 : 188), tr(STR_PET_RETIRE_HELP), 7);
    button(s, w, rect(r.x, r.bottom() - 2 * CONTROL_HEIGHT - UI_GAP, r.width, CONTROL_HEIGHT),
           tr(STR_PET_RETIRE_CONFIRM), ConfirmRetire, 0, true, nullptr, m.writable && m.historyWritable);
    button(s, w, rect(r.x, r.bottom() - CONTROL_HEIGHT, r.width, CONTROL_HEIGHT), tr(STR_PET_CANCEL), Back);
    return;
  }

  if (m.view == pet::View::LifeDetail && m.history && m.memory >= 0 && m.memory < m.history->count) {
    lifeDetailCard(s, m, w, r);
    return;
  }
  const bool retire = p.lifeStatus == pet::LifeStatus::Active && pet::stage(p.readingSeconds) == pet::Stage::Adult;
  const bool restart = p.lifeStatus != pet::LifeStatus::Active;
  const bool hasAction = retire || restart;
  const int footer = hasAction && !wide ? CONTROL_HEIGHT + UI_GAP : 0;
  const int pagerWidth = hasAction && wide ? r.width / 2 : r.width;
  const auto pagination = rect(r.x, r.bottom() - CONTROL_HEIGHT - footer, pagerWidth, CONTROL_HEIGHT);
  if (!m.historyWritable) {
    info(s, rect(r.x, r.y + 54, r.width, 110), tr(STR_PET_ARCHIVE_FAILED));
  } else if (!m.history || !m.history->count) {
    info(s, rect(r.x, r.y + 54, r.width, wide ? 126 : 200), tr(STR_PET_NO_LIVES_SHORT));
  } else {
    const int count = m.history->count;
    constexpr int perPage = 3;
    w.pages = (count + perPage - 1) / perPage;
    const int page = std::clamp(m.page, 0, w.pages - 1);
    const auto grid = rect(r.x, r.y + 40 + UI_GAP, r.width, pagination.y - UI_GAP - r.y - 40 - UI_GAP);
    const int rows = wide ? 1 : 3;
    const int columns = (perPage + rows - 1) / rows;
    const int rowH = (grid.height - (rows - 1) * UI_GAP) / rows;
    for (int i = 0; i < perPage; ++i) {
      const int index = count - 1 - page * perPage - i;
      if (index < 0) break;
      const auto row = rect(grid.x, grid.y + (i / columns) * (rowH + UI_GAP), grid.width, rowH);
      const auto box = cell(row, i % columns, columns, UI_GAP);
      lifeTile(s, m, w, box, index, !wide);
    }
    std::snprintf(w.label, sizeof(w.label), "%d / %d", page + 1, w.pages);
    pager(s, w, pagination, w.label, Page, page > 0, page + 1 < w.pages);
  }
  if (retire || restart)
    lifeButton(
        s, w,
        wide ? rect(pagination.right() + UI_GAP, pagination.y, r.right() - pagination.right() - UI_GAP, CONTROL_HEIGHT)
             : rect(r.x, r.bottom() - CONTROL_HEIGHT, r.width, CONTROL_HEIGHT),
        retire ? tr(STR_PET_RETIRE) : tr(STR_PET_NEW_COMPANION), retire ? OpenRetire : NewCompanion, true,
        m.writable && m.historyWritable);
}
}  // namespace petui

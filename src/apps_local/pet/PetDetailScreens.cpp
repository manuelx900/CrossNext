#include "../ui/PageIndicator.h"
#include "PetCareIcons.h"
#include "PetCatalogView.h"
#include "PetCelebration.h"
#include "PetOverview.h"
#include "PetPolishIcons.h"
#include "PetProjectPreview.h"
#include "PetReadingResults.h"
#include "PetReferenceIcons.h"
#include "PetWidgets.h"

namespace petui {
using namespace widget;
namespace {
void growthArrow(toybox::Screen& s, fui::Rect r) {
  static constexpr const char* arrow[] = {"    #   ", "    ##  ", "    ### ", "########",
                                          "########", "    ### ", "    ##  ", "    #   "};
  const int x = r.x + (r.width - 24) / 2, y = r.y + (r.height - 24) / 2;
  for (int row = 0; row < 8; ++row)
    for (int col = 0; col < 8; ++col)
      if (arrow[row][col] == '#') s.target().fill(rect(x + col * 3, y + row * 3, 3, 3), ink);
}
void growthCheck(toybox::Screen& s, fui::Rect r) { icon(s, r, referenceicons::MEDAL); }
void projects(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  if (!pet::homeUnlocked(*m.profile)) {
    info(s, rect(r.x, r.y + 20, r.width, 100), tr(STR_PET_HOME_LOCKED));
    button(s, w, rect(r.x, r.bottom() - 48, r.width, 48), tr(STR_PET_BACK_HOME), Back, 0, true);
    return;
  }
  text(s, rect(r.x + 5, r.y, r.width - 10, 34), tr(STR_PET_CATALOG_TITLE), 1);
  const bool wide = r.width > r.height;
  const int count = wide ? 4 : 6, columns = wide ? 4 : 2, rows = count / columns;
  const unsigned category = unsigned(std::clamp(m.projectSlot, 0, 3));
  static constexpr StrId categories[] = {StrId::STR_PET_REST_SLOT, StrId::STR_PET_DECOR_SLOT, StrId::STR_PET_GAME_SLOT,
                                         StrId::STR_PET_SCENE_SLOT};
  const int sideWidth = wide ? 148 : 0;
  for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
    const auto categoryBox = wide ? rect(r.x, r.y + 44 + int(slot) * 44, sideWidth, 44)
                                  : cell(rect(r.x, r.y + 44, r.width, 44), int(slot), 4, 0);
    button(s, w, categoryBox, I18N.get(categories[slot]), SelectCategory, int(slot), slot == category);
  }
  const auto filterRow =
      wide ? rect(r.x + sideWidth + 12, r.y + 44, r.width - sideWidth - 12, 44) : rect(r.x, r.y + 96, r.width, 44);
  for (unsigned filter = 0; filter < 2; ++filter) {
    std::snprintf(w.label, sizeof(w.label), filter ? tr(STR_PET_CATALOG_UNOWNED) : tr(STR_PET_CATALOG_OWNED),
                  catalogCount(*m.profile, category, CatalogFilter(filter)));
    button(s, w, cell(filterRow, int(filter), 2, 8), w.label, SelectCatalogFilter, int(filter),
           m.catalogFilter == CatalogFilter(filter));
  }
  const unsigned total = catalogCount(*m.profile, category, m.catalogFilter);
  w.pages = std::max(1, int((total + count - 1) / count));
  const int page = std::clamp(m.page, 0, w.pages - 1);
  const auto grid = wide ? rect(filterRow.x, r.y + 100, filterRow.width, r.height - 160)
                         : rect(r.x, r.y + 152, r.width, r.height - 212);
  const int h = (grid.height - (rows - 1) * GAP) / rows;
  if (!total) {
    frame(s, grid);
    const int artH = std::min(wide ? 96 : 180, int(grid.height) * 55 / 100);
    icon(s, rect(grid.x + 20, grid.y + (wide ? 8 : 26), grid.width - 40, artH),
         m.catalogFilter == CatalogFilter::Owned ? referenceicons::EMPTY : referenceicons::COMPLETE);
    const int copyY = grid.y + artH + (wide ? 14 : 44);
    prose(s, rect(grid.x + 22, copyY, grid.width - 44, grid.bottom() - copyY - 12),
          m.catalogFilter == CatalogFilter::Owned ? tr(STR_PET_CATALOG_EMPTY_OWNED) : tr(STR_PET_CATALOG_EMPTY_UNOWNED),
          5, fui::TextAlign::Center);
  }
  for (int n = 0; n < count; ++n) {
    const unsigned ordinal = unsigned(page * count + n);
    if (ordinal >= total) break;
    const unsigned item = catalogItem(*m.profile, category, m.catalogFilter, ordinal);
    const auto box = cell(rect(grid.x, grid.y + (n / columns) * (h + GAP), grid.width, h), n % columns, columns);
    const auto& p = *m.profile;
    const bool owned = pet::owns(p, item), equipped = p.equipped[category] == item;
    const bool current = equipped || (!owned && p.project == item);
    const bool inverse = button(s, w, box, "", OpenProjectPreview, int(item));
    if (current) s.target().stroke(box, ink, 8);
    const int side = std::max(18, std::min(54, h - 85));
    object(s, m, rect(box.x + (box.width - side) / 2, box.y + 8, side, side), item, inverse);
    prose(s, rect(box.x + 6, box.bottom() - 70, box.width - 12, 42), objectName(item), 3, fui::TextAlign::Center,
          inverse);
    text(s, rect(box.x + 6, box.bottom() - 28, box.width - 12, 23),
         equipped  ? tr(STR_PET_EQUIPPED)
         : current ? tr(STR_PET_CATALOG_CURRENT_SHORT)
                   : tr(STR_PET_CATALOG_VIEW),
         3, inverse, fui::TextAlign::Center);
  }
  const int backWidth = 200;
  button(s, w, rect(r.x, r.bottom() - 48, backWidth, 48), tr(STR_PET_BACK_HOME), Back);
  pageui::build(s, rect(r.x + backWidth + GAP, r.bottom() - 48, r.width - backWidth - GAP, 48), page + 1, w.pages, Page,
                -1, Page, 1);
}
void evolution(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const auto current = pet::stage(p.readingSeconds);
  const bool born = p.noticedStage == pet::Stage::Egg;
  const bool wide = r.width > r.height;
  const bool split = wide;
  const int width = std::min(int(r.width) - 24, wide ? 670 : 422);
  const int height = std::min(int(r.height) - 12, wide ? 338 : 680);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  s.target().fill(rect(box.x + 6, box.y + 6, box.width, box.height), ink);
  s.target().fill(box, paper);
  frame(s, box);
  const auto banner = rect(box.x + 10, box.y + 10, box.width - 20, 58);
  s.target().fill(banner, ink);
  text(s, banner.inset({0, 32, 0, 32}), tr(STR_PET_CONGRATULATIONS), 1, true, fui::TextAlign::Center);
  sparkle(s, banner.x + 18, banner.y + banner.height / 2, 18, true);
  sparkle(s, banner.right() - 18, banner.y + banner.height / 2, 18, true);
  text(s, rect(box.x + 16, box.y + (wide ? 74 : 86), box.width - 32, 40), displayName(p.name, p.species), 1, false,
       fui::TextAlign::Center);
  prose(s, rect(box.x + 18, box.y + (wide ? 116 : 138), box.width - 36, split ? 30 : 44),
        born ? tr(STR_PET_HATCHED) : tr(STR_PET_GREW), split ? 1 : 2, fui::TextAlign::Center);
  const auto stages = split ? rect(box.x + 16, box.y + 156, box.width * 50 / 100 - 24, box.height - 226)
                            : rect(box.x + 16, box.y + 196, box.width - 32, 194);
  const int arrowW = 26, stageW = (stages.width - arrowW - 16) / 2;
  for (int i = 0; i < 2; ++i) {
    const auto value = i ? current : p.noticedStage;
    const auto card = rect(i ? stages.right() - stageW : stages.x, stages.y, stageW, stages.height);
    frame(s, card);
    const unsigned artId = value == pet::Stage::Egg
                               ? unsigned(p.species)
                               : 3 + unsigned(p.species) * 16 + (unsigned(value) - 1) * 4 + (i ? 1 : 0);
    art(s, m, rect(card.x + 8, card.y + 8, card.width - 16, card.height - 54), artId, speciesIcon(p.species), false);
    const auto caption = rect(card.x + 6, card.bottom() - 39, card.width - 12, 33);
    if (!born) s.target().fill(caption, ink);
    prose(s, caption.inset({2, 2, 2, 2}), stageName(value), 2, fui::TextAlign::Center, !born);
  }
  growthArrow(s, rect(stages.x + stageW + 8, stages.y + (stages.height - 32) / 2, arrowW, 32));
  const auto message =
      split ? rect(stages.right() + 20, stages.y, box.right() - stages.right() - 36, stages.height)
            : rect(box.x + 18, stages.bottom() + 18, box.width - 36, box.bottom() - stages.bottom() - 92);
  const int markH = wide ? 40 : 74;
  if (born)
    growthCheck(s, rect(message.x, message.y, message.width, markH));
  else
    icon(s, rect(message.x + (message.width - 88) / 2, message.y, 88, markH), referenceicons::BOOK);
  prose(s, rect(message.x, message.y + markH + 8, message.width, message.height - markH - 8),
        born ? tr(STR_PET_CARE_UNLOCKED) : tr(STR_PET_GROWTH_THANKS), split ? 3 : 4, fui::TextAlign::Center);
  button(s, w, rect(box.x + 14, box.bottom() - 60, box.width - 28, 46), tr(STR_PET_CONTINUE), Continue, 0, true);
}
void reward(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool wide = r.width > r.height;
  frame(s, r);
  r = r.inset({10, 10, 10, 10});
  const auto banner = rect(r.x, r.y, r.width, wide ? 48 : 68);
  celebrationBanner(s, banner, tr(STR_PET_GOOD_READING), referenceicons::BOOK, true);
  const auto footer = rect(r.x, r.bottom() - 54, r.width, 54);
  const int sideWidth = wide ? r.width * 60 / 100 : r.width;
  const auto panels = wide
                          ? rect(r.right() - sideWidth, banner.bottom() + 6, sideWidth, footer.y - banner.bottom() - 12)
                          : rect(r.x, footer.y - 294, sideWidth, 284);
  const auto scene = wide ? rect(r.x, banner.bottom() + 8, r.width - sideWidth - 12, footer.y - banner.bottom() - 18)
                          : rect(r.x, banner.bottom() + 12, r.width, panels.y - banner.bottom() - 20);
  const int elapsedW = wide ? scene.width : scene.width * 52 / 100;
  const auto copy = rect(scene.x + 4, scene.y + (wide ? scene.height - 116 : 14), elapsedW - 8, 112);
  prose(s, rect(copy.x, copy.y, copy.width, 58), tr(STR_PET_SINCE_VISIT), 2);
  const auto elapsed = p.readingSeconds - std::min(p.readingSeconds, p.noticedSeconds);
  duration(w.label, sizeof(w.label), elapsed);
  const auto value = rect(copy.x, copy.y + 60, copy.width, 48);
  frame(s, value);
  text(s, value.inset({5, 5, 5, 5}), w.label, 1, false, fui::TextAlign::Center);
  const auto picture = wide ? rect(scene.x + 4, scene.y, scene.width - 8, scene.height - 126)
                            : rect(scene.x + elapsedW, scene.y, scene.width - elapsedW, scene.height);
  celebrationPicture(s, m, picture);
  const auto gains = rect(panels.x, panels.y, panels.width, wide ? 62 : 78);
  for (int i = 0; i < 2; ++i) {
    const auto box = cell(gains, i, 2);
    frame(s, box);
    const int markW = std::min(58, int(box.width) / 3);
    icon(s, rect(box.x + 8, box.y + 12, markW - 8, box.height - 24), i ? MEDICINE_ICON : careicons::FOOD);
    s.target().line({int16_t(box.x + markW + 4), int16_t(box.y + 10)},
                    {int16_t(box.x + markW + 4), int16_t(box.bottom() - 10)}, 1, ink);
    std::snprintf(w.label, sizeof(w.label), "+%llu", static_cast<unsigned long long>(readingGains(p, i != 0)));
    text(s, rect(box.x + markW + 14, box.y + 4, box.width - markW - 22, 35), w.label, 1);
    text(s, rect(box.x + markW + 14, box.bottom() - 30, box.width - markW - 22, 24),
         i ? tr(STR_PET_MEDICINE) : tr(STR_PET_PROVISIONS), 3);
  }
  const int progressH = wide ? 90 : 92;
  const auto progress = rect(panels.x, gains.bottom() + GAP, panels.width, progressH);
  growth(s, m, w, progress, true);
  const auto daily = rect(panels.x, progress.bottom() + GAP, panels.width, panels.bottom() - progress.bottom() - GAP);
  frame(s, daily);
  icon(s, rect(daily.x + 12, daily.y + 12, 46, daily.height - 24), referenceicons::GOAL);
  const int left = daily.x + 74, width = daily.width - 86;
  const bool available = dailyAvailable(m);
  const auto goal = unsigned(pet::GOALS[p.goalIndex]);
  if (available) {
    if (m.todaySeconds >= int(goal * 60))
      std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_GOAL_REACHED));
    else
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_DAILY_PROGRESS),
                    std::min(std::max(0, m.todaySeconds) / 60, int(goal)), goal);
  } else
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_DAILY_WAIT));
  text(s, rect(left, daily.y + 5, width, 30), w.label, 0, false, fui::TextAlign::Left, true);
  duration(w.secondary, sizeof(w.secondary), uint64_t(std::max(0, m.todaySeconds)));
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_READ_TODAY), w.secondary);
  if (available) prose(s, rect(left, daily.y + 37, width, daily.height - 42), w.label, 1);
  button(s, w, footer, tr(STR_PET_CONTINUE), Continue, 0, true);
}
}  // namespace
void details(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  switch (m.view) {
    case pet::View::Health:
    case pet::View::HealthHelp:
      careDetails(s, m, w, r);
      return;
    case pet::View::LifeDetail:
    case pet::View::Retire:
      lifeDetails(s, m, w, r);
      return;
    case pet::View::Games:
      games(s, m, w, r);
      return;
    case pet::View::Play:
      play(s, m, w, r);
      return;
    case pet::View::Projects:
      projects(s, m, w, r);
      return;
    case pet::View::ProjectPreview:
      projectPreview(s, m, w, r);
      return;
    case pet::View::Reward:
      reward(s, m, w, r);
      return;
    case pet::View::Evolution:
      evolution(s, m, w, r);
      return;
    case pet::View::Lives:
      lifeDetails(s, m, w, r);
      return;
    case pet::View::Notice:
      noticeWindow(s, m, w, r);
      return;
    case pet::View::ReadingInfo:
    case pet::View::Memories:
    case pet::View::Memory:
    case pet::View::Affection:
    case pet::View::Collection:
    case pet::View::Death:
    case pet::View::Name:
    case pet::View::New:
    case pet::View::Info:
    case pet::View::Rules:
    case pet::View::Dashboard:
    case pet::View::Care:
    case pet::View::Home:
    case pet::View::Album:
    case pet::View::Settings:
    case pet::View::SettingsConfirm:
      return;
  }
}
}  // namespace petui

#pragma once

#include "../ui/FitText.h"
#include "PetActionSurface.h"
#include "PetCatalogView.h"
#include "PetWidgets.h"

namespace petui::inlinecatalog {
using namespace widget;
inline void copy(toybox::Screen& s, fui::Rect box, const char* value, bool bold = false, int lines = 1,
                 bool inverse = false, fui::TextAlign align = fui::TextAlign::Left) {
  if (lines > 1 && box.height >= 2 * s.target().lineHeight(3)) {
    prose(s, box, value, lines, align, inverse);
    return;
  }
  fui::TextStyle style;
  style.font = box.height < s.target().lineHeight(4) ? 3 : 4;
  style.bold = bold;
  style.maxLines = 1;
  style.color = inverse ? fui::Color::White : fui::Color::Black;
  style.align = align;
  fittext::draw(s.target(), box, value, style, std::max(2, lines));
}
inline void control(toybox::Screen& s, Work& w, fui::Rect box, const char* title, fui::ActionId action, int value = 0,
                    bool selected = false, bool enabled = true) {
  const bool inverse = actionsurface::button(s, w, box, action, value, selected, enabled);
  copy(s, box.inset({3, 4, 3, 4}), title, selected, 1, inverse, fui::TextAlign::Center);
}
inline void projectCard(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool complete = pet::collectionComplete(p), pending = hasReadingProject(p), stacked = r.width < 300;
  const auto action = complete ? OpenCollection : OpenProjects;
  constexpr int value = 4;
  const bool inverse = actionsurface::button(s, w, r, action, value);
  const int side = std::min(stacked ? int(r.width) - 32 : int(r.width) / 3, int(r.height) - (stacked ? 140 : 24));
  const auto picture = stacked ? rect(r.x + (r.width - side) / 2, r.y + 36, side, side)
                               : rect(r.x + 12, r.y + (r.height - side) / 2, side, side);
  if (complete) {
    if (!m.collectionArt.draw || !m.collectionArt.draw(m.collectionArt.context, s.target(), picture, 0, inverse))
      icon(s, picture, extra_trophy_64, inverse);
  } else if (pending)
    object(s, m, picture, p.project, inverse);
  else {
    const int side = std::min({64, int(picture.width), int(picture.height)});
    const auto add = rect(picture.x + (picture.width - side) / 2, picture.y + (picture.height - side) / 2, side, side);
    actionsurface::panel(s, add);
    icon(s, add.inset({16, 16, 16, 16}), extra_dashboard_add_64);
  }
  const auto detail = stacked ? rect(r.x + 12, picture.bottom() + 10, r.width - 24, r.bottom() - picture.bottom() - 20)
                              : rect(picture.right() + 16, r.y + 10, r.right() - picture.right() - 28, r.height - 20);
  if (!complete && !pending) {
    const int height = std::min(80, int(detail.height));
    const int top = stacked ? detail.y + (detail.height - height) / 2 : r.y + (r.height - height) / 2;
    copy(s, rect(detail.x, top, detail.width, 24), tr(STR_PET_NEXT_DECORATION), true, 1, inverse,
         fui::TextAlign::Center);
    copy(s, rect(detail.x, top + 32, detail.width, height - 32), tr(STR_PET_PICK_DECORATION), true, 2, inverse,
         fui::TextAlign::Center);
    return;
  }
  copy(s, stacked ? rect(r.x + 8, r.y + 7, r.width - 16, 24) : rect(detail.x, detail.y, detail.width, 22),
       complete ? tr(STR_PET_COLLECTION_FINISHED) : tr(STR_PET_NEXT_DECORATION), true, 1, inverse,
       stacked ? fui::TextAlign::Center : fui::TextAlign::Left);
  const int top = detail.y + (stacked ? 0 : 26);
  copy(s, rect(detail.x, top, detail.width, 46),
       complete  ? tr(STR_PET_COLLECTION_TITLE)
       : pending ? objectName(p.project)
                 : tr(STR_PET_PICK_DECORATION),
       true, 2, inverse, stacked ? fui::TextAlign::Center : fui::TextAlign::Left);
  if (complete) {
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_COLLECTION_COUNT), pet::ownedCount(p), pet::CATALOG_COUNT);
    copy(s, rect(detail.x, top + 50, detail.width, 28), w.label, true, 1, inverse, fui::TextAlign::Center);
  } else if (pending) {
    const uint64_t target = pet::projectRequiredSeconds(pet::ownedCount(p));
    const uint64_t progress = std::min(pet::projectProgressSeconds(p), target);
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_PROJECT_FRACTION),
                  static_cast<unsigned long long>(progress / 60), static_cast<unsigned long long>(target / 60));
    copy(s, rect(detail.x, top + 48, detail.width, 24), w.label, true, 1, inverse,
         stacked ? fui::TextAlign::Center : fui::TextAlign::Left);
    const auto track = rect(detail.x, r.bottom() - 17, detail.width, 7);
    s.target().fill(track, fui::Paint::dither(fui::Color::LightGray), 3);
    s.target().fill(rect(track.x, track.y, target ? int(progress * track.width / target) : 0, track.height),
                    inverse ? paper : ink, 3);
  }
}
inline void home(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool unlocked = pet::homeUnlocked(p);
  static constexpr StrId slots[] = {StrId::STR_PET_REST_SLOT, StrId::STR_PET_DECOR_SLOT, StrId::STR_PET_GAME_SLOT,
                                    StrId::STR_PET_SCENE_SLOT};
  if (!unlocked) {
    copy(s, rect(r.x, r.y + 4, r.width, 94), tr(STR_PET_HOME_LOCKED), false, 4);
    control(s, w, rect(r.x, r.bottom() - 44, r.width, 44), tr(STR_PET_CONTINUE_READING), Read, 0, true);
    return;
  }
  const bool wide = s.device().width > s.device().height;
  const int footerH = wide ? COMPACT_HEIGHT : CONTROL_HEIGHT;
  const auto footer = rect(r.x, r.bottom() - footerH, r.width, footerH);
  fui::Rect goal, categories;
  if (wide) {
    const int left = r.width * 55 / 100;
    goal = rect(r.x, r.y, left - 8, r.height);
    categories = rect(r.x + left + 8, r.y, r.width - left - 8, r.height - footerH - UI_GAP);
  } else {
    categories = rect(r.x, r.y, r.width, 96);
    goal = rect(r.x, r.y + 96 + UI_GAP, r.width, r.height - 96 - footerH - 2 * UI_GAP);
  }
  projectCard(s, m, w, goal);
  for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
    const bool grid = wide;
    const int height = grid ? (categories.height - 12) / 2 : categories.height;
    const auto box =
        cell(rect(categories.x, categories.y + (grid ? int(slot / 2) * (height + 12) : 0), categories.width, height),
             grid ? int(slot % 2) : int(slot), grid ? 2 : 4, 8);
    const bool inverse = actionsurface::button(s, w, box, OpenProjects, int(slot));
    const auto item = pet::objectAvailable(p.equipped[slot]) ? p.equipped[slot] : pet::objectAt(slot, 0);
    const int side = box.height < 56 ? 18 : 44;
    const int top = box.height < 56 ? 4 : (box.height - 26 - side) / 2;
    object(s, m, rect(box.x + (box.width - side) / 2, box.y + top, side, side), item, inverse);
    copy(s, rect(box.x + 2, box.bottom() - 23, box.width - 4, 21), I18N.get(slots[slot]), false, 1, inverse,
         fui::TextAlign::Center);
  }
  const bool trophy = pet::collectionComplete(p);
  control(s, w, wide ? rect(categories.x, footer.y, categories.width, footer.height) : footer,
          trophy ? tr(STR_PET_COLLECTION_OPEN) : tr(STR_PET_DECORATE_HOME), trophy ? OpenCollection : OpenProjects,
          pet::collectionComplete(p) ? int(pet::objectSlot(p.project)) : 4, true);
}
inline void preview(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, unsigned item) {
  const auto& p = *m.profile;
  const unsigned slot = pet::objectSlot(item);
  const bool owned = pet::owns(p, item), placed = p.equipped[slot] == item;
  const bool wide = s.device().width > s.device().height;
  const int artWidth = box.width * (wide ? 16 : 38) / 100;
  object(s, m, rect(box.x + 6, box.y + 4, artWidth - 16, box.height - 8), item);
  const auto detail = rect(box.x + artWidth + 8, box.y, box.width - artWidth - 8 - (wide ? 200 : 0), box.height);
  const int nameHeight = wide ? 28 : 48;
  copy(s, rect(detail.x, detail.y, detail.width, nameHeight), objectName(item), true, wide ? 1 : 2);
  if (!m.writable) {
    copy(s, rect(detail.x, detail.y + nameHeight, detail.width, detail.height - nameHeight),
         tr(STR_PET_PREVIEW_READ_ONLY), false, 3);
    return;
  } else if (!owned) {
    w.previewProfile = p;
    w.previewProfile.equipped[slot] = uint8_t(item);
    if (p.project != item) {
      w.previewProfile.project = uint8_t(item);
      w.previewProfile.projectStartedSeconds = pet::projectReading(p);
    }
    remainingDuration(w.secondary, sizeof(w.secondary), pet::projectRemaining(w.previewProfile));
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_READING_LEFT_TIME), w.secondary);
    copy(s, rect(detail.x, detail.y + nameHeight, detail.width, detail.height - nameHeight - (wide ? 0 : 50)), w.label,
         false, 3);
  } else {
    copy(s, rect(detail.x, detail.y + nameHeight, detail.width, detail.height - nameHeight - (wide ? 0 : 50)),
         placed ? tr(STR_PET_EQUIPPED) : tr(STR_PET_CATALOG_VIEW), false, 2);
  }
  const bool current = !owned && p.project == item;
  const bool ready = current && pet::projectRemaining(p) == 0;
  control(s, w,
          wide ? rect(box.right() - 188, box.y + (box.height - 44) / 2, 188, 44)
               : rect(detail.x, detail.bottom() - 44, detail.width, 44),
          owned     ? (placed ? tr(STR_PET_REMOVE_HOME) : tr(STR_PET_CATALOG_PLACE))
          : ready   ? tr(STR_PET_GET_NOW)
          : current ? tr(STR_PET_CATALOG_CURRENT_SHORT)
                    : tr(STR_PET_CHOOSE_PROJECT),
          owned && placed ? RemoveProject : SelectProject, int(item), true, m.writable && (!current || ready));
}
inline void build(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  if (!pet::homeUnlocked(p)) {
    inlinecatalog::home(s, m, w, r);
    return;
  }
  const bool wide = s.device().width > s.device().height;
  const int controlH = wide ? 28 : 44;
  const int rowGap = wide ? GAP : UI_GAP, gridGap = UI_GAP;
  const int footerH = wide ? COMPACT_HEIGHT : CONTROL_HEIGHT;
  const unsigned slot = unsigned(std::clamp(m.projectSlot, 0, 3));
  static constexpr StrId slots[] = {StrId::STR_PET_REST_SLOT, StrId::STR_PET_DECOR_SLOT, StrId::STR_PET_GAME_SLOT,
                                    StrId::STR_PET_SCENE_SLOT};
  for (unsigned n = 0; n < pet::SLOT_COUNT; ++n)
    control(s, w, cell(rect(r.x, r.y, r.width, controlH), int(n), 4, GAP), I18N.get(slots[n]), SelectCategory, int(n),
            n == slot);
  for (unsigned n = 0; n < 2; ++n) {
    std::snprintf(w.label, sizeof(w.label), n ? tr(STR_PET_CATALOG_UNOWNED) : tr(STR_PET_CATALOG_OWNED),
                  catalogCount(p, slot, CatalogFilter(n)));
    control(s, w, cell(rect(r.x, r.y + controlH + rowGap, r.width, controlH), int(n), 2, 8), w.label,
            SelectCatalogFilter, int(n), m.catalogFilter == CatalogFilter(n));
  }
  const unsigned perPage = catalogPageSize(p, wide);
  const unsigned total = catalogCount(p, slot, m.catalogFilter);
  w.pages = std::max(1, int((total + perPage - 1) / perPage));
  const int page = std::clamp(m.page, 0, w.pages - 1);
  const bool showingPreview = m.view == pet::View::ProjectPreview;
  const int gridTop = r.y + 2 * controlH + rowGap + gridGap;
  const auto grid = rect(r.x, gridTop, r.width, r.bottom() - footerH - UI_GAP - gridTop);
  if (showingPreview && m.previewItem < pet::OBJECT_COUNT && pet::objectSlot(m.previewItem) == slot &&
      catalogMatches(p, m.previewItem, m.catalogFilter)) {
    preview(s, m, w, grid, m.previewItem);
  } else if (!total) {
    copy(s, grid.inset({12, 8, 12, 8}),
         m.catalogFilter == CatalogFilter::Owned ? tr(STR_PET_CATALOG_EMPTY_OWNED) : tr(STR_PET_CATALOG_EMPTY_UNOWNED),
         false, 4);
  } else {
    const int columns = int(perPage);
    const int rowH = grid.height;
    for (unsigned n = 0; n < perPage; ++n) {
      const unsigned item = catalogItem(p, slot, m.catalogFilter, unsigned(page) * perPage + n);
      if (item >= pet::OBJECT_COUNT) break;
      const auto box =
          cell(rect(grid.x, grid.y + int(n) / columns * (rowH + 8), grid.width, rowH), int(n) % columns, columns, 10);
      const bool inverse = actionsurface::button(s, w, box, OpenProjectPreview, int(item));
      if (wide) {
        object(s, m, rect(box.x + 8, box.y + 8, 42, box.height - 16), item, inverse);
        copy(s, rect(box.x + 56, box.y + 3, box.width - 64, box.height - 6), objectName(item), false, 2, inverse,
             fui::TextAlign::Center);
      } else {
        const int captionH = 56;
        object(s, m, rect(box.x + 12, box.y + 12, box.width - 24, box.height - captionH - 20), item, inverse);
        copy(s, rect(box.x + 6, box.bottom() - captionH - 4, box.width - 12, captionH), objectName(item), false, 3,
             inverse, fui::TextAlign::Center);
      }
      if (p.equipped[slot] == item) {
        const auto mark = rect(box.right() - 24, box.y + 7, 16, 16);
        s.target().fill(mark, inverse ? paper : ink, 8);
        const auto color = inverse ? ink : paper;
        s.target().line({int16_t(mark.x + 3), int16_t(mark.y + 8)}, {int16_t(mark.x + 7), int16_t(mark.y + 11)}, 2,
                        color);
        s.target().line({int16_t(mark.x + 7), int16_t(mark.y + 11)}, {int16_t(mark.x + 13), int16_t(mark.y + 5)}, 2,
                        color);
      }
    }
  }
  const bool hasPrevious =
      showingPreview ? catalogAdjacent(p, slot, m.catalogFilter, m.previewItem, -1) < pet::OBJECT_COUNT : page > 0;
  const bool hasNext = showingPreview ? catalogAdjacent(p, slot, m.catalogFilter, m.previewItem, 1) < pet::OBJECT_COUNT
                                      : page + 1 < w.pages;
  const auto ordinal = catalogOrdinal(p, slot, m.catalogFilter, m.previewItem);
  std::snprintf(w.label, sizeof(w.label), "%u / %u", showingPreview ? ordinal + 1 : unsigned(page + 1),
                showingPreview ? total : unsigned(w.pages));
  pager(s, w, rect(r.x, r.bottom() - footerH, r.width, footerH), w.label, showingPreview ? PreviewStep : Page,
        hasPrevious, hasNext);
}
}  // namespace petui::inlinecatalog

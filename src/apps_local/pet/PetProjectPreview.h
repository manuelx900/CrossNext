#pragma once

#include "PetCatalogView.h"
#include "PetReferenceIcons.h"
#include "PetWidgets.h"

namespace petui {
inline void previewArrow(toybox::Screen& s, Work& w, fui::Rect r, int step, bool enabled) {
  using namespace widget;
  const bool inverse = button(s, w, r, "", PreviewStep, step, false, nullptr, enabled);
  const int cx = r.x + r.width / 2, cy = r.y + r.height / 2, d = 9;
  s.target().line({int16_t(cx - step * d / 2), int16_t(cy - d)}, {int16_t(cx + step * d / 2), int16_t(cy)}, BORDER,
                  inverse ? paper : ink);
  s.target().line({int16_t(cx + step * d / 2), int16_t(cy)}, {int16_t(cx - step * d / 2), int16_t(cy + d)}, BORDER,
                  inverse ? paper : ink);
}
inline void projectPreview(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  using namespace widget;
  const auto& p = *m.profile;
  const unsigned id = m.previewItem;
  if (!pet::homeUnlocked(p) || id >= pet::OBJECT_COUNT) {
    info(s, rect(r.x, r.y + 20, r.width, 100), tr(STR_PET_HOME_LOCKED));
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_PET_BACK_CATALOG), Back);
    return;
  }
  const bool wide = r.width > r.height;
  if (!wide && r.height >= 620) {
    r.y += 20;
    r.height -= 40;
  }
  frame(s, r);
  r = r.inset({10, 10, 10, 10});
  const unsigned slot = pet::objectSlot(id);
  const auto ordinal = catalogOrdinal(p, slot, m.catalogFilter, id);
  const auto count = catalogCount(p, slot, m.catalogFilter);
  const bool previous = catalogAdjacent(p, slot, m.catalogFilter, id, -1) < pet::OBJECT_COUNT;
  const bool next = catalogAdjacent(p, slot, m.catalogFilter, id, 1) < pet::OBJECT_COUNT;
  const bool owned = pet::owns(p, id), current = owned ? p.equipped[slot] == id : p.project == id;
  const bool placed = owned && current && m.writable;
  const int statusH = wide ? 40 : 54;
  prose(s, rect(r.x + 8, r.y, r.width - 16, wide ? 44 : 52), objectName(id), 2, fui::TextAlign::Center);
  const auto room = wide ? rect(r.x, r.y + 52, r.width * 49 / 100, r.height - 114)
                         : rect(r.x, r.y + 66, r.width, std::min(260, int(r.height) * 43 / 100));
  frame(s, room);
  w.previewProfile = p;
  w.previewProfile.equipped[slot] = uint8_t(id);
  if (!owned) w.previewProfile.project = uint8_t(id);
  {
    Model scene = m;
    scene.profile = &w.previewProfile;
    habitat(s, scene, room.inset({4, 4, 4, 4}), 0);
  }
  auto detail = wide ? rect(room.right() + 12, room.y, r.right() - room.right() - 12, room.height - 54)
                     : rect(r.x, room.bottom() + 10, r.width, r.bottom() - room.bottom() - 136);
  if (placed) detail.height -= statusH + 8;
  frame(s, detail);
  const int side = wide ? std::min(70, int(detail.width) / 3) : 94;
  const auto image = rect(detail.x + 10, detail.y + 12, side, detail.height - 24);
  object(s, m, image, id);
  const int divider = image.right() + 12;
  s.target().line({int16_t(divider), int16_t(detail.y + 10)}, {int16_t(divider), int16_t(detail.bottom() - 10)}, 1,
                  ink);
  static constexpr StrId slots[] = {StrId::STR_PET_REST_SLOT, StrId::STR_PET_DECOR_SLOT, StrId::STR_PET_GAME_SLOT,
                                    StrId::STR_PET_SCENE_SLOT};
  const int left = divider + 12, width = detail.right() - left - 10, top = detail.y + 8;
  text(s, rect(left, top, width, 22), I18N.get(slots[slot]), 3);
  if (placed) {
    auto name = rect(left, top + 29, width, detail.bottom() - top - 37);
    fui::TextStyle style;
    style.font = 4;
    style.maxLines = 2;
    style.align = fui::TextAlign::Center;
    int height = 0;
    fui::layoutText(s.target(), name, objectName(id), style,
                    [&](const char*, fui::Rect line) { height = std::max(height, int(line.bottom() - name.y)); });
    name.y += std::max(0, (int(name.height) - height) / 2);
    name.height = height;
    s.target().text(name, objectName(id), style);
  } else {
    prose(s, rect(left, top + 25, width, 42), objectName(id), 2);
  }
  const auto note = rect(left, top + 69, width, detail.bottom() - top - 77);
  if (!m.writable) {
    prose(s, note, tr(STR_PET_PREVIEW_READ_ONLY), 4);
  } else if (owned) {
    if (!placed) prose(s, note, tr(STR_PET_PREVIEW_NO_CHANGE), 4);
  } else {
    remainingDuration(w.secondary, sizeof(w.secondary), pet::projectRemaining(w.previewProfile));
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_READING_LEFT_TIME), w.secondary);
    prose(s, rect(note.x, note.y, note.width, 22),
          current                                    ? tr(STR_PET_CATALOG_CURRENT_SHORT)
          : !pet::projectRemaining(w.previewProfile) ? tr(STR_PET_PROJECT_CAN_GET)
                                                     : w.label,
          1);
    if (!current) prose(s, rect(note.x, note.y + 23, note.width, note.height - 23), tr(STR_PET_PREVIEW_NO_CHANGE), 4);
  }
  const auto apply =
      wide ? rect(detail.x, room.bottom() - 46, detail.width, 46) : rect(r.x, r.bottom() - 116, r.width, 54);
  if (placed) {
    const auto status = rect(apply.x, apply.y - statusH - 8, apply.width, statusH);
    s.target().fill(status, ink);
    pixelui::copyCaps(w.label, sizeof(w.label), tr(STR_PET_EQUIPPED));
    const int textW = s.target().measureText(0, w.label, {}).width;
    const int x = status.x + (status.width - textW - 34) / 2;
    icon(s, rect(x, status.y + (statusH - 24) / 2, 24, 24), extra_checked_32, true);
    text(s, rect(x + 34, status.y, textW, statusH), w.label, 0, true);
  }
  const bool removable = owned && current;
  button(s, w, apply,
         removable ? tr(STR_PET_REMOVE_HOME)
         : current ? (owned ? tr(STR_PET_EQUIPPED) : tr(STR_PET_CATALOG_CURRENT_SHORT))
                   : (owned ? tr(STR_PET_CATALOG_PLACE) : tr(STR_PET_CHOOSE_PROJECT)),
         removable ? RemoveProject : SelectProject, int(id), !current, nullptr, m.writable && (!current || removable));
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_PREVIEW_POSITION), ordinal < count ? ordinal + 1 : 0, count);
  previewArrow(s, w, rect(r.x, r.bottom() - 54, 54, 54), -1, previous);
  previewArrow(s, w, rect(r.right() - 54, r.bottom() - 54, 54, 54), 1, next);
  const auto back = rect(r.x + 62, r.bottom() - 54, r.width - 124, 54);
  const bool inverse = button(s, w, back, "", Back);
  text(s, rect(back.x + 4, back.y + 4, back.width - 8, 23), tr(STR_PET_BACK_CATALOG), 3, inverse,
       fui::TextAlign::Center);
  text(s, rect(back.x + 4, back.y + 29, back.width - 8, 21), w.label, 3, inverse, fui::TextAlign::Center);
}
}  // namespace petui

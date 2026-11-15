#pragma once

#include "PetActionSurface.h"
#include "PetReadingStatus.h"

namespace petui::readingbar {
using namespace widget;
inline void build(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, bool enabled) {
  const auto& p = *m.profile;
  const bool egg = pet::stage(p.readingSeconds) == pet::Stage::Egg;
  const bool available = !egg && hasReadingProject(p);
  const bool project = available && m.readingDisplay == ReadingDisplay::Project;
  actionsurface::shadow(s, box);
  s.target().fill(box, ink, actionsurface::RADIUS);
  const auto read = rect(box.x, box.y, box.width - (egg ? 0 : 64), box.height);
  const bool inverse = actionsurface::button(s, w, read, Read, 0, true, enabled);
  const auto picture = rect(read.x + 12, read.y + (read.height - 48) / 2, 48, 48);
  if (egg) {
    const auto seconds = pet::nextStageSeconds(pet::Stage::Egg) - p.readingSeconds;
    const auto minutes = seconds / 60 + unsigned(seconds % 60 != 0);
    std::snprintf(w.label, sizeof(w.label), minutes == 1 ? tr(STR_PET_HATCH_MINUTE) : tr(STR_PET_HATCH_MINUTES),
                  static_cast<unsigned long long>(minutes));
    icon(s, picture, extra_reading_book_32, inverse);
  } else if (project) {
    const auto progress = projectProgress(p);
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_PROJECT_PROGRESS_MINUTES),
                  static_cast<unsigned long long>(progress.value / 60),
                  static_cast<unsigned long long>(progress.target / 60));
    object(s, m, picture, p.project, inverse);
  } else {
    readingTime(m, w.label, sizeof(w.label));
    icon(s, picture, extra_reading_book_32, inverse);
  }
  const auto textBox = rect(picture.right() + 12, read.y + 6, read.right() - picture.right() - 22, read.height - 12);
  fui::TextStyle label;
  label.font = 0;
  label.align = fui::TextAlign::Center;
  label.color = inverse ? fui::Color::White : fui::Color::Black;
  if (s.target().measureText(label.font, w.label, label).width <= textBox.width)
    s.target().text(toybox::inkCentred(s.target(), textBox, label), w.label, label);
  else
    prose(s, textBox, w.label, 2, fui::TextAlign::Center, inverse);
  if (egg) return;
  const auto toggle = rect(box.right() - 54, box.y + (box.height - 48) / 2, 48, 48);
  const bool toggleInverse = actionsurface::button(s, w, toggle, ToggleReadingDisplay, 0, false, enabled && available);
  const auto color = toggleInverse ? paper : ink;
  const int left = toggle.x + 12, right = toggle.right() - 12, y = toggle.y + toggle.height / 2;
  s.target().line({int16_t(left), int16_t(y - 5)}, {int16_t(right), int16_t(y - 5)}, 2, color);
  s.target().line({int16_t(right - 5), int16_t(y - 10)}, {int16_t(right), int16_t(y - 5)}, 2, color);
  s.target().line({int16_t(right), int16_t(y - 5)}, {int16_t(right - 5), int16_t(y)}, 2, color);
  s.target().line({int16_t(right), int16_t(y + 5)}, {int16_t(left), int16_t(y + 5)}, 2, color);
  s.target().line({int16_t(left + 5), int16_t(y)}, {int16_t(left), int16_t(y + 5)}, 2, color);
  s.target().line({int16_t(left), int16_t(y + 5)}, {int16_t(left + 5), int16_t(y + 10)}, 2, color);
}
}  // namespace petui::readingbar

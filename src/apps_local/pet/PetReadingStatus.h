#pragma once

#include "PetWidgets.h"

namespace petui {
namespace widget {
inline int64_t readingRemaining(const Model& m) {
  const auto& p = *m.profile;
  if (!m.clockKnown || m.historyChanged || m.now < 1577836800 || p.lastReadAt < 1577836800 || m.now < p.lastReadAt)
    return -1;
  return std::max<int64_t>(0, pet::absenceGrace(p.mode) - (m.now - p.lastReadAt));
}
inline void readingTime(const Model& m, char* value, size_t bytes) {
  const auto& p = *m.profile;
  if (p.lifeStatus != pet::LifeStatus::Active) {
    std::snprintf(value, bytes, "%s", tr(STR_PET_LIFE_ENDED));
    return;
  }
  if (p.mode == pet::Mode::Relaxed) {
    std::snprintf(value, bytes, "%s", tr(STR_PET_LIFE_NO_DEATH));
    return;
  }
  const auto remaining = readingRemaining(m);
  if (remaining <= 0) {
    std::snprintf(value, bytes, "%s", remaining < 0 ? tr(STR_PET_LIFE_UNKNOWN) : tr(STR_PET_LIFE_OVERDUE));
    return;
  }
  const uint64_t minutes = (uint64_t(remaining) + 59) / 60;
  if (minutes >= 1440)
    std::snprintf(value, bytes, tr(STR_PET_LIFE_DAYS), static_cast<unsigned long long>(minutes / 1440),
                  unsigned(minutes / 60 % 24));
  else
    std::snprintf(value, bytes, tr(STR_PET_LIFE_HOURS), static_cast<unsigned long long>(minutes / 60),
                  unsigned(minutes % 60));
}
inline bool readingUrgent(const Model& m) {
  const auto remaining = readingRemaining(m);
  return m.profile->created && m.profile->lifeStatus == pet::LifeStatus::Active &&
         m.profile->mode != pet::Mode::Relaxed && remaining >= 0 &&
         remaining <= std::min<int64_t>(86400, pet::absenceGrace(m.profile->mode) / 4);
}
inline void readingLife(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, bool compact = false) {
  const bool relaxed = m.profile->mode == pet::Mode::Relaxed;
  const bool urgent = readingUrgent(m);
  const bool inverse = button(s, w, r, "", urgent ? Read : OpenReadingInfo, 0, urgent, nullptr, !urgent || !m.loading);
  const int side = compact ? 22 : 26;
  icon(s, rect(r.x + 10, r.y + (r.height - side) / 2, side, side), extra_reading_book_32, inverse);
  const int x = r.x + side + 20, width = r.right() - x - 26;
  text(s, rect(x, r.y + 4, width, compact ? 20 : 26), relaxed ? tr(STR_PET_LIFE_GROW) : tr(STR_PET_LIFE_TITLE), 3,
       inverse);
  readingTime(m, w.label, sizeof(w.label));
  text(s, rect(x, r.y + (compact ? 26 : 34), width, compact ? 20 : 25), w.label, 3, inverse);
  icon(s, rect(r.right() - 22, r.y + (r.height - 18) / 2, 12, 18), icon_opens_32, inverse);
}
}  // namespace widget
}  // namespace petui

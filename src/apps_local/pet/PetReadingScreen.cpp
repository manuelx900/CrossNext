#include "PetReadingStatus.h"

namespace petui {
using namespace widget;
void readingInfo(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool wide = r.width > r.height;
  text(s, rect(r.x + 6, r.y, r.width - 12, 44), tr(STR_PET_LIFE_HELP), 1);
  const int left = wide ? r.width * 40 / 100 : r.width;
  const auto life = rect(r.x, r.y + 52, left, wide ? r.height - 114 : 200);
  panel(s, life);
  icon(s, rect(life.x + 12, life.y + 12, 34, 34), extra_reading_book_64);
  text(s, rect(life.x + 56, life.y + 9, life.width - 68, 38), modeName(p.mode), 1);
  readingTime(m, w.label, sizeof(w.label));
  text(s, rect(life.x + 12, life.y + 54, life.width - 24, 28), w.label, 0);
  if (p.mode != pet::Mode::Relaxed) {
    const unsigned hours = unsigned(pet::absenceGrace(p.mode) / 3600);
    std::snprintf(w.secondary, sizeof(w.secondary), hours >= 48 ? tr(STR_PET_GRACE_DAYS) : tr(STR_PET_GRACE_HOURS),
                  hours >= 48 ? hours / 24 : hours);
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_LIFE_RENEW), modeName(p.mode), w.secondary);
  }
  prose(s, rect(life.x + 12, life.y + 92, life.width - 24, life.height - 104),
        p.mode == pet::Mode::Relaxed ? tr(STR_PET_LIFE_RELAXED_HELP) : w.label, 7);
  const auto benefits = wide ? rect(life.right() + GAP, life.y, r.right() - life.right() - GAP, life.height)
                             : rect(r.x, life.bottom() + GAP, r.width, r.bottom() - life.bottom() - 70);
  panel(s, benefits);
  text(s, rect(benefits.x + 12, benefits.y + 7, benefits.width - 24, 28), tr(STR_PET_READING_BENEFITS), 1);
  const int introH = wide ? 48 : 72;
  const auto copy = [&](fui::Rect box, const char* value, int lines) { prose(s, box, value, lines); };
  copy(rect(benefits.x + 12, benefits.y + 42, benefits.width - 24, introH), tr(STR_PET_READING_EVERY_MINUTE), 3);
  const int suppliesY = benefits.y + 42 + introH + 8, foodH = wide ? 72 : 76;
  copy(rect(benefits.x + 12, suppliesY, benefits.width - 24, foodH), tr(STR_PET_READING_INTERVAL), 3);
  const int medicineY = suppliesY + foodH + 8;
  copy(rect(benefits.x + 12, medicineY, benefits.width - 24, 56), tr(STR_PET_READING_REMEDY), 2);
  if (!wide)
    copy(rect(benefits.x + 12, medicineY + 68, benefits.width - 24, benefits.bottom() - medicineY - 80),
         tr(STR_PET_LIFE_JOURNAL), 2);
  const auto actions = rect(r.x, r.bottom() - 54, r.width, 54);
  button(s, w, cell(actions, 0, 2), tr(STR_PET_CONTINUE), Back);
  button(s, w, cell(actions, 1, 2), tr(STR_PET_READING_LABEL), Read, 0, true, &extra_reading_book_32);
}
}  // namespace petui

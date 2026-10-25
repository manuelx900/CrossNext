#pragma once

#include "PetReadingStatus.h"

namespace petui {
namespace widget {
inline void growth(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto age = pet::stage(m.profile->readingSeconds);
  frame(s, r);
  const int mark = r.height < 58 ? 28 : 38;
  sprig(s, rect(r.x + 10, r.y + (r.height - mark) / 2, mark, mark), m.profile->species);
  const int left = r.x + mark + 22;
  const int right = r.width > 400 ? 62 : 8;
  if (age == pet::Stage::Adult) {
    text(s, rect(left, r.y + 3, r.right() - left - right, 24), tr(STR_PET_GROWTH_ADULT), 3);
    prose(s, rect(left, r.y + 27, r.right() - left - right, r.height - 30),
          pet::collectionComplete(*m.profile) ? tr(STR_PET_GROWTH_SUPPLIES_SHORT) : tr(STR_PET_GROWTH_ADULT_HELP), 2);
    if (right > 8) icon(s, rect(r.right() - 50, r.y + 12, 34, 34), extra_trophy_64);
    return;
  }
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GROWTH_NEXT), stageName(pet::Stage(unsigned(age) + 1)));
  text(s, rect(left, r.y + 1, r.right() - left - 10, 23), w.label, 3);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GROWTH_LEFT),
                static_cast<unsigned long long>((pet::nextStageSeconds(age) - m.profile->readingSeconds + 59) / 60));
  text(s, rect(left, r.y + 23, r.right() - left - 10, 22), w.label, 3);
  dashboard::bar(s, rect(left, r.bottom() - 9, r.right() - left - 10, 7),
                 m.profile->readingSeconds - pet::stageStart(age), pet::nextStageSeconds(age) - pet::stageStart(age));
}
inline bool dailyAvailable(const Model& m) { return m.today > 0 && m.clockKnown && !m.historyChanged && m.writable; }
inline void readingBar(toybox::Screen& s, fui::Rect r, uint64_t value, uint64_t target, bool inverse) {
  const auto bar = rect(r.x + 10, r.bottom() - 16, r.width - 20, 8);
  s.target().stroke(bar, inverse ? paper : ink, 2);
  s.target().fill(
      rect(bar.x + 2, bar.y + 2, target ? int(std::min(value, target) * std::max(0, int(bar.width) - 4) / target) : 0,
           std::max(0, int(bar.height) - 4)),
      inverse ? paper : ink);
}
inline void daily(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool available = dailyAvailable(m);
  const bool inverse = button(s, w, r, "", OpenReadingInfo, 0, false, nullptr, available);
  s.target().stroke(r, inverse ? paper : ink, 3);
  icon(s, rect(r.x + 9, r.y + 9, 21, 21), extra_reading_book_32, inverse);
  text(s, rect(r.x + 36, r.y + 4, r.width - 46, 26), tr(STR_PET_DAILY_CARD), 3, inverse);
  if (available)
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_DAILY_FRACTION),
                  std::min(unsigned(std::max(0, m.todaySeconds) / 60), unsigned(pet::GOALS[p.goalIndex])),
                  unsigned(pet::GOALS[p.goalIndex]));
  else
    std::snprintf(w.label, sizeof(w.label), "-");
  const bool compact = r.height < 120;
  text(s, rect(r.x + 10, r.y + (compact ? 30 : 50), r.width - 20, compact ? 30 : 34), w.label, compact ? 0 : 1, inverse,
       fui::TextAlign::Center);
  duration(w.secondary, sizeof(w.secondary), uint64_t(std::max(0, m.todaySeconds)));
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_DAILY_TOTAL), w.secondary);
  prose(s, rect(r.x + 10, r.bottom() - (compact ? 39 : 45), r.width - 20, compact ? 21 : 28),
        available ? w.label : tr(STR_PET_DAILY_WAIT), 1, fui::TextAlign::Left, inverse);
  const uint64_t seconds = available ? uint64_t(std::max(0, m.todaySeconds)) : 0;
  const uint64_t goal = uint64_t(pet::GOALS[p.goalIndex]) * 60;
  readingBar(s, r, seconds, goal, inverse);
}
inline void collection(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool inverse = button(s, w, r, "", OpenCollection);
  const auto drawTrophy = [&](fui::Rect area) {
    if (!m.collectionArt.draw || !m.collectionArt.draw(m.collectionArt.context, s.target(), area, 0, inverse))
      icon(s, area, extra_trophy_64, inverse);
  };
  if (r.height < 150) {
    s.target().stroke(r, inverse ? paper : ink, 3);
    text(s, rect(r.x + 8, r.y + 3, r.width - 16, 24), tr(STR_PET_COLLECTION_TITLE), 3, inverse, fui::TextAlign::Center);
    drawTrophy(rect(r.x + 8, r.y + 34, 88, std::min(70, int(r.height) - 63)));
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_COLLECTION_COUNT), pet::ownedCount(*m.profile),
                  pet::OBJECT_COUNT);
    const auto number = rect(r.x + 100, r.y + 43, r.width - 110, 30);
    text(s, number, w.label, s.target().measureText(0, w.label, {}).width <= number.width ? 0 : 3, inverse,
         fui::TextAlign::Center);
    text(s, rect(r.x + 8, r.bottom() - 28, r.width - 16, 22), tr(STR_PET_COLLECTION_OPEN), 3, inverse,
         fui::TextAlign::Center);
    return;
  }
  const int side = std::min(170, int(r.height) - 12);
  const auto picture = rect(r.x + 6, r.y + (r.height - side) / 2, side, side);
  drawTrophy(picture);
  const auto banner =
      rect(picture.right() + 10, r.y + std::max(18, (int(r.height) - 126) / 2), r.right() - picture.right() - 24, 40);
  s.target().stroke(banner, inverse ? paper : ink, 2);
  text(s, banner.inset({3, 5, 3, 5}), tr(STR_PET_COLLECTION_TITLE), 0, inverse, fui::TextAlign::Center);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_COLLECTION_COUNT), pet::ownedCount(*m.profile), pet::OBJECT_COUNT);
  text(s, rect(banner.x, banner.bottom() + 10, banner.width, 40), w.label, 1, inverse, fui::TextAlign::Center);
  text(s, rect(banner.x, r.bottom() - 42, banner.width, 27), tr(STR_PET_COLLECTION_OPEN), 3, inverse,
       fui::TextAlign::Center);
}
inline void objective(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool egg = pet::stage(p.readingSeconds) == pet::Stage::Egg;
  if (!egg && pet::collectionComplete(p)) {
    collection(s, m, w, r);
    return;
  }
  const bool ready = !egg && pet::owns(p, p.project);
  const auto progress = projectProgress(p);
  if (r.height < 165) {
    const bool compact = r.height < 120;
    const bool inverse = button(s, w, r, "", egg ? OpenReadingInfo : OpenProjects, 4);
    s.target().stroke(r, inverse ? paper : ink, 3);
    pixelui::copyCaps(w.label, sizeof(w.label), egg ? tr(STR_PET_HATCH_PROGRESS) : tr(STR_PET_NEXT_GOAL));
    fui::TextStyle heading;
    heading.font = 3;
    heading.maxLines = 1;
    heading.align = fui::TextAlign::Center;
    heading.color = inverse ? fui::Color::White : fui::Color::Black;
    s.target().text(rect(r.x + 9, r.y + 4, r.width - 18, compact ? 21 : 30), w.label, heading);
    const char* title = egg ? tr(STR_PET_HATCH_GOAL) : objectName(p.project);
    const bool longName = s.target().measureText(3, title, {}).width > r.width - 28;
    const auto image = compact    ? rect(r.x + 8, r.y + 31, 28, 28)
                       : longName ? rect(r.x + 10, r.y + 45, 28, 28)
                                  : rect(r.x + 10, r.y + 36, 42, 42);
    if (egg)
      art(s, m, image, unsigned(p.species), speciesIcon(p.species), false, inverse);
    else
      object(s, m, image, p.project, inverse);
    prose(s,
          compact    ? rect(r.x + 44, r.y + 25, r.width - 54, 42)
          : longName ? rect(r.x + 45, r.y + 34, r.width - 55, 46)
                     : rect(r.x + 60, r.y + 34, r.width - 70, 46),
          title, 2, fui::TextAlign::Left, inverse);
    const uint64_t value = egg ? p.readingSeconds : progress.value;
    const uint64_t target = egg ? pet::nextStageSeconds(pet::Stage::Egg) : progress.target;
    if (ready)
      std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_READY_SHORT));
    else
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_PROJECT_FRACTION),
                    static_cast<unsigned long long>(std::min(value, target) / 60),
                    static_cast<unsigned long long>(target / 60));
    text(s, rect(r.x + 10, r.bottom() - (compact ? 39 : 45), r.width - 20, compact ? 21 : 28), w.label, compact ? 3 : 0,
         inverse, fui::TextAlign::Center);
    readingBar(s, r, value, target, inverse);
    return;
  }
  const bool strip = r.width > r.height * 2;
  frame(s, r);
  text(s, rect(r.x + 10, r.y + 5, r.width - 20, 27), egg ? tr(STR_PET_HATCH_PROGRESS) : tr(STR_PET_NEXT_GOAL),
       strip ? 1 : 0);
  const auto picture = rect(r.x + 10, r.y + 39, 36, 36);
  if (egg)
    art(s, m, picture, unsigned(p.species), speciesIcon(p.species), false);
  else
    object(s, m, picture, p.project);
  if (ready)
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_READY_SHORT));
  else
    std::snprintf(
        w.label, sizeof(w.label), tr(STR_PET_MINUTES),
        static_cast<unsigned long long>(
            ((egg ? pet::nextStageSeconds(pet::Stage::Egg) - p.readingSeconds : pet::projectRemaining(p)) + 59) / 60));
  const auto number = strip ? rect(r.right() - 125, r.y + 38, 115, 36) : rect(r.x + 54, r.y + 39, r.width - 64, 36);
  text(s, number, w.label, 1, false, fui::TextAlign::Center);
  const auto name = strip ? rect(r.x + 58, r.y + 38, r.width - 193, r.height - 58)
                          : rect(r.x + 10, r.y + 79, r.width - 20, r.height - 102);
  prose(s, name, egg ? tr(STR_PET_HATCH_GOAL) : objectName(p.project), 4, fui::TextAlign::Center);
  dashboard::bar(s, rect(r.x + 10, r.bottom() - 17, r.width - 20, 8), egg ? p.readingSeconds : progress.value,
                 egg ? pet::nextStageSeconds(pet::Stage::Egg) : progress.target);
}
}  // namespace widget
}  // namespace petui

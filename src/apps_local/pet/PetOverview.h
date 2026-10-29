#pragma once

#include "PetReadingStatus.h"

namespace petui {
namespace widget {
inline void growth(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, bool expanded = false) {
  const auto age = pet::stage(m.profile->readingSeconds);
  frame(s, r);
  const int mark = r.height < 58 ? 28 : 38;
  if (expanded) {
    const int x = r.x + 10 + mark / 2, y = r.y + (r.height - 28) / 2;
    s.target().line({int16_t(x), int16_t(y + 28)}, {int16_t(x), int16_t(y)}, 3, ink);
    s.target().line({int16_t(x - 10), int16_t(y + 10)}, {int16_t(x), int16_t(y)}, 3, ink);
    s.target().line({int16_t(x), int16_t(y)}, {int16_t(x + 10), int16_t(y + 10)}, 3, ink);
  } else
    sprig(s, rect(r.x + 10, r.y + (r.height - mark) / 2, mark, mark), m.profile->species);
  const int left = r.x + mark + 22;
  const int right = r.width > 400 ? 62 : 8;
  if (age == pet::Stage::Adult) {
    text(s, rect(left, r.y + 3, r.right() - left - right, expanded ? 28 : 24), tr(STR_PET_GROWTH_ADULT),
         expanded ? 0 : 3, false, fui::TextAlign::Left, expanded);
    prose(s, rect(left, r.y + 27, r.right() - left - right, r.height - 30),
          pet::collectionComplete(*m.profile) ? tr(STR_PET_GROWTH_SUPPLIES_SHORT) : tr(STR_PET_GROWTH_ADULT_HELP), 2);
    if (right > 8) icon(s, rect(r.right() - 50, r.y + 12, 34, 34), extra_trophy_64);
    return;
  }
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GROWTH_NEXT), stageName(pet::Stage(unsigned(age) + 1)));
  if (expanded)
    text(s, rect(left, r.y + 3, r.right() - left - 10, 28), w.label, 0, false, fui::TextAlign::Left, true);
  else
    text(s, rect(left, r.y + 1, r.right() - left - 10, 23), w.label, 3);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GROWTH_LEFT),
                static_cast<unsigned long long>((pet::nextStageSeconds(age) - m.profile->readingSeconds + 59) / 60));
  if (expanded)
    text(s, rect(left, r.y + 31 + (r.height - 15 - 31 - 28) / 2, r.right() - left - 10, 28), w.label, 0);
  else
    text(s, rect(left, r.y + 23, r.right() - left - 10, 22), w.label, 3);
  dashboard::bar(s, rect(left, r.bottom() - (expanded ? 15 : 9), r.right() - left - 10, 7),
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
  s.target().stroke(r, inverse ? paper : ink, pixelui::kBorder);
  icon(s, rect(r.x + 9, r.y + 9, 21, 21), extra_reading_book_32, inverse);
  text(s, rect(r.x + 36, r.y + 4, r.width - 46, 26), tr(STR_PET_DAILY_CARD), 0, inverse);
  if (available)
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_DAILY_FRACTION),
                  std::min(unsigned(std::max(0, m.todaySeconds) / 60), unsigned(pet::GOALS[p.goalIndex])),
                  unsigned(pet::GOALS[p.goalIndex]));
  else
    std::snprintf(w.label, sizeof(w.label), "-");
  const bool compact = r.height <= 140;
  text(s, rect(r.x + 10, r.y + (compact ? 30 : 50), r.width - 20, compact ? 30 : 34), w.label, compact ? 0 : 1, inverse,
       fui::TextAlign::Center);
  duration(w.secondary, sizeof(w.secondary), uint64_t(std::max(0, m.todaySeconds)));
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_DAILY_TOTAL), w.secondary);
  prose(s, rect(r.x + 10, r.bottom() - (compact ? 70 : 45), r.width - 20, compact ? 48 : 28),
        available ? w.label : tr(STR_PET_DAILY_WAIT), compact ? 2 : 1, fui::TextAlign::Left, inverse);
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
  if (r.height < 150 || r.width < 300) {
    s.target().stroke(r, inverse ? paper : ink, pixelui::kBorder);
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
  const bool inverse = button(s, w, r, "", egg ? OpenReadingInfo : OpenProjects, 4);
  s.target().stroke(r, inverse ? paper : ink, pixelui::kBorder);
  constexpr int iconLimit = 80;
  const int side = std::max(20, std::min({iconLimit, int(r.width) * 40 / 100, int(r.height) - 40}));
  const auto content = rect(r.x + 12, r.y + 9, r.width - 24, r.height - 34);
  const auto image = rect(content.x, content.y + (content.height - side) / 2, side, side);
  const int copyX = image.right() + 12, copyW = r.right() - copyX - 12;
  cardCopy(s, rect(copyX, content.y, copyW, content.height - 30), egg ? tr(STR_PET_HATCH_GOAL) : objectName(p.project),
           inverse, 3, fui::TextAlign::Left);
  if (egg)
    art(s, m, image, unsigned(p.species), speciesIcon(p.species), false, inverse);
  else
    object(s, m, image, p.project, inverse);
  const uint64_t value = egg ? p.readingSeconds : progress.value;
  const uint64_t target = egg ? pet::nextStageSeconds(pet::Stage::Egg) : progress.target;
  if (ready)
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_READY_SHORT));
  else
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_PROJECT_FRACTION), static_cast<unsigned long long>(value / 60),
                  static_cast<unsigned long long>(target / 60));
  text(s, rect(copyX, content.bottom() - 27, copyW, 27), w.label, 0, inverse, fui::TextAlign::Left);
  readingBar(s, r, value, target, inverse);
}
}  // namespace widget
}  // namespace petui

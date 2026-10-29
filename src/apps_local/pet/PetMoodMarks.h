#pragma once

#include <FreeInkUI.h>

#include <algorithm>

#include "PetNeeds.h"
#include "PetState.h"

namespace petmood {
namespace fui = freeink::ui;
enum class Mood : uint8_t { None, Happy, Sad, Hungry, Tired, Sick };
inline fui::Rect portraitBounds(fui::Rect area) {
  const int side = std::min(
      {int(area.width), int(area.height), std::clamp(std::min(int(area.width) / 5, int(area.height) / 4), 36, 64)});
  return fui::makeRect(area.right() - side, area.y, side, side);
}
inline int overlap(fui::Rect a, fui::Rect b) {
  return std::max(0, std::min(int(a.right()), int(b.right())) - std::max(int(a.x), int(b.x))) *
         std::max(0, std::min(int(a.bottom()), int(b.bottom())) - std::max(int(a.y), int(b.y)));
}
inline fui::Rect roomBounds(fui::Rect area, fui::Rect subject, const fui::Rect* obstacles, unsigned count) {
  const int side = std::min(
      {int(area.width), int(area.height), std::clamp(std::min(int(area.width) / 10, int(area.height) / 5), 28, 42)});
  fui::Rect best{};
  int bestScore = INT32_MAX;
  for (unsigned candidate = 0; candidate < 4; ++candidate) {
    const int x = candidate == 0   ? subject.right() - side
                  : candidate == 1 ? subject.right() + 2
                  : candidate == 2 ? subject.x + (subject.width - side) / 2
                                   : subject.x - side - 2;
    const int y = candidate == 0 || candidate == 2 ? subject.y - side - 2 : subject.y - side / 3;
    const auto box = fui::makeRect(std::clamp(x, int(area.x), int(area.right()) - side),
                                   std::clamp(y, int(area.y), int(area.bottom()) - side), side, side);
    int score = overlap(box, subject) * 8;
    for (unsigned i = 0; i < count; ++i) score += overlap(box, obstacles[i]) * 2;
    if (score < bestScore) {
      best = box;
      bestScore = score;
    }
  }
  return best;
}
inline Mood select(const pet::Profile& p) {
  if (!p.created || p.lifeStatus != pet::LifeStatus::Active || pet::stage(p.readingSeconds) == pet::Stage::Egg)
    return Mood::None;
  switch (pet::need(p)) {
    case pet::Need::Sick:
      return Mood::Sick;
    case pet::Need::Sleeping:
      return Mood::None;
    case pet::Need::Hungry:
      return Mood::Hungry;
    case pet::Need::Tired:
      return Mood::Tired;
    case pet::Need::Company:
      return Mood::Sad;
    case pet::Need::Dirty:
    case pet::Need::None:
      return p.joy >= 60 ? Mood::Happy : Mood::Sad;
  }
  return Mood::None;
}
inline void draw(fui::DrawTarget& target, fui::Rect area, Mood mood, unsigned step) {
  if (mood == Mood::None || area.width < 16 || area.height < 16) return;
  const int maxSide = std::min(int(area.width), int(area.height));
  const int side = step % 2 ? maxSide * 4 / 5 : maxSide;
  const auto box = fui::makeRect(area.x + (area.width - side) / 2, area.y + (area.height - side) / 2, side, side);
  const auto ink = fui::Paint::solid(fui::Color::Black), paper = fui::Paint::solid(fui::Color::White);
  const auto rect = [&](int x, int y, int w, int h) {
    return fui::makeRect(box.x + x * side / 32, box.y + y * side / 32, (x + w) * side / 32 - x * side / 32,
                         (y + h) * side / 32 - y * side / 32);
  };
  const auto fill = [&](int x, int y, int w, int h, fui::Paint paint = fui::Paint::solid(fui::Color::Black)) {
    const auto part = rect(x, y, w, h);
    if (part.width > 0 && part.height > 0) target.fill(part, paint);
  };
  // Integer spans keep the stepped silhouette crisp at both icon sizes.
  for (int y = 0; y < 32; ++y) {
    const int dy = y * 2 - 31;
    for (const bool inner : {false, true}) {
      const int radius = inner ? 27 : 31;
      int left = 0;
      while (left < 16 && (left * 2 - 31) * (left * 2 - 31) + dy * dy > radius * radius) ++left;
      if (left < 16) fill(left, y, 32 - left * 2, 1, inner ? paper : ink);
    }
  }
  if (mood == Mood::Tired) {
    fill(7, 12, 6, 2);
    fill(20, 12, 6, 2);
  } else if (mood == Mood::Sick) {
    fill(7, 13, 3, 2);
    fill(10, 12, 3, 2);
    fill(12, 11, 2, 2);
    fill(18, 11, 2, 2);
    fill(20, 12, 3, 2);
    fill(23, 13, 3, 2);
    fill(21, 5, 1, 3);
    fill(23, 6, 1, 3);
    fill(25, 7, 1, 3);
  } else {
    fill(9, 10, 2, 5);
    fill(21, 10, 2, 5);
  }
  if (mood != Mood::Tired) {
    for (const int x : {6, 24}) {
      fill(x, 17, 1, 1);
      fill(x + 1, 18, 1, 1);
      fill(x + 2, 17, 1, 1);
    }
  }
  switch (mood) {
    case Mood::Happy:
      fill(11, 18, 11, 3);
      fill(12, 21, 9, 2);
      fill(13, 23, 7, 1);
      fill(14, 24, 5, 1);
      fill(15, 25, 3, 1);
      fill(13, 20, 7, 2, paper);
      fill(14, 22, 5, 1, paper);
      fill(15, 23, 3, 1, paper);
      break;
    case Mood::Sad:
      fill(14, 19, 5, 2);
      fill(12, 20, 9, 2);
      fill(11, 21, 3, 2);
      fill(19, 21, 3, 2);
      fill(10, 22, 3, 2);
      fill(20, 22, 3, 2);
      break;
    case Mood::Hungry:
      fill(11, 19, 12, 3);
      fill(12, 22, 10, 2);
      fill(14, 24, 7, 1);
      fill(18, 22, 3, 7);
      fill(13, 22, 7, 1, paper);
      fill(17, 23, 3, 1, paper);
      fill(19, 24, 1, 4, paper);
      break;
    case Mood::Tired:
      fill(14, 21, 6, 2);
      break;
    case Mood::Sick:
      fill(11, 22, 3, 2);
      fill(13, 21, 3, 2);
      fill(15, 22, 4, 2);
      fill(18, 21, 3, 2);
      fill(20, 22, 2, 2);
      break;
    case Mood::None:
      break;
  }
}
}  // namespace petmood

#pragma once

#include <algorithm>

#include "../ui/Dashboard.h"
#include "../ui/ToyboxIcons.h"
#include "PetFarewell.h"
#include "PetHabitatData.h"
#include "PetMoodMarks.h"
#include "PetPortrait.h"
#include "PetScreens.h"
#include "PetSleepAnimation.h"

namespace petui {
static_assert(sizeof(OBJECT_ART) / sizeof(OBJECT_ART[0]) == pet::OBJECT_COUNT);
static_assert(sizeof(PET_ART) / sizeof(PET_ART[0]) == HOME_ART_COUNT);
namespace habitatlayout {
inline constexpr int PET_WIDTHS[] = {66, 84, 104, 124, 140}, PET_HEIGHTS[] = {86, 94, 116, 137, 150};
inline fui::Rect rect(int x, int y, int w, int h) { return fui::makeRect(x, y, std::max(1, w), std::max(1, h)); }
inline fui::Rect scene(fui::Rect card, int header = 47) {
  const int width = std::min(int(card.width) - 14, (int(card.height) - header - 7) * 2) & ~1;
  return rect(card.x + (card.width - width) / 2, card.y + header + (card.height - header - 7 - width / 2) / 2, width,
              width / 2);
}
inline fui::Rect scale(fui::Rect scene, fui::Rect box) {
  const int x = scene.x + box.x * scene.width / 432, y = scene.y + box.y * scene.height / 216;
  return rect(x, y, (box.right() * scene.width / 432) + scene.x - x, (box.bottom() * scene.height / 216) + scene.y - y);
}
inline fui::Rect item(unsigned id) {
  const auto& a = OBJECT_ART[id];
  int x = 0, y = 0;
  switch (a.anchor) {
    case 0:
      x = 108 - a.width / 2;
      y = 190 - a.height;
      break;
    case 1:
      x = 389 - a.width / 2;
      y = 190 - a.height;
      break;
    case 2:
      x = 96 - a.width / 2;
      y = 54 - a.height / 2;
      break;
    case 3:
      x = 96 - a.width / 2;
      y = 4;
      break;
    case 4:
      x = 303 - a.width / 2;
      y = 202 - a.height;
      break;
    case 5:
      x = 43 - a.width / 2;
      y = 205 - a.height;
      break;
    case 6:
      x = 235 - a.width / 2;
      y = 24 - a.height / 2;
      break;
  }
  return rect(x, y, a.width, a.height);
}
inline bool sleeping(const pet::Profile& p) {
  return pet::stage(p.readingSeconds) != pet::Stage::Egg && p.illness == pet::Illness::Healthy &&
         (p.resting || p.lifeStatus == pet::LifeStatus::Retired) && p.lifeStatus != pet::LifeStatus::Lost;
}
inline bool peeking(const pet::Profile& p) {
  const unsigned rest = p.equipped[0];
  return sleeping(p) && rest < pet::OBJECT_COUNT && pet::objectSlot(rest) == 0 &&
         OBJECT_ART[rest].restPose == RestPose::Head;
}
inline fui::Rect pet(const pet::Profile& p) {
  const unsigned age = unsigned(pet::stage(p.readingSeconds));
  int maxW = PET_WIDTHS[age], maxH = PET_HEIGHTS[age], x = 233, base = 190;
  if (sleeping(p)) {
    maxW = PET_WIDTHS[age];
    maxH = 96;
    if (p.equipped[0] < pet::OBJECT_COUNT && pet::objectSlot(p.equipped[0]) == 0) {
      const auto& a = OBJECT_ART[p.equipped[0]];
      const auto bed = item(p.equipped[0]);
      x = bed.x + bed.width * a.sleepX / 100;
      base = bed.y + bed.height * a.sleepY / 100;
      const int ageScale = 76 + int(age) * 6;
      maxW = std::max(20, std::min(132, bed.width * a.sleepWidth / 100) * ageScale / 100);
      if (a.restPose == RestPose::Head) maxH = std::max(20, bed.height * a.openingHeight / 100) * ageScale / 100;
    }
  }
  const auto& sprite = PET_ART[pet::homeScene(p)];
  const int sourceW = peeking(p) ? sprite.width * petsleep::headWidth(p.species) / 100 : sprite.width;
  const int sourceH = peeking(p) ? sprite.height * petsleep::HEAD_HEIGHT / 100 : sprite.height;
  const int width = std::min(maxW, maxH * sourceW / sourceH);
  const int height = std::max(1, width * sourceH / sourceW);
  return rect(x - width / 2, base - height, width, height);
}
inline fui::Rect memoryPet(pet::Stage age, unsigned scene) {
  const auto& source = PET_ART[scene];
  const int width = std::min(PET_WIDTHS[unsigned(age)], PET_HEIGHTS[unsigned(age)] * source.width / source.height);
  const int height = std::max(1, width * source.height / source.width);
  return rect(233 - width / 2, 190 - height, width, height);
}
}  // namespace habitatlayout

inline bool habitatPair(const Model& m, fui::DrawTarget& target, fui::Rect area, unsigned mask) {
  if (!m.habitat.draw || !m.habitat.draw(m.habitat.context, target, area, mask, true)) return false;
  return m.habitat.draw(m.habitat.context, target, area, mask + 1, false);
}
inline bool breathingPose(const Model& m, fui::DrawTarget& target, fui::Rect area) {
  const auto& p = *m.profile;
  if (!m.sleepArt.draw) return false;
  if (pet::stage(p.readingSeconds) == pet::Stage::Egg) {
    if (!petsleep::breathing(p, m.view)) return false;
    const auto mask = petsleep::eggIndex(unsigned(p.species), m.sleepStep);
    if (!m.sleepArt.draw(m.sleepArt.context, target, area, mask, true)) return false;
    return m.sleepArt.draw(m.sleepArt.context, target, area, mask + 1, false);
  }
  const bool sleeping = habitatlayout::sleeping(p);
  if (!sleeping && !petsleep::breathing(p, m.view)) return false;
  const unsigned step = 0;
  const unsigned mask =
      sleeping && habitatlayout::peeking(p) ? petsleep::headIndex(p) : petsleep::fullBodyIndex(p, step);
  if (!m.sleepArt.draw(m.sleepArt.context, target, area, mask, true)) return false;
  return m.sleepArt.draw(m.sleepArt.context, target, area, mask + 1, false);
}
inline bool staticSleepHead(const Model& m, fui::DrawTarget& target, fui::Rect area) {
  if (!habitatlayout::peeking(*m.profile)) return false;
  const auto previous = target.clipRect();
  const int left = std::max(int(area.x), int(previous.x)), top = std::max(int(area.y), int(previous.y));
  const int right = std::min(int(area.right()), int(previous.right()));
  const int bottom = std::min(int(area.bottom()), int(previous.bottom()));
  if (right <= left || bottom <= top || !target.setClipRect(habitatlayout::rect(left, top, right - left, bottom - top)))
    return false;
  const auto full = habitatlayout::rect(area.x, area.y, area.width * 100 / petsleep::headWidth(m.profile->species),
                                        area.height * 100 / petsleep::HEAD_HEIGHT);
  const bool drawn =
      habitatPair(m, target, full, 482 + pet::homeScene(*m.profile) * 2) ||
      (m.scenes.draw && m.scenes.draw(m.scenes.context, target, full, pet::homeScene(*m.profile), false));
  target.setClipRect(previous);
  return drawn;
}
inline void sleepMarks(toybox::Screen& screen, const Model& m, fui::Rect area) {
  if (!petsleep::enabled(*m.profile, m.view)) return;
  const auto pet = habitatlayout::pet(*m.profile);
  const unsigned step = 0;
  static constexpr uint8_t count[] = {1, 2, 3, 2};
  int x = std::max(8, std::min(382, int(pet.x + pet.width / 4)));
  int y = std::max(36, int(pet.y) - 4);
  const unsigned rest = m.profile->equipped[0];
  if (rest < pet::OBJECT_COUNT && pet::objectSlot(rest) == 0) {
    const auto bed = habitatlayout::item(rest);
    y = std::max(36, std::min(y, int(bed.y) - 4));
    for (const unsigned slot : {1u, 3u}) {
      const unsigned id = m.profile->equipped[slot];
      if (id >= pet::OBJECT_COUNT || pet::objectSlot(id) != slot) continue;
      const auto item = habitatlayout::item(id);
      if (x < item.right() && x + 40 > item.x && y - 36 < item.bottom() && y > item.y)
        x = std::max(8, std::min(382, int(bed.right()) + 6));
    }
  }
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const auto paper = fui::Paint::solid(fui::Color::White);
  const int weight = std::max(1, int(area.width) / 216);
  for (unsigned i = 0; i < count[step]; ++i) {
    const int size = 6 + int(i) * 2;
    const auto z = habitatlayout::scale(
        area, habitatlayout::rect(x + int(i) * 13, y - int(i) * 11 - int(step) * 2 - size, size, size));
    for (const bool outline : {true, false}) {
      const int stroke = outline ? weight + 4 : weight;
      const auto paint = outline ? paper : ink;
      screen.target().line({z.x, z.y}, {z.right(), z.y}, stroke, paint);
      screen.target().line({z.right(), z.y}, {z.x, z.bottom()}, stroke, paint);
      screen.target().line({z.x, z.bottom()}, {z.right(), z.bottom()}, stroke, paint);
    }
  }
}
inline void habitat(toybox::Screen& screen, const Model& m, fui::Rect card, int header = 47, bool inverse = false) {
  using namespace habitatlayout;
  auto& target = screen.target();
  const auto& p = *m.profile;
  const auto area = scene(card, header);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  target.fill(area, fui::Paint::solid(fui::Color::White));
  const bool background =
      m.habitat.draw &&
      m.habitat.draw(m.habitat.context, target, rect(area.x, area.y, area.width / 2, area.height), 0, false) &&
      m.habitat.draw(m.habitat.context, target, rect(area.x + area.width / 2, area.y, area.width / 2, area.height), 1,
                     false);
  if (!background) {
    const auto floor = scale(area, rect(14, 135, 404, 1));
    target.line({floor.x, floor.y}, {floor.right(), floor.y}, 1, ink);
    target.stroke(scale(area, rect(350, 18, 62, 54)), ink, 2);
  }
  const auto drawItem = [&](unsigned slot) {
    const unsigned id = p.equipped[slot];
    if (id >= pet::OBJECT_COUNT || pet::objectSlot(id) != slot) return;
    const auto box = scale(area, item(id));
    if (!habitatPair(m, target, box, 2 + id * 2) && id < 12 && m.art.draw)
      m.art.draw(m.art.context, target, box, 51 + id, false);
  };
  drawItem(1);
  drawItem(3);
  drawItem(0);
  if (p.lifeStatus == pet::LifeStatus::Lost) {
    farewellPicture(screen, m, scale(area, rect(179, 28, 108, 150)), inverse);
    drawItem(2);
    return;
  }
  const auto picture = scale(area, habitatlayout::pet(p));
  if (!breathingPose(m, target, picture) && !staticSleepHead(m, target, picture) && !fullCreature(m, target, picture)) {
    if (!(m.scenes.draw && m.scenes.draw(m.scenes.context, target, picture, pet::homeScene(p), false))) {
      const unsigned age = unsigned(pet::stage(p.readingSeconds)), species = unsigned(p.species);
      const unsigned fallback = age ? 3 + species * 16 + (age - 1) * 4 : species;
      if (!(m.art.draw && m.art.draw(m.art.context, target, picture, fallback, false))) {
        static constexpr const freeink::Icon* icons[] = {&icon_cat_nature_32, &extra_moon_64, &extra_reading_flame_64};
        dashboard::icon(screen, picture, *icons[species]);
      }
    }
  }
  if (sleeping(p) && p.equipped[0] < pet::OBJECT_COUNT && pet::objectSlot(p.equipped[0]) == 0 && background) {
    const unsigned id = p.equipped[0];
    const auto bed = item(id);
    const auto strip = scale(area, rect(bed.x, bed.y + bed.height * OBJECT_ART[id].frontY / 100, bed.width,
                                        bed.height - bed.height * OBJECT_ART[id].frontY / 100));
    const auto previous = target.clipRect();
    const int left = std::max(int(strip.x), int(previous.x)), top = std::max(int(strip.y), int(previous.y));
    const int right = std::min(int(strip.right()), int(previous.right()));
    const int bottom = std::min(int(strip.bottom()), int(previous.bottom()));
    if (right > left && bottom > top && target.setClipRect(rect(left, top, right - left, bottom - top))) {
      drawItem(0);
      target.setClipRect(previous);
    }
  }
  drawItem(2);
  sleepMarks(screen, m, area);
  if (petsleep::moodVisible(p, m.view)) {
    fui::Rect obstacles[5] = {scale(area, rect(350, 18, 62, 54))};
    unsigned count = 1;
    for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
      const unsigned id = p.equipped[slot];
      if (id < pet::OBJECT_COUNT && pet::objectSlot(id) == slot) obstacles[count++] = scale(area, item(id));
    }
    petmood::draw(target, petmood::roomBounds(area, picture, obstacles, count), petmood::select(p), m.sleepStep);
  }
}
}  // namespace petui

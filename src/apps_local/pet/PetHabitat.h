#pragma once

#include <algorithm>

#include "../ui/Dashboard.h"
#include "../ui/ToyboxIcons.h"
#include "PetCompanionData.h"
#include "PetDecorationInk.h"
#include "PetFarewell.h"
#include "PetHabitatData.h"
#include "PetMoodMarks.h"
#include "PetPortrait.h"
#include "PetRestData.h"
#include "PetRoomEffects.h"
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
// Numeric values match the generated atlas; all placements use its 432 x 216 room.
enum class Anchor : uint8_t {
  Rest,
  FloorDecoration,
  WallDecoration,
  CeilingDecoration,
  Toy,
  FloorEnvironment,
  WallEnvironment
};
inline Anchor anchor(unsigned id) { return static_cast<Anchor>(OBJECT_ART[id].anchor); }
inline bool mounted(unsigned id) {
  return anchor(id) == Anchor::WallDecoration || anchor(id) == Anchor::CeilingDecoration ||
         anchor(id) == Anchor::WallEnvironment;
}
inline bool sharedWall(unsigned decoration, unsigned environment) {
  return pet::objectAvailable(decoration) && pet::objectSlot(decoration) == 1 && mounted(decoration) &&
         pet::objectAvailable(environment) && pet::objectSlot(environment) == 3 &&
         anchor(environment) == Anchor::WallEnvironment;
}
inline bool sharedWall(const pet::Profile& p) { return sharedWall(p.equipped[1], p.equipped[3]); }
inline fui::Rect item(unsigned id, bool shareWall = false) {
  const auto& a = OBJECT_ART[id];
  int x = 0, y = 0;
  switch (anchor(id)) {
    case Anchor::Rest:
      x = 108 - a.width / 2;
      y = 190 - a.height;
      break;
    case Anchor::FloorDecoration:
      x = 389 - a.width / 2;
      y = 190 - a.height;
      break;
    case Anchor::WallDecoration:
      x = 96 - a.width / 2;
      y = 54 - a.height / 2;
      break;
    case Anchor::CeilingDecoration:
      x = 96 - a.width / 2;
      y = 4;
      break;
    case Anchor::Toy:
      x = std::max(303 - a.width / 2, 233 + PET_WIDTHS[unsigned(pet::Stage::Adult)] / 2 + 8);
      y = 208 - a.height;
      break;
    case Anchor::FloorEnvironment:
      x = 43 - a.width / 2;
      y = 205 - a.height;
      break;
    case Anchor::WallEnvironment:
      x = 84 - a.width / 2;
      y = 4;
      break;
  }
  if (shareWall && mounted(id)) {
    // Two wall ornaments share the space above the bed, leaving the pet and window clear.
    const int width = std::min(64, int(a.width)), height = std::max(1, a.height * width / a.width);
    const int center = anchor(id) == Anchor::WallEnvironment ? 52 : 128;
    return rect(center - width / 2, 4, width, height);
  }
  return rect(x, y, a.width, a.height);
}
inline constexpr int REST_TOP = [] {
  int height = 0;
  for (const auto& art : OBJECT_ART)
    if (art.anchor == 0) height = std::max(height, int(art.height));
  return 190 - height;
}();
inline fui::Rect placedItem(fui::Rect scene, unsigned id, int wallTop = -1, bool shareWall = false) {
  auto box = scale(scene, item(id, shareWall));
  if (mounted(id) && wallTop >= 0) {
    box.y = std::max(int(box.y), wallTop + std::max(6, int(scene.width) / 36));
    const int height = std::min(int(box.height), scene.y + REST_TOP * scene.height / 216 - box.y - 4);
    if (height <= 0) return {box.x, box.y, 0, 0};
    const int width = std::max(1, box.width * height / box.height);
    box.x += (box.width - width) / 2;
    box.width = width;
    box.height = height;
  }
  return box;
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
  if (mask >= 2 && mask < 2 + pet::OBJECT_COUNT * 2 && !pet::objectAvailable((mask - 2) / 2)) return false;
  if (!m.habitat.draw || !m.habitat.draw(m.habitat.context, target, area, mask, true)) return false;
  if (mask >= 2 && mask < 2 + pet::OBJECT_COUNT * 2) {
    DecorationInk ink(target);
    return m.habitat.draw(m.habitat.context, ink, area, mask + 1, false);
  }
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
inline void sleepMarks(toybox::Screen& screen, const Model& m, fui::Rect area, int wallTop = -1) {
  if (m.restPhase != petrest::Phase::Idle || !petsleep::enabled(*m.profile, m.view)) return;
  const auto pet = habitatlayout::pet(*m.profile);
  const unsigned step = m.sleepStep % 4;
  int x = std::max(8, std::min(368, int(pet.x + pet.width / 4)));
  int y = std::max(56, int(pet.y) - 4);
  const unsigned rest = m.profile->equipped[0];
  if (rest < pet::OBJECT_COUNT && pet::objectSlot(rest) == 0) {
    const auto bed = habitatlayout::item(rest);
    y = std::max(56, std::min(y, int(bed.y) - 4));
    for (const unsigned slot : {1u, 3u}) {
      const unsigned id = m.profile->equipped[slot];
      if (!pet::objectAvailable(id) || pet::objectSlot(id) != slot) continue;
      const auto item = habitatlayout::placedItem(area, id, wallTop, habitatlayout::sharedWall(*m.profile));
      const auto marks = habitatlayout::scale(area, habitatlayout::rect(x, y - 52, 56, 52));
      if (marks.x < item.right() && marks.right() > item.x && marks.y < item.bottom() && marks.bottom() > item.y)
        x = std::max(8, std::min(368, int(bed.right()) + 6));
    }
  }
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const auto paper = fui::Paint::solid(fui::Color::White);
  const int weight = std::max(1, int(area.width) / 216);
  for (unsigned i = 0; i < 3; ++i) {
    const int size = 8 + int(i) * 3;
    const int phase = int((step + i) % 4);
    const auto z = habitatlayout::scale(
        area, habitatlayout::rect(x + int(i) * 14 + phase * 2, y - int(i) * 12 - phase * 3 - size, size, size));
    for (const bool outline : {true, false}) {
      const int stroke = outline ? weight + 4 : weight;
      const auto paint = outline ? paper : ink;
      screen.target().line({z.x, z.y}, {z.right(), z.y}, stroke, paint);
      screen.target().line({z.right(), z.y}, {z.x, z.bottom()}, stroke, paint);
      screen.target().line({z.x, z.bottom()}, {z.right(), z.bottom()}, stroke, paint);
    }
  }
}
inline bool foodPicture(const Model& m, fui::DrawTarget& target, fui::Rect box, unsigned food, unsigned eaten) {
  if (food >= petfeed::choices(m.feedingArt) || eaten > petfeed::BITE_COUNT || !m.sleepArt.draw) return false;
  unsigned mask;
  switch (m.feedingArt) {
    case petfeed::Art::Extended:
    case petfeed::Art::Progressive:
      mask = petfeed::progressiveFoodIndex(food, eaten);
      break;
    case petfeed::Art::Simple:
      if (food >= 3 || eaten == petfeed::BITE_COUNT) return false;
      mask = petfeed::foodIndex(food, eaten != 0);
      break;
    case petfeed::Art::Legacy:
      return false;
  }
  return m.sleepArt.draw(m.sleepArt.context, target, box, mask, true) &&
         m.sleepArt.draw(m.sleepArt.context, target, box, mask + 1, false);
}
inline fui::Rect registeredActor(const Model& m, fui::Rect idle, const petreact::Registration& registration) {
  const auto& p = *m.profile;
  const unsigned index = unsigned(p.species) * 4 + unsigned(pet::stage(p.readingSeconds)) - 1;
  const unsigned pose = habitatlayout::sleeping(p) ? petsleep::BODY_POSES : unsigned(petsleep::bodyPose(p));
  static_assert(sizeof(petreact::BODIES) / sizeof(petreact::BODIES[0]) == 12 * (petsleep::BODY_POSES + 1));
  const auto& reference = petreact::REFERENCES[index];
  const auto& body = petreact::BODIES[index * (petsleep::BODY_POSES + 1) + pose];
  const bool peeking = habitatlayout::peeking(p);
  const int sourceWidth = peeking ? body.width * petsleep::headWidth(p.species) / 100 : body.width;
  const int sourceHeight = peeking ? body.height * petsleep::HEAD_HEIGHT / 100 : body.height;
  const int width = std::min(int(idle.width), int(idle.height) * sourceWidth / sourceHeight);
  const int height = width * sourceHeight / sourceWidth;
  const int x = idle.x + (idle.width - width) / 2;
  const int y = idle.y + (idle.height - height) / 2;
  const unsigned idleScale = std::min(unsigned(width) * 1024 / sourceWidth, unsigned(height) * 1024 / sourceHeight);
  const int scale = int(uint64_t(body.bodySizeQ8) * idleScale / reference.bodySizeQ8);
  const int footX = x + int(uint64_t(body.footXQ8) * width / (unsigned(sourceWidth) * 256));
  const int footY = y + body.footY * height / sourceHeight;
  const int referenceFootX = int(reference.footXQ8);
  const int left = footX + int(int64_t(registration.left * 256 - referenceFootX) * scale / (256 * 1024));
  const int top = footY + int(int64_t(registration.top - int(reference.footY)) * scale / 1024);
  const int right =
      footX + int(int64_t((registration.left + registration.width) * 256 - referenceFootX) * scale / (256 * 1024));
  const int bottom = footY + int(int64_t(registration.top + registration.height - int(reference.footY)) * scale / 1024);
  return habitatlayout::rect(left, top, right - left, bottom - top);
}
inline fui::Rect registeredReaction(const Model& m, fui::Rect idle) {
  const auto slot = petreact::poseSlot(m.reactionKind, m.profile->species, pet::stage(m.profile->readingSeconds));
  return slot < petreact::POSE_COUNT ? registeredActor(m, idle, petreact::REGISTRATIONS[slot]) : idle;
}
inline fui::Rect restPlacement(const pet::Profile& p, petrest::Phase phase, fui::Rect scene, fui::Rect actor) {
  const unsigned id = p.equipped[0];
  if (id >= pet::OBJECT_COUNT || pet::objectSlot(id) != 0) return actor;
  const auto bed = habitatlayout::item(id);
  const auto& art = OBJECT_ART[id];
  int x = 233 + (bed.x + bed.width * (int(art.sleepX) + 30) / 100 - 233) * int(petrest::progress(phase)) / 3;
  int base = 190;
  if (phase == petrest::Phase::SettleSecond || phase == petrest::Phase::Exit) {
    x = bed.x + bed.width * art.sleepX / 100;
    base = bed.y + bed.height * art.sleepY / 100;
  }
  actor.x += (x - 233) * scene.width / 432;
  actor.y += (base - 190) * scene.height / 216;
  return actor;
}
inline void habitatFigures(toybox::Screen& screen, const Model& m, fui::Rect area, bool background,
                           bool inverse = false, int wallTop = -1) {
  using namespace habitatlayout;
  auto& target = screen.target();
  const auto& p = *m.profile;
  const bool shareWall = sharedWall(p);
  const auto picture = scale(area, habitatlayout::pet(p));
  auto celebrationActor = picture;
  const auto age =
      m.view == pet::View::Evolution ? petreact::growthStage(p, m.reactionPhase) : pet::stage(p.readingSeconds);
  if (m.view == pet::View::Evolution) {
    const unsigned scene =
        age == pet::Stage::Egg ? unsigned(p.species) : 3 + unsigned(p.species) * 12 + (unsigned(age) - 1) * 3;
    celebrationActor = scale(area, memoryPet(age, scene));
  }
  if (p.lifeStatus == pet::LifeStatus::Active) roomeffects::behind(screen, m, celebrationActor);
  const auto drawItem = [&](unsigned slot) {
    const unsigned id = p.equipped[slot];
    if (!pet::objectAvailable(id) || pet::objectSlot(id) != slot) return;
    const auto box = placedItem(area, id, wallTop, shareWall);
    if (box.empty()) return;
    if (!habitatPair(m, target, box, 2 + id * 2) && id < 12 && m.art.draw) {
      DecorationInk ink(target);
      m.art.draw(m.art.context, ink, box, 51 + id, false);
    }
  };
  const unsigned environment = p.equipped[3];
  const bool groundEnvironment = pet::objectAvailable(environment) && pet::objectSlot(environment) == 3 &&
                                 anchor(environment) == Anchor::FloorEnvironment;
  drawItem(1);
  if (!groundEnvironment) drawItem(3);
  drawItem(0);
  if (p.lifeStatus == pet::LifeStatus::Lost) {
    farewellPicture(screen, m, scale(area, rect(179, 28, 108, 150)), inverse);
    if (groundEnvironment) drawItem(3);
    drawItem(2);
    return;
  }
  const bool eating = petfeed::eating(m.feedingPhase) && pet::canCare(p);
  const auto pair = [&](fui::Rect box, unsigned mask) {
    return m.feedingArt != petfeed::Art::Legacy && m.sleepArt.draw &&
           m.sleepArt.draw(m.sleepArt.context, target, box, mask, true) &&
           m.sleepArt.draw(m.sleepArt.context, target, box, mask + 1, false);
  };
  if (m.view == pet::View::Evolution) {
    const auto actor = celebrationActor;
    const auto pose = m.reactionPhase == petreact::Phase::Reveal || m.reactionPhase == petreact::Phase::Result
                          ? petsleep::bodyPose(p)
                          : petsleep::Pose::Normal;
    const unsigned mask = age == pet::Stage::Egg ? petsleep::eggIndex(unsigned(p.species), 0)
                                                 : petsleep::bodyIndex(p.species, age, pose, 0);
    if (m.sleepArt.draw) {
      m.sleepArt.draw(m.sleepArt.context, target, actor, mask, !petreact::silhouette(m.reactionPhase));
      if (!petreact::silhouette(m.reactionPhase)) m.sleepArt.draw(m.sleepArt.context, target, actor, mask + 1, false);
    }
    if (groundEnvironment) drawItem(3);
    drawItem(2);
    roomeffects::paint(screen, m, actor);
    return;
  }
  bool reactionPose = false;
  if (!eating && m.reactionKind != petreact::Kind::None && m.reactionKind != petreact::Kind::Growth &&
      m.reactionPhase != petreact::Phase::Result && pet::canCare(p) &&
      (m.reactionKind != petreact::Kind::Game || m.gameResult.outcome != GameOutcome::CareNeeded)) {
    const bool second = m.reactionPhase == petreact::Phase::Second;
    auto actor = m.reactionArt == petreact::Art::Registered ? registeredReaction(m, picture) : picture;
    if (m.reactionKind == petreact::Kind::Game && second) actor.y -= 6;
    if (m.reactionArt != petreact::Art::None)
      reactionPose = pair(actor, petreact::artIndex(m.reactionKind, p.species, pet::stage(p.readingSeconds),
                                                    m.reactionPhase, m.reactionArt));
    if (!reactionPose && m.reactionKind == petreact::Kind::Petting && m.curiosityAvailable)
      reactionPose = pair(picture, petcurious::artIndex(p.species, pet::stage(p.readingSeconds),
                                                        second ? petcurious::Phase::Blink : petcurious::Phase::Look));
    if (!reactionPose) {
      reactionPose = pair(
          actor, petsleep::bodyIndex(p.species, pet::stage(p.readingSeconds), petsleep::Pose::Happy, second ? 1 : 0));
    }
  }
  bool restPose = false;
  if (!eating && !reactionPose && m.restArtAvailable && m.restPhase != petrest::Phase::Idle && pet::canCare(p) &&
      p.illness == pet::Illness::Healthy) {
    const bool entering = m.restPhase == petrest::Phase::SettleSecond || m.restPhase == petrest::Phase::Exit;
    if (entering && habitatlayout::peeking(p)) {
      restPose = pair(picture, petsleep::headIndex(p));
    } else {
      unsigned mask;
      const petreact::Registration* registration;
      if (petrest::walking(m.restPhase)) {
        const auto slot = petrest::walkSlot(p.species, pet::stage(p.readingSeconds), petrest::returning(m.restPhase));
        registration = &petrest::REGISTRATIONS[slot];
        mask = petrest::artIndex(slot, petrest::second(m.restPhase));
      } else {
        const auto kind = m.restPhase == petrest::Phase::StretchFirst || m.restPhase == petrest::Phase::StretchSecond
                              ? petcompanion::Kind::Stretch
                              : petcompanion::Kind::Sit;
        const auto slot = petcompanion::poseSlot(kind, p.species, pet::stage(p.readingSeconds));
        registration = &petcompanion::REGISTRATIONS[slot];
        mask = petcompanion::artIndex(
            slot, petrest::second(m.restPhase) ? petcompanion::Phase::Second : petcompanion::Phase::First);
      }
      const auto actor = restPlacement(p, m.restPhase, area, registeredActor(m, picture, *registration));
      restPose = pair(actor, mask);
    }
  }
  bool companionPose = false;
  if (!eating && !reactionPose && !restPose && m.companionPhase != petcompanion::Phase::Idle &&
      m.feedingArt == petfeed::Art::Extended && petcompanion::awake(p)) {
    const auto slot = petcompanion::poseSlot(m.companionKind, p.species, pet::stage(p.readingSeconds));
    if (slot < 36) {
      static_assert(sizeof(petcompanion::REGISTRATIONS) / sizeof(petcompanion::REGISTRATIONS[0]) == 36);
      const auto actor = registeredActor(m, picture, petcompanion::REGISTRATIONS[slot]);
      companionPose = pair(actor, petcompanion::artIndex(slot, m.companionPhase));
    }
  }
  const bool curiousPose =
      !eating && !reactionPose && !restPose && !companionPose && m.curiosityPhase != petcurious::Phase::Idle &&
      petcurious::eligible(p, m.view) &&
      pair(picture, petcurious::artIndex(p.species, pet::stage(p.readingSeconds), m.curiosityPhase));
  const bool fedPose =
      eating &&
      pair(picture, petfeed::petIndex(unsigned(p.species), unsigned(pet::stage(p.readingSeconds)), m.feedingPhase));
  if (!reactionPose && !restPose && !companionPose && !curiousPose && !fedPose && !breathingPose(m, target, picture) &&
      !staticSleepHead(m, target, picture) && !fullCreature(m, target, picture)) {
    if (!(m.scenes.draw && m.scenes.draw(m.scenes.context, target, picture, pet::homeScene(p), false))) {
      const unsigned age = unsigned(pet::stage(p.readingSeconds)), species = unsigned(p.species);
      const unsigned fallback = age ? 3 + species * 16 + (age - 1) * 4 : species;
      if (!(m.art.draw && m.art.draw(m.art.context, target, picture, fallback, false))) {
        static constexpr const freeink::Icon* icons[] = {&icon_cat_nature_32, &extra_moon_64, &extra_reading_flame_64};
        dashboard::icon(screen, picture, *icons[species]);
      }
    }
  }
  if ((sleeping(p) || m.restPhase == petrest::Phase::SettleSecond || m.restPhase == petrest::Phase::Exit) &&
      p.equipped[0] < pet::OBJECT_COUNT && pet::objectSlot(p.equipped[0]) == 0 && background) {
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
  if (groundEnvironment) drawItem(3);
  drawItem(2);
  if (!eating) roomeffects::paint(screen, m, picture);
  if (eating) {
    const auto actor = habitatlayout::pet(p);
    const auto food = scale(area, rect(actor.x - 26, actor.bottom() - 22, 58, 36));
    if (!foodPicture(m, target, food, m.selectedFood, m.foodStage) && m.foodStage < petfeed::BITE_COUNT)
      dashboard::icon(screen, food, extra_dashboard_apple_64);
    return;
  }
  sleepMarks(screen, m, area, wallTop);
  if (petsleep::moodVisible(p, m.view) &&
      (m.restPhase == petrest::Phase::Idle || m.restPhase == petrest::Phase::StretchFirst ||
       m.restPhase == petrest::Phase::StretchSecond)) {
    fui::Rect obstacles[5] = {scale(area, rect(350, 18, 62, 54))};
    unsigned count = 1;
    for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
      const unsigned id = p.equipped[slot];
      if (pet::objectAvailable(id) && pet::objectSlot(id) == slot)
        obstacles[count++] = placedItem(area, id, wallTop, shareWall);
    }
    const auto anchor = picture.inset({int16_t(picture.height / 8), 0, 0, 0});
    const auto moodBox = petmood::roomBounds(area, anchor, obstacles, count);
    auto emotion = petcompanion::emotion(m.companionKind);
    auto emotionPhase = m.companionPhase;
    if (emotion == petcompanion::Emotion::None && m.curiosityPhase != petcurious::Phase::Idle) {
      emotion = petcompanion::Emotion::Curious;
      emotionPhase =
          m.curiosityPhase == petcurious::Phase::Blink ? petcompanion::Phase::Second : petcompanion::Phase::First;
    }
    auto mood = m.restPhase == petrest::Phase::StretchFirst || m.restPhase == petrest::Phase::StretchSecond
                    ? petmood::Mood::Satisfied
                    : petmood::select(p);
    if (emotionPhase != petcompanion::Phase::Idle) {
      switch (emotion) {
        case petcompanion::Emotion::Curious:
          mood = petmood::Mood::Curious;
          break;
        case petcompanion::Emotion::Satisfied:
          mood = petmood::Mood::Satisfied;
          break;
        case petcompanion::Emotion::Surprised:
          mood = petmood::Mood::Surprised;
          break;
        case petcompanion::Emotion::None:
          break;
      }
    }
    petmood::draw(target, moodBox, mood);
  }
}

inline void habitat(toybox::Screen& screen, const Model& m, fui::Rect card, int header = 47, bool inverse = false) {
  using namespace habitatlayout;
  auto& target = screen.target();
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
  habitatFigures(screen, m, area, background, inverse);
}
}  // namespace petui

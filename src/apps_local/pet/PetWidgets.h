#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/Dashboard.h"
#include "../ui/ToyboxIcons.h"
#include "PetCareAdvice.h"
#include "PetFarewell.h"
#include "PetHabitat.h"
#include "PetPalette.h"
#include "PetPersonality.h"
#include "PetReferenceIcons.h"
#include "PetScreens.h"

namespace petui {
namespace widget {
inline constexpr int GAP = 8, BORDER = 2;
inline constexpr auto ink = fui::Paint::solid(fui::Color::Black);
inline constexpr auto paper = fui::Paint::solid(fui::Color::White);
inline constexpr int CARE_METRICS_H = 84, CARE_ACTIONS_H = 92, HOME_SLOTS_H = 104, HOME_PROJECT_H = 180,
                     ALBUM_STAGES_H = 180;
inline constexpr int UI_GAP = 16, UI_INSET = 16, SETUP_CONFIG_H = 78;
inline fui::Rect rect(int x, int y, int w, int h) { return fui::makeRect(x, y, std::max(0, w), std::max(0, h)); }
inline fui::Rect cell(fui::Rect r, int index, int count, int gap = GAP) {
  const int width = (r.width - (count - 1) * gap) / count;
  return rect(r.x + index * (width + gap), r.y, index + 1 == count ? r.right() - r.x - index * (width + gap) : width,
              r.height);
}
inline void frame(toybox::Screen& s, fui::Rect r) { s.target().stroke(r, ink, BORDER); }
inline void text(toybox::Screen& s, fui::Rect r, const char* value, int font = 0, bool inverse = false,
                 fui::TextAlign align = fui::TextAlign::Left) {
  pixelui::text(s.target(), r, value, font, inverse ? fui::Color::White : fui::Color::Black, align);
}
inline void prose(toybox::Screen& s, fui::Rect r, const char* value, int lines = 2,
                  fui::TextAlign align = fui::TextAlign::Left, bool inverse = false) {
  fui::TextStyle style;
  style.font = 4;
  style.align = align;
  style.color = inverse ? fui::Color::White : fui::Color::Black;
  for (const int font : {4, 3}) {
    if (s.target().lineHeight(font) > r.height) continue;
    style.font = font;
    style.maxLines = std::max(1, std::min(lines, int(r.height) / std::max(1, int(s.target().lineHeight(font)))));
    const char* next = value;
    bool complete = true;
    fui::layoutText(s.target(), r, value, style, [&](const char* run, fui::Rect) {
      while (*next == ' ' || *next == '\n') ++next;
      const auto length = std::strlen(run);
      if (std::strncmp(next, run, length))
        complete = false;
      else
        next += length;
    });
    while (*next == ' ' || *next == '\n') ++next;
    if (complete && !*next) break;
  }
  s.target().text(r, value, style);
}
inline void icon(toybox::Screen& s, fui::Rect r, const freeink::Icon& mark, bool inverse = false) {
  dashboard::icon(s, r, mark, inverse);
}
inline bool button(toybox::Screen& s, Work& w, fui::Rect r, const char* label, fui::ActionId id, int value = 0,
                   bool filled = false, const freeink::Icon* mark = nullptr, bool enabled = true) {
  const bool inverse = enabled && s.frame().buttonSelected(id, value, filled);
  w.button = {};
  w.button.label = "";
  w.button.action = enabled ? id : fui::NO_ACTION;
  w.button.value = int16_t(value);
  w.button.styles = pixelui::rowStyles();
  w.button.minTouchSize = 0;
  w.button.state = !enabled ? fui::StateDisabled : inverse ? fui::StateSelected : fui::StateNormal;
  s.button(w.button, r);
  if (mark) {
    const int side = std::min(34, int(r.height) - 18);
    icon(s, rect(r.x + 14, r.y + (r.height - side) / 2, side, side), *mark, inverse);
    r.x += side + 20;
    r.width -= side + 26;
  }
  text(s, r.inset({4, 8, 4, 8}), label, 0, inverse, fui::TextAlign::Center);
  return inverse;
}
inline void tile(toybox::Screen& s, Work& w, fui::Rect r, const char* label, fui::ActionId id,
                 const freeink::Icon& mark, int value = 0, bool enabled = true) {
  const bool inverse = button(s, w, r, "", id, value, false, nullptr, enabled);
  const int side = std::max(18, std::min(42, int(r.height) - 42));
  icon(s, rect(r.x + (r.width - side) / 2, r.y + 8, side, side), mark, inverse);
  text(s, rect(r.x + 4, r.bottom() - 30, r.width - 8, 25), label, 3, inverse, fui::TextAlign::Center);
}
inline const freeink::Icon& speciesIcon(pet::Species value) {
  switch (value) {
    case pet::Species::Silva:
      return icon_cat_nature_32;
    case pet::Species::Nox:
      return extra_moon_64;
    case pet::Species::Ember:
      return extra_reading_flame_64;
  }
  return icon_cat_animals_32;
}
inline void cross(toybox::Screen& s, fui::Rect r, bool inverse = false) {
  static constexpr fui::Point points[] = {{4, 0},  {8, 0}, {8, 4}, {12, 4}, {12, 8}, {8, 8}, {8, 12},
                                          {4, 12}, {4, 8}, {0, 8}, {0, 4},  {4, 4},  {4, 0}};
  for (unsigned i = 1; i < 13; ++i)
    s.target().line(
        {int16_t(r.x + points[i - 1].x * (r.width - 2) / 12), int16_t(r.y + points[i - 1].y * (r.height - 2) / 12)},
        {int16_t(r.x + points[i].x * (r.width - 2) / 12), int16_t(r.y + points[i].y * (r.height - 2) / 12)}, 2,
        inverse ? paper : ink);
}
inline void sprig(toybox::Screen& s, fui::Rect r, pet::Species species, bool inverse = false) {
  icon(s, r, speciesIcon(species), inverse);
}
inline void grass(toybox::Screen& s, fui::Rect r) {
  const int y = r.bottom() - 2, x = r.x + r.width / 2;
  for (const auto p : {fui::Point{-9, -6}, fui::Point{-5, -14}, fui::Point{1, -18}, fui::Point{8, -10}})
    s.target().line({int16_t(x), int16_t(y)}, {int16_t(x + p.x), int16_t(y + p.y)}, 2, ink);
  s.target().fill(rect(x - 14, y - 1, 3, 2), ink);
  s.target().fill(rect(x + 11, y - 3, 3, 2), ink);
}
inline void arrowRight(toybox::Screen& s, int x, int y, int width = 12) {
  s.target().line({int16_t(x), int16_t(y)}, {int16_t(x + width), int16_t(y)}, 2, ink);
  s.target().line({int16_t(x + width - 5), int16_t(y - 5)}, {int16_t(x + width), int16_t(y)}, 2, ink);
  s.target().line({int16_t(x + width - 5), int16_t(y + 5)}, {int16_t(x + width), int16_t(y)}, 2, ink);
}
inline void chevron(toybox::Screen& s, fui::Rect r, bool inverse = false) {
  const auto paint = inverse ? paper : ink;
  const int y = r.y + r.height / 2;
  s.target().line({r.x, r.y}, {int16_t(r.right() - 1), int16_t(y)}, 3, paint);
  s.target().line({int16_t(r.right() - 1), int16_t(y)}, {r.x, int16_t(r.bottom() - 1)}, 3, paint);
}
inline const freeink::Icon& objectIcon(unsigned value) {
  static constexpr const freeink::Icon* icons[] = {&icon_cat_house_32,  &extra_moon_64,       &icon_cat_house_32,
                                                   &icon_cat_nature_32, &extra_light_on_64,   &icon_cat_nature_32,
                                                   &icon_cat_sports_32, &icon_cat_house_32,   &extra_circle_dot_64,
                                                   &icon_cat_nature_32, &icon_cat_science_32, &extra_chart_64};
  return *icons[value < 12 ? value : std::min(pet::objectSlot(value), 3u) * 3];
}
inline void durationMinutes(char* out, size_t size, uint64_t minutes) {
  if (minutes >= 60)
    std::snprintf(out, size, tr(STR_PET_HOURS_MINUTES), static_cast<unsigned long long>(minutes / 60),
                  unsigned(minutes % 60));
  else
    std::snprintf(out, size, tr(STR_PET_MINUTES), static_cast<unsigned long long>(minutes));
}
inline void duration(char* out, size_t size, uint64_t seconds) { durationMinutes(out, size, seconds / 60); }
inline void remainingDuration(char* out, size_t size, uint64_t seconds) {
  durationMinutes(out, size, seconds / 60 + unsigned(seconds % 60 != 0));
}
inline void art(toybox::Screen& s, const Model& m, fui::Rect r, unsigned id, const freeink::Icon& fallback,
                bool caption = true, bool inverse = false) {
  if (id < 3 && petsleep::breathing(*m.profile, m.view) && m.sleepArt.draw &&
      m.sleepArt.draw(m.sleepArt.context, s.target(), r, petsleep::eggIndex(id, m.sleepStep) + 1, inverse))
    return;
  if (id >= 3 && id < 51 && petsleep::breathing(*m.profile, m.view) && m.sleepArt.draw &&
      m.sleepArt.draw(m.sleepArt.context, s.target(), r, petsleep::graphicIndex(id, m.sleepStep) + 1, inverse))
    return;
  if (m.art.draw && m.art.draw(m.art.context, s.target(), r, id, inverse)) return;
  const int side = std::max(20, std::min({104, int(r.width) - 20, int(r.height) - (caption ? 42 : 12)}));
  icon(s, rect(r.x + (r.width - side) / 2, r.y + (r.height - side - (caption ? 24 : 0)) / 2, side, side), fallback,
       inverse);
  if (caption && r.width >= 175)
    text(s, rect(r.x + 6, r.bottom() - 27, r.width - 12, 22), tr(STR_PET_ART_PENDING), 3, inverse,
         fui::TextAlign::Center);
}
inline unsigned creatureArt(const pet::Profile& p) {
  const auto life = pet::stage(p.readingSeconds);
  if (life == pet::Stage::Egg) return unsigned(p.species);
  const unsigned mood =
      p.lifeStatus != pet::LifeStatus::Active || p.illness != pet::Illness::Healthy || p.resting || p.energy < 25 ? 3
      : p.saciety < 30                                                                                            ? 2
      : p.joy >= 75                                                                                               ? 1
                                                                                                                  : 0;
  return 3 + unsigned(p.species) * 16 + (unsigned(life) - 1) * 4 + mood;
}
inline fui::Rect portraitBody(const pet::Profile& p, fui::Rect r) {
  static constexpr int scale[] = {100, 82, 88, 94, 100};
  const int size = scale[unsigned(pet::stage(p.readingSeconds))];
  const int width = r.width * size / 100, height = r.height * size / 100;
  return rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
}
inline fui::Rect portraitMoodBounds(const pet::Profile& p, fui::Rect area) {
  auto badge = petmood::portraitBounds(area);
  const auto body = portraitBody(p, area);
  const auto& source = PET_ART[pet::homeScene(p)];
  const int width = std::min(int(body.width), int(body.height) * source.width / source.height);
  const int height = width * source.height / source.width;
  const int x = body.x + (body.width + width) / 2 + 4;
  const int y = body.y + (body.height - height) / 2 + height / 12;
  badge.x = int16_t(std::max(int(area.x), std::min(x, int(area.right() - badge.width))));
  badge.y = int16_t(std::max(int(area.y), std::min(y, int(area.bottom() - badge.height))));
  return badge;
}
inline void creature(toybox::Screen& s, const Model& m, fui::Rect r, bool portrait = false) {
  const auto& p = *m.profile;
  const bool sleeping = p.resting && p.illness == pet::Illness::Healthy;
  const auto badge = portrait ? portraitMoodBounds(p, r) : petmood::portraitBounds(r);
  const bool showMood = petmood::select(p) != petmood::Mood::None;
  const int band = pet::stage(p.readingSeconds) == pet::Stage::Egg ? 0
                   : sleeping                                      ? std::min(34, std::max(0, int(r.height) - 48))
                   : showMood ? std::min(int(badge.height) + 4, std::max(0, int(r.height) - 48))
                              : 0;
  if (sleeping && band) {
    const int x = portrait ? badge.x + 4 : r.x + r.width / 3, y = r.y + 20;
    for (unsigned i = 0; i <= petsleep::frame(m.sleepStep); ++i) {
      const int size = 6 + int(i) * 2, left = x + int(i) * 13, top = y - int(i) * 8;
      for (const bool halo : {true, false}) {
        const auto paint = halo ? paper : ink;
        const int weight = halo ? 6 : 2;
        s.target().line({int16_t(left), int16_t(top)}, {int16_t(left + size), int16_t(top)}, weight, paint);
        s.target().line({int16_t(left + size), int16_t(top)}, {int16_t(left), int16_t(top + size)}, weight, paint);
        s.target().line({int16_t(left), int16_t(top + size)}, {int16_t(left + size), int16_t(top + size)}, weight,
                        paint);
      }
    }
  } else if (showMood && band) {
    petmood::draw(s.target(), badge, petmood::select(p), m.sleepStep);
  }
  if (portrait) {
    r = portraitBody(p, r);
  } else {
    r.y += band;
    r.height -= band;
  }
  if (!fullCreature(m, s.target(), r, false, portrait && m.portraitBackgrounds))
    art(s, m, r, creatureArt(p), speciesIcon(p.species));
}
inline void object(toybox::Screen& s, const Model& m, fui::Rect r, unsigned id, bool inverse = false) {
  if (id < pet::OBJECT_COUNT && m.habitat.draw && m.habitat.draw(m.habitat.context, s.target(), r, 3 + id * 2, inverse))
    return;
  if (id < 12 && m.art.draw && m.art.draw(m.art.context, s.target(), r, 51 + id, inverse)) return;
  icon(s, r, objectIcon(id), inverse);
}
inline const char* mood(const pet::Profile& p) {
  if (p.lifeStatus == pet::LifeStatus::Lost) return tr(STR_PET_LIFE_LOST);
  if (p.lifeStatus == pet::LifeStatus::Retired) return tr(STR_PET_LIFE_RETIRED);
  if (pet::stage(p.readingSeconds) == pet::Stage::Egg) return tr(STR_PET_EGG_MOOD);
  switch (pet::need(p)) {
    case pet::Need::Sick:
      return tr(STR_PET_SICK);
    case pet::Need::Sleeping:
      return tr(STR_PET_RESTING);
    case pet::Need::Hungry:
      return tr(STR_PET_HUNGRY);
    case pet::Need::Tired:
      return tr(STR_PET_TIRED);
    case pet::Need::Dirty:
      return tr(STR_PET_NEEDS_CLEANING);
    case pet::Need::Company:
      return tr(STR_PET_WANTS_COMPANY);
    case pet::Need::None:
      return tr(STR_PET_HAPPY);
  }
  return "";
}
inline void identity(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, int font = 2) {
  std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_GENERATION),
                static_cast<unsigned long>(m.profile->generation));
  pixelui::copyCaps(w.label, sizeof(w.label), w.secondary);
  const int genWidth = s.target().measureText(3, w.label, {}).width;
  pixelui::copyCaps(w.label, sizeof(w.label), displayName(m.profile->name, m.profile->species));
  const int nameWidth = std::max(1, int(r.width) - genWidth - 12);
  if (r.height >= 44 && s.target().measureText(3, w.label, {}).width > nameWidth) {
    text(s, rect(r.x, r.y, r.width, r.height - 22), displayName(m.profile->name, m.profile->species), 0);
    text(s, rect(r.x, r.bottom() - 22, r.width, 22), w.secondary, 3);
    return;
  }
  for (const int candidate : {font, 1, 0, 3}) {
    font = candidate;
    if (s.target().measureText(font, w.label, {}).width <= nameWidth) break;
  }
  text(s, rect(r.x, r.y, nameWidth, r.height), displayName(m.profile->name, m.profile->species), font);
  text(s, rect(r.right() - genWidth, r.y, genWidth, r.height), w.secondary, 3, false, fui::TextAlign::Right);
}
inline bool portraitBackground(toybox::Screen& s, const Model& m, fui::Rect r) {
  if (!m.portraitBackgrounds || !m.scenes.draw) return false;
  const int width = (r.width - 4) & ~1;
  const int height = width * 144 / 448;
  if (width <= 0 || height <= 0 || height > r.height - 4) return false;
  const auto area = rect(r.x + (r.width - width) / 2, r.bottom() - height - 2, width, height);
  const unsigned first = HOME_ART_COUNT + unsigned(m.profile->species) * 2;
  for (unsigned half = 0; half < 2; ++half) {
    const auto piece = rect(area.x + int(half) * width / 2, area.y, width / 2, height);
    if (!m.scenes.draw(m.scenes.context, s.target(), piece, first + half, false)) {
      s.target().fill(area, paper);
      return false;
    }
  }
  return true;
}
inline void hero(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, bool habitat = false,
                 Palette* palette = nullptr) {
  const auto& p = *m.profile;
  PaletteScope scope(palette, p.lifeStatus == pet::LifeStatus::Lost);
  if (scope.inverted()) s.target().fill(r, paper);
  frame(s, r);
  if (p.lifeStatus == pet::LifeStatus::Lost) {
    identity(s, m, w, rect(r.x + 12, r.y + 8, r.width - 24, 47));
    text(s, rect(r.x + 12, r.y + 60, r.width - 24, 28), tr(STR_PET_DIED_STATUS), 0);
    farewellPicture(s, m, rect(r.x + 12, r.y + 99, r.width - 24, r.height - 111), scope.inverted());
    return;
  }
  const bool background = portraitBackground(s, m, r);
  identity(s, m, w, rect(r.x + 12, r.y + 8, r.width - 24, 50));
  icon(s, rect(r.x + 12, r.y + 67, 28, 28), speciesIcon(p.species));
  text(s, rect(r.x + 52, r.y + 64, r.width - 64, 34), stageName(pet::stage(p.readingSeconds)), 0);
  const auto action = statusAction(p);
  const bool guided = action != fui::NO_ACTION;
  const int controlW = guided ? std::min(148, int(r.width) - 130) : 0;
  const auto* reaction = companionLine(m);
  const auto* hint = guided ? statusHint(p) : nullptr;
  const auto status = rect(r.x + 12, r.y + 103, r.width - 24 - (guided ? controlW + 8 : 0), 44);
  prose(s, status, hint ? hint : reaction ? reaction : mood(p), 2);
  if (guided) {
    const auto control = rect(r.right() - controlW - 12, r.y + 102, controlW, 44);
    button(s, w, control, statusLink(p), action, action == ChangeTab ? 1 : 0);
  }
  const auto image = rect(r.x + 12, r.y + 154, r.width - 24, r.height - 164);
  creature(s, m, image, true);
  if (p.lifeStatus == pet::LifeStatus::Active && !background && pet::stage(p.readingSeconds) != pet::Stage::Egg) {
    const auto body = portraitBody(p, image);
    grass(s, rect(image.x + 10, body.bottom() - 24, 24, 22));
    grass(s, rect(image.right() - 34, body.bottom() - 24, 24, 22));
  }
  if (habitat) {
    const int side = 28;
    for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot)
      if (p.equipped[slot] < pet::OBJECT_COUNT)
        object(s, m, rect(image.x + int(slot) * (side + 5), image.bottom() - side, side, side), p.equipped[slot]);
  }
}
inline void room(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, Palette* palette = nullptr) {
  const auto& p = *m.profile;
  PaletteScope scope(palette, p.lifeStatus == pet::LifeStatus::Lost);
  if (scope.inverted()) s.target().fill(r, paper);
  frame(s, r);
  icon(s, rect(r.x + 12, r.y + 9, 27, 27), icon_cat_places_32);
  std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_GENERATION), static_cast<unsigned long>(p.generation));
  pixelui::copyCaps(w.label, sizeof(w.label), w.secondary);
  const int genWidth = s.target().measureText(3, w.label, {}).width;
  pixelui::copyCaps(w.label, sizeof(w.label), displayName(p.name, p.species));
  const bool stacked = genWidth + s.target().measureText(3, w.label, {}).width + 12 > r.width - 60;
  if (stacked)
    identity(s, m, w, rect(r.x + 48, r.y + 3, r.width - 60, 44), 0);
  else
    identity(s, m, w, rect(r.x + 48, r.y + 5, r.width - 60, 22), 0);
  text(s, rect(r.x + 48, r.y + (stacked ? 51 : 28), r.width - 60, 18),
       p.lifeStatus == pet::LifeStatus::Lost ? tr(STR_PET_DIED_STATUS) : stageName(pet::stage(p.readingSeconds)), 3);
  habitat(s, m, r, stacked ? 72 : 47, scope.inverted());
}
inline void metric(toybox::Screen& s, Work& w, fui::Rect r, const char* label, unsigned value,
                   const freeink::Icon& mark, bool locked = false) {
  s.target().stroke(r, ink, 1);
  const bool vertical = r.height >= 132 || (r.width < 130 && r.height >= 90);
  if (locked && !vertical) {
    text(s, rect(r.x + 6, r.y + 6, r.width - 12, 24), label, 3, false, fui::TextAlign::Center);
    icon(s, rect(r.x + (r.width - 28) / 2, r.y + 37, 28, 28), extra_lock_32);
    return;
  }
  if (vertical) {
    const bool compact = r.height < 132;
    const int side = compact ? 28 : 40;
    icon(s, rect(r.x + (r.width - side) / 2, r.y + (compact ? 6 : 10), side, side), mark);
    prose(s, rect(r.x + 5, r.y + (compact ? 34 : 53), r.width - 10, 28), label, 1, fui::TextAlign::Center);
    if (locked) {
      icon(s, rect(r.x + (r.width - 28) / 2, r.y + (compact ? 65 : 84), 28, 28), extra_lock_32);
      return;
    }
    std::snprintf(w.label, sizeof(w.label), "%u/100", value);
    const int valueFont = s.target().measureText(0, "100/100", {}).width <= r.width - 10 ? 0 : 3;
    text(s, rect(r.x + 5, r.y + (compact ? 62 : 83), r.width - 10, compact ? 26 : 28), w.label, valueFont, false,
         fui::TextAlign::Center);
    dashboard::bar(s, rect(r.x + 9, r.bottom() - (compact ? 14 : 22), r.width - 18, compact ? 8 : 13), value, 100);
    return;
  }
  const bool compact = r.height < 72;
  const int side = std::min(compact ? 24 : 32, int(r.width) / 4);
  icon(s, rect(r.x + 8, r.y + 8, side, side), mark);
  const int x = r.x + side + 14, width = r.right() - x - 6;
  prose(s, rect(x, r.y + 2, width, compact ? 21 : 28), label, 1);
  std::snprintf(w.label, sizeof(w.label), "%u / 100", value);
  text(s, rect(x, r.y + (compact ? 24 : 28), width, compact ? 21 : 26), w.label, compact ? 3 : 0);
  dashboard::bar(s, rect(r.x + 9, r.bottom() - (compact ? 10 : 20), r.width - 18, compact ? 5 : 10), value, 100);
}
inline void incubation(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  if (m.profile->lifeStatus != pet::LifeStatus::Active) {
    frame(s, rect(r.x, r.y + 16, r.width, 154));
    icon(s, rect(r.x + (r.width - 42) / 2, r.y + 32, 42, 42), extra_lock_32);
    prose(s, rect(r.x + 16, r.y + 90, r.width - 32, 64), mood(*m.profile), 2, fui::TextAlign::Center);
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_PET_NEW_COMPANION), NewCompanion, 0, true);
    return;
  }
  const bool wide = r.width > r.height;
  const int width = std::min(int(r.width) - 16, wide ? 630 : 422);
  const int available = r.height - 70;
  const int height = std::min(available, wide ? 250 : 480);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (available - height) / 2, width, height);
  frame(s, box);
  const bool side = wide;
  const auto heading = rect(box.x + 16, box.y + 16, box.width - 32, wide ? 88 : side ? 104 : 130);
  icon(s,
       side ? rect(heading.x, heading.y + 12, 64, 64) : rect(heading.x + (heading.width - 56) / 2, heading.y, 56, 56),
       extra_lock_32);
  const auto copy = side ? rect(heading.x + 80, heading.y, heading.width - 80, heading.height) : heading;
  text(s, rect(copy.x, copy.y + (side ? 0 : 68), copy.width, 32), tr(STR_PET_CARE_LOCKED), 1, false,
       fui::TextAlign::Center);
  const auto explanation = rect(box.x + 18, heading.bottom() + 4, box.width - 36, wide ? 52 : side ? 58 : 68);
  prose(s, explanation, tr(STR_PET_EGG_HEALTH), 3, fui::TextAlign::Center);
  const auto progress = rect(box.x + 18, explanation.bottom() + 12, box.width - 36, 45);
  const auto remaining = pet::nextStageSeconds(pet::Stage::Egg) - m.profile->readingSeconds;
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_HATCH_LEFT),
                static_cast<unsigned long long>((remaining + 59) / 60));
  text(s, rect(progress.x, progress.y, progress.width, 24), w.label, 0, false, fui::TextAlign::Center);
  dashboard::bar(s, rect(progress.x, progress.bottom() - 12, progress.width, 12), m.profile->readingSeconds,
                 pet::nextStageSeconds(pet::Stage::Egg));
  if (!wide) {
    const auto features = rect(box.x + 18, progress.bottom() + 16, box.width - 36, 86);
    static constexpr StrId names[] = {StrId::STR_PET_FEED, StrId::STR_PET_REST, StrId::STR_PET_MINIGAMES,
                                      StrId::STR_PET_HEALTH_SHORT};
    for (int i = 0; i < 4; ++i) {
      const auto item = cell(rect(features.x, features.y + (i / 2) * 43, features.width, 37), i % 2, 2, 12);
      icon(s, rect(item.x, item.y + 7, 20, 20), extra_lock_32);
      text(s, rect(item.x + 28, item.y, item.width - 28, item.height), I18N.get(names[i]), 3);
    }
  }
  button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_PET_CONTINUE_READING), Read, 0, true,
         &extra_reading_book_32);
}
struct ProjectProgress {
  uint64_t value, target;
};
inline ProjectProgress projectProgress(const pet::Profile& p) {
  if (pet::owns(p, p.project)) return {1, 1};
  const auto owned = pet::ownedCount(p);
  const uint64_t reading = pet::projectReading(p), previous = owned ? pet::projectThreshold(owned - 1) : 0;
  // Older or imported collections may predate their reading credit.
  const uint64_t start = reading >= previous ? previous : 0;
  return {reading - start, pet::projectThreshold(owned) - start};
}
inline void project(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, bool prominent = false) {
  const auto& p = *m.profile;
  const bool completed = pet::owns(p, p.project);
  if (!prominent && completed && r.height >= 155) {
    frame(s, r);
    s.target().fill(rect(r.x + 18, r.y + 19, 24, 4), ink);
    s.target().fill(rect(r.right() - 42, r.y + 19, 24, 4), ink);
    text(s, rect(r.x + 52, r.y + 3, r.width - 104, 31), tr(STR_PET_NEXT_GOAL), 0, false, fui::TextAlign::Center);
    const int side = std::min(114, int(r.height) - 62);
    const auto picture = rect(r.x + 12, r.y + 47, side, side);
    icon(s, picture, referenceicons::EMPTY_PROJECT);
    prose(s, rect(picture.right() + 16, picture.y, r.right() - picture.right() - 30, picture.height),
          tr(STR_PET_HOME_NO_SELECTION), 5);
    return;
  }
  const auto progress = projectProgress(p);
  const bool hatching =
      prominent && pet::stage(p.readingSeconds) == pet::Stage::Egg && p.lifeStatus == pet::LifeStatus::Active;
  if (!prominent && r.height >= 155) {
    frame(s, r);
    s.target().fill(rect(r.x + 18, r.y + 19, 24, 4), ink);
    s.target().fill(rect(r.right() - 42, r.y + 19, 24, 4), ink);
    text(s, rect(r.x + 52, r.y + 3, r.width - 104, 31), tr(STR_PET_NEXT_GOAL), 0, false, fui::TextAlign::Center);
    const int side = std::min(114, int(r.height) - 55);
    const auto picture = rect(r.x + 12, r.y + 49, side, side);
    frame(s, picture);
    object(s, m, picture.inset({9, 9, 9, 9}), p.project);
    const auto copy = rect(picture.right() + 15, picture.y, r.right() - picture.right() - 29, picture.height);
    prose(s, rect(copy.x, copy.y + 3, copy.width, 46), objectName(p.project), 2);
    if (completed)
      std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_PROJECT_READY));
    else
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_READING_LEFT),
                    static_cast<unsigned long long>((pet::projectRemaining(p) + 59) / 60));
    prose(s, rect(copy.x, copy.y + 52, copy.width, copy.height - 71), w.label, 2);
    dashboard::bar(s, rect(copy.x, copy.bottom() - 18, copy.width, 13), progress.value, progress.target);
    return;
  }
  frame(s, r);
  text(s, rect(r.x + 10, r.y + 6, r.width - 20, 28), tr(STR_PET_NEXT_GOAL), 1);
  if (prominent) {
    const int side = std::min(100, int(r.height) - 85);
    const auto picture = rect(r.x + 16, r.y + 50, side, side);
    if (hatching)
      art(s, m, picture, unsigned(p.species), speciesIcon(p.species), false);
    else
      object(s, m, picture, p.project);
    const int copyX = r.x + side + 32, copyW = r.right() - copyX - 16;
    text(s, rect(copyX, r.y + 46, copyW, 27), hatching ? tr(STR_PET_HATCH_GOAL) : objectName(p.project), 0);
    if (hatching)
      std::snprintf(
          w.label, sizeof(w.label), tr(STR_PET_MINUTES),
          static_cast<unsigned long long>((pet::nextStageSeconds(pet::Stage::Egg) - p.readingSeconds + 59) / 60));
    else if (completed)
      std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_PROJECT_READY));
    else
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_MINUTES),
                    static_cast<unsigned long long>((pet::projectRemaining(p) + 59) / 60));
    text(s, rect(copyX, r.y + 77, copyW, 46), w.label, completed && !hatching ? 0 : 2);
    if (hatching || !completed)
      text(s, rect(copyX, r.y + 126, copyW, 20), hatching ? tr(STR_PET_READING_TO_HATCH) : tr(STR_PET_READING_FOR_GOAL),
           3);
    if (hatching) {
      dashboard::bar(s, rect(r.x + 16, r.bottom() - 28, r.width - 32, 15), p.readingSeconds,
                     pet::nextStageSeconds(pet::Stage::Egg));
      return;
    }
    dashboard::bar(s, rect(r.x + 16, r.bottom() - 28, r.width - 32, 15), progress.value, progress.target);
    return;
  }
  const int side = std::min(78, int(r.height) - 42);
  object(s, m, rect(r.x + 12, r.y + 38, side, side), p.project);
  const auto info = rect(r.x + side + 26, r.y + 38, r.width - side - 38, r.height - 45);
  const bool compact = r.height < 115;
  const int titleH = compact ? 22 : 28;
  text(s, rect(info.x, info.y, info.width, titleH), objectName(p.project), compact ? 3 : 0);
  const uint64_t remaining = pet::projectRemaining(p);
  if (completed)
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_PROJECT_READY));
  else
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_READING_LEFT),
                  static_cast<unsigned long long>((remaining + 59) / 60));
  text(s, rect(info.x, info.y + titleH + 1, info.width, compact ? 20 : 24), w.label, 3);
  dashboard::bar(s, rect(info.x, info.bottom() - 14, info.width, 12), progress.value, progress.target);
}
inline void guidance(toybox::Screen& s, fui::Rect r, const char* message) {
  frame(s, r);
  const int side = 36, x = r.x + UI_INSET, y = r.y + (r.height - side) / 2;
  s.target().stroke(rect(x, y, side, side), ink, 3, side / 2);
  s.target().fill(rect(x + 16, y + 8, 4, 13), ink, 1);
  s.target().fill(rect(x + 16, y + 25, 4, 4), ink, 1);
  prose(s, rect(x + side + 14, r.y + UI_INSET, r.width - side - UI_INSET * 2 - 14, r.height - UI_INSET * 2), message,
        5);
}
inline void info(toybox::Screen& s, fui::Rect r, const char* message) {
  frame(s, r);
  icon(s, rect(r.x + 10, r.y + (r.height - 28) / 2, 28, 28), extra_information_32);
  prose(s, rect(r.x + 48, r.y + 5, r.width - 58, r.height - 10), message, 3);
}
}  // namespace widget
}  // namespace petui

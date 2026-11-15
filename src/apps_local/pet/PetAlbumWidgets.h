#pragma once

#include "../reading/ReadingStatsCore.h"
#include "PetActionSurface.h"
#include "PetAlbum.h"
#include "PetReadingStatus.h"
#include "PetReferenceIcons.h"
#include "PetRoomBackground.h"

namespace petui {
namespace albumwidget {
using namespace widget;
inline void caption(toybox::Screen& s, fui::Rect r, const char* value, int lines = 2, bool inverse = false,
                    fui::FontId font = 4, bool emphasis = false) {
  fui::TextStyle style;
  style.font = font;
  style.bold = emphasis;
  style.align = fui::TextAlign::Center;
  style.color = inverse ? fui::Color::White : fui::Color::Black;
  const int capacity = int(r.height) / std::max(1, int(s.target().lineHeight(3)));
  fittext::draw(s.target(), r, value, style, std::max(lines, capacity));
}
inline void vignette(toybox::Screen& s, const Model& m, fui::Rect r, unsigned id, bool fullWidth = false,
                     uint8_t radius = CONTROL_RADIUS) {
  s.target().fill(r, paper);
  s.target().stroke(r, ink, BORDER, radius);
  const auto inner = fullWidth ? r.inset({3, 3, 3, 3}) : r.inset({5, 5, 5, 5});
  const int width = std::min(int(inner.width), int(inner.height) * 2);
  const auto area =
      fullWidth ? inner
                : rect(inner.x + (inner.width - width) / 2, inner.y + (inner.height - width / 2) / 2, width, width / 2);
  const bool captured = memoryCaptured(*m.profile, id);
  const uint32_t snapshot = captured ? m.profile->moments[id] : 0;
  auto& target = s.target();
  const auto previous = target.clipRect();
  const int left = std::max(int(inner.x), int(previous.x)), top = std::max(int(inner.y), int(previous.y));
  const int right = std::min(int(inner.right()), int(previous.right()));
  const int bottom = std::min(int(inner.bottom()), int(previous.bottom()));
  if (right <= left || bottom <= top || !target.setClipRect(rect(left, top, right - left, bottom - top))) return;
  struct RestoreClip {
    fui::DrawTarget& target;
    fui::Rect previous;
    ~RestoreClip() { target.setClipRect(previous); }
  } restore{target, previous};
  const bool background = roombackground::memory(target, m, inner, area.y + area.width * 135 / 432);
  if (!background) target.fill(inner, paper);
  if (!background && m.habitat.draw) {
    const int side = area.width / 2;
    const int y = fullWidth ? area.bottom() - side : inner.y + (inner.height - side) / 2;
    m.habitat.draw(m.habitat.context, target, rect(area.x, y, side, side), 0, false);
    m.habitat.draw(m.habitat.context, target, rect(area.x + side, y, side, side), 1, false);
  }
  const unsigned environment = captured ? pet::momentObject(snapshot, 3) : 255;
  const bool shareWall = captured && habitatlayout::sharedWall(pet::momentObject(snapshot, 1), environment);
  const bool groundEnvironment = pet::objectAvailable(environment) &&
                                 habitatlayout::anchor(environment) == habitatlayout::Anchor::FloorEnvironment;
  const auto drawItem = [&](unsigned slot) {
    if (!captured) return;
    const unsigned item = pet::momentObject(snapshot, slot);
    if (!pet::objectAvailable(item) || pet::objectSlot(item) != slot) return;
    const auto box = habitatlayout::placedItem(area, item, -1, shareWall);
    if (!habitatPair(m, target, box, 2 + item * 2)) object(s, m, box, item);
  };
  drawItem(1);
  if (!groundEnvironment) drawItem(3);
  drawItem(0);
  const auto drawPet = [&] {
    const auto age = memoryStage(*m.profile, id);
    const unsigned species = unsigned(m.profile->species), pose = captured ? unsigned(pet::momentPose(snapshot)) : 0;
    const unsigned scene = age == pet::Stage::Egg ? species
                                                  : 3 + species * 12 + (unsigned(age) - 1) * 3 +
                                                        (pose == 4   ? 2
                                                         : pose == 5 ? 1
                                                                     : 0);
    const auto picture = habitatlayout::scale(area, habitatlayout::memoryPet(age, scene));
    const unsigned mask = age == pet::Stage::Egg ? petsleep::eggIndex(species, 0)
                          : pose == 5
                              ? ((species * 4 + unsigned(age) - 1) * petsleep::FRAMES) * 2
                              : petsleep::bodyIndex(m.profile->species, age, petsleep::Pose(std::min(pose, 4u)), 0);
    if (m.sleepArt.draw && m.sleepArt.draw(m.sleepArt.context, s.target(), picture, mask, true) &&
        m.sleepArt.draw(m.sleepArt.context, s.target(), picture, mask + 1, false))
      return;
    if (habitatPair(m, s.target(), picture, 482 + scene * 2)) return;
    s.target().fill(picture, paper);
    const unsigned graphic =
        age == pet::Stage::Egg ? species : 3 + species * 16 + (unsigned(age) - 1) * 4 + std::min(pose, 3u);
    art(s, m, picture, graphic, speciesIcon(m.profile->species), false);
  };
  drawPet();
  if (groundEnvironment) drawItem(3);
  drawItem(2);
}
inline int memoryDay(const pet::Profile& p, unsigned id) {
  const int day = id < pet::MOMENT_COUNT && p.momentDays[id] ? p.momentDays[id] : id == 2 ? p.bornDay : 0;
  return day > 0 && day <= readingstats::dayIndex({2199, 12, 31}) ? day : 0;
}
inline void memoryDate(char* out, size_t bytes, const pet::Profile& p, unsigned id, dateformat::Order order,
                       bool compact = false) {
  const int day = memoryDay(p, id);
  if (day) {
    const auto date = readingstats::dateFromDay(day);
    dateformat::format(out, bytes, unsigned(date.day), unsigned(date.month), unsigned(date.year), order, compact);
  } else
    std::snprintf(out, bytes, "%s", tr(STR_PET_MEMORY_DATE_UNKNOWN));
}
inline void memoryHeading(char* out, size_t bytes, const pet::Profile& p, unsigned id, dateformat::Order order) {
  char date[48], generation[32];
  memoryDate(date, sizeof(date), p, id, order, true);
  std::snprintf(generation, sizeof(generation), tr(STR_PET_GENERATION), static_cast<unsigned long>(p.generation));
  std::snprintf(out, bytes, tr(STR_PET_MEMORY_HEADING), date, generation);
}
inline void memoryReading(char* out, size_t bytes, const pet::Profile& p, unsigned id) {
  if (id >= pet::MOMENT_COUNT || !p.momentDays[id]) {
    std::snprintf(out, bytes, "%s", tr(STR_PET_MEMORY_TIME_UNKNOWN));
    return;
  }
  std::snprintf(out, bytes, tr(STR_PET_MEMORY_ACCUMULATED), static_cast<unsigned long long>(p.momentReading[id] / 60));
}
inline unsigned memoryAt(const pet::Profile& p, unsigned ordinal) {
  uint8_t ids[MEMORY_COUNT];
  for (unsigned i = 0; i < MEMORY_COUNT; ++i) ids[i] = i;
  std::sort(ids, ids + MEMORY_COUNT, [&](unsigned a, unsigned b) {
    const bool haveA = memoryOwned(p, a), haveB = memoryOwned(p, b);
    if (haveA != haveB) return haveA;
    if (!haveA) return a < b;
    const int dayA = a < pet::MOMENT_COUNT ? p.momentDays[a] : 0;
    const int dayB = b < pet::MOMENT_COUNT ? p.momentDays[b] : 0;
    if (dayA != dayB) return dayA > dayB;
    const uint64_t readA = a < pet::MOMENT_COUNT ? p.momentReading[a] : 0;
    const uint64_t readB = b < pet::MOMENT_COUNT ? p.momentReading[b] : 0;
    if (readA != readB) return readA > readB;
    return a > b;
  });
  return ids[std::min(ordinal, MEMORY_COUNT - 1)];
}
inline const char* memoryRequirement(const pet::Profile& p, unsigned id, Work& w) {
  if (id >= pet::MOMENT_COUNT && id < MEMORY_COUNT) return memoryTitle(id);
  if (memoryUnrecorded(p, id)) return tr(STR_PET_MEMORY_UNRECORDED_HELP);
  if (p.lifeStatus != pet::LifeStatus::Active) return tr(STR_PET_MEMORY_MISSED);
  if (id == 1 && pet::collectionComplete(p)) return tr(STR_PET_MEMORY_HOME_COMPLETE);
  if (id >= unsigned(pet::Moment::Treatment) && id < pet::MOMENT_COUNT) {
    static constexpr unsigned counts[] = {1, 5, 20};
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_MEMORY_CURES_REQUIREMENT),
                  counts[id - unsigned(pet::Moment::Treatment)]);
    return w.secondary;
  }
  if (id >= unsigned(pet::Moment::Games5) && id < unsigned(pet::Moment::Treatment)) {
    static constexpr unsigned counts[] = {5, 25, 100};
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_MEMORY_GAMES_REQUIREMENT),
                  counts[id - unsigned(pet::Moment::Games5)]);
    return w.secondary;
  }
  if (id >= pet::LEGACY_MOMENT_COUNT && id < unsigned(pet::Moment::Games5)) {
    static constexpr unsigned hours[] = {1, 5, 10, 25, 50, 100};
    duration(w.label, sizeof(w.label), uint64_t(hours[id - pet::LEGACY_MOMENT_COUNT]) * 3600);
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_MEMORY_READ_REQUIREMENT), w.label);
    return w.secondary;
  }
  if (id == 0 || (id >= 6 && id <= 8)) {
    duration(w.label, sizeof(w.label), pet::stageStart(id == 0 ? pet::Stage::Newborn : pet::Stage(id - 4)));
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_MEMORY_READ_REQUIREMENT), w.label);
    return w.secondary;
  }
  static constexpr StrId hints[] = {
      StrId::STR_PET_MEMORY_ARRIVAL_REQUIREMENT,   StrId::STR_PET_MEMORY_HOME_REQUIREMENT,
      StrId::STR_PET_MEMORY_ARRIVAL_REQUIREMENT,   StrId::STR_PET_MEMORY_MEAL_REQUIREMENT,
      StrId::STR_PET_MEMORY_AFFECTION_REQUIREMENT, StrId::STR_PET_MEMORY_HEALTH_REQUIREMENT,
      StrId::STR_PET_MEMORY_ARRIVAL_REQUIREMENT,   StrId::STR_PET_MEMORY_ARRIVAL_REQUIREMENT,
      StrId::STR_PET_MEMORY_ARRIVAL_REQUIREMENT,   StrId::STR_PET_MEMORY_REST_REQUIREMENT,
      StrId::STR_PET_MEMORY_GAME_REQUIREMENT};
  return I18N.get(hints[id < pet::LEGACY_MOMENT_COUNT ? id : 2]);
}
inline void memoryCard(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, unsigned id, bool compact = false) {
  const bool owned = memoryOwned(*m.profile, id);
  const bool inverse = button(s, w, r, "", OpenMemory, int(id));
  if (owned)
    memoryHeading(w.label, sizeof(w.label), *m.profile, id, m.dateOrder);
  else
    std::snprintf(w.label, sizeof(w.label), "%s",
                  memoryUnrecorded(*m.profile, id) ? tr(STR_PET_MEMORY_UNRECORDED) : tr(STR_PET_MEMORY_UPCOMING));
  const int headingH = s.target().measureText(3, w.label, {}).width > r.width - 14 ? 2 * s.target().lineHeight(3) : 22;
  text(s, rect(r.x + 7, r.y + 4, r.width - 14, headingH), w.label, 3, inverse, fui::TextAlign::Center);
  const int imageHeight = std::max(28, std::min((int(r.width) - 12) / 2 + 10, int(r.height) - 124) - (headingH - 22));
  const auto image = rect(r.x + 6, r.y + headingH + 7, r.width - 12, imageHeight);
  if (owned)
    vignette(s, m, image, id, false, CONTROL_RADIUS - 6);
  else {
    s.target().stroke(image, ink, BORDER, CONTROL_RADIUS - 6);
    icon(s, rect(image.x + (image.width - 32) / 2, image.y + (image.height - 32) / 2, 32, 32),
         memoryUnrecorded(*m.profile, id) ? extra_information_32 : extra_lock_32);
  }
  if (owned)
    memoryReading(w.label, sizeof(w.label), *m.profile, id);
  else
    std::snprintf(
        w.label, sizeof(w.label), "%s",
        memoryUnrecorded(*m.profile, id) ? tr(STR_PET_MEMORY_TIME_UNKNOWN) : tr(STR_PET_MEMORY_PENDING_SHORT));
  const int minutesY = compact ? r.bottom() - 27 : image.bottom() + 8;
  text(s, rect(r.x + 8, minutesY, r.width - 16, 21), w.label, 3, inverse, fui::TextAlign::Center);
  const int titleY = compact ? image.bottom() + 4 : minutesY + 31;
  const int titleBottom = compact ? minutesY - 5 : r.bottom() - 8;
  caption(s, rect(r.x + 8, titleY, r.width - 16, titleBottom - titleY), memoryTitle(id), 3, inverse);
}
inline void sectionHead(toybox::Screen& s, Work& w, fui::Rect r, const char* title, fui::ActionId action, int font = 0,
                        const char* actionLabel = nullptr) {
  if (!actionLabel) actionLabel = tr(STR_PET_VIEW_ALL);
  if (r.width < 380) {
    text(s, rect(r.x + 10, r.y + 6, r.width - 20, 22), title, 3);
    const auto control = rect(r.x + 10, r.y + 34, r.width - 20, 32);
    const bool inverse = button(s, w, control, "", action);
    text(s, rect(control.x + 8, control.y + 4, control.width - 38, 24), actionLabel, 3, inverse,
         fui::TextAlign::Center);
    chevron(s, rect(control.right() - 23, control.y + 9, 8, 14), inverse);
    return;
  }
  pixelui::copyCaps(w.label, sizeof(w.label), actionLabel);
  const int control = std::min(int(r.width) / 2, std::max(126, int(s.target().measureText(3, w.label, {}).width) + 40));
  pixelui::copyCaps(w.secondary, sizeof(w.secondary), title);
  for (const int candidate : {font, 0, 3}) {
    font = candidate;
    if (s.target().measureText(font, w.secondary, {}).width <= r.width - control - 28) break;
  }
  text(s, rect(r.x + 9, r.y + 10, r.width - control - 28, 28), title, font);
  const bool inverse = button(s, w, rect(r.right() - control - 8, r.y + 8, control, 32), "", action);
  text(s, rect(r.right() - control - 4, r.y + 13, control - 27, 22), actionLabel, 3, inverse, fui::TextAlign::Center);
  chevron(s, rect(r.right() - 25, r.y + 17, 8, 14), inverse);
}
inline void stages(toybox::Screen& s, const Model& m, fui::Rect r) {
  actionsurface::panel(s, r);
  text(s, rect(r.x + 10, r.y + 10, r.width - 20, 28), tr(STR_PET_LIFE_STAGES), 0);
  const auto row = rect(r.x + 8, r.y + 42, r.width - 16, r.height - 50);
  const unsigned current = unsigned(pet::stage(m.profile->readingSeconds));
  constexpr int gap = 12;
  const int available = std::max(0, int(row.width) - gap * 4);
  constexpr int labelH = 72;
  fui::FontId labelFont = 4;
  for (unsigned i = 0; i < 5 && labelFont == 4; ++i) {
    const int width = available * int(i + 1) / 5 - available * int(i) / 5 + 10;
    fui::TextStyle style;
    style.font = 4;
    style.bold = i == current;
    const char* start = stageName(pet::Stage(i));
    while (*start && labelFont == 4) {
      const char* end = std::strchr(start, ' ');
      const size_t length = end ? size_t(end - start) : std::strlen(start);
      char word[48];
      if (length >= sizeof(word)) {
        labelFont = 3;
        break;
      }
      std::memcpy(word, start, length);
      word[length] = '\0';
      if (s.target().measureText(4, word, style).width > width || s.target().lineHeight(4) * 2 > labelH) labelFont = 3;
      start += length + (end ? 1 : 0);
    }
  }
  for (unsigned i = 0; i < 5; ++i) {
    const int x = row.x + available * int(i) / 5 + gap * int(i);
    const int width = available * int(i + 1) / 5 - available * int(i) / 5;
    const auto box = rect(x, row.y, width, row.height);
    const bool locked = i > current;
    const int imageW = std::min(72, int(box.width) - 2);
    const auto image =
        rect(box.x + (box.width - imageW) / 2, box.y + 8, imageW, std::max(28, int(box.height) - labelH - 20));
    if (locked) {
      const auto lock = rect(image.x + (image.width - 28) / 2, image.y + std::max(0, (image.height - 56) / 2), 28, 32);
      icon(s, lock, referenceicons::LOCK);
      char remaining[32];
      const auto seconds = pet::stageStart(pet::Stage(i)) - m.profile->readingSeconds;
      std::snprintf(remaining, sizeof(remaining), tr(STR_PET_STAGE_MINUTES),
                    static_cast<unsigned long long>(seconds / 60 + unsigned(seconds % 60 != 0)));
      text(s, rect(box.x, lock.bottom() + 5, box.width, 22), remaining, 3, false, fui::TextAlign::Center);
    } else
      art(s, m, image, i ? 3 + unsigned(m.profile->species) * 16 + (i - 1) * 4 : unsigned(m.profile->species),
          speciesIcon(m.profile->species), false);
    const auto label = rect(box.x - 5, box.bottom() - labelH, box.width + 10, labelH);
    caption(s, label, stageName(pet::Stage(i)), 2, false, labelFont, i == current && labelFont == 4);
    if (i == current && labelFont == 3)
      s.target().fill(rect(label.x + label.width / 4, label.bottom() - 4, label.width / 2, 2), ink);
    if (i < 4) {
      arrowRight(s, box.right(), image.y + image.height / 2, gap);
    }
  }
}
}  // namespace albumwidget
}  // namespace petui

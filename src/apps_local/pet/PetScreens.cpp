#include <algorithm>
#include <cstdio>

#include "../ui/RequiredResources.h"
#include "PetCareAdvice.h"
#include "PetCareIcons.h"
#include "PetCelebration.h"
#include "PetHeader.h"
#include "PetOverview.h"
#include "PetPalette.h"
#include "PetPolishIcons.h"

namespace petui {
const char* playBlockText(pet::PlayBlock reason) {
  switch (reason) {
    case pet::PlayBlock::None:
      return "";
    case pet::PlayBlock::Unavailable:
      return tr(STR_PET_EGG_HEALTH);
    case pet::PlayBlock::Sick:
      return tr(STR_PET_PLAY_SICK);
    case pet::PlayBlock::Sleeping:
      return tr(STR_PET_WAKE_FIRST);
    case pet::PlayBlock::Hungry:
      return tr(STR_PET_PLAY_HUNGRY);
    case pet::PlayBlock::Tired:
      return tr(STR_PET_PLAY_TIRED);
  }
  return "";
}
using namespace widget;
const char* eggName(pet::Species value) {
  static constexpr StrId keys[] = {StrId::STR_PET_EGG_A, StrId::STR_PET_EGG_B, StrId::STR_PET_EGG_C};
  return I18N.get(keys[unsigned(value)]);
}
const char* displayName(const char* name, pet::Species species) { return name[0] ? name : eggName(species); }
const char* stageName(pet::Stage value) {
  static constexpr StrId keys[] = {StrId::STR_PET_EGG, StrId::STR_PET_NEWBORN, StrId::STR_PET_BABY,
                                   StrId::STR_PET_JUVENILE, StrId::STR_PET_ADULT};
  return I18N.get(keys[unsigned(value)]);
}
const char* modeName(pet::Mode value) {
  static constexpr StrId keys[] = {StrId::STR_PET_RELAXED, StrId::STR_PET_NORMAL, StrId::STR_PET_HARD,
                                   StrId::STR_PET_EXTREME};
  return I18N.get(keys[unsigned(value)]);
}
const char* objectName(unsigned value) {
  return value < pet::OBJECT_COUNT ? I18N.get(OBJECT_ART[value].name) : tr(STR_PET_EMPTY_SLOT);
}
namespace {
bool hasTabs(pet::View view) {
  switch (view) {
    case pet::View::Dashboard:
    case pet::View::Care:
    case pet::View::Home:
    case pet::View::Album:
    case pet::View::Projects:
    case pet::View::Games:
    case pet::View::Play:
    case pet::View::Lives:
    case pet::View::Health:
    case pet::View::LifeDetail:
      return true;
    case pet::View::Affection:
    case pet::View::Memory:
    case pet::View::Memories:
    case pet::View::ReadingInfo:
    case pet::View::Collection:
    case pet::View::Death:
    case pet::View::Name:
    case pet::View::New:
    case pet::View::Info:
    case pet::View::Rules:
    case pet::View::Settings:
    case pet::View::SettingsConfirm:
    case pet::View::Reward:
    case pet::View::Evolution:
    case pet::View::Notice:
    case pet::View::Retire:
    case pet::View::ProjectPreview:
    case pet::View::HealthHelp:
      return false;
  }
  return false;
}
void tabs(toybox::Screen& s, Work& w, const Model& m) {
  static constexpr StrId main[] = {StrId::STR_PET_APP, StrId::STR_PET_CARE, StrId::STR_PET_HOME, StrId::STR_PET_ALBUM};
  static constexpr StrId first[] = {StrId::STR_PET_NEW, StrId::STR_PET_INFO, StrId::STR_PET_RULES};
  const bool onboarding = m.view == pet::View::New || m.view == pet::View::Info || m.view == pet::View::Rules;
  const int count = onboarding ? 3 : 4;
  const int selected = onboarding ? (m.view == pet::View::New ? 0 : m.view == pet::View::Info ? 1 : 2) : int(m.tab);
  for (int i = 0; i < count; ++i) {
    pixelui::copyCaps(w.tabLabels[i], sizeof(w.tabLabels[i]), I18N.get(onboarding ? first[i] : main[i]));
    w.tabs[i] = {};
    w.tabs[i].label = w.tabLabels[i];
    w.tabs[i].value = int16_t(i);
    w.tabs[i].selected = i == selected;
  }
  w.tabBar = {};
  w.tabBar.tabs = w.tabs;
  w.tabBar.count = uint8_t(count);
  w.tabBar.action = onboarding ? SetupTab : ChangeTab;
  w.tabBar.inputMask = fui::InputTouch;
  w.tabBar.text.font = 0;
  w.tabBar.text.align = fui::TextAlign::Center;
  w.tabBar.tabStyles = pixelui::rowStyles();
  w.tabBar.tabInset = {};
  w.tabBar.contentInset = {};
  w.tabBar.minTouchSize = 0;
  const auto band = s.takeTop(46, GAP);
  fui::tabBar(s.frame(), band, w.tabBar);

  frame(s, band);
}
}  // namespace
void dashboard(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, Palette* palette) {
  const auto& p = *m.profile;
  const bool wide = r.width > r.height;
  const bool ended = p.lifeStatus != pet::LifeStatus::Active;
  const bool urgent = readingUrgent(m);
  if (wide) {
    const int left = r.width * 44 / 100;
    if (urgent) readingLife(s, m, w, rect(r.x, r.y, left, 54), true);
    hero(s, m, w, rect(r.x, r.y + (urgent ? 62 : 0), left, r.height - (urgent ? 62 : 0)), false, palette);
    r = rect(r.x + left + GAP, r.y, r.width - left - GAP, r.height);
  } else if (urgent) {
    readingLife(s, m, w, rect(r.x, r.y, r.width, 54), true);
    r.y += 62;
    r.height -= 62;
  }
  const int metricsH = ended ? (wide ? 76 : 84) : (wide ? 142 : 136);
  const int actionsH = ended ? (wide ? 64 : 68) : 0, footerH = urgent ? 0 : 54;
  const int projectH = wide ? 106 : ended ? 122 : 132;
  const int heroH = r.height - metricsH - projectH - actionsH - footerH - GAP * (ended ? 4 : urgent ? 2 : 3);
  if (!wide) {
    hero(s, m, w, rect(r.x, r.y, r.width, heroH), false, palette);
    r.y += heroH + GAP;
    r.height -= heroH + GAP;
  }
  const auto metrics = rect(r.x, r.y, r.width, metricsH);
  const bool locked = pet::stage(p.readingSeconds) == pet::Stage::Egg;
  if (ended) {
    frame(s, metrics);
    text(s, rect(metrics.x + 12, metrics.y + 5, metrics.width - 24, 24), tr(STR_PET_LIFE_ENDED), 0);
    duration(w.label, sizeof(w.label), p.readingSeconds);
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_LIFE_SUMMARY), w.label, unsigned(p.sharedDays));
    prose(s, rect(metrics.x + 12, metrics.y + 34, metrics.width - 24, metrics.height - 40), w.secondary, 2);
  } else {
    metric(s, w, cell(metrics, 0, 4), tr(STR_PET_SACIETY), p.saciety, icon_cat_food_32, locked);
    metric(s, w, cell(metrics, 1, 4), tr(STR_PET_JOY), p.joy, icon_hearts_32, locked);
    metric(s, w, cell(metrics, 2, 4), tr(STR_PET_ENERGY), p.energy, extra_power_64, locked);
    metric(s, w, cell(metrics, 3, 4), tr(STR_PET_HYGIENE), p.hygiene, SOAP_ICON, locked);
  }
  const auto goal = rect(r.x, metrics.bottom() + GAP, r.width, projectH);
  if (ended && pet::collectionComplete(p))
    collection(s, m, w, goal);
  else if (ended)
    project(s, m, w, goal);
  else {
    objective(s, m, w, cell(goal, 0, 2));
    daily(s, m, w, cell(goal, 1, 2));
  }
  int footerY = goal.bottom() + GAP;
  if (ended) {
    const auto actions = rect(r.x, footerY, r.width, actionsH);
    button(s, w, cell(actions, 0, 2), tr(STR_PET_NEW_COMPANION), NewCompanion);
    button(s, w, cell(actions, 1, 2), tr(STR_PET_ALBUM), OpenLives, 0, false, &extra_reading_book_32);
    footerY = actions.bottom() + GAP;
  }
  if (!urgent)
    button(s, w, rect(r.x, footerY, r.width, footerH), tr(STR_PET_CONTINUE_READING), Read, 0, true,
           &extra_reading_book_64, !m.loading);
}
namespace {
struct CarePanel {
  fui::Rect copy;
  bool inverse;
};
CarePanel carePanel(toybox::Screen& s, Work& w, fui::Rect box, const char* title, fui::ActionId action,
                    const freeink::Icon& mark, bool enabled, int value = 0) {
  const bool inverse = button(s, w, box, "", action, value, false, nullptr, enabled);
  const int markWidth = box.width < 300 ? 62 : 78;
  const int side = std::min(int(box.height) - 14, markWidth - 10), divider = box.x + markWidth;
  icon(s, rect(box.x + (markWidth - side) / 2, box.y + (box.height - side) / 2, side, side), mark, inverse);
  s.target().line({int16_t(divider), int16_t(box.y + 7)}, {int16_t(divider), int16_t(box.bottom() - 7)}, 1,
                  inverse ? paper : ink);
  chevron(s, rect(box.right() - 22, box.y + (box.height - 18) / 2, 10, 18), inverse);
  if (box.width >= 400) {
    const int detail = divider + 156;
    text(s, rect(divider + 10, box.y + 8, 136, box.height - 16), title, 0, inverse);
    return {rect(detail + 12, box.y + 7, box.right() - detail - 46, box.height - 14), inverse};
  }
  text(s, rect(divider + 10, box.y + 3, box.width - markWidth - 42, 25), title, 0, inverse);
  return {rect(divider + 10, box.y + 29, box.width - markWidth - 42, box.height - 34), inverse};
}
void careBenefit(toybox::Screen& s, fui::Rect box, const char* label, const freeink::Icon* mark, bool inverse) {
  if (mark) {
    icon(s, rect(box.x + 4, box.y + (box.height - 18) / 2, 18, 18), *mark, inverse);
    box.x += 26;
    box.width -= 29;
  } else {
    box.x += 2;
    box.width -= 4;
  }
  prose(s, box, label, 1, fui::TextAlign::Left, inverse);
}
}  // namespace
void care(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool wide = r.width > r.height;
  if (p.lifeStatus != pet::LifeStatus::Active) {
    frame(s, r);
    const auto banner = rect(r.x + 12, r.y + 12, r.width - 24, wide ? 48 : 62);
    celebrationBanner(s, banner, tr(STR_PET_CARE_CLOSED), icon_hearts_32, true, false);
    const auto actions = rect(r.x + 12, r.bottom() - 72, r.width - 24, 60);
    const auto copy =
        wide ? rect(r.x + r.width / 2, banner.bottom() + 18, r.width / 2 - 24, actions.y - banner.bottom() - 36)
             : rect(r.x + 24, banner.bottom() + 26, r.width - 48, 104);
    prose(s, copy, tr(STR_PET_CARE_CLOSED_HELP), 4, fui::TextAlign::Center);
    const auto picture = wide ? rect(r.x + 30, banner.bottom() + 10, r.width / 2 - 60, actions.y - banner.bottom() - 22)
                              : rect(r.x + 38, copy.bottom() + 12, r.width - 76, actions.y - copy.bottom() - 38);
    const auto age = pet::stage(p.readingSeconds);
    const unsigned sleeping = age == pet::Stage::Egg
                                  ? petsleep::eggIndex(unsigned(p.species), 0)
                                  : ((unsigned(p.species) * 4 + unsigned(age) - 1) * petsleep::FRAMES) * 2;
    if (!m.sleepArt.draw || !m.sleepArt.draw(m.sleepArt.context, s.target(), picture, sleeping + 1, false))
      art(s, m, picture,
          age == pet::Stage::Egg ? unsigned(p.species) : 3 + unsigned(p.species) * 16 + (unsigned(age) - 1) * 4 + 3,
          speciesIcon(p.species), false);
    sparkle(s, picture.x + 12, picture.y + picture.height / 3, 18);
    sparkle(s, picture.right() - 12, picture.y + 14, 15);
    const int lineY = actions.y - 14, center = r.x + r.width / 2;
    s.target().line({int16_t(r.x + 18), int16_t(lineY)}, {int16_t(center - 14), int16_t(lineY)}, 2, ink);
    s.target().line({int16_t(center + 14), int16_t(lineY)}, {int16_t(r.right() - 18), int16_t(lineY)}, 2, ink);
    sparkle(s, center, lineY, 16);
    button(s, w, cell(actions, 0, 2), tr(STR_PET_GENERATIONS_SHORT), OpenLives, 0, false, &extra_reading_book_32);
    button(s, w, cell(actions, 1, 2), tr(STR_PET_BACK_PET), ChangeTab, 0, true);
    return;
  }
  const bool hatched = pet::canCare(p), canWrite = hatched && m.writable;
  const int leftW = wide ? r.width * 34 / 100 : r.width;
  const auto stock = rect(r.x, r.y, leftW, wide ? 76 : 54);
  frame(s, stock);
  for (int i = 0; i < 2; ++i) {
    const auto entry = wide ? rect(stock.x + 8, stock.y + 6 + i * 34, stock.width - 16, 30)
                            : cell(rect(stock.x + 8, stock.y + 10, stock.width - 16, 34), i, 2, 8);
    icon(s, rect(entry.x, entry.y + 1, 28, 28), i ? careicons::REMEDY : careicons::FOOD);
    std::snprintf(w.label, sizeof(w.label), i ? tr(STR_PET_MEDICINE_STOCK) : tr(STR_PET_FOOD_STOCK),
                  static_cast<unsigned long long>(i ? pet::medicine(p) : pet::food(p)));
    const auto copy = rect(entry.x + 34, entry.y, entry.width - 36, entry.height);
    pixelui::copyCaps(w.secondary, sizeof(w.secondary), w.label);
    const int font = s.target().measureText(0, w.secondary, {}).width <= copy.width ? 0 : 3;
    text(s, copy, w.secondary, font);
  }
  const int metricH = wide ? std::min(60, (int(r.height) - stock.height - GAP * 3) / 3) : 76;
  auto metrics = rect(r.x, stock.bottom() + GAP, leftW, wide ? metricH * 3 + GAP * 2 : metricH);
  auto area = wide ? rect(stock.right() + 16, r.y, r.right() - stock.right() - 16, r.height)
                   : rect(r.x, metrics.bottom() + 12, r.width, r.bottom() - metrics.bottom() - 12);
  const int hygieneW = (area.width - GAP * 2) / 3;
  const auto hygiene = rect(area.x, area.y, hygieneW, metricH);
  const auto note = rect(hygiene.right() + GAP, area.y, area.right() - hygiene.right() - GAP, metricH);
  const int top = note.bottom() + GAP;
  const auto actions = rect(area.x, top, area.width, r.bottom() - top);
  const int height = (actions.height - GAP * (wide ? 2 : 4)) / (wide ? 3 : 5);
  const auto healthBox = rect(actions.x, actions.y, actions.width, height);
  const auto first = rect(actions.x, healthBox.bottom() + GAP, actions.width, height);
  if (wide) {
    metric(s, w, rect(metrics.x, metrics.y, metrics.width, metricH), tr(STR_PET_SACIETY), p.saciety,
           careicons::SACIETY);
    metric(s, w, rect(metrics.x, metrics.y + metricH + GAP, metrics.width, metricH), tr(STR_PET_JOY), p.joy,
           careicons::JOY);
    metric(s, w, rect(metrics.x, metrics.y + (metricH + GAP) * 2, metrics.width, metricH), tr(STR_PET_ENERGY), p.energy,
           careicons::ENERGY);
  } else {
    metric(s, w, cell(metrics, 0, 3), tr(STR_PET_SACIETY), p.saciety, careicons::SACIETY);
    metric(s, w, cell(metrics, 1, 3), tr(STR_PET_JOY), p.joy, careicons::JOY);
    metric(s, w, cell(metrics, 2, 3), tr(STR_PET_ENERGY), p.energy, careicons::ENERGY);
  }
  metric(s, w, hygiene, tr(STR_PET_HYGIENE), p.hygiene, careicons::HYGIENE);
  const bool treatment = p.illness != pet::Illness::Healthy;
  const auto noteAction = treatment || advisedCare(p, m.today) == OpenHealth ? OpenHealth : fui::NO_ACTION;
  const bool noteInverse = button(s, w, note, "", noteAction, 0, false, nullptr, noteAction != fui::NO_ACTION);
  const int markWidth = 40;
  s.target().fill(rect(note.x, note.y, markWidth, note.height), noteInverse ? paper : ink);
  icon(s, rect(note.x + (markWidth - 28) / 2, note.y + (note.height - 28) / 2, 28, 28), READING_INFO_ICON,
       !noteInverse);
  if (p.resting) {
    const auto remaining = restRemaining(m);
    if (remaining > 0) {
      remainingDuration(w.label, sizeof(w.label), uint64_t(remaining));
      std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_REST_ENDS), w.label);
    } else if (!remaining) {
      std::snprintf(w.secondary, sizeof(w.secondary), "%s", tr(STR_PET_REST_COMPLETE));
    } else {
      std::snprintf(w.secondary, sizeof(w.secondary), "%s", tr(STR_PET_REST_TIME_UNKNOWN));
    }
    if (p.illness != pet::Illness::Healthy)
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_REST_STATUS), tr(STR_PET_NEEDS_REMEDY), w.secondary);
    else
      std::snprintf(w.label, sizeof(w.label), "%s", w.secondary);
  }
  prose(s, rect(note.x + markWidth + 12, note.y + 6, note.width - markWidth - 42, note.height - 12),
        p.resting ? w.label : careAdvice(p), 3, fui::TextAlign::Left, noteInverse);
  if (noteAction != fui::NO_ACTION)
    chevron(s, rect(note.right() - 19, note.y + (note.height - 16) / 2, 7, 16), noteInverse);
  const auto health = carePanel(s, w, healthBox, tr(STR_PET_HEALTH_SHORT), OpenHealth, careicons::HEALTH, true);
  static constexpr StrId conditions[] = {StrId::STR_PET_HEALTHY, StrId::STR_PET_COLD, StrId::STR_PET_MALAISE};
  prose(s, health.copy, I18N.get(conditions[unsigned(p.illness)]), 1, fui::TextAlign::Left, health.inverse);
  const auto feed = carePanel(s, w, wide ? cell(first, 0, 2) : first, tr(STR_PET_FEED), Feed, careicons::FEED,
                              canWrite && pet::food(p) && p.saciety < 100, p.foodChoice);
  std::snprintf(w.label, sizeof(w.label), wide ? tr(STR_PET_FEED_CHANGE_SHORT) : tr(STR_PET_FEED_CHANGE),
                unsigned(p.saciety), std::min(100u, unsigned(p.saciety) + 25));
  if (p.saciety == 100) std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_ALREADY_FULL));
  const bool noFood = p.saciety < 100 && !pet::food(p);
  const int benefitH = wide ? (height - 34) / 2 : feed.copy.height / 2;
  const auto feedText = rect(feed.copy.x, feed.copy.y, feed.copy.width, noFood ? feed.copy.height : benefitH);
  prose(s, feedText, noFood ? tr(STR_PET_CARE_ADVICE_FOOD) : w.label, 2, fui::TextAlign::Left, feed.inverse);
  if (!noFood)
    careBenefit(s, rect(feed.copy.x, feed.copy.bottom() - benefitH, feed.copy.width, benefitH),
                p.saciety == 100 ? tr(STR_PET_FOOD_NO_COST) : tr(STR_PET_FOOD_COST), nullptr, feed.inverse);
  const auto stroke = carePanel(
      s, w, wide ? cell(first, 1, 2) : rect(first.x, first.bottom() + GAP, first.width, height), tr(STR_PET_STROKE),
      Stroke, careicons::AFFECTION, hatched && (m.writable || m.today <= 0 || m.today <= p.lastPetDay));
  if (m.writable && m.today > 0 && m.today > p.lastPetDay && p.joy < 100)
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_GAME_GAIN_VALUE),
                  std::min(pet::moodGain(p), 100u - p.joy));
  else
    std::snprintf(w.secondary, sizeof(w.secondary), "%s", tr(STR_PET_CARE_VISIT_ONLY));
  prose(s, stroke.copy, w.secondary, 2, fui::TextAlign::Left, stroke.inverse);
  const auto last = rect(first.x, first.bottom() + (wide ? GAP : height + GAP * 2), first.width, height);
  const auto rest = carePanel(s, w, wide ? cell(last, 0, 2) : last, p.resting ? tr(STR_PET_WAKE) : tr(STR_PET_REST),
                              Rest, careicons::REST, canWrite);
  if (p.resting) {
    prose(s, rest.copy, tr(STR_PET_CARE_WAKE_EFFECT), 2, fui::TextAlign::Left, rest.inverse);
  } else {
    careBenefit(s, rect(rest.copy.x, rest.copy.y, rest.copy.width, benefitH),
                wide ? tr(STR_PET_REST_BENEFIT_SHORT) : tr(STR_PET_REST_BENEFIT), nullptr, rest.inverse);
    careBenefit(s, rect(rest.copy.x, rest.copy.bottom() - benefitH, rest.copy.width, benefitH),
                tr(STR_PET_CARE_REST_EFFECT), nullptr, rest.inverse);
  }
  const auto games = carePanel(s, w, wide ? cell(last, 1, 2) : rect(last.x, last.bottom() + GAP, last.width, height),
                               tr(STR_PET_GAMES_SHORT), OpenGames, careicons::GAMES, hatched);
  switch (gameOffer(m)) {
    case GameOffer::Blocked:
      std::snprintf(w.secondary, sizeof(w.secondary), "%s", gameCareBlock(p));
      break;
    case GameOffer::Reward: {
      const unsigned low = std::min(pet::gameMoodGain(p, pet::Game::Pairs), 100u - p.joy);
      const unsigned high = std::min(pet::gameMoodGain(p, pet::Game::Puzzle), 100u - p.joy);
      if (low == high)
        std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_GAME_GAIN_VALUE), low);
      else
        std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_GAME_GAIN_RANGE), low, high);
      careBenefit(s, rect(games.copy.x, games.copy.y, games.copy.width, benefitH), w.secondary, nullptr, games.inverse);
      careBenefit(s, rect(games.copy.x, games.copy.bottom() - benefitH, games.copy.width, benefitH),
                  tr(STR_PET_GAME_COST), nullptr, games.inverse);
      return;
    }
    case GameOffer::Practice:
      std::snprintf(w.secondary, sizeof(w.secondary), "%s", tr(STR_PET_CARE_GAME_PRACTICE));
      break;
  }
  prose(s, games.copy, w.secondary, 3, fui::TextAlign::Left, games.inverse);
}
void home(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, Palette* palette) {
  const auto& p = *m.profile;
  const bool wide = r.width > r.height;
  if (wide) {
    const int left = r.width * 44 / 100;
    room(s, m, w, rect(r.x, r.y, left, r.height), palette);
    r = rect(r.x + left + GAP, r.y, r.width - left - GAP, r.height);
  } else {
    const int height = r.height - HOME_SLOTS_H - HOME_PROJECT_H - 54 - GAP * 3;
    room(s, m, w, rect(r.x, r.y, r.width, height), palette);
    r.y += height + GAP;
    r.height -= height + GAP;
  }
  const bool unlocked = pet::homeUnlocked(p);
  const auto slots = rect(r.x, r.y, r.width, wide ? 78 : HOME_SLOTS_H);
  static constexpr StrId names[] = {StrId::STR_PET_REST_SLOT, StrId::STR_PET_DECOR_SLOT, StrId::STR_PET_GAME_SLOT,
                                    StrId::STR_PET_SCENE_SLOT};
  static constexpr const freeink::Icon* empty[] = {&referenceicons::EMPTY_REST, &referenceicons::EMPTY_DECOR,
                                                   &referenceicons::EMPTY_GAME, &referenceicons::EMPTY_SCENE};
  bool decorated = false;
  for (unsigned i = 0; i < pet::SLOT_COUNT; ++i) {
    const auto box = cell(slots, int(i), 4, 6);
    const bool inverse = button(s, w, box, "", OpenProjects, int(i), false, nullptr, unlocked);
    const auto preview = rect(box.x + 8, box.y + 7, box.width - 16, box.height - 39);
    if (!unlocked)
      icon(s, preview, extra_lock_32);
    else if (p.equipped[i] < pet::OBJECT_COUNT) {
      object(s, m, preview, p.equipped[i], inverse);
      decorated = true;
    } else
      icon(s, preview, *empty[i], inverse);
    text(s, rect(box.x + 3, box.bottom() - 26, box.width - 6, 22), I18N.get(names[i]), 3, inverse,
         fui::TextAlign::Center);
  }
  const auto goal = rect(r.x, slots.bottom() + GAP, r.width, wide ? r.bottom() - slots.bottom() - 70 : HOME_PROJECT_H);
  if (unlocked && pet::collectionComplete(p) && decorated)
    collection(s, m, w, goal);
  else if (unlocked)
    project(s, m, w, goal);
  else {
    frame(s, goal);
    text(s, rect(goal.x + 10, goal.y + 5, goal.width - 20, 27), tr(STR_PET_NEXT_GOAL), 1);
    icon(s, rect(goal.x + 14, goal.y + 43, 40, 40), extra_lock_32);
    prose(s, rect(goal.x + 66, goal.y + 39, goal.width - 80, goal.height - 49), tr(STR_PET_HOME_LOCKED), 3);
  }
  button(s, w, rect(r.x, goal.bottom() + GAP, r.width, 54), tr(STR_PET_DECORATE_HOME), OpenProjects,
         pet::collectionComplete(p) ? int(pet::objectSlot(p.project)) : 4, true,
         unlocked ? &extra_dashboard_add_64 : &extra_lock_32, unlocked);
}
void build(toybox::Screen& s, const Model& m, Work& w, Palette* palette) {
  pixelui::begin(s);
  pixelui::Header header;
  header.title = !m.resourcesReady                                                       ? tr(STR_PET_APP)
                 : m.view == pet::View::Settings || m.view == pet::View::SettingsConfirm ? tr(STR_PET_SETTINGS)
                 : m.view == pet::View::ProjectPreview                                   ? tr(STR_PET_PREVIEW)
                 : m.view == pet::View::Reward                                           ? tr(STR_PET_REWARD)
                 : m.view == pet::View::Evolution                                        ? tr(STR_PET_EVOLUTION)
                                                                                         : tr(STR_PET_APP);
  header.back = true;
  if (m.resourcesReady && (m.view == pet::View::Dashboard || m.view == pet::View::Care || m.view == pet::View::Home ||
                           m.view == pet::View::Album || m.view == pet::View::Affection || m.view == pet::View::Games ||
                           m.view == pet::View::Play)) {
    header.mark = fui::bitmapFromIcon(icon_go_settings_32);
    header.markAction = OpenSettings;
    header.secondaryMark = fui::bitmapFromIcon(READING_INFO_ICON);
    header.secondaryMarkAction = OpenReadingInfo;
    header.actionInputMask = fui::InputDefault;
    header.compactPercent = true;
  }
  pixelui::header(s, header);
  if (!m.resourcesReady) {
    requiredresources::build(s, "/apps/pet/", Back, w.button);
    return;
  }
  if (hasTabs(m.view) || m.view == pet::View::New || m.view == pet::View::Info || m.view == pet::View::Rules)
    tabs(s, w, m);
  auto body = s.body();
  if (m.loading) {
    info(s, rect(body.x, body.y + body.height / 3, body.width, 88), tr(STR_PET_LOADING));
    return;
  }
  if (m.profile->lifeStatus == pet::LifeStatus::Active && pet::stage(m.profile->readingSeconds) == pet::Stage::Egg &&
      (m.view == pet::View::Care || m.view == pet::View::Health || m.view == pet::View::Games ||
       m.view == pet::View::Play)) {
    incubation(s, m, w, body);
    return;
  }
  switch (m.view) {
    case pet::View::ReadingInfo:
      readingInfo(s, m, w, body);
      break;
    case pet::View::Memories:
    case pet::View::Memory:
      memoryDetails(s, m, w, body);
      break;
    case pet::View::Affection:
      affectionWindow(s, m, w, body);
      break;
    case pet::View::Collection:
      collectionWindow(s, m, w, body);
      break;
    case pet::View::Death:
      deathWindow(s, m, w, body);
      break;
    case pet::View::Name:
      naming(s, m, w, body);
      break;
    case pet::View::Dashboard:
      dashboard(s, m, w, body, palette);
      break;
    case pet::View::Care:
      care(s, m, w, body);
      break;
    case pet::View::Home:
      home(s, m, w, body, palette);
      break;
    case pet::View::Album:
      album(s, m, w, body);
      break;
    case pet::View::New:
    case pet::View::Info:
    case pet::View::Rules:
      setup(s, m, w, body);
      break;
    case pet::View::Settings:
    case pet::View::SettingsConfirm:
      settings(s, m, w, body);
      break;
    case pet::View::Health:
    case pet::View::HealthHelp:
    case pet::View::LifeDetail:
    case pet::View::Retire:
    case pet::View::Projects:
    case pet::View::ProjectPreview:
    case pet::View::Games:
    case pet::View::Play:
    case pet::View::Reward:
    case pet::View::Evolution:
    case pet::View::Lives:
    case pet::View::Notice:
      details(s, m, w, body);
      break;
  }
}
void render(fui::DrawTarget& target, const fui::DeviceContext& device, const fui::InputSnapshot& input,
            toybox::Interactions& interactions, toybox::HeaderStatus status, const Model& m, Work& w) {
  Palette palette(target, false);
  palette.fill(device.screen(), paper);
  toybox::Frame frame(palette, device, input, interactions, nullptr, status);
  toybox::Screen screen(frame);
  build(screen, m, w, &palette);
}
}  // namespace petui

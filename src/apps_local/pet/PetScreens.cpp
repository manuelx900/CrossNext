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
#include "PetSimpleScreens.h"

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
  return pet::objectAvailable(value) ? I18N.get(OBJECT_ART[value].name) : tr(STR_PET_EMPTY_SLOT);
}
namespace {
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
  w.tabBar.tabStyles = chrome::lightHeader ? lightui::tabStyles() : pixelui::rowStyles();
  w.tabBar.tabInset = {};
  w.tabBar.contentInset = {};
  w.tabBar.minTouchSize = 0;
  const auto band = s.takeTop(46, GAP);
  for (int i = 0; i < count; ++i)
    fittext::fit(s.target(), rect(0, 0, band.width / count - 12, band.height - 4), w.tabs[i].label, w.tabBar.text, 2);
  if (chrome::lightHeader)
    lightui::tabs(s.frame(), band, w.tabBar);
  else {
    fui::tabBar(s.frame(), band, w.tabBar);
    frame(s, band);
  }
}
}  // namespace
void dashboard(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, Palette* palette) {
  if (m.profile->lifeStatus == pet::LifeStatus::Active) {
    simple::dashboard(s, m, w, r);
    return;
  }
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
  const int metricsH = ended ? (wide ? 76 : 84) : (wide ? 108 : 136);
  const int actionsH = ended ? (wide ? 64 : 68) : 0, footerH = urgent ? 0 : 54;
  const int projectH = wide ? (ended ? 106 : 140) : ended ? 122 : 140;
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
    metric(s, w, cell(metrics, 0, 4), tr(STR_PET_FULLNESS), p.saciety, icon_cat_food_32, locked);
    metric(s, w, cell(metrics, 1, 4), tr(STR_PET_JOY), p.joy, icon_hearts_32, locked);
    metric(s, w, cell(metrics, 2, 4), tr(STR_PET_ENERGY), p.energy, extra_power_64, locked);
    metric(s, w, cell(metrics, 3, 4), tr(STR_PET_HYGIENE), p.hygiene, SOAP_ICON, locked);
  }
  const auto goal = rect(r.x, metrics.bottom() + GAP, r.width, projectH);
  if (ended && pet::collectionComplete(p))
    collection(s, m, w, goal);
  else if (ended)
    project(s, m, w, goal);
  else
    objective(s, m, w, goal);
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
void care(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  if (m.profile->lifeStatus == pet::LifeStatus::Active) {
    simple::care(s, m, w, r);
    return;
  }
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
}

void home(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, Palette* palette) {
  const bool wide = r.width > r.height;
  if (wide) {
    const int left = r.width * 44 / 100;
    widget::room(s, m, w, rect(r.x, r.y, left, r.height), palette);
    r = rect(r.x + left + GAP, r.y, r.width - left - GAP, r.height);
  } else {
    const int height = r.height - HOME_SLOTS_H - HOME_PROJECT_H - 54 - GAP * 3;
    widget::room(s, m, w, rect(r.x, r.y, r.width, height), palette);
    r.y += height + GAP;
    r.height -= height + GAP;
  }
  homeControls(s, m, w, r, wide);
}
void homeControls(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, bool wide) {
  const auto& p = *m.profile;
  const bool unlocked = pet::homeUnlocked(p);
  const auto slots = rect(r.x, r.y, r.width, wide ? 78 : HOME_SLOTS_H);
  static constexpr StrId names[] = {StrId::STR_PET_REST_SLOT, StrId::STR_PET_DECOR_SLOT, StrId::STR_PET_GAME_SLOT,
                                    StrId::STR_PET_SCENE_SLOT};
  static constexpr const freeink::Icon* empty[] = {&referenceicons::EMPTY_REST, &referenceicons::EMPTY_DECOR,
                                                   &referenceicons::EMPTY_GAME, &referenceicons::EMPTY_SCENE};
  bool decorated = false;
  if (chrome::lightHeader) frame(s, slots);
  for (unsigned i = 0; i < pet::SLOT_COUNT; ++i) {
    const auto box = cell(slots, int(i), 4, chrome::lightHeader ? 0 : 6);
    bool inverse = false;
    if (chrome::lightHeader) {
      if (i) s.target().fill(rect(box.x, box.y, 1, box.height), lightui::rule);
      if (unlocked) s.frame().hit(box, OpenProjects, int(i));
    } else {
      inverse = button(s, w, box, "", OpenProjects, int(i), false, nullptr, unlocked);
    }
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
  const auto goal = rect(r.x, slots.bottom() + GAP, r.width, r.bottom() - slots.bottom() - 70);
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
void build(toybox::Screen& s, const Model& m, Work& w, Palette* palette, bool lightHeader, bool uppercaseHeader) {
  pixelui::begin(s);
  chrome::recordPaintedHeader(0, false);
  if (m.resourcesReady && !m.loading && m.profile && m.profile->lifeStatus == pet::LifeStatus::Active &&
      (m.view == pet::View::Dashboard || m.view == pet::View::Home || m.view == pet::View::Care ||
       m.view == pet::View::Games || m.view == pet::View::Projects || m.view == pet::View::ProjectPreview ||
       m.view == pet::View::Evolution || m.view == pet::View::Reward || m.view == pet::View::Health)) {
    switch (m.view) {
      case pet::View::Care:
        simple::care(s, m, w, simple::body(s));
        break;
      case pet::View::Games:
        simple::games(s, m, w, simple::body(s));
        break;
      case pet::View::Home:
        simple::decorate(s, m, w, simple::body(s));
        break;
      case pet::View::Projects:
      case pet::View::ProjectPreview:
        simple::catalog(s, m, w, simple::body(s));
        break;
      case pet::View::Dashboard:
      case pet::View::Evolution:
      case pet::View::Reward:
        simple::dashboard(s, m, w, simple::body(s));
        break;
      case pet::View::Health:
        simple::health(s, m, w, simple::body(s));
        break;
      default:
        return;  // The guard admits only room views.
    }
    if (m.view != pet::View::Dashboard && m.view != pet::View::Evolution && m.view != pet::View::Reward)
      simple::closeDrawer(s, w);
    PaletteScope header(palette, !lightHeader);
    simple::navigation(s, m, w, nullptr, true, uppercaseHeader);
    return;
  }
  const char* title = !m.resourcesReady                                                       ? tr(STR_PET_APP)
                      : m.view == pet::View::Album                                            ? tr(STR_PET_ALBUM)
                      : m.view == pet::View::Settings || m.view == pet::View::SettingsConfirm ? tr(STR_PET_SETTINGS)
                      : m.view == pet::View::ProjectPreview                                   ? tr(STR_PET_PREVIEW)
                      : m.view == pet::View::Reward                                           ? tr(STR_PET_REWARD)
                      : m.view == pet::View::Evolution                                        ? tr(STR_PET_EVOLUTION)
                                                                                              : tr(STR_PET_APP);
  const bool actions =
      m.resourcesReady && (m.view == pet::View::Dashboard || m.view == pet::View::Care || m.view == pet::View::Home ||
                           m.view == pet::View::Album || m.view == pet::View::Affection || m.view == pet::View::Games ||
                           m.view == pet::View::Play);
  {
    PaletteScope header(palette, !lightHeader);
    simple::navigation(s, m, w, title, actions, uppercaseHeader);
  }
  if (!m.resourcesReady) {
    requiredresources::build(s, "/apps/pet-v2/", Back, w.button);
    return;
  }
  if (m.view == pet::View::New || m.view == pet::View::Info || m.view == pet::View::Rules) tabs(s, w, m);
  auto body = simple::contentBody(s);
  if (m.view == pet::View::New || m.view == pet::View::Info || m.view == pet::View::Rules) body = s.body();
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
  const bool lightHeader = chrome::headerIsLight();
  struct ControlStyle {
    bool lightHeader = chrome::lightHeader;
    ControlStyle() { chrome::lightHeader = true; }
    ~ControlStyle() { chrome::lightHeader = lightHeader; }
  } controls;
  struct NaturalLabels {
    bool previous = pixelui::uppercaseLabels;
    NaturalLabels() { pixelui::uppercaseLabels = false; }
    ~NaturalLabels() { pixelui::uppercaseLabels = previous; }
  } naturalLabels;
  Palette palette(target, false);
  palette.fill(device.screen(), paper);
  toybox::Frame frame(palette, device, input, interactions, nullptr, status);
  toybox::Screen screen(frame);
  build(screen, m, w, &palette, lightHeader, naturalLabels.previous);
  chrome::recordPaintedHeader(device.safeRect().y + simple::HEADER_HEIGHT, lightHeader);
}
}  // namespace petui

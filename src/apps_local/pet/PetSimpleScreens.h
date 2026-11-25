#pragma once

#include "../ui/FitText.h"
#include "PetActionSurface.h"
#include "PetCareActionIcons.h"
#include "PetCareAdvice.h"
#include "PetCareIcons.h"
#include "PetHeader.h"
#include "PetInlineCatalog.h"
#include "PetOverview.h"
#include "PetPolishIcons.h"
#include "PetReadingBar.h"
#include "PetReadingResults.h"
#include "PetResultCards.h"
#include "PetRoom.h"
#include "PetSymbols.h"

namespace petui::simple {
using namespace widget;
inline constexpr int HEADER_HEIGHT = toybox::kMenuHeaderVisibleHeight, NEEDS_HEIGHT = 82, CONTROLS_HEIGHT = 176,
                     ACTION_ICON_SIZE = 44;
inline constexpr uint8_t ACTION_RADIUS = actionsurface::RADIUS;
inline constexpr uint8_t DRAWER_RADIUS = CONTROL_RADIUS + UI_INSET;
inline void controlShadow(toybox::Screen& s, fui::Rect r) {
  lightui::surface(s.target(), rect(r.x + 2, r.y + 3, r.width, r.height), ink);
}
inline void copy(toybox::Screen& s, fui::Rect r, const char* value, bool bold = false,
                 fui::TextAlign align = fui::TextAlign::Left, bool inverse = false, int lines = 1,
                 fui::FontId font = 4) {
  if (lines > 1 && r.height >= 2 * s.target().lineHeight(3)) {
    prose(s, r, value, lines, align, inverse);
    return;
  }
  fui::TextStyle style;
  style.font = font;
  style.bold = bold;
  style.maxLines = 1;
  style.align = align;
  style.color = inverse ? fui::Color::White : fui::Color::Black;
  fittext::draw(s.target(), r, value, style, std::max(2, lines));
}
inline fui::Rect body(toybox::Screen& s) {
  const auto safe = s.device().safeRect();
  return rect(safe.x + 6, safe.y + HEADER_HEIGHT, safe.width - 12, safe.height - HEADER_HEIGHT - 6);
}
inline fui::Rect contentBody(toybox::Screen& s) {
  const auto safe = s.device().safeRect();
  const int top = safe.y + HEADER_HEIGHT + UI_GAP;
  return rect(safe.x + UI_INSET, top, safe.width - 2 * UI_INSET, safe.bottom() - UI_INSET - SHADOW_DEPTH - top);
}
inline void navigation(toybox::Screen& s, const Model& m, Work& w, const char* title = nullptr, bool actions = true,
                       bool uppercaseHeader = false) {
  const auto safe = s.device().safeRect();
  s.target().fill(rect(0, 0, s.device().width, safe.y + HEADER_HEIGHT), paper);
  const auto back = pixelui::geometry(s.device()).back;
  const int rowY = safe.y + 12;
  backui::publish(s.device().hasTouch, back);
  backui::paint(s.target(), back, fui::Color::Black);
  const bool album = m.view == pet::View::Dashboard;
  const int trailingRight = safe.right() - UI_INSET;
  for (int i = 0; actions && i < 2; ++i) {
    const auto box = rect(trailingRight - 48 - i * 54, safe.y + 12, 48, 48);
    const bool inverse = button(s, w, box, "", i ? (album ? OpenAlbum : OpenReadingInfo) : OpenSettings, 0, true);
    symbols::draw(s, box.inset({8, 8, 8, 8}),
                  i ? (album ? symbols::Kind::Album : symbols::Kind::Info) : symbols::Kind::Settings, inverse);
  }
  const auto& status = s.headerStatus();
  const int right = trailingRight - (actions ? 112 : 8);
  fui::TextStyle clock;
  clock.font = fui::HEADER_CLOCK_FONT;
  clock.bold = true;
  clock.align = fui::TextAlign::Right;
  const int titleX = back.right() + 4;
  const auto clockBox =
      fui::headerClockRect(s.target(), rect(titleX + 100, rowY, right - titleX - 108, 48), status.clock, clock);
  if (!clockBox.empty()) s.target().text(clockBox, status.clock, clock);
  const int titleRight = clockBox.empty() ? right - 8 : clockBox.x - 16;
  if (uppercaseHeader)
    pixelui::copyCaps(w.label, sizeof(w.label), title ? title : tr(STR_PET_APP));
  else
    pixelui::copyNatural(w.label, sizeof(w.label), title ? title : tr(STR_PET_APP));
  copy(s, rect(titleX, rowY, titleRight - titleX, 48), w.label, true, fui::TextAlign::Left, false, 1,
       fui::HEADER_CLOCK_FONT);
}
inline void needs(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  actionsurface::panel(s, r);
  const auto& p = *m.profile;
  const unsigned values[] = {p.saciety, p.joy, p.energy, p.hygiene};
  static constexpr StrId labels[] = {StrId::STR_PET_FULLNESS, StrId::STR_PET_JOY, StrId::STR_PET_ENERGY,
                                     StrId::STR_PET_HYGIENE};
  static constexpr symbols::Kind marks[] = {symbols::Kind::Food, symbols::Kind::Health, symbols::Kind::Energy,
                                            symbols::Kind::Sparkles};
  const bool locked = pet::stage(p.readingSeconds) == pet::Stage::Egg;
  for (int n = 0; n < 4; ++n) {
    const auto box = cell(r, n, 4, 0);
    const bool tall = r.height >= 78;
    const int side = tall ? 26 : 22, rowY = box.y + (tall ? 34 : 28);
    symbols::draw(s, rect(box.x + 7, box.y + 7, side, side), marks[n]);
    copy(s, rect(box.x + 5, rowY, box.width - 10, tall ? 30 : 26), I18N.get(labels[n]), true, fui::TextAlign::Center,
         false, 1, 3);
    if (locked) {
      icon(s, rect(box.x + side + 14, box.y + 7, 22, 24), extra_lock_32);
    } else {
      std::snprintf(w.label, sizeof(w.label), "%u/100", values[n]);
      copy(s, rect(box.x + side + 12, box.y + 5, box.width - side - 16, 26), w.label, true, fui::TextAlign::Center,
           false, 1, 0);
    }
    const auto track = rect(box.x + 7, box.bottom() - 13, box.width - 14, 6);
    s.target().fill(track, fui::Paint::dither(fui::Color::LightGray), 3);
    s.target().fill(rect(track.x, track.y, track.width * (locked ? 0 : values[n]) / 100, track.height), ink, 3);
    if (n < 3)
      s.target().line({int16_t(box.right()), int16_t(box.y + 10)}, {int16_t(box.right()), int16_t(box.bottom() - 10)},
                      1, ink);
  }
}
inline void nameDetail(const Model& m, Work& w) {
  const auto& p = *m.profile;
  std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_GENERATION), static_cast<unsigned long>(p.generation));
  const auto age =
      m.view == pet::View::Evolution ? petreact::growthStage(p, m.reactionPhase) : pet::stage(p.readingSeconds);
  if (m.clockKnown && p.bornDay > 0 && m.today >= p.bornDay) {
    const auto days = unsigned(int64_t(m.today) - p.bornDay);
    std::snprintf(w.label, sizeof(w.label), days == 1 ? tr(STR_PET_AGE_DAY) : tr(STR_PET_AGE_DAYS), days);
  } else {
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_AGE_UNKNOWN));
  }
  char detail[128];
  std::snprintf(detail, sizeof(detail), "%s · %s · %s", stageName(age), w.secondary, w.label);
  std::snprintf(w.label, sizeof(w.label), "%s", detail);
}
inline void namePlate(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  lightui::surface(s.target(), r, paper);
  text(s, rect(r.x + 8, r.y + 2, r.width - 16, 30), displayName(p.name, p.species), 0, false, fui::TextAlign::Center);
  nameDetail(m, w);
  const int detailH = r.height - 38 - (pet::stage(p.readingSeconds) == pet::Stage::Egg ? 24 : 0);
  copy(s, rect(r.x + 8, r.y + 34, r.width - 16, detailH), w.label, false, fui::TextAlign::Center);
  if (pet::stage(p.readingSeconds) == pet::Stage::Egg) {
    std::snprintf(
        w.label, sizeof(w.label), tr(STR_PET_GROWTH_LEFT),
        static_cast<unsigned long long>((pet::nextStageSeconds(pet::Stage::Egg) - p.readingSeconds + 59) / 60));
    copy(s, rect(r.x + 8, r.y + 38 + detailH, r.width - 16, 22), w.label, false, fui::TextAlign::Center);
  }
}
inline void actionTile(toybox::Screen& s, Work& w, fui::Rect box, const char* title, symbols::Kind mark,
                       fui::ActionId action, int value = 0, bool enabled = true, const char* detail = nullptr,
                       bool locked = false) {
  const bool inverse = actionsurface::button(s, w, box, action, value, false, enabled && !locked);
  const int side = std::min(ACTION_ICON_SIZE, int(box.height) - (detail ? 60 : 42));
  const auto picture = rect(box.x + (box.width - side) / 2, box.y + 10, side, side);
  symbols::draw(s, picture, mark, inverse);
  if (locked) {
    const auto badge = rect(picture.right() - 12, picture.bottom() - 16, 22, 22);
    s.target().fill(badge.inset({-2, -2, -2, -2}), paper, 6);
    icon(s, badge, extra_lock_32);
  }
  if (detail) {
    const int top = box.y + side + 12, height = std::min(24, int(box.bottom()) - 32 - top);
    copy(s, rect(box.x + 5, top, box.width - 10, height), detail, false, fui::TextAlign::Center, inverse, 1, 3);
  }
  copy(s, rect(box.x + 3, box.bottom() - 32, box.width - 6, 28), title, true, fui::TextAlign::Center, inverse);
}
struct MainLayout {
  fui::Rect needs, picture, controls;
};
inline MainLayout mainLayout(fui::Rect body) {
  MainLayout result;
  const bool wide = body.width > body.height;
  const int needsH = NEEDS_HEIGHT;

  body.y += 16;
  body.height -= 16;
  result.needs = rect(body.x, body.y, body.width, needsH);
  body.y += needsH + 12;
  body.height -= needsH + 12;
  if (wide) {
    const int width = body.width * 56 / 100;
    result.picture = rect(body.x, body.y, width - 8, body.height);
    result.controls = rect(body.x + width + 4, body.y, body.width - width - 4, body.height);
  } else {
    const int controlsH = CONTROLS_HEIGHT;
    result.picture = rect(body.x, body.y, body.width, body.height - controlsH - 8);
    result.controls = rect(body.x, result.picture.bottom() + 8, body.width, controlsH);
  }
  return result;
}
inline void mainPicture(toybox::Screen& s, const Model& m, Work& w, const MainLayout& layout,
                        const pet::Profile* preview = nullptr) {
  const auto bounds = rect(0, chrome::headerHeight, s.device().width, s.device().height - chrome::headerHeight);
  const auto interior = roomview::layout(bounds, layout.picture, layout.controls);
  const bool wide = bounds.width > bounds.height;
  const bool egg = pet::stage(m.profile->readingSeconds) == pet::Stage::Egg;
  const int width = wide ? layout.picture.width - 36 : layout.needs.width * 9 / 10;
  const int x = wide ? layout.picture.x + 18 : layout.needs.x + (layout.needs.width - width) / 2;
  nameDetail(m, w);
  const int detailH = s.target().measureText(3, w.label, {}).width > width - 16 ? 2 * s.target().lineHeight(3) : 22;
  const auto name = rect(x, layout.needs.bottom() + 12, width, 38 + detailH + (egg ? 24 : 0));
  if (!preview && petfeed::eating(m.feedingPhase)) {
    w.previewProfile = *m.profile;
    w.previewProfile.resting = false;
    w.previewProfile.illness = pet::Illness::Healthy;
    preview = &w.previewProfile;
  }
  if (!preview && m.restPhase != petrest::Phase::Idle) {
    w.previewProfile = *m.profile;
    const unsigned bed = m.profile->equipped[0];
    const bool entering = m.restPhase == petrest::Phase::SettleSecond || m.restPhase == petrest::Phase::Exit;
    w.previewProfile.resting =
        entering && bed < pet::OBJECT_COUNT && pet::objectSlot(bed) == 0 && OBJECT_ART[bed].restPose == RestPose::Head;
    preview = &w.previewProfile;
  }

  roomview::paint(s, m, interior, preview, name.bottom() + GAP);
  needs(s, m, w, layout.needs);
  namePlate(s, m, w, name);
  if (m.view == pet::View::Dashboard && !preview && !egg && pet::canCare(*m.profile)) {
    const auto subject = habitatlayout::scale(interior.scene, habitatlayout::pet(*m.profile));
    const int touchWidth = std::min(int(interior.scene.width), std::max(44, int(subject.width) + 12));
    const int touchHeight = std::min(int(interior.scene.height), std::max(44, int(subject.height) + 12));
    const int touchX = std::clamp(subject.x + subject.width / 2 - touchWidth / 2, int(interior.scene.x),
                                  int(interior.scene.right()) - touchWidth);
    const int touchY = std::clamp(subject.y + subject.height / 2 - touchHeight / 2, int(interior.scene.y),
                                  int(interior.scene.bottom()) - touchHeight);
    s.frame().hit(rect(touchX, touchY, touchWidth, touchHeight), TouchCompanion, 0, fui::InputTouch);
  }
  const char* hint = nullptr;
  switch (m.companionKind) {
    case petcompanion::Kind::WakeFirst:
      hint = tr(STR_PET_GAME_WAKE_SHORT);
      break;
    case petcompanion::Kind::Hungry:
      hint = tr(STR_PET_TOUCH_HUNGRY);
      break;
    case petcompanion::Kind::Tired:
      hint = tr(STR_PET_TOUCH_TIRED);
      break;
    case petcompanion::Kind::Dirty:
      hint = tr(STR_PET_TOUCH_DIRTY);
      break;
    case petcompanion::Kind::Sick:
      hint = tr(STR_PET_TOUCH_SICK);
      break;
    case petcompanion::Kind::None:
    case petcompanion::Kind::Sit:
    case petcompanion::Kind::Stretch:
    case petcompanion::Kind::Greet:
    case petcompanion::Kind::Discover:
    case petcompanion::Kind::Satisfied:
      break;
  }
  if (m.view == pet::View::Dashboard && hint && m.companionPhase != petcompanion::Phase::Idle) {
    const auto hintBox = wide ? rect(layout.picture.x, layout.picture.bottom() - 54, layout.picture.width, 46)
                              : rect(layout.needs.x, layout.controls.y - 56, layout.needs.width, 46);
    actionsurface::panel(s, hintBox);
    copy(s, hintBox.inset({5, 10, 5, 10}), hint, true, fui::TextAlign::Center);
  }
}
inline void closeAction(toybox::Screen& s, Work& w, fui::Rect box, fui::ActionId action, int value = 0) {
  const bool inverse = actionsurface::button(s, w, box, action, value);
  const auto paint = inverse ? paper : ink;
  s.target().line({int16_t(box.x + 14), int16_t(box.y + 14)}, {int16_t(box.right() - 14), int16_t(box.bottom() - 14)},
                  2, paint);
  s.target().line({int16_t(box.right() - 14), int16_t(box.y + 14)}, {int16_t(box.x + 14), int16_t(box.bottom() - 14)},
                  2, paint);
}
inline void readingReceipt(toybox::Screen& s, const Model& m, Work& w) {
  const bool wide = s.device().width > s.device().height;
  const auto controls = mainLayout(body(s)).controls;
  const auto box =
      rect(wide ? controls.x : 14, s.device().height - 274, wide ? controls.width : s.device().width - 28, 260);
  actionsurface::panel(s, box);
  const auto bounds = body(s);
  const auto inner =
      rect(std::max(int(box.x) + 12, int(bounds.x) + 8), box.y + 10,
           std::min(int(box.right()) - 12, int(bounds.right()) - 8) - std::max(int(box.x) + 12, int(bounds.x) + 8),
           std::min(int(box.bottom()) - 12, int(bounds.bottom()) - 8) - box.y - 10);
  symbols::draw(s, rect(inner.x, inner.y + 4, 32, 32), symbols::Kind::Read);
  const auto title =
      pet::stage(m.profile->readingSeconds) == pet::Stage::Adult ? tr(STR_PET_GROWTH_ADULT) : tr(STR_PET_GOOD_READING);
  copy(s, rect(inner.x + 42, inner.y + 5, inner.width - 94, 32), title, true);
  closeAction(s, w, rect(inner.right() - 48, inner.y, 48, 48), Back);
  const auto hero = rect(inner.x, inner.y + 44, inner.width, 64);
  const auto rewards = rect(inner.x, inner.y + 112, inner.width, 60);
  const auto today = rect(inner.x, inner.y + 178, inner.width, 22);
  const auto proceed = rect(inner.x, inner.bottom() - 36, inner.width, 36);
  copy(s, rect(hero.x, hero.y, hero.width, 22), tr(STR_PET_SINCE_VISIT), false, fui::TextAlign::Center, false, 1, 3);
  duration(w.secondary, sizeof(w.secondary),
           m.profile->readingSeconds - std::min(m.profile->readingSeconds, m.profile->noticedSeconds));
  copy(s, rect(hero.x, hero.y + 24, hero.width, hero.height - 24), w.secondary, true, fui::TextAlign::Center, false, 1,
       1);
  for (int n = 0; n < 2; ++n) {
    const auto card = cell(rewards, n, 2, 12);
    actionsurface::panel(s, card);
    const int mark = 22;
    symbols::draw(s, rect(card.x + 10, card.y + 8, mark, mark), n ? symbols::Kind::Health : symbols::Kind::Food);
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_REWARD_AMOUNT),
                  static_cast<unsigned long long>(readingGains(*m.profile, n != 0)));
    copy(s, rect(card.x + mark + 18, card.y + 4, card.width - mark - 26, 30), w.label, true, fui::TextAlign::Center,
         false, 1, 0);
    copy(s, rect(card.x + 6, card.bottom() - 25, card.width - 12, 22),
         n ? tr(STR_PET_MEDICINE) : tr(STR_PET_PROVISIONS), false, fui::TextAlign::Center, false, 1, 3);
  }
  if (m.today > 0 && m.clockKnown && !m.historyChanged && m.writable) {
    duration(w.secondary, sizeof(w.secondary), uint64_t(std::max(0, m.todaySeconds)));
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_READ_TODAY), w.secondary);
  } else
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_READING_UNAVAILABLE));
  const bool projectReady = pet::homeUnlocked(*m.profile) && !pet::owns(*m.profile, m.profile->project) &&
                            pet::projectRemaining(*m.profile) == 0;
  copy(s, today, w.label, false, fui::TextAlign::Center, false, 1, 3);
  const bool inverse = actionsurface::button(s, w, proceed, Continue, 0, true, m.writable);
  copy(s, proceed.inset({6, 8, 6, 8}), projectReady ? tr(STR_PET_GET_NOW) : tr(STR_PET_CONTINUE), true,
       fui::TextAlign::Center, inverse);
}
inline void reactionCard(toybox::Screen& s, const Model& m, Work& w, const MainLayout& layout) {
  if (m.view == pet::View::Reward) {
    readingReceipt(s, m, w);
    return;
  }
  const bool evolution = m.view == pet::View::Evolution;
  if (!evolution && m.reactionKind == petreact::Kind::None) return;
  const bool begin = evolution && m.reactionPhase == petreact::Phase::Idle;
  const int height = evolution ? (begin ? 150 : 118) : 186;
  const auto box = s.device().width > s.device().height
                       ? rect(layout.controls.x, layout.controls.y + 24, layout.controls.width, height)
                       : rect(layout.controls.x,
                              std::min(layout.controls.y + (layout.controls.height - height) / 2,
                                       s.device().safeRect().bottom() - height - 3),
                              layout.controls.width, height);
  actionsurface::panel(s, box);
  const char* title = tr(STR_PET_EVOLUTION);
  const char* note = nullptr;
  resultcards::Metric metrics[2];
  unsigned count = 0;
  if (evolution) {
    title = m.reactionPhase == petreact::Phase::Reveal || m.reactionPhase == petreact::Phase::Result
                ? tr(STR_PET_EVOLUTION)
                : tr(STR_PET_GROWING);
  } else {
    switch (m.reactionKind) {
      case petreact::Kind::None:
      case petreact::Kind::Growth:
        break;
      case petreact::Kind::Petting:
        title = tr(STR_PET_PETTING_FEEDBACK);
        metrics[count++] = {symbols::Kind::Care, tr(STR_PET_JOY), m.affection.before, m.affection.after};
        break;
      case petreact::Kind::Cleaning:
        title = tr(STR_PET_CLEANING_FEEDBACK);
        note = tr(STR_PET_CLEAN_FREE);
        metrics[count++] = {symbols::Kind::Clean, tr(STR_PET_HYGIENE), m.careResult.hygieneBefore,
                            m.careResult.hygieneAfter};
        metrics[count++] = {symbols::Kind::Health, tr(STR_PET_JOY), m.careResult.joyBefore, m.careResult.joyAfter};
        break;
      case petreact::Kind::Healing:
        title = tr(STR_PET_HEALING_FEEDBACK);
        std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_CURE_COST), unsigned(m.careResult.medicineSpent));
        note = w.secondary;
        metrics[count++] = {symbols::Kind::Clean, tr(STR_PET_HYGIENE), m.careResult.hygieneBefore,
                            m.careResult.hygieneAfter};
        metrics[count++] = {symbols::Kind::Health, tr(STR_PET_HEALTH_SHORT), 0, 0, tr(STR_PET_HEALTHY)};
        break;
      case petreact::Kind::Game:
        switch (m.gameResult.outcome) {
          case GameOutcome::Rewarded:
            title = tr(STR_PET_GAME_FINISHED);
            note = tr(STR_PET_GAME_OFFER_REWARD);
            break;
          case GameOutcome::Practice:
            title = tr(STR_PET_GAME_PRACTICE);
            note = tr(STR_PET_GAME_NO_COST);
            break;
          case GameOutcome::CareNeeded:
            title = tr(STR_PET_NEEDS_CARE_INLINE);
            break;
        }
        metrics[count++] = {symbols::Kind::Health, tr(STR_PET_JOY), m.gameResult.joyBefore, m.gameResult.joyAfter};
        metrics[count++] = {symbols::Kind::Energy, tr(STR_PET_ENERGY), m.gameResult.energyBefore,
                            m.gameResult.energyAfter};
        break;
    }
  }
  copy(s, rect(box.x + 12, box.y + 8, box.width - 82, 30), title, true);
  closeAction(s, w, rect(box.right() - 56, box.y + 7, 48, 48), evolution ? Back : DismissReaction);
  if (note) copy(s, rect(box.x + 12, box.y + 40, box.width - 82, 26), note, true);
  if (evolution || !count) {
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GROWTH_STAGES_INLINE), stageName(m.profile->noticedStage),
                  stageName(pet::stage(m.profile->readingSeconds)));
    copy(s, rect(box.x + 12, box.y + 49, box.width - 24, 28), w.label);
    if (begin)
      inlinecatalog::control(s, w, rect(box.x + 12, box.bottom() - 50, box.width - 24, 40), tr(STR_PET_GROWTH_BEGIN),
                             Continue, 0, true, m.writable);
    return;
  }
  const auto row = rect(box.x + 12, box.y + 76, box.width - 24, box.height - 89);
  for (unsigned n = 0; n < count; ++n) {
    const auto tile = cell(row, int(n), int(count), 12);
    resultcards::metric(s, w, tile, metrics[n]);
  }
}
inline void dashboard(toybox::Screen& s, const Model& m, Work& w, fui::Rect body) {
  const auto& p = *m.profile;
  const bool wide = body.width > body.height;
  const auto layout = mainLayout(body);
  mainPicture(s, m, w, layout);
  const auto controls = layout.controls;
  const bool interactive = m.view == pet::View::Dashboard && m.reactionKind == petreact::Kind::None;
  if (!interactive) {
    reactionCard(s, m, w, layout);
    return;
  }
  if (m.feedResult.visible) {
    const bool eating = petfeed::eating(m.feedingPhase);
    const int height = eating ? 44 : 112;
    const auto receipt =
        rect(layout.picture.x + 4, wide ? layout.picture.bottom() - height - 6 : controls.y - height - 12,
             layout.picture.width - 8, height);
    actionsurface::panel(s, receipt);
    if (eating) {
      copy(s, receipt.inset({4, 8, 4, 8}), tr(STR_PET_EATING), false, fui::TextAlign::Center);
    } else {
      symbols::draw(s, rect(receipt.x + 12, receipt.y + 12, 32, 32), symbols::Kind::Food);
      copy(s, rect(receipt.x + 54, receipt.y + 8, receipt.width / 2 - 60, 46), tr(STR_PET_NOTICE_FED), true);
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_FOOD_USED), 1u);
      copy(s, rect(receipt.x + 12, receipt.bottom() - 42, receipt.width / 2 - 16, 32), w.label, true);
      resultcards::metric(s, w, rect(receipt.x + receipt.width / 2, receipt.y + 8, receipt.width / 2 - 10, 94),
                          {symbols::Kind::Food, tr(STR_PET_FULLNESS), m.feedResult.before, m.feedResult.after});
    }
  }
  const bool egg = pet::stage(p.readingSeconds) == pet::Stage::Egg;
  const int readH = 70;
  const int top = controls.y;
  const int available = controls.bottom() - readH - 12 - top;
  const int rows = wide ? 2 : 1;
  const int cols = 4 / rows, gap = 8;
  const int width = (controls.width - (cols - 1) * gap) / cols;
  const int height = (available - (rows - 1) * gap) / rows;
  for (int n = 0; n < 4; ++n) {
    const auto box = rect(controls.x + n % cols * (width + gap), top + n / cols * (height + gap), width, height);
    switch (n) {
      case 0:
        actionTile(s, w, box, tr(STR_PET_CARE), symbols::Kind::Care, ChangeTab, 1, interactive, nullptr, egg);
        break;
      case 1:
        actionTile(s, w, box, tr(STR_PET_PLAY), symbols::Kind::Play, OpenGames, 0, interactive && pet::canCare(p),
                   nullptr, egg);
        break;
      case 2:
        actionTile(s, w, box, tr(STR_PET_HEALTH_SHORT), symbols::Kind::Health, OpenHealth, 0,
                   interactive && pet::canCare(p), nullptr, egg);
        break;
      case 3:
        actionTile(s, w, box, tr(STR_PET_DECORATE_ACTION), symbols::Kind::Decorate, ChangeTab, 2,
                   interactive && pet::homeUnlocked(p), nullptr, egg);
        break;
    }
  }
  readingbar::build(s, m, w, rect(controls.x, controls.bottom() - readH, controls.width, readH),
                    interactive && !m.loading);
  reactionCard(s, m, w, layout);
}
inline int drawerTop(toybox::Screen& s) {
  return s.device().width > s.device().height ? mainLayout(body(s)).needs.bottom() + UI_GAP : s.device().height / 2;
}
inline void closeDrawer(toybox::Screen& s, Work& w) {
  closeAction(s, w,
              rect(s.device().safeRect().right() - UI_INSET - CONTROL_HEIGHT, drawerTop(s) + GAP, CONTROL_HEIGHT,
                   CONTROL_HEIGHT),
              ChangeTab);
}
inline fui::Rect drawer(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, const char* title,
                        const pet::Profile* preview = nullptr) {
  const auto layout = mainLayout(r);
  const auto sheet = rect(0, drawerTop(s), s.device().width, s.device().height - drawerTop(s));
  const auto previous = s.target().clipRect();
  // Preserve the room behind the rounded top corners; the opaque sheet covers everything below.
  const int bottom = std::min(int(previous.bottom()), int(sheet.y) + DRAWER_RADIUS);
  const bool clipped =
      bottom > previous.y && s.target().setClipRect(rect(previous.x, previous.y, previous.width, bottom - previous.y));
  mainPicture(s, m, w, layout, preview);
  if (clipped) s.target().setClipRect(previous);
  s.target().fill(rect(sheet.x, sheet.y - 6, sheet.width, sheet.height + 6), ink, DRAWER_RADIUS, fui::CornersTop);
  s.target().fill(sheet, paper, DRAWER_RADIUS, fui::CornersTop);
  const auto safe = s.device().safeRect();
  const int left = safe.x + UI_INSET, right = safe.right() - UI_INSET;
  copy(s, rect(left, sheet.y + 8, right - left - CONTROL_HEIGHT - UI_GAP, CONTROL_HEIGHT), title, true,
       fui::TextAlign::Left, false, 1, 1);
  const int top = sheet.y + CONTROL_HEIGHT + UI_GAP;
  const auto inner = rect(left, top, right - left, safe.bottom() - UI_INSET - SHADOW_DEPTH - top);
  return inner;
}
inline void games(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  gamesPanel(s, m, w, drawer(s, m, w, r, tr(STR_PET_PLAY)));
}
inline void healthState(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, bool vertical) {
  const auto& p = *m.profile;
  const bool sick = p.illness != pet::Illness::Healthy;
  actionsurface::panel(s, box);
  const bool compact = vertical && box.height < 200;
  const int side = vertical ? (compact ? 40 : 64) : 56;
  const auto picture = vertical ? rect(box.x + (box.width - side) / 2, box.y + (compact ? 10 : 14), side, side)
                                : rect(box.x + 14, box.y + (box.height - side) / 2, side, side);
  symbols::reference(s, picture, careactionicons::HEALTH, false, careactionicons::SIZE);
  const auto badge = rect(picture.right() - 15, picture.bottom() - 15, 20, 20);
  s.target().fill(badge, paper, 10);
  s.target().stroke(badge, ink, 2, 10);
  const int x = badge.x, y = badge.y;
  if (sick) {
    s.target().line({int16_t(x + 10), int16_t(y + 5)}, {int16_t(x + 10), int16_t(y + 11)}, 2, ink);
    s.target().fill(rect(x + 9, y + 14, 2, 2), ink);
  } else {
    s.target().line({int16_t(x + 5), int16_t(y + 10)}, {int16_t(x + 9), int16_t(y + 14)}, 2, ink);
    s.target().line({int16_t(x + 9), int16_t(y + 14)}, {int16_t(x + 15), int16_t(y + 6)}, 2, ink);
  }
  const int left = vertical ? box.x + 8 : picture.right() + 14;
  const int width = vertical ? box.width - 16 : box.right() - 106 - left;
  const int top = vertical ? picture.bottom() + (compact ? 8 : 12) : box.y + (sick ? 12 : (box.height - 34) / 2);
  const auto align = vertical ? fui::TextAlign::Center : fui::TextAlign::Left;
  copy(s, rect(left, top, width, compact ? 28 : 34), sick ? tr(STR_PET_HEALTH_STATUS_SICK) : tr(STR_PET_HEALTHY), true,
       align, false, 1, 0);
  if (sick) {
    static constexpr StrId conditions[] = {StrId::STR_PET_HEALTHY, StrId::STR_PET_COLD, StrId::STR_PET_MALAISE};
    copy(s, rect(left, top + (compact ? 30 : 38), width, compact ? 20 : 24), I18N.get(conditions[unsigned(p.illness)]),
         false, align, false, 1, 3);
  }
  if (vertical && box.height < 200) {
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_MEDICINE_STOCK),
                  static_cast<unsigned long long>(pet::medicine(p)));
    copy(s, rect(box.x + 8, box.bottom() - 30, box.width - 16, 22), w.label, false, fui::TextAlign::Center, false, 1,
         3);
  } else {
    const auto supplies = vertical ? rect(box.x + 8, box.bottom() - 86, box.width - 16, 72)
                                   : rect(box.right() - 96, box.y + (box.height - 64) / 2, 84, 64);
    std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(pet::medicine(p)));
    copy(s, rect(supplies.x, supplies.y, supplies.width, 34), w.label, true, fui::TextAlign::Center, false, 1, 0);
    copy(s, rect(supplies.x, supplies.y + 38, supplies.width, 22), tr(STR_PET_MEDICINE), false, fui::TextAlign::Center,
         false, 1, 3);
  }
}
inline void careTile(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, fui::ActionId action, const char* title,
                     const char* hint, const uint8_t* illustration, bool enabled, bool horizontal, int value = 0,
                     const char* detail = nullptr) {
  const bool muted = action == Cure && m.profile->illness == pet::Illness::Healthy;
  const auto mutedInk = fui::Paint::dither(fui::Color::DarkGray);
  bool inverse = false;
  if (muted) {
    s.target().fill(box, paper, CONTROL_RADIUS);
    s.target().stroke(box, mutedInk, 1, CONTROL_RADIUS);
  } else {
    inverse = actionsurface::button(s, w, box, action, value, false, enabled);
  }
  constexpr int TITLE_HEIGHT = 32, GAP_AFTER_ART = 5;
  const int footerHeight = std::max(42, 2 * int(s.target().lineHeight(3)) + 4);
  const bool sideBySide = horizontal || box.height < TITLE_HEIGHT + GAP_AFTER_ART + footerHeight + 54;
  const int side = std::max(
      0, sideBySide ? std::min({72, int(box.height) - 12, int(box.width) / 3}) : std::min(84, int(box.width) - 16));
  const int pictureTop = sideBySide ? box.y + (box.height - side) / 2
                                    : box.y + std::max(9, (box.height - side - TITLE_HEIGHT - footerHeight - 23) / 2);
  const auto picture = rect(sideBySide ? box.x + 10 : box.x + (box.width - side) / 2, pictureTop, side, side);
  if (action == Stroke)
    symbols::draw(s, picture, symbols::Kind::Care, inverse);
  else if (action != OpenFood || !foodPicture(m, s.target(), picture, 5, 0))
    symbols::reference(s, picture, illustration, inverse, careactionicons::SIZE);

  const int left = sideBySide ? picture.right() + 8 : box.x + 6;
  const int titleY = sideBySide ? box.y + std::max(2, (box.height - TITLE_HEIGHT - footerHeight) / 2)
                                : picture.bottom() + GAP_AFTER_ART;
  const int width = box.right() - left - 6;
  const auto titleBox = rect(left, titleY, width, TITLE_HEIGHT);
  if (muted)
    fittext::draw(s.target(), titleBox, title, 4, 2, fui::TextAlign::Center, fui::Color::DarkGray);
  else
    copy(s, titleBox, title, true, fui::TextAlign::Center, inverse);
  if (!hint) return;

  const int footerX = sideBySide ? left : box.x + 8;
  const int footerY = sideBySide ? titleBox.bottom() : box.bottom() - footerHeight - 11;
  const int footerWidth = sideBySide ? width : box.width - 16;
  const auto footer = rect(footerX, footerY, footerWidth, std::min(footerHeight, box.bottom() - footerY - 3));
  s.target().stroke(footer, muted ? mutedInk : inverse ? paper : ink, 1, 7);
  const auto inside = rect(footer.x + 4, footer.y + 2, footer.width - 8, footer.height - 4);
  const int lineHeight = detail ? inside.height / 2 : int(inside.height);
  fui::TextStyle style;
  style.font = 3;
  style.align = fui::TextAlign::Center;
  style.color = inverse ? fui::Color::White : fui::Color::Black;
  style.maxLines = 1;
  fittext::draw(s.target(), rect(inside.x, inside.y, inside.width, lineHeight), hint, style, detail ? 1 : 2);
  if (detail)
    copy(s, rect(inside.x, inside.y + lineHeight, inside.width, lineHeight), detail, false, fui::TextAlign::Center,
         inverse, 1, 3);
}

inline void healthOption(toybox::Screen& s, const Model& m, Work& w, fui::Rect box, bool healing, bool horizontal) {
  const auto& p = *m.profile;
  const bool healthy = p.illness == pet::Illness::Healthy;
  const bool enabled = m.writable && pet::canCare(p) && (healing ? !healthy && pet::medicine(p) : p.hygiene < 100);
  const char* hint = healthActionHint(m, healing);
  const char* cost = nullptr;
  if (p.resting && pet::canCare(p) && (!healing || !healthy)) {
    hint = tr(STR_PET_GAME_WAKE_SHORT);
  } else if (enabled && healing) {
    hint = tr(STR_PET_CURE_BENEFIT);
    cost = tr(STR_PET_CURE_ONE);
  } else if (enabled) {
    std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_CARE_HYGIENE_GAIN), 100u - p.hygiene);
    hint = w.secondary;
  }
  careTile(s, m, w, box, healing ? Cure : Clean, healing ? tr(STR_PET_CURE) : tr(STR_PET_CLEAN), hint,
           healing ? careactionicons::HEALTH : careactionicons::CLEAN, enabled, horizontal, 0, cost);
}
inline void health(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto inner = drawer(s, m, w, r, tr(STR_PET_HEALTH_SHORT));
  const bool wide = s.device().width > s.device().height;
  const auto help = rect(s.device().safeRect().right() - UI_INSET - 2 * CONTROL_HEIGHT - UI_GAP, drawerTop(s) + GAP,
                         CONTROL_HEIGHT, CONTROL_HEIGHT);
  const bool inverse = actionsurface::button(s, w, help, OpenHealthHelp);
  symbols::draw(s, help.inset({8, 8, 8, 8}), symbols::Kind::Info, inverse);
  auto status = rect(inner.x, inner.y, inner.width, 84);
  auto actions =
      rect(inner.x, status.bottom() + UI_GAP, inner.width, inner.bottom() - status.bottom() - UI_GAP - SHADOW_DEPTH);
  bool verticalStatus = false;
  if (wide) {
    const int width = inner.width * 28 / 100;
    status = rect(inner.x, inner.y, width, inner.height - 4);
    actions = rect(status.right() + 12, inner.y, inner.right() - status.right() - 12, inner.height - 4);
    verticalStatus = true;
  }
  healthState(s, m, w, status, verticalStatus);
  for (int i = 0; i < 2; ++i) healthOption(s, m, w, cell(actions, i, 2, 12), i != 0, wide);
}
inline void decorate(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  inlinecatalog::home(s, m, w, drawer(s, m, w, r, tr(STR_PET_DECORATE_ACTION)));
}
inline void catalog(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const pet::Profile* preview = nullptr;
  if (m.view == pet::View::ProjectPreview && m.previewItem < pet::OBJECT_COUNT &&
      pet::objectSlot(m.previewItem) == unsigned(std::clamp(m.projectSlot, 0, 3))) {
    w.previewProfile = *m.profile;
    w.previewProfile.equipped[m.projectSlot] = m.previewItem;
    preview = &w.previewProfile;
  }
  auto content = drawer(s, m, w, r, tr(STR_PET_DECORATE_ACTION), preview);
  if (s.device().width > s.device().height) {
    content.y -= 8;
    content.height += 8;
  }
  inlinecatalog::build(s, m, w, content);
}
inline void care(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  auto inner = drawer(s, m, w, r, m.choosingFood ? tr(STR_PET_FOODS) : tr(STR_PET_CARE));
  if (m.choosingFood && pet::canCare(p)) {
    const int headerGap = s.device().width > s.device().height ? 12 : 20;
    inner.y += headerGap;
    inner.height -= headerGap;
    const unsigned selected = std::min(unsigned(m.selectedFood), petfeed::choices(m.feedingArt) - 1);
    const unsigned page = selected / petfeed::FOODS_PER_PAGE;
    const auto previous = rect(inner.right() - 144, inner.y - 4, 32, 30);
    const auto next = rect(inner.right() - 36, inner.y - 4, 32, 30);
    button(s, w, previous, "", FoodPage, -1, false, nullptr, page > 0);
    std::snprintf(w.label, sizeof(w.label), "%u/%u", page + 1,
                  petfeed::choices(m.feedingArt) / petfeed::FOODS_PER_PAGE);
    copy(s, rect(inner.right() - 108, inner.y - 4, 68, 30), w.label, false, fui::TextAlign::Center);
    button(s, w, next, "", FoodPage, 1, false, nullptr,
           page + 1 < petfeed::choices(m.feedingArt) / petfeed::FOODS_PER_PAGE);
    for (const auto box : {previous, next}) {
      const int direction = box.x == previous.x ? -1 : 1;
      const int x = box.x + box.width / 2, y = box.y + box.height / 2;
      s.target().line({int16_t(x - direction * 3), int16_t(y - 6)}, {int16_t(x + direction * 3), int16_t(y)}, 2, ink);
      s.target().line({int16_t(x + direction * 3), int16_t(y)}, {int16_t(x - direction * 3), int16_t(y + 6)}, 2, ink);
    }
    static constexpr StrId names[] = {StrId::STR_PET_BERRIES,   StrId::STR_PET_CAKE,  StrId::STR_PET_NIGHT_FRUIT,
                                      StrId::STR_PET_HAMBURGER, StrId::STR_PET_PIZZA, StrId::STR_PET_SANDWICH,
                                      StrId::STR_PET_KIBBLE,    StrId::STR_PET_PATE,  StrId::STR_PET_DENTAL};
    const int top = inner.y + 32;
    const char* hint = !m.writable        ? tr(STR_PET_CARE_NO_SAVE)
                       : p.saciety == 100 ? tr(STR_PET_ALREADY_FULL)
                       : !pet::food(p)    ? tr(STR_PET_SIMPLE_NO_FOOD)
                                          : nullptr;
    const int height = inner.bottom() - top - (hint ? 44 : 4);
    const bool canServe = m.writable && pet::food(p) && p.saciety < 100;
    for (unsigned column = 0; column < petfeed::FOODS_PER_PAGE; ++column) {
      const unsigned n = page * petfeed::FOODS_PER_PAGE + column;
      const auto box = cell(rect(inner.x, top, inner.width, height), column, petfeed::FOODS_PER_PAGE, 10);
      const bool inverse = actionsurface::button(s, w, box, SelectFood, int(n), false, canServe);
      const bool compact = box.height < 90;
      const int side = std::min(54, int(box.height) - 8);
      const auto picture = compact ? rect(box.x + 6, box.y + 4, side, side)
                                   : rect(box.x + 8, box.y + 8, box.width - 16, int(box.height) - 64);
      if (!foodPicture(m, s.target(), picture, n, 0)) symbols::draw(s, picture, symbols::Kind::Food, inverse);
      const auto label = compact
                             ? rect(picture.right() + 6, box.y + 2, box.right() - picture.right() - 12, box.height - 4)
                             : rect(box.x + 6, box.bottom() - 50, box.width - 12, 44);
      copy(s, label, I18N.get(names[n]), false, fui::TextAlign::Center, inverse);
    }
    if (hint) copy(s, rect(inner.x, inner.bottom() - 36, inner.width, 28), hint, false, fui::TextAlign::Center);
    return;
  }
  const bool wide = s.device().width > s.device().height;
  const auto stock =
      wide ? rect(inner.x, inner.y, inner.width * 28 / 100, inner.height - 4) : rect(inner.x, inner.y, inner.width, 96);
  actionsurface::panel(s, stock);
  const int stockIcon = wide ? 40 : 56;
  const auto stockPicture = wide ? rect(stock.x + (stock.width - stockIcon) / 2, stock.y + 12, stockIcon, stockIcon)
                                 : rect(stock.x + 14, stock.y + (stock.height - stockIcon) / 2, stockIcon, stockIcon);
  if (!foodPicture(m, s.target(), stockPicture, 6, 0)) symbols::draw(s, stockPicture, symbols::Kind::Food);
  std::snprintf(w.label, sizeof(w.label), tr(STR_PET_FOOD_STOCK), static_cast<unsigned long long>(pet::food(p)));
  const auto stockText =
      wide
          ? rect(stock.x + 8, stockPicture.bottom() + 12, stock.width - 16, stock.bottom() - stockPicture.bottom() - 24)
          : rect(stockPicture.right() + 14, stock.y + 12, stock.right() - stockPicture.right() - 28, stock.height - 24);
  if (p.resting) {
    const int lineH = 28;
    const int y = stockText.y + std::max(0, (int(stockText.height) - lineH * 2 - 4) / 2);
    copy(s, rect(stockText.x, y, stockText.width, lineH), w.label, true,
         wide ? fui::TextAlign::Center : fui::TextAlign::Left, false, 1, 0);
    copy(s, rect(stockText.x, y + lineH + 4, stockText.width, lineH), tr(STR_PET_GAME_WAKE_SHORT), true,
         fui::TextAlign::Center, false, 1, 3);
  } else {
    copy(s, stockText, w.label, true, wide ? fui::TextAlign::Center : fui::TextAlign::Left, false, 1, 0);
  }
  if (!pet::canCare(p)) {
    const auto guidance = wide ? rect(stock.right() + 12, inner.y, inner.right() - stock.right() - 12, inner.height)
                               : rect(inner.x, stock.bottom() + 14, inner.width, inner.bottom() - stock.bottom() - 18);
    prose(s, guidance, tr(STR_PET_EGG_HEALTH), 5);
    return;
  }
  const auto choices = wide ? rect(stock.right() + 12, inner.y, inner.right() - stock.right() - 12, inner.height - 4)
                            : rect(inner.x, stock.bottom() + 14, inner.width, inner.bottom() - stock.bottom() - 18);
  const char* feedHint = !m.writable ? tr(STR_PET_CARE_NO_SAVE) : nullptr;
  if (m.writable && !p.resting) {
    if (p.saciety >= 100) {
      feedHint = tr(STR_PET_ALREADY_FULL);
    } else if (!pet::food(p) || p.spentFood == UINT32_MAX) {
      feedHint = tr(STR_PET_SIMPLE_NO_FOOD);
    } else {
      std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_CARE_FOOD_GAIN),
                    std::min(pet::FOOD_FULLNESS_GAIN, 100u - p.saciety));
      feedHint = w.secondary;
    }
  }
  careTile(s, m, w, cell(choices, 0, 3, 10), OpenFood, tr(STR_PET_FEED), feedHint, careactionicons::FOOD, m.writable,
           false, p.foodChoice);

  const char* strokeHint = nullptr;
  if (m.writable && !p.resting && m.today > 0) {
    if (m.today > p.lastPetDay && p.joy < 100) {
      std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_GAME_GAIN_VALUE),
                    std::min(pet::moodGain(p), 100u - p.joy));
      strokeHint = w.secondary;
    } else if (m.today <= p.lastPetDay) {
      strokeHint = tr(STR_PET_CARE_DAILY_BONUS);
    }
  }
  careTile(s, m, w, cell(choices, 1, 3, 10), Stroke, tr(STR_PET_STROKE), strokeHint, careactionicons::STROKE,
           m.writable, false);

  const char* restHint = nullptr;
  const char* restDetail = nullptr;
  if (m.writable) {
    if (p.resting) {
      restHint = tr(STR_PET_CARE_WAKE_EFFECT);
    } else if (p.energy < 100) {
      std::snprintf(w.secondary, sizeof(w.secondary), tr(STR_PET_CARE_REST_GAIN), 100u - p.energy);
      restHint = w.secondary;
      restDetail = tr(STR_PET_CARE_REST_EFFECT);
    } else {
      restHint = tr(STR_PET_ENERGY_FULL);
    }
  }
  careTile(s, m, w, cell(choices, 2, 3, 10), Rest, p.resting ? tr(STR_PET_WAKE) : tr(STR_PET_REST), restHint,
           careactionicons::REST, m.writable, false, 0, restDetail);
}
}  // namespace petui::simple

#include "QuestScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/PageIndicator.h"
#include "../ui/RequiredResources.h"
#include "QuestActionIcons.h"
#include "QuestAvatar.h"
#include "QuestCampaign.h"
#include "QuestCampaignStory.h"
#include "QuestCatalog.h"
#include "QuestEnemyArt.h"
#include "QuestExpansionText.h"
#include "QuestJourneyData.h"
#include "QuestJourneyScreens.h"
#include "QuestMemoryArt.h"
#include "QuestMilestones.h"
#include "QuestPurchaseNotice.h"
#include "QuestReadingReturnArt.h"
#include "QuestRequestData.h"
#include "QuestRequestUi.h"
#include "QuestRequests.h"
#include "QuestSagaCovers.h"
#include "QuestSceneData.h"
#include "QuestScenePlan.h"
#include "QuestShopCompleteArt.h"
#include "QuestWidgets.h"
namespace questui {
namespace fui = freeink::ui;
namespace {
using namespace widget;
static_assert(questassets::STORY_SCENES == quest::CAMPAIGN_CHAPTERS, "Every campaign path needs its own illustration");
constexpr StrId CATEGORIES[] = {StrId::STR_QUEST_CLOTHING, StrId::STR_QUEST_BACKGROUND_SLOT, StrId::STR_QUEST_CAM_PET,
                                StrId::STR_QUEST_CAM_DECORATIONS};
constexpr const char* ROMANS[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII"};
void name(Work& w, int item) {
  if (!quest::visibleItem(item)) {
    w.name[0] = 0;
    return;
  }
  if (quest::huntOutfit(item)) {
    const int hunt = quest::huntOutfitSource(item);
    std::snprintf(w.name, sizeof(w.name), quest::specialHunt(hunt) ? "%s" : tr(STR_QUEST_HUNTS_OUTFIT_NAME),
                  requestui::outfitName(hunt));
    return;
  }
  if (item == quest::kInkRelic) {
    std::snprintf(w.name, sizeof(w.name), "%s", tr(STR_QUEST_HUNTS_INK_RELIC));
    return;
  }
  if (item >= quest::kExpandedItemStart) {
    const char* value = item <= quest::kKingDecoration
                            ? I18N.get(questexp::DECOR_NAMES[item - quest::kExpandedItemStart])
                        : item == quest::kKingBody       ? tr(STR_QUEST_CAM_EXP_CROWN_BODY)
                        : item == quest::kKingBackground ? tr(STR_QUEST_CAM_EXP_CROWN_BACKGROUND)
                                                         : tr(STR_QUEST_CAM_EXP_CROWN_PET);
    std::snprintf(w.name, sizeof(w.name), "%s", value);
    return;
  }
  const int index = item - quest::first(quest::slot(item));
  switch (quest::slot(item)) {
    case quest::Slot::Body:
      std::snprintf(w.name, sizeof(w.name), "%s", I18N.get(questcatalog::body[index]));
      break;
    case quest::Slot::Background:
      std::snprintf(w.name, sizeof(w.name), "%s", I18N.get(questcatalog::background[index]));
      break;
    case quest::Slot::Pet:
      std::snprintf(w.name, sizeof(w.name), "%s", I18N.get(questcatalog::pet[index]));
      break;
    case quest::Slot::Decoration:
      std::snprintf(w.name, sizeof(w.name), "%s", I18N.get(queststory::DECOR_NAMES[index]));
      break;
  }
}
void sprite(toybox::Screen& s, const Model& m, int item, fui::Rect box) {
  if (!m.assets || !quest::visibleItem(item)) return;
  if (quest::huntOutfit(item)) {
    questextra::paint(s.target(), box,
                      questextra::figure(*m.assets, m.rewardAssets, item, m.profile->gender, m.outfitAssets));
    return;
  }
  if (item >= quest::kExpandedItemStart) {
    if (item == quest::kKingBackground) {
      s.target().fill(box, paper);
      if (m.rewardAssets)
        m.rewardAssets->draw(s.target(), box, questassets::KING_BACKGROUND_ICON, fui::BitmapMode::Contain);
    } else {
      const auto art = item == quest::kKingBody
                           ? questextra::figure(*m.assets, m.rewardAssets, item, m.profile->gender, m.outfitAssets)
                       : item == quest::kKingPet ? questextra::pet(*m.assets, m.rewardAssets, item)
                                                 : questextra::decoration(*m.assets, m.rewardAssets, item);
      questextra::paint(s.target(), box, art);
    }
    return;
  }
  const int index = item - quest::first(quest::slot(item));
  switch (quest::slot(item)) {
    case quest::Slot::Body:
      questavatar::paint(*m.assets, s.target(), box, questassets::FIGURES + index * 2 + int(m.profile->gender),
                         questassets::FIGURE_MASKS + index * 2 + int(m.profile->gender));
      break;
    case quest::Slot::Background:
      s.target().fill(box, paper);
      m.assets->draw(s.target(), box, questassets::BACKGROUND_ICONS + index, fui::BitmapMode::Contain);
      break;
    case quest::Slot::Pet:
      questavatar::paint(*m.assets, s.target(), box, questassets::PET_ICONS + index,
                         questassets::PET_ICON_MASKS + index);
      break;
    case quest::Slot::Decoration:
      questavatar::paint(*m.assets, s.target(), box, questassets::DECORATIONS + index,
                         questassets::DECORATION_MASKS + index);
      break;
  }
}
void scene(toybox::Screen& s, const Model& m, fui::Rect r, int preview = -1,
           questavatar::Cast cast = questavatar::Cast::Equipped,
           questavatar::Framing framing = questavatar::Framing::Contain) {
  if (m.assets)
    questavatar::draw(s.target(), r, *m.profile, *m.assets, preview, 0, framing, m.rewardAssets, cast, m.outfitAssets);
}
void pager(toybox::Screen& s, Work& w, fui::Rect r, int page, int pages) {
  w.page = page;
  w.pages = pages;
  pagination(s, w, r, page + 1, pages);
}
void tile(toybox::Screen& s, Work& w, fui::Rect r, const char* label, View view, const freeink::Icon& mark,
          bool interactive = true) {
  const bool inverse = button(s, w, r, "", interactive ? OpenView : fui::NO_ACTION, int(view));
  if (r.height < 66) {
    if (view == View::Requests)
      questicons::monsterHead(s.target(), rect(r.x + 12, r.y + (r.height - 24) / 2, 24, 24), true, inverse);
    else
      icon(s, rect(r.x + 8, r.y + (r.height - 24) / 2, 32, 24), mark, inverse);
    text(s, rect(r.x + 46, r.y + 3, r.width - 52, r.height - 6), label, 3, true, fui::TextAlign::Center, inverse);
    return;
  }
  const int height = std::max(24, std::min(48, int(r.height) - 38)), width = height * 4 / 3;
  if (view == View::Requests)
    questicons::monsterHead(s.target(), rect(r.x + (r.width - height) / 2, r.y + 6, height, height), true, inverse);
  else
    icon(s, rect(r.x + (r.width - width) / 2, r.y + 6, width, height), mark, inverse);
  text(s, rect(r.x + 2, r.bottom() - 30, r.width - 4, 25), label, 0, true, fui::TextAlign::Center, inverse);
}
void metrics(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  panel(s, r);
  static constexpr StrId LABELS[] = {StrId::STR_QUEST_LEVEL_LABEL, StrId::STR_QUEST_COINS_LABEL,
                                     StrId::STR_QUEST_STREAK_LABEL, StrId::STR_QUEST_BEASTS_SHORT};
  static constexpr const freeink::Icon* MARKS[] = {&extra_circle_dot_32, &extra_trophy_32, &extra_reading_flame_32,
                                                   &extra_reading_book_32};
  const auto& p = *m.profile;
  for (int i = 0; i < 4; ++i) {
    const auto box = rect(r.x + r.width * i / 4, r.y, r.width * (i + 1) / 4 - r.width * i / 4, r.height);
    if (i) s.target().fill(rect(box.x, box.y + 10, 1, box.height - 20), ink);
    if (!i && m.view != View::Experience) {
      s.frame().hit(box, ShowExperience, 0, fui::InputDefault);
      if (s.frame().buttonSelected(ShowExperience, 0, false)) s.target().stroke(box.inset({3, 3, 3, 3}), ink, 2, 8);
    }
    const char* caption = I18N.get(LABELS[i]);
    const int labelWidth = s.target().measureText(3, caption, {}).width;
    const int left = box.x + std::max(3, (int(box.width) - labelWidth - 23) / 2);
    if (box.width < 100) {
      text(s, rect(box.x + 2, box.y + 5, box.width - 4, 24), caption, 3, false, fui::TextAlign::Center);
    } else {
      if (i == 1)
        questicons::coin(s.target(), rect(left, box.y + 7, 19, 19));
      else if (i == 3)
        questicons::monsterHead(s.target(), rect(left, box.y + 7, 19, 19), false);
      else
        icon(s, rect(left, box.y + 7, 19, 19), *MARKS[i]);
      text(s, rect(left + 23, box.y + 5, box.right() - left - 26, 24), caption, 3);
    }
    const uint64_t value = i == 0   ? quest::level(p.xp)
                           : i == 1 ? quest::coins(p)
                           : i == 2 ? quest::adventureStreak(p, *m.summary, m.today)
                                    : quest::defeatedBeasts(p);
    std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(value));
    text(s, rect(box.x + 4, box.y + 31, box.width - 8, box.height - 37), w.label, 0, true, fui::TextAlign::Center);
  }
}
void experience(toybox::Screen& s, const quest::Profile& p, Work& w, fui::Rect r) {
  const auto level = quest::level(p.xp);
  const bool maximum = level == quest::MAX_LEVEL;
  const uint64_t span = maximum ? quest::threshold(level) : quest::threshold(level + 1) - quest::threshold(level);
  const uint64_t value = maximum ? span : p.xp - quest::threshold(level);
  text(s, rect(r.x + 4, r.y, r.width / 2 - 8, 26), tr(STR_QUEST_EXPERIENCE), 0, true);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_XP), static_cast<unsigned long long>(value),
                static_cast<unsigned long long>(span));
  text(s, rect(r.x + r.width / 2, r.y, r.width / 2 - 4, 26), w.label, 3, false, fui::TextAlign::Right);
  progress(s, rect(r.x + 4, r.bottom() - 12, r.width - 8, 9), value, span);
}
void bonusStrip(toybox::Screen& s, const Model& m, Work& w, fui::Rect r);
void experienceDetails(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const auto level = quest::level(p.xp);
  const bool maximum = level == quest::MAX_LEVEL;
  const uint64_t span = maximum ? 1 : quest::threshold(level + 1) - quest::threshold(level);
  const uint64_t value = maximum ? 1 : p.xp - quest::threshold(level);
  constexpr int desiredWidth = 440;
  const int width = std::min(desiredWidth, int(r.width)), height = std::min(432, int(r.height));
  const bool compact = height < 420, tight = height < 360;
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  panel(s, box);
  icon(s, rect(box.x + 18, box.y + 18, 36, 36), extra_trophy_64);
  text(s, rect(box.x + 66, box.y + 14, box.width - 84, 42), tr(STR_QUEST_EXPERIENCE), 0, true);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_LEVEL), int(level));
  text(s,
       rect(box.x + 16,
            box.y + (tight     ? 46
                     : compact ? 54
                               : 66),
            box.width - 32, 32),
       w.label, 1, true, fui::TextAlign::Center);
  if (maximum)
    std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(p.xp));
  else
    std::snprintf(w.label, sizeof(w.label), "%llu / %llu", static_cast<unsigned long long>(value),
                  static_cast<unsigned long long>(span));
  text(s,
       rect(box.x + 16,
            box.y + (tight     ? 78
                     : compact ? 90
                               : 106),
            box.width - 32, compact ? 40 : 58),
       w.label, COIN_VALUE_FONT, false, fui::TextAlign::Center);
  progress(s, rect(box.x + 24, box.y + (tight ? 120 : compact ? 140 : 176), box.width - 48, 10), value, span);
  text(s,
       rect(box.x + 20,
            box.y + (tight     ? 136
                     : compact ? 160
                               : 196),
            box.width - 40, 24),
       tr(STR_QUEST_CAM_BONUSES), 3, true);
  bonusStrip(s, m, w, rect(box.x + 20, box.y + (tight ? 164 : compact ? 188 : 226), box.width - 40, tight ? 36 : 48));
  const int hintY = tight ? 202 : compact ? 246 : 292;
  const int hintHeight = std::max(0, height - 74 - hintY);
  if (maximum) {
    text(s, rect(box.x + 20, box.y + hintY, box.width - 40, std::min(66, hintHeight)), tr(STR_QUEST_MILESTONE_150), 4,
         false, fui::TextAlign::Center, false, 3);
  } else {
    text(s, rect(box.x + 20, box.y + hintY, box.width / 2 - 28, std::min(38, hintHeight)),
         tr(STR_QUEST_COLLECTION_AFTER_PURCHASE), 4);
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_VICTORY_XP_PRIZE),
                  static_cast<unsigned long long>(span - value));
    text(s, rect(box.x + box.width / 2, box.y + hintY, box.width / 2 - 20, std::min(38, hintHeight)), w.label, 0, true,
         fui::TextAlign::Right);
  }
  button(s, w, rect(box.x + 16, box.bottom() - 66, box.width - 32, 50), tr(STR_DONE), Back, 0, true);
}

void heroName(toybox::Screen& s, const Model& m, fui::Rect r) {
  text(s, rect(r.x, r.y, r.width, 28), m.profile->name[0] ? m.profile->name : tr(STR_QUEST_HERO_NAME_DEFAULT), 0, true,
       fui::TextAlign::Center);
  text(s, rect(r.x, r.y + 32, r.width, r.height - 32), rankTitle(quest::level(m.profile->xp)), 0, false,
       fui::TextAlign::Center);
}
void genderCards(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, int chosen) {
  for (int n = 0; n < 2; ++n) {
    const int width = (r.width - GAP) / 2;
    const auto box = rect(r.x + n * (width + GAP), r.y, width, r.height);
    button(s, w, box, "", Gender, n, false, m.writable);
    s.target().fill(box.inset({3, 3, 3, 3}), paper, RADIUS - 2);
    if (m.assets)
      questavatar::paint(*m.assets, s.target(), rect(box.x + 12, box.y + 12, box.width - 24, box.height - 58),
                         questassets::FIGURES + n, questassets::FIGURE_MASKS + n);
    text(s, rect(box.x + 8, box.bottom() - 40, box.width - 16, 30),
         n ? tr(STR_QUEST_FEMININE) : tr(STR_QUEST_MASCULINE), 0, true, fui::TextAlign::Center);
    if (chosen == n) {
      s.target().stroke(box, ink, 4, RADIUS);
      icon(s, rect(box.right() - 35, box.y + 9, 26, 26), extra_checked_32);
    }
  }
}
void naming(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = s.device().width > s.device().height;
  const auto left = rect(r.x, r.y, wide ? r.width * 45 / 100 : r.width, r.height);
  text(s, rect(left.x, left.y, left.width, 38), m.creating ? tr(STR_QUEST_CAM_YOUR_NAME) : tr(STR_QUEST_NAME_TITLE), 0,
       true, fui::TextAlign::Center);
  const auto controls = wide ? rect(left.right() + GAP, r.y, r.right() - left.right() - GAP, r.height) : r;
  const auto footer = rect(controls.x, r.bottom() - 54, controls.width, 54);
  const auto space = rect(controls.x, footer.y - GAP - 44, controls.width, 44);
  const int keysHeight = wide ? 140 : 200;
  const auto keys = rect(controls.x, space.y - GAP - keysHeight, controls.width, keysHeight);
  const auto field = rect(left.x, wide ? left.bottom() - 94 : keys.y - GAP - 94, left.width, 94);
  const auto picture = rect(left.x, left.y + 104, left.width, std::max(1, field.y - left.y - 116));
  if (m.assets) {
    questextra::paint(s.target(), picture,
                      questextra::figure(*m.assets, m.rewardAssets, m.profile->equipped[0],
                                         quest::Gender(std::clamp(m.chosenGender, 0, 1)), m.outfitAssets));
  }
  panel(s, field);
  text(s, rect(field.x + 12, field.y + 8, field.width - 120, 24), tr(STR_QUEST_NAME_LABEL), 3);
  std::snprintf(w.label, sizeof(w.label), "%u / %u", quest::nameLength(m.nameDraft), quest::NAME_LIMIT);
  text(s, rect(field.right() - 100, field.y + 8, 88, 24), w.label, 3, false, fui::TextAlign::Right);
  text(s, rect(field.x + 12, field.y + 36, field.width - 24, 46),
       m.nameDraft[0] ? m.nameDraft : tr(STR_QUEST_NAME_PLACEHOLDER), 0, true, fui::TextAlign::Center);
  w.keys = wordlekeys::keyboardLayout(keys, keys.y);
  if (wide) w.keys.keyH = 44;
  wordlekeys::draw(s, keys, w.keys, NameKey);
  button(s, w, space, tr(STR_PET_NAME_SPACE), NameKey, ' ');
  button(s, w, footer, m.creating ? tr(STR_QUEST_CAM_START) : tr(STR_QUEST_SAVE_NAME), ConfirmName, 0, true,
         m.writable && m.nameDraft[0] != 0);
}

void campaignAdvance(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto value = quest::campaignProgress(*m.profile);
  icon(s, rect(r.x + 2, r.y, 22, 22), extra_reading_book_32);
  text(s, rect(r.x + 34, r.y, r.width - 114, 26), tr(STR_QUEST_CAM_TITLE), 0, true);
  if (value > 0 && value < 100)
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_FRACTION), unsigned(value / 100),
                  unsigned(value / 10 % 10));
  else
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_PERCENT), unsigned(value / 100));
  text(s, rect(r.right() - 78, r.y, 76, 26), w.label, 0, true, fui::TextAlign::Right);
  progress(s, rect(r.x + 2, r.y + 32, r.width - 4, 9), value, 10000);
  const auto minutes = quest::campaignMinutes(*m.profile);
  if (m.profile->campaign.state == quest::AdventureState::Active &&
      m.profile->campaign.phase != quest::BattlePhase::Complete)
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_BATTLE_MINUTE_PROGRESS),
                  static_cast<unsigned long long>(minutes.read), static_cast<unsigned long long>(minutes.target));
  else
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_RETURN_TIME),
                  static_cast<unsigned long long>(minutes.read));
  text(s, rect(r.x + 2, r.y + 42, r.width - 4, 24), w.label, 0, false);
}
void nextEncounter(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool discovery = quest::visibleItem(p.campaign.pendingReward);
  const bool campaignReward =
      p.campaign.phase == quest::BattlePhase::Victory || p.campaign.phase == quest::BattlePhase::Story;
  const bool postgame = quest::hasWon(p) && p.endingSeen;
  const int huntId = quest::validRequest(p.activeRequest) ? p.activeRequest
                     : postgame                           ? quest::requestAt(p, false, 0)
                                                          : -1;
  const bool hunt = !discovery && !campaignReward && quest::validRequest(huntId);
  const bool opening = !hunt && !discovery && !campaignReward &&
                       (quest::needsPrologue(p) || quest::needsIntroduction(p) || quest::needsEncounterStory(p));
  const bool complete = !hunt && p.campaign.phase == quest::BattlePhase::Complete;
  const bool selected = button(s, w, r, "",
                               m.view == View::Experience ? fui::NO_ACTION
                               : hunt                     ? RequestInfo
                               : postgame                 ? OpenHuntCollection
                                                          : ResumeJourney,
                               hunt ? huntId : 0);
  if (selected) s.target().fill(r.inset({4, 4, 4, 4}), paper, RADIUS - 4);
  const int left = r.x + 66, right = r.right() - 32;
  const auto art = rect(r.x + 10, r.y + (r.height - 44) / 2, 44, 44);
  if (discovery) {
    sprite(s, m, p.campaign.pendingReward, art);
  } else if (hunt || (!opening && !complete)) {
    questicons::bow(s.target(), art);
  } else {
    icon(s, art, complete ? extra_trophy_32 : questaction::sagaTab);
  }
  const auto* mode = (hunt || postgame) ? tr(STR_QUEST_RETURN_HUNT_ONLY) : tr(STR_QUEST_CAM_TITLE);
  fui::TextStyle tagStyle;
  tagStyle.bold = true;
  const int tagWidth = int(s.target().measureText(3, mode, tagStyle).width) + 20;
  const int statusY = r.bottom() - 8 - 22;
  statusPill(s, rect(left, statusY, tagWidth, 26), mode, true, nullptr, 3);
  const char* title;
  if (discovery) {
    name(w, p.campaign.pendingReward);
    title = w.name;
  } else if (hunt) {
    title = huntTitle(huntId);
  } else if (complete) {
    title = postgame ? tr(STR_QUEST_POSTGAME_COLLECTION) : tr(STR_QUEST_CAM_WORLD_SAFE);
  } else if (opening) {
    title = quest::needsPrologue(p) ? tr(STR_QUEST_CAM_BEGIN_INVITE) : tr(STR_QUEST_CAM_BATTLE_CHOOSE_PATH);
  } else {
    title = journeyTitle(p.campaign.encounter);
  }
  fui::TextStyle titleStyle;
  titleStyle.bold = true;
  const int titleFont = s.target().measureText(0, title, titleStyle).width <= right - left ? 0 : 3;
  text(s, rect(left, r.y + 3, right - left, 32), title, titleFont, true, fui::TextAlign::Left, false, 2);
  uint32_t value = 0, target = 0;
  const char* status;
  if (discovery || campaignReward) {
    status = tr(STR_QUEST_CAM_RECEIVE);
  } else if (hunt) {
    value = quest::requestValue(p, huntId, m.summary);
    target = quest::requestTarget(p, huntId);
    if (p.activeRequest != huntId) {
      std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_QUEST_POSTGAME_DISCOVER));
    } else if (quest::requestGoal(p, huntId) == quest::RequestGoal::Minutes)
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_ORDER_PROGRESS_MINUTES), unsigned(value),
                    unsigned(target));
    else
      requestui::goalText(w, m, huntId, true, true);
    status = value >= target ? tr(STR_QUEST_CAM_RECEIVE) : w.label;
  } else if (opening) {
    status = quest::needsPrologue(p) ? tr(STR_QUEST_CAM_START) : tr(STR_QUEST_CAM_AVAILABLE);
  } else if (complete) {
    status = postgame ? tr(STR_QUEST_POSTGAME_REVISIT) : tr(STR_QUEST_CAM_DONE);
  } else {
    value = p.campaign.encounterSeconds;
    target = quest::encounterTarget(p);
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_ORDER_PROGRESS_MINUTES),
                  unsigned(std::min(value, target) / 60), unsigned((target + 59) / 60));
    status = w.label;
  }
  const int statusLeft = left + tagWidth + 8;
  text(s, rect(statusLeft, statusY, right - statusLeft, 26), status, 3);
  questicons::chevron(s.target(), rect(r.right() - 24, r.y + (r.height - 18) / 2, 14, 18));
}
void dashboard(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = s.device().width > s.device().height;
  const auto side = wide ? rect(r.right() - r.width * 46 / 100, r.y, r.width * 46 / 100, r.height) : r;
  const int footerHeight = 72;
  const auto footer = rect(side.x, r.bottom() - footerHeight, side.width, footerHeight);
  const auto actions =
      wide ? rect(side.x, side.y + 84, side.width, footer.y - side.y - 96) : rect(r.x, footer.y - 98, r.width, 88);
  metrics(s, m, w, rect(side.x, side.y, side.width, 72));
  const auto identity = wide ? rect(r.x, r.y - 4, side.x - r.x - GAP, 64) : rect(r.x, r.y + 78, r.width, 64);
  const auto safe = s.device().safeRect();
  constexpr int identityGap = 8;
  const int sceneTop = identity.bottom() + (wide ? 0 : identityGap);
  const int sceneHeight = (wide ? r.bottom() : actions.y - 6) - identity.bottom() - (wide ? 0 : 8);
  scene(
      s, m,
      wide ? rect(identity.x, sceneTop, identity.width, sceneHeight) : rect(safe.x, sceneTop, safe.width, sceneHeight),
      -1, questavatar::Cast::Equipped, questavatar::Framing::Cover);
  heroName(s, m, identity);

  static constexpr StrId LABELS[] = {StrId::STR_QUEST_CAM_TITLE, StrId::STR_QUEST_QUESTS, StrId::STR_QUEST_EQUIPMENT,
                                     StrId::STR_QUEST_GEAR_SHOP};
  static constexpr View VIEWS[] = {View::Campaign, View::Missions, View::Gear, View::Shop};
  static constexpr const freeink::Icon* MARKS[] = {&questaction::campaign, &questaction::missions,
                                                   &questaction::equipment, &questaction::shop};
  const int columns = wide ? 2 : 4, rows = wide ? 2 : 1;
  const int width = (actions.width - (columns - 1) * 9) / columns, height = (actions.height - (rows - 1) * 9) / rows;
  for (int n = 0; n < 4; ++n) {
    const auto box = rect(actions.x + n % columns * (width + 9), actions.y + n / columns * (height + 9), width, height);
    const bool hunts = n == 0 && quest::hasWon(*m.profile) && m.profile->endingSeen;
    tile(s, w, box, hunts ? tr(STR_QUEST_CAM_REQUESTS) : I18N.get(LABELS[n]), hunts ? View::Requests : VIEWS[n],
         *MARKS[n], m.view != View::Experience);
  }
  nextEncounter(s, m, w, footer);
}
void pageTitle(toybox::Screen& s, fui::Rect& r, const char* title) {
  ornamentTitle(s, rect(r.x + 2, r.y, r.width - 4, 38), title);
  r.y += 48;
  r.height -= 48;
}
void bonusStrip(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto value = quest::bonuses(*m.profile);
  const unsigned values[] = {quest::heroPower(*m.profile), value.experience, value.fortune};
  for (int n = 0; n < 3; ++n) {
    const int width = (r.width - 16) / 3;
    const auto box = rect(r.x + n * (width + 8), r.y, width, r.height);
    s.target().stroke(box, ink, 2, 12);
    if (n == 0)
      questicons::swords(s.target(), rect(box.x + 10, box.y + 10, 22, 22));
    else if (n == 1)
      questicons::book(s.target(), rect(box.x + 10, box.y + 10, 22, 22));
    else
      questicons::coin(s.target(), rect(box.x + 10, box.y + 10, 22, 22));
    std::snprintf(w.label, sizeof(w.label), n ? tr(STR_QUEST_CAM_PLUS) : tr(STR_QUEST_COLLECTION_POWER_POINTS),
                  values[n]);
    text(s, rect(box.x + 40, box.y + 5, box.width - 45, r.height - 10), w.label, 0, true, fui::TextAlign::Center);
  }
}
void missions(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  pageTitle(s, r, tr(STR_QUEST_QUESTS));
  const bool wide = s.device().width > s.device().height;
  const int width = wide ? (r.width - 2 * GAP) / 3 : r.width;
  const int height = wide ? r.height : (r.height - 2 * GAP) / 3;
  const int huntExtra = wide ? 0 : 20;
  const int missionHeight = height - huntExtra / 2;
  static constexpr StrId TITLES[] = {StrId::STR_QUEST_CAM_MISSION_DAILY, StrId::STR_QUEST_CAM_MISSION_WEEKLY,
                                     StrId::STR_QUEST_CAM_REQUESTS};
  static constexpr const freeink::Icon* MARKS[] = {&questaction::dailyMission, &questaction::weeklyMission,
                                                   &questaction::storyMission};
  for (int n = 0; n < 3; ++n) {
    const int cardHeight = n == 2 ? height + huntExtra : missionHeight;
    const auto box =
        rect(r.x + (wide ? n * (width + GAP) : 0), r.y + (wide ? 0 : n * (missionHeight + GAP)), width, cardHeight);
    panel(s, box);
    const int badge = wide ? 54 : 62, left = box.x + 16, top = box.y + 12;
    const auto badgeBox = rect(left, top, badge, badge);
    framedIcon(s, badgeBox, *MARKS[n]);
    const int divider = badgeBox.right() + 12, titleX = divider + 12;
    s.target().line({int16_t(divider), int16_t(top)}, {int16_t(divider), int16_t(top + badge)}, 2, ink);
    text(s, rect(titleX, box.y + 10, box.right() - titleX - 12, 28), I18N.get(TITLES[n]), wide ? 3 : 0, true);
    const auto descriptionBox = wide ? rect(box.x + 16, box.y + 82, box.width - 32, n == 2 ? 64 : 48)
                                     : rect(titleX, box.y + 42, box.right() - titleX - 12, 36);
    if (n == 2)
      text(s, descriptionBox, tr(STR_QUEST_CAM_REQUEST_HINT), 0, false, fui::TextAlign::Left, false, 2);
    else {
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_MINUTES_TARGET), quest::missionTarget(quest::Mission(n)));
      text(s, descriptionBox, w.label, 0, false, fui::TextAlign::Left, false, 2);
    }
    const auto action = wide ? rect(box.x + 16, box.bottom() - 58, box.width - 32, 44)
                             : rect(box.right() - 142, box.bottom() - 58, 126, 44);
    if (n == 2) {
      const int copyTop = box.y + (wide ? 158 : 88);
      const unsigned available = quest::requestCount(*m.profile, false),
                     collected = quest::requestCount(*m.profile, true);
      const char* description = quest::validRequest(m.profile->activeRequest) ? huntTitle(m.profile->activeRequest)
                                : !m.profile->campaign.prologueSeen           ? tr(STR_QUEST_CAM_REQUEST_START)
                                : available                                   ? tr(STR_QUEST_CAM_HUNT_AVAILABLE_HINT)
                                : quest::hasWon(*m.profile) ? tr(STR_QUEST_CAM_REQUEST_COMPLETE_COLLECTION)
                                : collected                 ? tr(STR_QUEST_CAM_REQUEST_ALL_COLLECTED)
                                                            : tr(STR_QUEST_CAM_REQUEST_EMPTY);
      text(s, rect(box.x + 16, copyTop, box.width - 32, std::max(0, action.y - copyTop - 8)), description, 0, false,
           fui::TextAlign::Left, false, wide ? 4 : 3);
      button(s, w, rect(box.x + 16, action.y, box.width - 32, action.height), tr(STR_QUEST_CAM_OPEN_REQUESTS), OpenView,
             int(View::Requests), true);
      continue;
    }
    const auto type = quest::Mission(n);
    const auto target = quest::missionTarget(type);
    const auto value = std::min<uint64_t>(target, quest::missionValue(*m.profile, *m.summary, m.today, type));
    const int barY = box.y + (wide ? 126 : cardHeight < 200 ? 82 : 94);
    progress(s, rect(box.x + 16, barY, box.width - 32, 8), value, target);
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_PROGRESS_MINUTES), static_cast<unsigned long long>(value),
                  target);
    text(s, rect(box.x + 16, barY + 12, box.width - 32, 30), w.label, 0);
    const bool claimed = quest::missionClaimed(*m.profile, m.today, type);
    button(s, w, action, claimed ? tr(STR_QUEST_CAM_DONE) : tr(STR_QUEST_CAM_RECEIVE), Claim, n,
           !claimed && value >= target, m.writable && !claimed && value >= target);
    const auto reward = wide ? rect(box.x + 16, action.y - 54, box.width - 32, 44)
                             : rect(box.x + 16, action.y, action.x - box.x - 28, 44);
    s.target().stroke(reward, ink, 2, 12);
    questicons::coin(s.target(), rect(reward.x + 8, reward.y + 11, 22, 22));
    const int coinWidth = std::max(62, int(reward.width) * 38 / 100);
    std::snprintf(w.label, sizeof(w.label), "%u", unsigned(quest::missionCoins(type)));
    text(s, rect(reward.x + 36, reward.y + 8, coinWidth - 38, 28), w.label, 0);
    icon(s, rect(reward.x + coinWidth, reward.y + 11, 22, 22), extra_trophy_32);
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_XP), unsigned(quest::missionXp(type)));
    text(s, rect(reward.x + coinWidth + 28, reward.y + 8, reward.width - coinWidth - 32, 28), w.label, 0);
  }
}
void bonusValue(toybox::Screen& s, Work& w, fui::Rect r, int kind, unsigned value) {
  const int side = std::min(24, int(r.height) - 4), top = r.y + (r.height - side) / 2;
  if (kind == 0 || kind == 3)
    questicons::swords(s.target(), rect(r.x, top, side, side));
  else if (kind == 1)
    questicons::book(s.target(), rect(r.x, top, side, side));
  else
    questicons::coin(s.target(), rect(r.x, top, side, side));
  std::snprintf(w.label, sizeof(w.label),
                kind == 0 || kind == 3 ? tr(STR_QUEST_COLLECTION_POWER_GAIN) : tr(STR_QUEST_CAM_PLUS), value);
  text(s, rect(r.x + side + 7, r.y, r.width - side - 7, r.height), w.label, r.height < 26 ? 3 : 0, true);
}
void equipmentPreview(toybox::Screen& s, const Model& m, int item, fui::Rect art) {
  if (quest::slot(item) != quest::Slot::Background) {
    sprite(s, m, item, art);
  } else if (item == quest::kKingBackground && m.rewardAssets) {
    questextra::tiles(s.target(), questavatar::sceneRect(art), *m.rewardAssets, questassets::KING_BACKGROUND);
  } else if (m.assets) {
    questavatar::background(s.target(), questavatar::sceneRect(art), *m.assets,
                            item - quest::first(quest::Slot::Background));
  }
}
int itemTitleFont(toybox::Screen& s, const char* title, int width, bool compact) {
  const int preferred = compact ? 3 : 4;
  fui::TextStyle style;
  style.bold = true;
  char word[64];
  unsigned used = 0;
  int lineWidth = 0, lines = 1;
  const int spaceWidth = s.target().measureText(preferred, " ", style).width;
  for (const char* at = title;; ++at) {
    if (*at == ' ' || !*at) {
      word[used] = 0;
      const int wordWidth = s.target().measureText(preferred, word, style).width;
      if (wordWidth > width) return 3;
      if (lineWidth && lineWidth + spaceWidth + wordWidth > width) {
        if (++lines > 2) return 3;
        lineWidth = wordWidth;
      } else {
        lineWidth += (lineWidth ? spaceWidth : 0) + wordWidth;
      }
      used = 0;
      if (!*at) return preferred;
    } else {
      if (used + 1 == sizeof(word)) return 3;
      word[used++] = *at;
    }
  }
}
void purchasePrice(toybox::Screen& s, Work& w, fui::Rect box, int item, bool unlocked) {
  const bool compact = box.height < 100;
  panel(s, box);
  s.target().fill(box.inset({2, 2, 2, 2}), ink, RADIUS - 2);
  text(s, rect(box.x + 16, box.y + (compact ? 4 : 7), box.width - 32, compact ? 18 : 22),
       tr(STR_QUEST_COLLECTION_PRICE), 3, true, fui::TextAlign::Left, true);
  const auto mark = rect(box.x + 14, box.y + (compact ? 29 : 37), 36, 36);
  s.target().fill(mark, paper, 18);
  questicons::coin(s.target(), mark.inset({5, 5, 5, 5}));
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_PRICE_COINS), unsigned(quest::price(item)));
  text(s, rect(box.x + 64, box.y + (compact ? 23 : 30), box.width - 80, compact ? 38 : 42), w.label, 1, true,
       fui::TextAlign::Left, true);
  const int path = quest::pathUnlockChapter(item);
  if (path >= 0) {
    if (unlocked)
      std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_QUEST_VICTORY_PATH_UNLOCKED));
    else
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_REWARD_SOURCE), ROMANS[path / 10],
                    unsigned(path % 10 + 1));
  } else
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_REQUIRES_LEVEL), unsigned(quest::shopLevel(item)));
  text(s, rect(box.x + 64, box.y + (compact ? 62 : 73), box.width - 80, compact ? 20 : 25), w.label, compact ? 3 : 0,
       false, fui::TextAlign::Left, true);
}
void itemDetail(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const int item = m.preview;
  const bool wide = s.device().width > s.device().height;
  const bool owned = quest::owns(p, item), usingItem = quest::equipped(p, item),
             exclusive = quest::storyExclusive(item);
  const bool liveEncounter =
      p.campaign.state == quest::AdventureState::Active && p.campaign.phase == quest::BattlePhase::Reading;
  name(w, item);
  pageTitle(s, r, w.name);
  const auto action = rect(r.x, r.bottom() - 54, r.width, 54);
  const auto picture =
      wide ? rect(r.x, r.y, r.width * 44 / 100, r.height)
           : rect(r.x, r.y, r.width,
                  std::max(120, int(r.height) - (owned ? 344 : 390) + (liveEncounter ? 0 : 108) + (owned ? 70 : 0)));
  panel(s, picture);
  constexpr int artInset = 16;
  const auto art = picture.inset({artInset, artInset, 42, artInset});
  equipmentPreview(s, m, item, art);
  if (m.view == View::Gear && owned) {
    const auto favorite = rect(picture.right() - 58, picture.y + 12, 44, 44);
    const bool inverse = button(s, w, favorite, "", FavoriteItem, item, false, m.writable);
    questicons::star(s.target(), favorite.inset({7, 7, 7, 7}), quest::isFavorite(p, item), inverse);
  }
  const auto state = rect(picture.x + 12, picture.bottom() - 32, picture.width - 24, 24);
  text(s, state,
       usingItem ? tr(STR_QUEST_EQUIPPED)
       : owned   ? tr(STR_QUEST_CAM_EXP_DETAIL_OWNED)
                 : tr(STR_QUEST_COLLECTION_PREVIEW),
       0, false, fui::TextAlign::Center);
  const auto controls = wide ? rect(picture.right() + GAP, r.y, r.right() - picture.right() - GAP, r.height)
                             : rect(r.x, picture.bottom() + GAP, r.width, action.y - picture.bottom() - 2 * GAP);
  const auto effect = quest::gearBonus(item, quest::book(p));
  const unsigned values[] = {unsigned(effect.power + effect.affinity), effect.experience, effect.fortune};
  static constexpr StrId labels[] = {StrId::STR_QUEST_COLLECTION_POWER, StrId::STR_QUEST_CAM_EXP_DETAIL_XP,
                                     StrId::STR_QUEST_CAM_EXP_DETAIL_COINS};
  const bool compact = wide;
  const int gap = compact ? 8 : GAP, benefitHeight = compact ? 58 : 76;
  const bool buying = !owned && !exclusive;
  for (int n = 0; n < 3; ++n) {
    const int width = (controls.width - 16) / 3;
    const auto card = rect(controls.x + n * (width + 8), controls.y, width, benefitHeight);
    panel(s, card);
    const auto value = rect(card.x + 12, card.y + (compact ? 4 : 10), card.width - 24, 32);
    bonusValue(s, w, value, n, values[n]);
    text(s, rect(card.x + 4, card.y + (compact ? 33 : 43), card.width - 8, compact ? 22 : 26), I18N.get(labels[n]),
         compact ? 3 : 0, false, fui::TextAlign::Center);
  }
  const auto compare = rect(controls.x, controls.y + benefitHeight + gap, controls.width, compact ? 80 : 96);
  if (liveEncounter) {
    panel(s, compare);
    icon(s, rect(compare.x + 12, compare.y + (compact ? 11 : 18), 28, 28), extra_clock_32);
    text(s, rect(compare.x + 52, compare.y + 5, compare.width - 64, 24), tr(STR_QUEST_CAM_EXP_DETAIL_COMPARE), 0);
    const auto duration = rect(compare.x + 52, compare.y + 36, compare.width - 68, compare.height - 42);
    const unsigned current = quest::encounterTarget(p) / 60, proposed = quest::encounterTarget(p, item) / 60;
    const int center = duration.x + duration.width / 2;
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_VICTORY_MINUTES), current);
    text(s, rect(duration.x, duration.y, duration.width / 2 - 26, duration.height), w.label, 0, true,
         fui::TextAlign::Center);
    const int y = duration.y + duration.height / 2;
    s.target().line({int16_t(center - 18), int16_t(y)}, {int16_t(center + 18), int16_t(y)}, 3, ink);
    s.target().line({int16_t(center + 10), int16_t(y - 8)}, {int16_t(center + 18), int16_t(y)}, 3, ink);
    s.target().line({int16_t(center + 10), int16_t(y + 8)}, {int16_t(center + 18), int16_t(y)}, 3, ink);
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_VICTORY_MINUTES), proposed);
    text(s, rect(center + 26, duration.y, duration.width / 2 - 26, duration.height), w.label, 0, true,
         fui::TextAlign::Center);
  }
  const int sourceY = liveEncounter ? compare.bottom() + gap : controls.y + benefitHeight + gap;
  const auto source = rect(controls.x, sourceY, controls.width, owned ? 58 : compact ? 90 : 104);
  if (buying)
    purchasePrice(s, w, source, item, quest::shopUnlocked(p, item));
  else if (!owned) {
    panel(s, source);
    icon(s, rect(source.x + 12, source.y + 22, 28, 28), exclusive ? questaction::storyMission : questaction::shop);
    text(s, rect(source.x + 52, source.y + 5, source.width - 64, 24),
         quest::slot(item) == quest::Slot::Decoration ? tr(STR_QUEST_CAM_EXP_DETAIL_DECOR)
         : owned                                      ? tr(STR_QUEST_CAM_EXP_DETAIL_OWNED)
                                                      : tr(STR_QUEST_CAM_EXP_DETAIL_SOURCE),
         0);
    const int saga = quest::rewardSaga(item);
    if (quest::huntOutfit(item) || quest::huntExtraSource(item) >= 0) {
      std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_QUEST_HUNTS_REWARD_SOURCE));
    } else if (saga >= 0) {
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_SAGA_REWARD_SOURCE), ROMANS[saga]);
    } else if (item >= quest::kExpandedItemStart) {
      const auto origin = item <= quest::kKingDecoration
                              ? questexp::DECORATIONS[item - quest::kExpandedItemStart].source
                              : questexp::Source::Victory;
      if (origin == questexp::Source::Chapter) {
        const auto chapter = questexp::DECORATIONS[item - quest::kExpandedItemStart].sourceId;
        std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_EXP_FROM_CHAPTER), unsigned(chapter % 10 + 1),
                      ROMANS[chapter / 10]);
      } else
        std::snprintf(w.label, sizeof(w.label), "%s",
                      origin == questexp::Source::Request ? tr(STR_QUEST_CAM_EXP_FROM_REQUEST)
                                                          : tr(STR_QUEST_CAM_EXP_FROM_VICTORY));
    } else if (exclusive) {
      const int encounter = quest::rewardEncounter(item);
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_REWARD_SOURCE), ROMANS[encounter / 60],
                    unsigned(encounter / 6 % 10 + 1));
    } else if (!owned)
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_EXP_DETAIL_PRICE), quest::price(item),
                    quest::shopLevel(item));
    else
      std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_QUEST_CAM_AVAILABLE));
    text(s, rect(source.x + 52, source.y + 35, source.width - 64, compact ? 48 : 58), w.label, compact ? 3 : 0, false,
         fui::TextAlign::Left, false, 2);
  }
  const int actionHeight = compact ? 44 : 54;
  const auto buttonBox =
      wide ? rect(controls.x, controls.bottom() - actionHeight, controls.width, actionHeight) : action;
  const bool removable = quest::slot(item) != quest::Slot::Body;
  button(s, w, buttonBox,
         usingItem   ? removable ? tr(STR_QUEST_UNEQUIP) : tr(STR_QUEST_EQUIPPED)
           : owned ? tr(STR_QUEST_EQUIP)
         : exclusive ? (quest::huntOutfit(item) || quest::huntExtraSource(item) >= 0 ? tr(STR_QUEST_HUNTS_REWARD_ACTION)
                                                                                     : tr(STR_QUEST_CAM_STORY_ONLY))
                     : tr(STR_QUEST_BUY),
         Outfit, item, true,
         m.writable && (owned || (!exclusive && quest::shopUnlocked(p, item))) && (!usingItem || removable));
}
void purchaseConfirm(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const int item = m.preview;
  if (!quest::availableItem(item)) return;
  if (quest::coins(*m.profile) < quest::price(item)) {
    insufficientCoins(s, m, w, r, item);
    return;
  }
  pageTitle(s, r, tr(STR_QUEST_COLLECTION_BUY_CONFIRM));
  const bool wide = s.device().width > s.device().height;
  const int width = std::min(wide ? 752 : 440, int(r.width)), height = std::min(wide ? 326 : 620, int(r.height));
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  panel(s, box);
  const auto inner = box.inset({14, 14, 14, 14});
  const int artWidth = wide ? inner.width * 36 / 100 : inner.width;
  const int portraitTop = inner.bottom() - 108 - GAP - 58 - GAP - 104;
  const auto art = wide ? rect(inner.x, inner.y, artWidth, inner.height)
                        : rect(inner.x, inner.y + 52, inner.width, std::max(0, portraitTop - GAP - inner.y - 52));
  equipmentPreview(s, m, item, art);
  const auto side = wide ? rect(art.right() + GAP, inner.y, inner.right() - art.right() - GAP, inner.height) : inner;
  name(w, item);
  text(s, rect(side.x, side.y, side.width, 48), w.name, 0, true, fui::TextAlign::Left, false, 2);
  const bool compact = wide && inner.height < 274;
  const int top = wide ? side.y + (compact ? 52 : 56) : portraitTop;
  purchasePrice(s, w, rect(side.x, top, side.width, 104), item, quest::shopUnlocked(*m.profile, item));
  const auto wallet = rect(side.x, top + (compact ? 108 : 116), side.width, compact ? 32 : 58);
  const int half = (wallet.width - GAP) / 2;
  const auto balance = quest::coins(*m.profile);
  for (int n = 0; n < 2; ++n) {
    const auto cell = rect(wallet.x + n * (half + GAP), wallet.y, half, wallet.height);
    panel(s, cell);
    text(s, rect(cell.x + 8, cell.y + 4, compact ? cell.width / 2 : cell.width - 16, 22),
         n ? tr(STR_QUEST_COLLECTION_AFTER_PURCHASE) : tr(STR_QUEST_COLLECTION_YOUR_COINS), 3);
    std::snprintf(w.label, sizeof(w.label), "%llu",
                  static_cast<unsigned long long>(n ? (balance >= quest::price(item) ? balance - quest::price(item) : 0)
                                                    : balance));
    text(s,
         rect(compact ? cell.x + cell.width / 2 : cell.x + 8, cell.y + (compact ? 2 : 27),
              compact ? cell.width / 2 - 8 : cell.width - 16, 28),
         w.label, 0, true, fui::TextAlign::Center);
  }
  const auto actions = rect(side.x, side.bottom() - (wide ? 44 : 108), side.width, wide ? 44 : 108);
  const int buttonWidth = wide ? (actions.width - GAP) / 2 : actions.width;
  button(s, w, rect(actions.x, actions.y, buttonWidth, wide ? 44 : 48), tr(STR_QUEST_COLLECTION_BUY_KEEP), Back, 0,
         true);
  button(s, w,
         rect(wide ? actions.x + buttonWidth + GAP : actions.x, wide ? actions.y : actions.bottom() - 48,
              wide ? actions.width - buttonWidth - GAP : actions.width, wide ? 44 : 48),
         tr(STR_QUEST_COLLECTION_BUY_ACCEPT), ConfirmPurchase, item, false,
         m.writable && !quest::owns(*m.profile, item) && !quest::storyExclusive(item) &&
             quest::coins(*m.profile) >= quest::price(item) && quest::shopUnlocked(*m.profile, item));
  s.frame().limitButtonActions({Back, ConfirmPurchase});
}
void completedShop(toybox::Screen& s, Work& w, fui::Rect body) {
  const bool wide = s.device().width > s.device().height;
  constexpr int desiredSide = 128;
  const int height = std::min(int(body.height) - 8, wide ? 172 : desiredSide + 172);
  const auto group = rect(body.x + 8, body.y + (body.height - height) / 2, body.width - 16, height);
  panel(s, group);
  const int side = std::max(32, std::min(desiredSide, height - (wide ? 24 : 172)));
  const auto picture = wide ? rect(group.x + 12, group.y + (group.height - side) / 2, side, side)
                            : rect(group.x + (group.width - side) / 2, group.y + 16, side, side);
  s.target().bitmap(picture, questshopart::COMPLETE, fui::BitmapMode::Contain, ink);
  const int left = wide ? picture.right() + 16 : group.x + 16;
  const int width = group.right() - left - 16;
  const auto action = rect(left, group.bottom() - 64, width, 48);
  const auto caption =
      wide ? rect(left, group.y + 10, width, action.y - group.y - 16) : rect(left, picture.bottom() + 16, width, 64);
  text(s, caption, tr(STR_QUEST_SHOP_EMPTY), 0, true, fui::TextAlign::Center, false, 3);
  button(s, w, action, tr(STR_QUEST_MY_GEAR), OpenView, int(View::Gear), true);
}
void catalogue(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool inventory = m.view == View::Gear, wide = s.device().width > s.device().height;
  if (quest::availableItem(m.preview)) {
    itemDetail(s, m, w, r);
    return;
  }
  const int titleY = r.y;
  text(s, rect(r.x + 2, r.y, r.width - 204, 38), inventory ? tr(STR_QUEST_MY_GEAR) : tr(STR_QUEST_GEAR_SHOP), 0, true);
  const auto wallet = rect(r.right() - 142, titleY, 142, 38);
  s.target().stroke(wallet, ink, 2, 14);
  questicons::coin(s.target(), rect(wallet.x + 12, wallet.y + 7, 24, 24));
  std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(quest::coins(p)));
  text(s, rect(wallet.x + 44, wallet.y + 2, wallet.width - 54, 34), w.label, 0, true, fui::TextAlign::Center);
  const auto infoBox = rect(wallet.x - 50, titleY - 3, 44, 44);
  const bool infoInverse = button(s, w, infoBox, "", OpenView, int(View::Info));
  icon(s, infoBox.inset({10, 10, 10, 10}), extra_information_32, infoInverse);
  r.y += 48;
  r.height -= 48;
  bonusStrip(s, m, w, rect(r.x, r.y, r.width, 44));
  for (int n = 0; n < quest::kCategoryCount; ++n) {
    const int width = (r.width - 18) / quest::kCategoryCount;
    const auto box = rect(r.x + n * (width + 6), r.y + 56, width, 44);
    const bool inverse = button(s, w, box, "", Category, n, n == m.category);
    text(s, box.inset({2, 2, 2, 2}), I18N.get(CATEGORIES[n]), 3, true, fui::TextAlign::Center, inverse);
  }
  const auto selected = quest::Slot(std::clamp(m.category, 0, quest::kCategoryCount - 1));
  const bool favoritesOnly = m.inventoryFilter == InventoryFilter::Favorites;
  if (inventory) {
    const int width = (r.width - GAP) / 2;
    for (int n = 0; n < 2; ++n) {
      const auto box = rect(r.x + n * (width + GAP), r.y + 112, width, 40);
      const bool inverse = button(s, w, box, "", InventoryFilterChange, n, n == int(m.inventoryFilter));
      if (n) questicons::star(s.target(), rect(box.x + 10, box.y + 7, 26, 26), true, inverse);
      text(s, rect(box.x + (n ? 40 : 6), box.y + 3, box.width - (n ? 46 : 12), box.height - 6),
           n ? tr(STR_QUEST_COLLECTION_FAVORITES) : tr(STR_QUEST_COLLECTION_RECENT), 0, true, fui::TextAlign::Center,
           inverse);
    }
  }
  const int columns = wide ? 3 : 2, rows = wide ? 1 : 2, capacity = columns * rows;
  uint16_t visible[4];
  const int count = inventory ? quest::inventoryCount(p, selected, favoritesOnly)
                              : quest::shopPage(p, selected, m.page, capacity, visible);
  const int pages = std::max(1, (count + capacity - 1) / capacity), page = std::clamp(m.page, 0, pages - 1);
  if (!inventory && page != m.page) quest::shopPage(p, selected, page, capacity, visible);
  const auto footer = rect(r.x, r.bottom() - 44, r.width, 44);
  const int pagerWidth = std::min(230, int(footer.width) * 60 / 100);
  pager(s, w, rect(footer.x + (footer.width - pagerWidth) / 2, footer.y, pagerWidth, 44), page, pages);
  const int contentTop = inventory ? 164 : 112;
  const auto body = rect(r.x, r.y + contentTop, r.width, footer.y - r.y - contentTop - 12);
  if (!count) {
    if (!inventory && quest::shopCount(p, selected, quest::ShopView::Catalogue) == 0) {
      completedShop(s, w, body);
      return;
    }
    text(s, body,
         inventory ? (favoritesOnly ? tr(STR_QUEST_COLLECTION_NO_FAVORITES) : tr(STR_QUEST_NO_GEAR))
                   : tr(STR_QUEST_CAM_ORDER_SHOP_NEXT_LEVEL),
         inventory ? 3 : 1, false, fui::TextAlign::Center, false, inventory ? 3 : 4);
    return;
  }
  const int width = (body.width - (columns - 1) * GAP) / columns;
  const int height = std::min(230, count <= columns ? int(body.height) : (body.height - (rows - 1) * GAP) / rows);
  for (int n = 0; n < capacity && page * capacity + n < count; ++n) {
    const int item = inventory ? quest::inventoryItem(p, selected, favoritesOnly, page * capacity + n) : visible[n];
    const auto box = rect(body.x + n % columns * (width + GAP), body.y + n / columns * (height + GAP), width, height);
    button(s, w, box, "", Preview, item);
    s.target().fill(box.inset({3, 3, 3, 3}), paper, RADIUS - 2);
    name(w, item);
    int artWidth = box.width * 42 / 100;
    if (wide) {
      fui::TextStyle titleStyle;
      titleStyle.font = 3;
      titleStyle.bold = true;
      if (fui::textAreaWalk(s.target(), box.width - artWidth - 14, w.name, titleStyle,
                            [](uint32_t, const fui::TextAreaLine&) {}) > 2)
        artWidth = box.width * 34 / 100;
    }
    sprite(s, m, item, rect(box.x + 6, box.y + 8, artWidth - 6, box.height - 16));
    const bool compact = box.height < 178;
    const bool unseen = inventory && quest::isNewItem(p, item);
    const int titleInset = unseen ? 40 : compact ? 10 : 26;
    const auto details =
        rect(box.x + artWidth + 2, box.y + titleInset, box.width - artWidth - 8, box.height - titleInset - 10);
    const int titleHeight = compact ? 48 : 72;
    const int titleFont = itemTitleFont(s, w.name, details.width, compact);
    text(s, rect(details.x, details.y, details.width, titleHeight), w.name, titleFont, true, fui::TextAlign::Left,
         false, compact ? 2 : 3);
    const auto effect = quest::gearBonus(item, quest::book(p));
    const unsigned power = effect.power + effect.affinity;
    const int kind = selected == quest::Slot::Body                              ? 0
                     : effect.experience && effect.experience >= effect.fortune ? 1
                     : effect.fortune                                           ? 2
                                                                                : 0;
    if (box.height >= 144)
      bonusValue(s, w, rect(details.x, details.y + titleHeight + (compact ? 2 : 8), details.width, compact ? 20 : 28),
                 kind,
                 kind == 0   ? power
                 : kind == 1 ? effect.experience
                             : effect.fortune);
    const bool equipped = quest::equipped(p, item);
    if (inventory) {
      if (!compact || !unseen)
        statusPill(s, rect(details.x, details.bottom() - 30, details.width, 28),
                   equipped ? tr(STR_QUEST_CAM_IN_USE) : tr(STR_QUEST_CAM_AVAILABLE), equipped, nullptr, wide ? 3 : 4);
    } else {
      const int priceHeight = compact ? 22 : 30;
      const int levelHeight = compact ? 20 : 24;
      const auto priceBox =
          rect(details.x, details.bottom() - priceHeight - levelHeight - 4, details.width, priceHeight);
      s.target().stroke(priceBox, ink, 2, priceHeight / 2);
      questicons::coin(s.target(), rect(priceBox.x + 7, priceBox.y + (priceHeight - 18) / 2, 18, 18));
      std::snprintf(w.label, sizeof(w.label), "%u", unsigned(quest::price(item)));
      text(s, rect(priceBox.x + 30, priceBox.y + 1, priceBox.width - 36, priceHeight - 2), w.label, compact ? 3 : 0,
           true);
      if (quest::pathUnlockChapter(item) >= 0)
        std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_QUEST_CAM_AVAILABLE));
      else
        std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_ITEM_LEVEL), unsigned(quest::shopLevel(item)));
      const auto requirement = rect(details.x, details.bottom() - levelHeight, details.width, levelHeight);
      text(s, requirement, w.label, 3, true, fui::TextAlign::Left, false, 2);
    }
    if (unseen) {
      fui::TextStyle tag;
      tag.font = 3;
      tag.bold = true;
      const int badge =
          std::min(int(box.width) - 20, int(s.target().measureText(3, tr(STR_UI_NEW_66AABD), tag).width) + 24);
      statusPill(s, rect(box.right() - badge - 10, box.y + 8, badge, 26), tr(STR_UI_NEW_66AABD), true, nullptr, 3);
    }
    if (inventory && quest::isFavorite(p, item)) questicons::star(s.target(), rect(box.x + 8, box.y + 8, 24, 24));
    if (equipped && !compact && !unseen) icon(s, rect(box.right() - 26, box.y + 6, 18, 18), extra_checked_32);
    if (equipped || s.frame().buttonSelected(Preview, item)) s.target().stroke(box, ink, 3, RADIUS);
  }
}
void setup(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = s.device().width > s.device().height;
  text(s, rect(r.x, r.y, r.width, 40), tr(STR_QUEST_CAM_NEW_STORY), 0, true, fui::TextAlign::Center);
  text(s, rect(r.x, r.y + 48, r.width, 28), tr(STR_QUEST_CAM_CHOOSE_HERO), 3, false, fui::TextAlign::Center);
  const auto cards = rect(r.x, r.y + 90, r.width, std::max(100, int(r.height) - 160));
  genderCards(s, m, w, cards, m.chosenGender);
  button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_QUEST_NEXT_STEP), Start, 0, true,
         m.writable && m.chosenGender >= 0 && m.chosenGender <= 1);
  (void)wide;
}
void profile(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  pageTitle(s, r, tr(STR_QUEST_PROFILE));
  const bool wide = s.device().width > s.device().height;
  const auto cards =
      wide ? rect(r.x, r.y, r.width / 2 - GAP, r.height) : rect(r.x, r.y, r.width, std::min(260, int(r.height) / 2));
  genderCards(s, m, w, cards, int(m.profile->gender));
  const auto details = wide ? rect(cards.right() + GAP, r.y, r.right() - cards.right() - GAP, r.height)
                            : rect(r.x, cards.bottom() + GAP, r.width, r.bottom() - cards.bottom() - GAP);
  const auto identity = rect(details.x, details.y, details.width, std::min(162, int(details.height) - 68));
  panel(s, identity);
  text(s, rect(identity.x + 12, identity.y + 8, identity.width - 24, 32), m.profile->name, 0, true,
       fui::TextAlign::Center);
  text(s, rect(identity.x + 12, identity.y + 44, identity.width - 24, 28), rankTitle(quest::level(m.profile->xp)), 3,
       false, fui::TextAlign::Center);
  experience(s, *m.profile, w, rect(identity.x + 14, identity.bottom() - 66, identity.width - 28, 48));
  button(s, w, rect(details.x, details.bottom() - 54, details.width, 54), tr(STR_QUEST_RENAME), Rename, 0, false,
         m.writable);
}
void settings(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  ornamentTitle(s, rect(r.x + 2, r.y, r.width - 4, 38), tr(STR_QUEST_CAM_SETTINGS));
  r.y += 56;
  r.height -= 56;
  const bool wide = s.device().width > s.device().height;
  static constexpr StrId LABELS[] = {StrId::STR_QUEST_PROFILE, StrId::STR_QUEST_CAM_DATA, StrId::STR_QUEST_CAM_BONUSES};
  static constexpr View VIEWS[] = {View::Profile, View::ReadingData, View::Info};
  static constexpr const freeink::Icon* MARKS[] = {&icon_cat_people_32, &extra_reading_book_32,
                                                   &extra_dashboard_cards_64};
  const int columns = wide ? 2 : 1, width = (r.width - (columns - 1) * GAP) / columns;
  const int rows = wide ? 2 : 3;
  const int height = std::min(wide ? 74 : 80, (int(r.height) - 64 - (rows - 1) * GAP) / rows);
  for (int n = 0; n < 3; ++n) {
    const auto box = rect(r.x + n % columns * (width + GAP), r.y + n / columns * (height + GAP), width, height);
    button(s, w, box, "", OpenView, int(VIEWS[n]));
    const int side = height - 24, divider = box.x + side + 30;
    framedIcon(s, rect(box.x + 12, box.y + 12, side, side), *MARKS[n]);
    s.target().line({int16_t(divider), int16_t(box.y + 14)}, {int16_t(divider), int16_t(box.bottom() - 14)}, 2, ink);
    text(s, rect(divider + 14, box.y + 8, box.right() - divider - 48, height - 16), I18N.get(LABELS[n]), 0, true);
    questicons::chevron(s.target(), rect(box.right() - 25, box.y + (height - 18) / 2, 14, 18));
  }
  button(s, w, rect(r.x, r.bottom() - 50, r.width, 50), tr(STR_QUEST_VICTORY_NEW_GAME), EndAdventure, 0, false,
         m.writable);
}
void rewards(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, uint64_t xp) {
  const int gap = 24, width = (r.width - gap) / 2;
  fui::TextStyle labelStyle;
  labelStyle.bold = true;
  for (int n = 0; n < 2; ++n) {
    const auto box = rect(r.x + n * (width + gap), r.y, width, r.height);
    s.target().stroke(box, ink, 2, std::min(14, int(r.height) / 2));
    if (n == 0)
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_RETURN_XP), static_cast<unsigned long long>(xp));
    else
      std::snprintf(w.label, sizeof(w.label), "%u", unsigned(quest::battleCoins(*m.profile)));
    const int side = std::min(24, int(r.height) - 10),
              labelWidth =
                  std::min(int(box.width) - side - 24, int(s.target().measureText(0, w.label, labelStyle).width)),
              left = box.x + (box.width - side - 8 - labelWidth) / 2;
    if (n == 0)
      icon(s, rect(left, box.y + (box.height - side) / 2, side, side), extra_trophy_32);
    else
      questicons::coin(s.target(), rect(left, box.y + (box.height - side) / 2, side, side));
    text(s, rect(left + side + 8, box.y + 2, labelWidth, box.height - 4), w.label, 0, true, fui::TextAlign::Center);
  }
  s.target().line({int16_t(r.x + r.width / 2), int16_t(r.y + 6)}, {int16_t(r.x + r.width / 2), int16_t(r.bottom() - 6)},
                  1, ink);
}
enum class StoryKind : uint8_t { Prologue, SagaOpening, Saga, Approach, Aftermath };
void illustratedStory(toybox::Screen&, const Model&, Work&, fui::Rect, StoryKind);
void metricCard(toybox::Screen& s, fui::Rect box, const char* label, const freeink::Icon& mark) {
  panel(s, box);
  fui::TextStyle style;
  style.bold = true;
  const int side = 28, gap = 10;
  const int width = std::min(int(box.width) - side - gap - 24, int(s.target().measureText(0, label, style).width));
  const int left = box.x + (box.width - side - gap - width) / 2;
  icon(s, rect(left, box.y + (box.height - side) / 2, side, side), mark);
  text(s, rect(left + side + gap, box.y + 4, width, box.height - 8), label, 0, true);
}
void combatProgress(toybox::Screen& s, Work& w, fui::Rect box, const char* status, const freeink::Icon& mark,
                    uint32_t value, uint32_t target) {
  panel(s, box);
  const int top = box.y + (box.height - 60) / 2;
  icon(s, rect(box.x + 14, box.y + (box.height - 30) / 2, 30, 30), mark);
  text(s, rect(box.x + 58, top, box.width - 72, 30), status, 0, true);
  std::snprintf(w.name, sizeof(w.name), tr(STR_QUEST_CAM_BATTLE_MINUTE_PROGRESS),
                static_cast<unsigned long long>(std::min(value, target) / 60),
                static_cast<unsigned long long>((target + 59) / 60));
  text(s, rect(box.x + 58, top + 30, box.width - 72, 30), w.name, 0);
}
void encounter(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;
  const bool wide = s.device().width > s.device().height;
  const auto phase = p.campaign.phase;
  if (p.campaign.state == quest::AdventureState::Unfinished) {
    pageTitle(s, r, tr(STR_QUEST_CAM_UNFINISHED));
    scene(s, m, rect(r.x, r.y, r.width, r.height - 70), -1, questavatar::Cast::HeroOnly);
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_QUEST_CAM_OPEN_PATH), CampaignSection,
           int(CampaignTab::Path), true);
    return;
  }
  if (quest::needsIntroduction(p) || quest::needsEncounterStory(p)) {
    pageTitle(s, r, tr(STR_QUEST_CAM_BATTLE_CHOOSE_PATH));
    const auto picture = rect(r.x, r.y, r.width, std::max(1, int(r.height) - 70));
    if (m.assets)
      questavatar::draw(s.target(), picture, p, *m.assets, -1, questassets::SAGA_SCENES + quest::book(p) * 2,
                        questavatar::Framing::Tall, m.rewardAssets, questavatar::Cast::HeroOnly, m.outfitAssets);
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_QUEST_CAM_OPEN_PATH), CampaignSection,
           int(CampaignTab::Path), true);
    return;
  }
  if (phase == quest::BattlePhase::Complete) {
    pageTitle(s, r, tr(STR_QUEST_CAM_WORLD_SAFE));
    scene(s, m, rect(r.x, r.y, r.width, r.height - 78), -1, questavatar::Cast::HeroOnly);
    campaignAdvance(s, m, w, rect(r.x, r.bottom() - 66, r.width, 66));
    return;
  }
  if (phase == quest::BattlePhase::Victory) {
    pageTitle(s, r, journeyResult(p.campaign.encounter));
    const int reward = p.campaign.encounter % 6 == 5 ? quest::chapterReward(quest::chapter(p)) : quest::kGearCount;
    const bool place = quest::visibleItem(reward) && !quest::owns(p, reward);
    const auto controls = rect(wide ? r.x + r.width / 2 + GAP : r.x, wide ? r.y : r.bottom() - (place ? 186 : 108),
                               wide ? r.width / 2 - GAP : r.width,
                               wide    ? r.height
                               : place ? 186
                                       : 108);
    const auto picture =
        wide ? rect(r.x, r.y, r.width / 2 - GAP, r.height) : rect(r.x, r.y, r.width, controls.y - r.y - GAP);
    if (questjourney::kind(p.campaign.encounter) != questjourney::Kind::Combat)
      journeyScene(s, m, picture, p.campaign.encounter, true);
    else if (m.assets)
      questenemy::paint(s.target(), picture, *m.assets, p, questenemy::Pose::Current, m.enemyAssets);
    if (place) {
      sprite(s, m, reward, rect(controls.x, controls.y, 62, 62));
      name(w, reward);
      text(s, rect(controls.x + 76, controls.y, controls.width - 76, 56), w.name, 3, true, fui::TextAlign::Left, false,
           2);
    }
    rewards(s, m, w, rect(controls.x, controls.bottom() - 104, controls.width, 44), quest::battleXp(p));
    button(s, w, rect(controls.x, controls.bottom() - 54, controls.width, 54), tr(STR_QUEST_CAM_RECEIVE), BattleReward,
           0, true, m.writable);
    return;
  }
  ornamentTitle(s, rect(r.x, r.y, r.width, 38), journeyTitle(p.campaign.encounter), true);
  const bool compact = wide && r.height < 222;
  const auto hint = wide ? rect(r.x + r.width * 56 / 100, r.y + 46, r.width * 44 / 100, compact ? 0 : 74)
                         : rect(r.x, r.y + 46, r.width, 56);
  const auto progressBox = wide ? rect(hint.x, hint.bottom() + (compact ? 0 : GAP), hint.width,
                                       r.bottom() - hint.bottom() - (compact ? 0 : GAP))
                                : rect(r.x, r.bottom() - 76, r.width, 76);
  const auto picture = wide ? rect(r.x, r.y + 46, hint.x - r.x - GAP, r.height - 46)
                            : rect(r.x, hint.bottom() + GAP, r.width, progressBox.y - hint.bottom() - 24);
  panel(s, picture);
  if (m.assets)
    questenemy::battle(s.target(), picture.inset({6, 6, 6, 6}), *m.assets, p, questenemy::Pose::Current,
                       wide ? questavatar::Framing::Contain : questavatar::Framing::Tall, m.enemyAssets, m.rewardAssets,
                       -1, nullptr, m.outfitAssets);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_REMAINING), quest::remainingMinutes(p));
  combatProgress(s, w, progressBox, w.label, extra_clock_32, p.campaign.encounterSeconds, quest::encounterTarget(p));
  if (!hint.empty()) {
    panel(s, hint);
    icon(s, rect(hint.x + 12, hint.y + (hint.height - 28) / 2, 28, 28), questaction::sagaTab);
    text(s, rect(hint.x + 52, hint.y + 8, hint.width - 64, hint.height - 16), tr(STR_QUEST_CAM_BATTLE_HINT), 0, false,
         fui::TextAlign::Left, false, 2);
  }
}
void illustratedStory(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, StoryKind kind) {
  const auto& p = *m.profile;
  const bool volume = kind == StoryKind::Saga;
  const bool opening = kind == StoryKind::SagaOpening;
  const bool onboarding = kind == StoryKind::Prologue;
  const bool aftermath = kind == StoryKind::Aftermath;
  const unsigned encounter = onboarding || volume || opening
                                 ? p.campaign.encounter
                                 : std::clamp(m.selectedEncounter, 0, int(quest::battleAnchor(quest::chapter(p))));
  const unsigned chapter = encounter / quest::ENCOUNTERS_PER_CHAPTER;
  const unsigned book = volume ? std::clamp(m.selectedBook, 0, 11) : chapter / quest::CHAPTERS_PER_BOOK;
  const bool wide = s.device().width > s.device().height;
  if (kind == StoryKind::Approach && !m.recalling &&
      (p.campaign.state != quest::AdventureState::Active || p.campaign.phase != quest::BattlePhase::Reading ||
       chapter != quest::chapter(p))) {
    w.page = 0;
    w.pages = 1;
    pageTitle(s, r, tr(STR_QUEST_CAM_OPEN_PATH));
    button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_QUEST_CAM_BACK_PATH), ReturnPath, 0, true);
    return;
  }
  const int replayPrefix = volume && book == 0 && p.campaign.prologueSeen ? PROLOGUE_SECTIONS : 0;
  const int prologueSections = onboarding ? PROLOGUE_SECTIONS : replayPrefix;
  const unsigned version = volume ? questscene::sagaVersion(p, book) : 0;
  const auto heading = rect(r.x, r.y, r.width, 88);
  r.y += 88;
  r.height -= 88;
  const auto action = rect(r.x, r.bottom() - 54, r.width, 54);
  const int portraitHeight = std::min(16 + (r.width - 16) * 2 / 3, (int(r.height) - 78) * 60 / 100);
  const auto picture =
      wide ? rect(r.x, r.y, r.width * 46 / 100, r.height - 70) : rect(r.x, r.y, r.width, portraitHeight);
  const auto copy = wide ? rect(picture.right() + GAP, r.y, r.right() - picture.right() - GAP, r.height - 76)
                         : rect(r.x, picture.bottom() + GAP, r.width, action.y - picture.bottom() - 24);
  const auto words = copy.inset({6, 6, 6, 6});
  fui::TextStyle style;
  style.font = NARRATIVE_FONT;
  style.maxLines = 1;
  const int nativeLineHeight = std::max(1, int(s.target().lineHeight(style.font)));
  const int lineHeight = (nativeLineHeight * 6 + 4) / 5;
  const int capacity = std::max(1, int(words.height) / lineHeight);
  const auto prose = [&](int section) {
    if (section < prologueSections) return I18N.get(queststory::PROLOGUE_TEXTS[section]);
    if (volume || opening) return I18N.get(questscene::SAGA[book][version]);
    if (aftermath) return tr(STR_QUEST_CAM_ORDER_CLEAR_PATH);
    return I18N.get(questscene::APPROACH[chapter]);
  };
  struct Section {
    const char* text;
    int lines;
    int pages;
  };
  Section sections[PROLOGUE_SECTIONS + 1]{};
  const int sectionCount = prologueSections + !onboarding;
  w.pages = 0;
  for (int n = 0; n < sectionCount; ++n) {
    const char* value = prose(n);
    const int lines =
        fui::textAreaWalk(s.target(), words.width, value, style, [](uint32_t, const fui::TextAreaLine&) {});
    const int pages = std::max(1, (lines + capacity - 1) / capacity);
    sections[n] = {value, lines, pages};
    w.pages += pages;
  }
  int requestedPage = m.introductionPage;
  if (volume && requestedPage < 0) {
    const auto position = questscene::decodeBookmark(p.sagaPositions[book], replayPrefix, version);
    requestedPage = std::min(position.page, sections[position.section].pages - 1);
    for (int n = 0; n < position.section; ++n) requestedPage += sections[n].pages;
  }
  w.page = std::clamp(requestedPage, 0, w.pages - 1);
  int section = 0, sectionPage = w.page;
  while (sectionPage >= sections[section].pages) {
    sectionPage -= sections[section].pages;
    ++section;
  }
  if (volume) w.storyPosition = questscene::encodeBookmark(section, sectionPage, replayPrefix, version);
  const bool prologuePage = section < prologueSections;
  icon(s, rect(heading.x + 2, heading.y + 2, 26, 26), questaction::sagaTab);
  if (prologuePage)
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_QUEST_CAM_INTRO_TITLE));
  else if (volume || opening)
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_SAGA_NUMBER), ROMANS[book]);
  else
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_CHAPTER), unsigned(chapter % 10 + 1));
  text(s, rect(heading.x + 38, heading.y, heading.width - 210, 40), w.label, 0, true);
  const auto jump = rect(heading.right() - 160, heading.y, 44, 44);
  const bool jumpInverse = button(s, w, jump, "", StoryPageOpen, 0, false, w.pages > 1);
  icon(s, jump.inset({9, 9, 9, 9}), icon_wiki_contents_32, jumpInverse);
  const auto number = rect(heading.right() - 104, heading.y, 104, 44);
  panel(s, number);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_INTRO_PAGE), unsigned(w.page + 1), unsigned(w.pages));
  text(s, number.inset({4, 5, 4, 5}), w.label, 3, false, fui::TextAlign::Center);
  ornamentTitle(s, rect(heading.x + 2, heading.y + 48, heading.width - 4, 38),
                prologuePage        ? I18N.get(queststory::PROLOGUE_TITLES[section])
                : volume || opening ? I18N.get(queststory::BOOKS[book])
                                    : I18N.get(queststory::CHAPTERS[chapter]));
  panel(s, picture);
  const auto art = picture.inset({8, 12, 8, 12});
  if (prologuePage) {
    if (section == 1 && m.assets && m.assets->ready()) {
      questavatar::storyBackground(s.target(), art, *m.assets, questassets::KING_PROLOGUE);
    } else if (section == 2) {
      questmemory::draw(s.target(), art, p, m.assets, 0, questenemy::Pose::Weakened, m.enemyAssets, m.rewardAssets,
                        m.outfitAssets);
    } else if (m.assets && m.assets->ready()) {
      const int backdrop =
          section == 0 ? quest::first(quest::Slot::Background) + questassets::COUNTRY_PATH_BACKGROUND : -1;
      questavatar::draw(s.target(), art, p, *m.assets, backdrop, section == 0 ? 0 : questassets::SAGA_SCENES,
                        questavatar::Framing::Contain, m.rewardAssets, questavatar::Cast::HeroOnly, m.outfitAssets);
    } else {
      questcover::draw(s.target(), art, 0);
    }
  } else if ((volume || opening) && m.storyAssets && m.storyAssets->ready()) {
    const unsigned pageCount = std::max(1, sections[section].pages - 1);
    const unsigned path =
        book * quest::CHAPTERS_PER_BOOK + unsigned(sectionPage) * (quest::CHAPTERS_PER_BOOK - 1) / pageCount;
    questavatar::storyBackground(s.target(), art, *m.storyAssets, path * 2);
  } else if (!volume && !opening && !aftermath && m.storyAssets && m.storyAssets->ready() &&
             chapter < questassets::STORY_SCENES) {
    questavatar::storyBackground(s.target(), art, *m.storyAssets, chapter * 2);
  } else if (!volume && !opening && m.assets && m.assets->ready()) {
    const auto region = questavatar::storyBackground(s.target(), art, *m.assets, questassets::SAGA_SCENES + book * 2);
    questenemy::paint(s.target(), region.inset({6, 22, 6, 22}), *m.assets, p,
                      aftermath ? questenemy::Pose::Defeated : questenemy::Pose::Ready, m.enemyAssets, int(encounter));
  } else if (m.assets && m.assets->ready()) {
    questavatar::draw(s.target(), art, p, *m.assets, -1, questassets::SAGA_SCENES + book * 2,
                      questavatar::Framing::Contain, m.rewardAssets, questavatar::Cast::HeroOnly, m.outfitAssets);
  } else {
    questcover::draw(s.target(), art, book);
  }
  const auto& current = sections[section];
  const int linesPerPage = std::max(1, (current.lines + current.pages - 1) / current.pages);
  fui::textAreaWalk(s.target(), words.width, current.text, style, [&](uint32_t index, const fui::TextAreaLine& run) {
    const int row = int(index) - sectionPage * linesPerPage;
    if (row < 0 || row >= linesPerPage) return;
    const auto bytes = std::min<size_t>(run.len, sizeof(w.storyLine) - 1);
    std::memcpy(w.storyLine, current.text + run.start, bytes);
    w.storyLine[bytes] = 0;
    narrativeLine(s, rect(words.x, words.y + row * lineHeight, words.width, nativeLineHeight), w.storyLine, style,
                  !run.hardBreak && current.text[run.start + run.len]);
  });
  fui::ActionId lastAction = ReturnPath;
  int lastValue = 0;
  const char* label = tr(STR_QUEST_CAM_OPEN_PATH);
  bool enabled = true;
  switch (kind) {
    case StoryKind::Prologue:
      lastAction = FinishPrologue;
      label = tr(STR_QUEST_CAM_BATTLE_START);
      enabled = m.writable;
      break;
    case StoryKind::SagaOpening:
      lastAction = FinishSagaIntro;
      label = tr(STR_QUEST_CAM_SAGA_START);
      enabled = m.writable;
      break;
    case StoryKind::Saga:
      label = tr(STR_QUEST_CAM_BACK_SAGA);
      lastAction = CampaignSection;
      lastValue = int(CampaignTab::Saga);
      break;
    case StoryKind::Aftermath:
      if (m.view == View::AtlasPlace) {
        lastAction = AtlasBook;
        lastValue = book;
        label = tr(STR_QUEST_CAM_ATLAS);
      }
      break;
    case StoryKind::Approach:
      if (!m.recalling && encounter == quest::battleAnchor(chapter)) {
        lastAction = BeginEncounter;
        lastValue = int(encounter);
        label = tr(STR_QUEST_CAM_PATH_FIGHT);
        enabled = m.writable;
      }
      break;
  }
  if (m.recalling) label = tr(STR_QUEST_CONTINUE);
  const bool nextPage = w.page + 1 < w.pages;
  const bool previous = w.pages > 1;
  const int previousWidth = previous ? (action.width - GAP) / 2 : 0;
  if (previous)
    button(s, w, rect(action.x, action.y, previousWidth, action.height), tr(STR_QUEST_CAM_READER_PREVIOUS),
           IntroductionPage, -1, false, w.page > 0);
  const int left = previous ? previousWidth + GAP : 0;
  button(s, w, rect(action.x + left, action.y, action.width - left, action.height),
         nextPage ? tr(STR_QUEST_CAM_READER_NEXT) : label, nextPage ? IntroductionPage : lastAction,
         nextPage ? 1 : lastValue, true, nextPage || enabled);
}
void storyPages(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  pageTitle(s, r, tr(STR_QUEST_CAM_READER_JUMP));
  const bool wide = s.device().width > s.device().height;
  const int gap = 12;
  const auto controls = wide ? rect(r.x + r.width / 2 + gap, r.y, r.width / 2 - gap, r.height)
                             : rect(r.x, r.y + 142, r.width, r.height - 142);
  const auto display = wide ? rect(r.x, r.y, r.width / 2 - gap, r.height) : rect(r.x, r.y, r.width, 126);
  panel(s, display);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_READER_RANGE), m.storyPageCount);
  text(s, rect(display.x + 14, display.y + 14, display.width - 28, 32), w.label, 0, false, fui::TextAlign::Center);
  std::snprintf(w.label, sizeof(w.label), "%d", m.storyPageDraft);
  text(s, rect(display.x + 14, display.y + 54, display.width - 28, wide ? 80 : 50), w.label, 4, true,
       fui::TextAlign::Center);
  const int columns = 3;
  const int rows = 12 / columns;
  const int keyWidth = (controls.width - gap * (columns - 1)) / columns;
  const int keyHeight = (controls.height - gap * rows - 48) / rows;
  for (int n = 0; n < 12; ++n) {
    const auto key = rect(controls.x + (n % columns) * (keyWidth + gap), controls.y + (n / columns) * (keyHeight + gap),
                          keyWidth, keyHeight);
    if (n == 9)
      button(s, w, key, tr(STR_QUEST_CAM_READER_ERASE), StoryPageErase);
    else if (n == 11)
      button(s, w, key, tr(STR_QUEST_CAM_READER_OPEN), StoryPageApply, 0, true,
             m.storyPageDraft > 0 && m.storyPageDraft <= m.storyPageCount);
    else {
      const int digit = n == 10 ? 0 : n + 1;
      std::snprintf(w.label, sizeof(w.label), "%d", digit);
      button(s, w, key, w.label, StoryPageDigit, digit);
    }
  }
  button(s, w, rect(controls.x, controls.bottom() - 48, controls.width, 48), tr(STR_QUEST_CAM_READER_CANCEL), Back);
}
void sagaOutcome(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, bool returning) {
  const auto& p = *m.profile;
  const unsigned book = quest::book(p);
  const bool wide = s.device().width > s.device().height;
  const bool final = book + 1 == quest::CAMPAIGN_BOOKS;
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_SAGA_VICTORY), ROMANS[book]);
  pageTitle(s, r, w.label);
  const auto action = rect(r.x, r.bottom() - 52, r.width, 52);
  const auto awards = rect(r.x, action.y - 52, r.width, 42);
  const auto grid = rect(r.x, r.y, r.width, awards.y - r.y - GAP);
  constexpr int columns = 2;
  const int rows = quest::kCategoryCount / columns;
  const int width = (grid.width - (columns - 1) * GAP) / columns;
  const int height = (grid.height - (rows - 1) * GAP) / rows;
  const char* newLabel = tr(STR_UI_NEW_66AABD);
  fui::TextStyle badgeStyle;
  badgeStyle.font = 3;
  badgeStyle.bold = true;
  const int badgeWidth = s.target().measureText(3, newLabel, badgeStyle).width + 20;
  for (int n = 0; n < quest::kCategoryCount; ++n) {
    const auto box = rect(grid.x + n % columns * (width + GAP), grid.y + n / columns * (height + GAP), width, height);
    panel(s, box);
    const int item = quest::sagaReward(book, quest::Slot(n));
    name(w, item);
    const auto badge = rect(box.right() - badgeWidth - 9, box.y + 9, badgeWidth, 26);
    s.target().fill(badge, ink, 9);
    text(s, badge, newLabel, 3, true, fui::TextAlign::Center, true);
    if (wide) {
      const int artWidth = box.width * 38 / 100;
      sprite(s, m, item, rect(box.x + 8, box.y + 5, artWidth, box.height - 12));
      text(s,
           rect(box.x + artWidth + 14, badge.bottom() + 2, box.width - artWidth - 24,
                box.bottom() - badge.bottom() - 10),
           w.name, 3, true, fui::TextAlign::Left, false, 2);
    } else {
      const auto caption = rect(box.x + 6, box.bottom() - 54, box.width - 12, 48);
      sprite(s, m, item, rect(box.x + 8, badge.bottom() + 4, box.width - 16, caption.y - badge.bottom() - 6));
      text(s, caption, w.name, 3, true, fui::TextAlign::Center, false, 2);
    }
  }
  const auto* report = returning ? m.readingReturn : nullptr;
  rewards(s, m, w, awards, report ? report->xp : quest::battleXp(p));
  const char* label = report && report->requestId >= 0 ? tr(STR_QUEST_CAM_HUNT_SEE_PROGRESS)
                      : final                          ? tr(STR_QUEST_CAM_BATTLE_FINISH)
                                                       : tr(STR_QUEST_CAM_SAGA_NEXT);
  button(s, w, action, label, returning ? DismissReadingReturn : StoryContinue, 0, true, m.writable);
}
void battleOutcome(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, bool returning) {
  w.page = 0;
  w.pages = 1;
  const auto& p = *m.profile;
  const auto chapter = quest::chapter(p);
  if (quest::lastPathInSaga(chapter)) {
    sagaOutcome(s, m, w, r, returning);
    return;
  }
  const bool wide = s.device().width > s.device().height;
  const bool final = chapter + 1 == quest::CAMPAIGN_CHAPTERS;
  pageTitle(s, r, tr(STR_QUEST_CAM_BATTLE_VICTORY));
  const int leftWidth = wide ? r.width * 52 / 100 : r.width;
  const auto side = wide ? rect(r.x + leftWidth + GAP, r.y, r.width - leftWidth - GAP, r.height) : r;
  const auto action = rect(side.x, side.bottom() - 50, side.width, 50);
  const int gift = quest::chapterReward(chapter), decor = quest::chapterDecoration(chapter);
  const int count = int(quest::visibleItem(gift)) + int(quest::visibleItem(decor));
  const int row = wide ? (side.height < 300 ? 34 : 42) : 52;
  const auto gains = rect(side.x, action.y - 50 - count * (row + GAP), side.width, 40);
  const auto status = rect(side.x, wide ? side.y : gains.y - 100, side.width, 88);
  const auto picture = rect(r.x, r.y, leftWidth, wide ? r.height : status.y - r.y - GAP);
  panel(s, picture);
  if (m.assets)
    questenemy::battle(s.target(), picture.inset({6, 8, 6, 8}), *m.assets, p, questenemy::Pose::Defeated,
                       wide ? questavatar::Framing::Contain : questavatar::Framing::Tall, m.enemyAssets, m.rewardAssets,
                       -1, nullptr, m.outfitAssets);
  icon(s, rect(status.x + 4, status.y + 6, 26, 26), extra_checked_32);
  text(s, rect(status.x + 40, status.y, status.width - 44, 36), journeyTitle(p.campaign.encounter), 0, true);
  const auto seconds = p.campaign.encounterSeconds ? p.campaign.encounterSeconds : quest::encounterTarget(p);
  const auto target = (seconds + 59) / 60;
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_BATTLE_MINUTE_PROGRESS),
                static_cast<unsigned long long>(target), static_cast<unsigned long long>(target));
  const auto* report = returning ? m.readingReturn : nullptr;
  const int sessionWidth = report && report->seconds ? status.width * 39 / 100 : 0;
  text(s, rect(status.x + 40, status.y + 36, status.width - 44 - sessionWidth, 30), w.label, 0);
  if (sessionWidth) {
    std::snprintf(
        w.label, sizeof(w.label),
        report->seconds >= 60 ? tr(STR_QUEST_CAM_BATTLE_SESSION_MINUTES) : tr(STR_QUEST_CAM_BATTLE_SESSION_SECONDS),
        static_cast<unsigned long long>(report->seconds >= 60 ? report->seconds / 60 : report->seconds));
    text(s, rect(status.right() - sessionWidth, status.y + 36, sessionWidth - 2, 30), w.label, 3, false,
         fui::TextAlign::Right);
  }
  progress(s, rect(status.x, status.bottom() - 10, status.width, 8), 1, 1);
  rewards(s, m, w, gains, report ? report->xp : quest::battleXp(p));
  int y = gains.bottom() + GAP;
  for (int item : {gift, decor}) {
    if (!quest::visibleItem(item)) continue;
    const auto box = rect(side.x, y, side.width, row);
    panel(s, box);
    sprite(s, m, item, rect(box.x + 7, box.y + 4, 38, box.height - 9));
    name(w, item);
    text(s, rect(box.x + 54, box.y + 3, box.width - 64, box.height - 6), w.name, wide ? 3 : 0, false,
         fui::TextAlign::Left, false, 2);
    y += row + GAP;
  }
  const char* label = final ? tr(STR_QUEST_CAM_BATTLE_FINISH) : tr(STR_QUEST_CAM_BATTLE_NEXT);
  if (returning && m.readingReturn && m.readingReturn->requestId >= 0) label = tr(STR_QUEST_CAM_HUNT_SEE_PROGRESS);
  button(s, w, action, label, returning ? DismissReadingReturn : StoryContinue, 0, true, m.writable);
}
void campaign(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& p = *m.profile;

  if (quest::needsPrologue(p)) {
    illustratedStory(s, m, w, r, StoryKind::Prologue);
    return;
  }
  if (showsSagaOpening(p, m.campaignTab)) {
    illustratedStory(s, m, w, r, StoryKind::SagaOpening);
    return;
  }
  if (m.campaignTab == CampaignTab::Encounter && p.campaign.phase == quest::BattlePhase::Story) {
    battleOutcome(s, m, w, r, false);
    return;
  }
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_BOOK), ROMANS[quest::book(p)]);
  icon(s, rect(r.x, r.y, 24, 24), questaction::sagaTab);
  text(s, rect(r.x + 34, r.y, r.width / 2 - 34, 26), w.label, 0, true);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_CHAPTER), unsigned(quest::chapter(p) % 10 + 1));
  text(s, rect(r.x + r.width / 2, r.y, r.width / 2, 26), w.label, 0, true, fui::TextAlign::Right);
  static constexpr StrId TABS[] = {StrId::STR_QUEST_CAM_SAGA, StrId::STR_QUEST_CAM_PATH,
                                   StrId::STR_QUEST_CAM_ENCOUNTER};
  static constexpr const freeink::Icon* TAB_ICONS[] = {&questaction::sagaTab, &questaction::pathTab,
                                                       &questaction::campaign};
  fui::TextStyle tabStyle;
  tabStyle.font = 0;
  tabStyle.bold = true;
  int tabWidths[3], wanted = 0;
  for (int n = 0; n < 3; ++n) {
    tabWidths[n] = 40 + s.target().measureText(0, I18N.get(TABS[n]), tabStyle).width;
    wanted += tabWidths[n];
  }
  const int available = r.width - 16, spare = available - wanted;
  int tabX = r.x;
  for (int n = 0; n < 3; ++n) {
    const int width = n == 2       ? r.right() - tabX
                      : spare >= 0 ? tabWidths[n] + spare / 3
                                   : available * tabWidths[n] / wanted;
    const auto box = rect(tabX, r.y + 34, width, 44);
    const bool inverse = button(s, w, box, "", CampaignSection, n, int(m.campaignTab) == n);
    icon(s, rect(box.x + 7, box.y + 10, 24, 24), *TAB_ICONS[n], inverse);
    text(s, rect(box.x + 34, box.y + 2, box.width - 40, 40), I18N.get(TABS[n]), 0, true, fui::TextAlign::Center,
         inverse);
    tabX += width + 8;
  }
  r.y += 90;
  r.height -= 90;
  switch (m.campaignTab) {
    case CampaignTab::Encounter:
      encounter(s, m, w, r);
      break;
    case CampaignTab::Path: {
      pageTitle(s, r, I18N.get(queststory::BOOKS[quest::book(p)]));
      const bool wide = s.device().width > s.device().height;
      const int capacity = wide ? 4 : 5, columns = wide ? 2 : 1, rows = (capacity + columns - 1) / columns;
      const int pages = (quest::CHAPTERS_PER_BOOK + capacity - 1) / capacity;
      const int page = std::clamp(m.page, 0, pages - 1);
      const int height = (r.height - 64 - (rows - 1) * GAP) / rows, width = (r.width - (columns - 1) * GAP) / columns;
      for (int n = 0; n < capacity; ++n) {
        const unsigned chapter = quest::book(p) * 10 + page * capacity + n;
        if (chapter >= (quest::book(p) + 1) * 10) break;
        const unsigned id = quest::battleAnchor(chapter);
        const bool done = chapter < quest::completedPaths(p);
        const bool current = chapter == quest::chapter(p) && !done;
        const auto box = rect(r.x + n % columns * (width + GAP), r.y + n / columns * (height + GAP), width, height);
        const bool selected = current && p.campaign.state == quest::AdventureState::Active &&
                              p.campaign.encounter == id && !quest::needsIntroduction(p) &&
                              !quest::needsEncounterStory(p);
        const bool resume = current && (selected || p.campaign.phase != quest::BattlePhase::Reading);
        if (current && p.campaign.state == quest::AdventureState::Active) {
          const bool focused = button(s, w, box, "", resume ? BeginEncounter : EncounterInfo, int(id));
          if (focused) s.target().fill(box.inset({3, 3, 3, 3}), paper, RADIUS - 3);
          s.target().stroke(box, ink, focused ? 7 : 5, RADIUS);
        } else {
          panel(s, box);
        }
        const int side = std::min(48, height - 20);
        const int x = box.x + side + 40, available = box.right() - x - (done ? 48 : 14);
        const char* title = journeyTitle(id);
        constexpr int font = 0;
        const int titleHeight = s.target().lineHeight(font), statusHeight = s.target().lineHeight(font);
        const int top = box.y + (height - titleHeight - statusHeight - 6) / 2;
        framedIcon(s, rect(box.x + 12, box.y + (height - side) / 2, side, side),
                   done || current ? questaction::campaign : extra_lock_32);
        if (done) {
          const int cx = box.right() - 34, cy = box.y + (height - 26) / 2;
          s.target().fill(rect(cx, cy, 26, 26), ink, 13);
          s.target().line({int16_t(cx + 6), int16_t(cy + 13)}, {int16_t(cx + 11), int16_t(cy + 18)}, 3, paper);
          s.target().line({int16_t(cx + 11), int16_t(cy + 18)}, {int16_t(cx + 20), int16_t(cy + 8)}, 3, paper);
        }
        s.target().line({int16_t(x - 14), int16_t(top)},
                        {int16_t(x - 14), int16_t(top + titleHeight + statusHeight + 6)}, 2, ink);
        text(s, rect(x, top, available, titleHeight), title, font, false, fui::TextAlign::Left, false, 2);
        std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_REMAINING), quest::remainingMinutes(p));
        text(s, rect(x, top + titleHeight + 6, available, statusHeight),
             done      ? tr(STR_QUEST_CAM_DONE)
             : current ? p.campaign.phase != quest::BattlePhase::Reading ? tr(STR_QUEST_CAM_REWARD_READY)
                         : selected                                      ? w.label
                                                                         : tr(STR_QUEST_CAM_AVAILABLE)
                       : tr(STR_QUEST_CAM_SAGA_LOCKED),
             font, false);
      }
      w.page = page;
      w.pages = pages;
      pager(s, w, rect(r.x, r.bottom() - 44, r.width, 44), page, pages);
      break;
    }
    case CampaignTab::Saga: {
      const bool wide = s.device().width > s.device().height;
      const int columns = 2;
      const int capacity = sagaPageSize(wide), rows = capacity / columns, pages = (12 + capacity - 1) / capacity;
      const bool rowCard = wide;
      if (!wide) {
        ornamentTitle(s, rect(r.x, r.y, r.width - 116, 38), tr(STR_QUEST_CAM_SAGA_TITLE));
        button(s, w, rect(r.right() - 108, r.y, 108, 38), tr(STR_QUEST_CAM_ATLAS), OpenView, int(View::Atlas), false,
               p.campaign.prologueSeen);
        r.y += 48;
        r.height -= 48;
      }
      const int page = std::clamp(m.page, 0, pages - 1), width = (r.width - (columns - 1) * GAP) / columns,
                height = (r.height - 128 - (rows - 1) * GAP) / rows;
      for (int n = 0; n < capacity; ++n) {
        const int id = page * capacity + n;
        const auto box = rect(r.x + n % columns * (width + GAP), r.y + n / columns * (height + GAP), width, height);
        const bool unlocked = unsigned(id) <= quest::book(p);
        button(s, w, box, "", SelectBook, id, false, unlocked);
        s.target().fill(box.inset({3, 3, 3, 3}), paper, RADIUS - 2);
        const auto ribbon = rect(box.x + 12, box.y + 2, 34, 28);
        s.target().fill(ribbon, ink);
        for (int dy = 0; dy < 8; ++dy) {
          s.target().fill(rect(ribbon.x, ribbon.bottom() + dy, 17 - dy, 1), ink);
          s.target().fill(rect(ribbon.x + 17 + dy, ribbon.bottom() + dy, 17 - dy, 1), ink);
        }
        text(s, rect(ribbon.x, ribbon.y, ribbon.width, 24), ROMANS[id], 3, true, fui::TextAlign::Center, true);
        const int artWidth = rowCard ? std::min(150, int(box.width) * 38 / 100) : box.width - 28;
        const auto art = rowCard ? rect(box.x + 12, box.y + 38, artWidth, box.height - 48)
                                 : rect(box.x + 14, box.y + 30, box.width - 28, std::max(44, height - 128));
        questcover::draw(s.target(), art, unsigned(id));
        if (!unlocked) {
          const int side = std::min(48, int(art.height));
          const auto lock = rect(art.x + (art.width - side) / 2, art.y + (art.height - side) / 2, side, side);
          s.target().fill(lock, paper, 10);
          s.target().stroke(lock, ink, 2, 10);
          icon(s, lock.inset({5, 5, 5, 5}), extra_lock_32);
        }
        const auto copy = rowCard ? rect(art.right() + 14, box.y + 18, box.right() - art.right() - 26, box.height - 30)
                                  : rect(box.x + 12, box.bottom() - 94, box.width - 24, 82);
        text(s, rect(copy.x, copy.y, copy.width, copy.height - 32), I18N.get(queststory::BOOKS[id]), 3, true,
             fui::TextAlign::Center, false, 2);
        const bool done = unsigned(id) < quest::book(p) || p.campaign.phase == quest::BattlePhase::Complete,
                   current = unsigned(id) == quest::book(p);
        statusPill(s, rect(copy.x, copy.bottom() - 30, copy.width, 28),
                   done      ? tr(STR_QUEST_CAM_DONE)
                   : current ? quest::needsIntroduction(p) ? tr(STR_QUEST_CAM_AVAILABLE) : tr(STR_QUEST_CAM_IN_PROGRESS)
                             : tr(STR_QUEST_CAM_SAGA_LOCKED),
                   current && !done, done ? &extra_checked_32 : nullptr);
      }
      pager(s, w, rect(r.x, r.bottom() - 116, wide ? r.width - 120 : r.width, 44), page, pages);
      if (wide)
        button(s, w, rect(r.right() - 108, r.bottom() - 116, 108, 44), tr(STR_QUEST_CAM_ATLAS), OpenView,
               int(View::Atlas), false, p.campaign.prologueSeen);
      campaignAdvance(s, m, w, rect(r.x, r.bottom() - 66, r.width, 66));
      break;
    }
  }
}
void readingResult(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const questreturn::Report empty;
  const auto& report = m.readingReturn ? *m.readingReturn : empty;

  const bool combat = report.kind == questreturn::Kind::Combat;
  const bool advancing = combat || report.kind == questreturn::Kind::Journey;
  const bool playing = questreturn::isPlaying(m.attackPhase);
  const bool hit = m.attackPhase == questreturn::Phase::Impact || !playing;
  const auto shown = hit ? report.after : report.before;
  const bool defeated = hit && report.victory;
  const bool wide = s.device().width > s.device().height;
  pageTitle(s, r, report.requestId >= 0 ? tr(STR_QUEST_RETURN_CAMPAIGN) : tr(STR_QUEST_CAM_RETURN_TITLE));
  const auto action = rect(r.x, r.bottom() - 54, r.width, 54);
  const bool gifts = report.victory && !questreturn::sagaCelebration(report, *m.profile);
  const int giftHeight = gifts && quest::campaignUnlocks(quest::chapter(*m.profile)).count ? 116 : 0;
  fui::Rect time, picture, gains, outcome;
  if (wide) {
    const int artWidth = r.width * 52 / 100;
    picture = rect(r.x, r.y, artWidth, action.y - r.y - 12);
    const int x = picture.right() + GAP, width = r.right() - x;
    time = rect(x, r.y, width, 32);
    gains = rect(x, time.bottom() + 8, width, 32);
    outcome = rect(x, gains.bottom() + 8, width, action.y - gains.bottom() - 20 - giftHeight);
  } else {
    outcome = rect(r.x, action.y - giftHeight - GAP - 76, r.width, 76);
    gains = rect(r.x, outcome.y - GAP - 44, r.width, 44);
    time = rect(r.x, gains.y - GAP - 48, r.width, 48);
    picture = rect(r.x, r.y, r.width, std::max(80, time.y - GAP - r.y));
  }
  const int giftTop = action.y - giftHeight;
  std::snprintf(w.label, sizeof(w.label),
                report.seconds >= 60 ? tr(STR_QUEST_CAM_RETURN_TIME) : tr(STR_QUEST_CAM_RETURN_SECONDS),
                static_cast<unsigned long long>(report.seconds >= 60 ? report.seconds / 60 : report.seconds));
  metricCard(s, time, w.label, questaction::sagaTab);
  panel(s, picture);
  const auto art = picture.inset({6, 8, 6, 8});
  if (m.assets) {
    if (combat) {
      const auto pose =
          playing ? questreturn::monsterPose(m.attackPhase) : questenemy::progressPose(shown, report.target);
      questenemy::battle(s.target(), art, *m.assets, *m.profile, pose, questavatar::Framing::Tall, m.enemyAssets,
                         m.rewardAssets, -1, nullptr, m.outfitAssets);
      questreturn::letters(s.target(), art, *m.assets, *m.profile, m.attackPhase,
                           uint32_t(report.seconds) ^ report.before, questavatar::Framing::Tall, m.rewardAssets,
                           m.outfitAssets);
    } else if (report.kind == questreturn::Kind::Journey) {
      journeyScene(s, m, art, m.profile->campaign.encounter, report.victory);
    } else {
      const bool freeReading = report.kind == questreturn::Kind::Complete;
      questavatar::draw(s.target(), art, *m.profile, *m.assets, -1,
                        freeReading ? 0 : questassets::SAGA_SCENES + quest::book(*m.profile) * 2,
                        freeReading ? questavatar::Framing::Contain : questavatar::Framing::Tall, m.rewardAssets,
                        freeReading ? questavatar::Cast::Equipped : questavatar::Cast::HeroOnly, m.outfitAssets);
    }
  }
  const int badgeWidth = advancing ? (gains.width - GAP) / 2 : gains.width;
  const auto xp = rect(gains.x, gains.y, badgeWidth, gains.height);
  std::snprintf(w.label, sizeof(w.label), report.victory ? tr(STR_QUEST_CAM_RETURN_XP) : tr(STR_QUEST_VICTORY_XP_PRIZE),
                static_cast<unsigned long long>(report.victory ? report.xp : quest::battleXp(*m.profile)));
  metricCard(s, xp, advancing ? w.label : tr(STR_QUEST_CAM_RETURN_LOGGED),
             advancing ? extra_trophy_32 : questaction::sagaTab);
  if (advancing) {
    const auto advance = rect(xp.right() + GAP, gains.y, badgeWidth, gains.height);
    const uint32_t applied = report.after > report.before ? report.after - report.before : 0;
    std::snprintf(w.label, sizeof(w.label),
                  applied >= 60 ? tr(STR_QUEST_CAM_RETURN_PROGRESS) : tr(STR_QUEST_CAM_RETURN_PROGRESS_SECONDS),
                  applied >= 60 ? applied / 60 : applied);
    if (report.victory) {
      std::snprintf(w.label, sizeof(w.label), "%u", m.profile->campaign.victoryCoins);
      panel(s, advance);
      questicons::coin(s.target(), rect(advance.x + 12, advance.y + 10, 24, 24));
      text(s, rect(advance.x + 44, advance.y, advance.width - 54, advance.height), w.label, 0, true,
           fui::TextAlign::Center);
    } else {
      metricCard(s, advance, w.label, journeyIcon(questjourney::kind(m.profile->campaign.encounter)));
    }
  }
  const auto mark = advancing ? (defeated ? &extra_checked_32 : &extra_clock_32) : &questaction::sagaTab;
  const auto remaining = report.target > shown ? (report.target - shown + 59) / 60 : 0;
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_REMAINING), remaining);
  const char* status = advancing ? defeated ? journeyResult(m.profile->campaign.encounter) : w.label
                       : report.kind == questreturn::Kind::Complete ? tr(STR_QUEST_CAM_RETURN_PEACE)
                       : report.kind == questreturn::Kind::Logged   ? tr(STR_QUEST_CAM_RETURN_LOGGED)
                                                                    : tr(STR_QUEST_CAM_RETURN_BANKED);
  if (advancing) {
    combatProgress(s, w, outcome, status, *mark, shown, report.target);
  } else {
    panel(s, outcome);
    icon(s, rect(outcome.x + 14, outcome.y + 14, 30, 30), *mark);
    text(s, rect(outcome.x + 58, outcome.y + 8, outcome.width - 72, outcome.height - 16), status, 0, false,
         fui::TextAlign::Left, false, 3);
  }
  if (giftHeight) {
    const auto rewards = quest::campaignUnlocks(quest::chapter(*m.profile));
    const int count = std::min(2, int(rewards.count));
    const auto row = rect(outcome.x, giftTop, outcome.width, giftHeight - GAP);
    for (int n = 0; n < count; ++n) {
      const auto box =
          rect(row.x + n * (row.width + GAP) / count, row.y, (row.width - (count - 1) * GAP) / count, row.height);
      panel(s, box);
      const int artWidth = count == 1 ? 86 : 62;
      sprite(s, m, rewards.items[n], rect(box.x + 8, box.y + 8, artWidth, box.height - 16));
      const int left = box.x + artWidth + 20;
      text(s, rect(left, box.y + 8, box.right() - left - 8, 24), tr(STR_QUEST_VICTORY_SHOP_UNLOCK), 3, true);
      name(w, rewards.items[n]);
      text(s, rect(left, box.y + 35, box.right() - left - 8, box.height - 42), w.name, 0, true, fui::TextAlign::Left,
           false, 2);
    }
  }
  const auto next = questreturn::nextPanel(questreturn::Panel::Reading, report, *m.profile);
  const char* continuation = next == questreturn::Panel::Request   ? tr(STR_QUEST_CAM_HUNT_SEE_PROGRESS)
                             : next == questreturn::Panel::Victory ? tr(STR_QUEST_CAM_READER_NEXT)
                                                                   : tr(STR_QUEST_RETURN_MENU);
  button(s, w, action, continuation, DismissReadingReturn, 0, true, !playing);
}
void tryOut(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const int item = quest::nextOutfitOffer(*m.profile);
  if (!quest::availableItem(item)) return;
  pageTitle(s, r, tr(STR_QUEST_VICTORY_TRY_OUT));
  const bool wide = s.device().width > s.device().height;
  const auto controls = wide ? rect(r.x + r.width * 56 / 100 + GAP, r.y, r.width * 44 / 100 - GAP, r.height)
                             : rect(r.x, r.bottom() - 170, r.width, 170);
  const auto picture =
      wide ? rect(r.x, r.y, r.width * 56 / 100, r.height) : rect(r.x, r.y, r.width, controls.y - r.y - GAP);
  panel(s, picture);
  if (m.assets)
    questavatar::draw(s.target(), picture.inset({8, 8, 8, 8}), *m.profile, *m.assets, item, 0,
                      questavatar::Framing::Contain, m.rewardAssets, questavatar::Cast::Equipped, m.outfitAssets);
  name(w, item);
  text(s, rect(controls.x + 8, controls.y, controls.width - 16, 46), w.name, 0, true, fui::TextAlign::Center, false, 2);
  button(s, w, rect(controls.x, controls.bottom() - 112, controls.width, 50), tr(STR_QUEST_VICTORY_TRY_USE), TryOutUse,
         0, true, m.writable);
  button(s, w, rect(controls.x, controls.bottom() - 50, controls.width, 50), tr(STR_QUEST_VICTORY_TRY_STORE),
         TryOutStore, 0, false, m.writable);
}
void celebration(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const int index = questmilestone::pending(*m.profile);
  if (index < 0) return;
  const bool wide = s.device().width > s.device().height;
  pageTitle(s, r, tr(STR_QUEST_MILESTONE_TITLE));
  const auto action = rect(r.x, r.bottom() - 54, r.width, 54);
  const auto side = wide ? rect(r.x + r.width * 56 / 100 + GAP, r.y, r.width * 44 / 100 - GAP, action.y - r.y - GAP)
                         : rect(r.x, action.y - 172, r.width, 160);
  auto picture =
      wide ? rect(r.x, r.y, r.width * 56 / 100, action.y - r.y - GAP) : rect(r.x, r.y, r.width, side.y - r.y - GAP);
  auto levelBox = rect(side.x, side.y, side.width, 56);
  panel(s, picture);
  if (m.assets) {
    auto* art = m.celebrationAssets && m.celebrationAssets->ready() ? m.celebrationAssets : nullptr;
    questavatar::draw(s.target(), picture.inset({6, 6, 6, 6}), *m.profile, *m.assets, -1, 0,
                      questavatar::Framing::Contain, m.rewardAssets, questavatar::Cast::Equipped, m.outfitAssets, art,
                      unsigned(index) * 2);
  }
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_MILESTONE_LEVEL), questmilestone::LEVELS[index]);
  statusPill(s, levelBox, w.label, true, &extra_trophy_32, 1);
  const int rankY = side.y + 62;
  text(s, rect(side.x + 8, rankY, side.width - 16, 36), rankTitle(questmilestone::LEVELS[index]), 0, true,
       fui::TextAlign::Center);
  static constexpr StrId MESSAGES[] = {StrId::STR_QUEST_MILESTONE_50, StrId::STR_QUEST_MILESTONE_100,
                                       StrId::STR_QUEST_MILESTONE_150};
  text(s, rect(side.x + 8, rankY + 40, side.width - 16, side.bottom() - rankY - 40), I18N.get(MESSAGES[index]), 0,
       false, fui::TextAlign::Center, false, 3);
  button(s, w, action, tr(STR_QUEST_CONTINUE), MilestoneContinue, 0, true, m.writable);
}
void lore(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  illustratedStory(
      s, m, w, r,
      m.recalling && questjourney::context(*m.profile).beat != 0 ? StoryKind::Aftermath : StoryKind::Approach);
}
void bookDetail(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  illustratedStory(s, m, w, r, StoryKind::Saga);
}
void readingData(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  pageTitle(s, r, tr(STR_QUEST_CAM_DATA));
  const bool wide = s.device().width > s.device().height;
  const int columns = 2, rows = 2, height = wide ? std::min(86, (int(r.height) - 62 - GAP) / 2) : 104,
            metricsWidth = wide ? r.width * 47 / 100 : r.width, width = (metricsWidth - GAP) / columns;
  static constexpr StrId LABELS[] = {StrId::STR_QUEST_CAM_TODAY_LABEL, StrId::STR_QUEST_CAM_WEEK_LABEL,
                                     StrId::STR_QUEST_CAM_TOTAL, StrId::STR_QUEST_CAM_BOOKS};
  static constexpr const freeink::Icon* MARKS[] = {&extra_clock_32, &questaction::weeklyMission, &questaction::sagaTab,
                                                   &extra_reading_book_32};
  const uint64_t values[] = {
      quest::adventureDay(*m.profile, *m.summary, m.today), quest::adventureWeek(*m.profile, *m.summary, m.today),
      quest::adventureSeconds(*m.profile, *m.summary), quest::adventureBooks(*m.profile, *m.summary)};
  for (int n = 0; n < 4; ++n) {
    const auto box = rect(r.x + n % columns * (width + GAP), r.y + n / columns * (height + GAP), width, height);
    panel(s, box);
    icon(s, rect(box.x + 10, box.y + 7, 22, 22), *MARKS[n]);
    text(s, rect(box.x + 40, box.y + 5, box.width - 48, 24), I18N.get(LABELS[n]), 3, true);
    if (n == 3)
      std::snprintf(w.label, sizeof(w.label), "%llu", static_cast<unsigned long long>(values[n]));
    else if (n == 2)
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_HOURS),
                    static_cast<unsigned long long>(values[n] / 3600), unsigned(values[n] / 60 % 60));
    else
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_MINUTES),
                    static_cast<unsigned long long>(values[n] / 60));
    text(s, rect(box.x + 6, box.y + 34, box.width - 12, box.height - 40), w.label, 0, true, fui::TextAlign::Center);
  }
  const int top = r.y + rows * (height + GAP);
  const auto chart = wide ? rect(r.x + metricsWidth + GAP, r.y, r.width - metricsWidth - GAP, r.height - 62)
                          : rect(r.x, top, r.width, r.bottom() - top - 62);
  panel(s, chart);
  icon(s, rect(chart.x + 14, chart.y + 12, 24, 24), questaction::weeklyMission);
  text(s, rect(chart.x + 48, chart.y + 8, chart.width - 64, 30), tr(STR_QUEST_CAM_WEEK_LABEL), 0, true);
  const int chartTop = chart.y + 48, chartHeight = std::max(8, int(chart.height) - 82);
  uint32_t maximum = 60;
  const int start = readingstats::weekStart(m.today, false);
  for (int n = 0; n < 7; ++n) maximum = std::max(maximum, quest::adventureDay(*m.profile, *m.summary, start + n));
  for (int n = 0; n < 7; ++n) {
    const int barWidth = std::max(6, (int(chart.width) - 24) / 16),
              x = chart.x + 12 + (n * 2 + 1) * (chart.width - 24) / 14 - barWidth / 2;
    const unsigned seconds = quest::adventureDay(*m.profile, *m.summary, start + n);
    const int h = uint64_t(seconds) * chartHeight / maximum;
    if (h) s.target().fill(rect(x, chartTop + chartHeight - h, barWidth, h), ink, 3);
    std::snprintf(w.label, sizeof(w.label), "%u", unsigned(n + 1));
    text(s, rect(x - 4, chartTop + chartHeight + 5, barWidth + 8, 22), w.label, 4, false, fui::TextAlign::Center);
  }
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_STREAK),
                quest::adventureStreak(*m.profile, *m.summary, m.today));
  statusPill(s, rect(r.x, r.bottom() - 44, r.width, 42), w.label, false, &questaction::weeklyMission);
}
void plan(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  pageTitle(s, r, tr(STR_QUEST_CAM_LONG_SAGA));
  const bool wide = s.device().width > s.device().height;
  const auto overview = wide ? rect(r.x, r.y, r.width * 48 / 100, r.height - 72) : rect(r.x, r.y, r.width, 194);
  panel(s, overview);
  const int artWidth = wide ? 118 : 154;
  questcover::draw(s.target(), rect(overview.x + 10, overview.y + 8, artWidth, overview.height - 16),
                   quest::book(*m.profile));
  const auto copy =
      rect(overview.x + artWidth + 24, overview.y + 10, overview.width - artWidth - 38, overview.height - 20);
  text(s, rect(copy.x, copy.y, copy.width, 62), tr(STR_QUEST_CAM_STRUCTURE), 0, true, fui::TextAlign::Left, false, 2);
  text(s, rect(copy.x, copy.bottom() - (wide ? 48 : 72), copy.width, wide ? 48 : 72), tr(STR_QUEST_CAM_PACING_NOTE), 0,
       false, fui::TextAlign::Left, false, wide ? 2 : 3);
  const auto pacing = wide ? rect(overview.right() + GAP, r.y, r.right() - overview.right() - GAP, r.height - 72)
                           : rect(r.x, overview.bottom() + 18, r.width, r.bottom() - overview.bottom() - 90);
  const int top = pacing.y;
  for (int n = 0; n < 3; ++n) {
    const int width = (pacing.width - 16) / 3, minutes = n == 0 ? 15 : n == 1 ? 30 : 60;
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_MINUTES), static_cast<unsigned long long>(minutes));
    button(s, w, rect(pacing.x + n * (width + 8), top, width, 44), w.label, PlanMinutes, minutes,
           m.dailyMinutes == minutes);
  }
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_CAM_NEXT_READING), unsigned(std::clamp(m.dailyMinutes, 15, 60)));
  const auto estimate = rect(pacing.x, top + 60, pacing.width, wide ? 40 : 62);
  statusPill(s, estimate, w.label, false, &extra_clock_32, 0);
  button(s, w, rect(r.x, r.bottom() - 54, r.width, 54), tr(STR_QUEST_CAM_OPEN_BOOKS), CampaignSection,
         int(CampaignTab::Saga), true);
}
void info(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  pageTitle(s, r, tr(STR_QUEST_CAM_BONUSES));
  const auto effect = quest::bonuses(*m.profile);
  const unsigned values[] = {quest::heroPower(*m.profile), effect.experience, effect.fortune};
  static constexpr StrId LABELS[] = {StrId::STR_QUEST_COLLECTION_POWER, StrId::STR_QUEST_CAM_READ_XP,
                                     StrId::STR_QUEST_CAM_MORE_COINS};
  static constexpr const freeink::Icon* MARKS[] = {&questaction::campaign, &questaction::dailyMission,
                                                   &questaction::phase0};
  const bool wide = s.device().width > s.device().height;
  const int rowHeight = wide ? 64 : 92, leftWidth = wide ? (r.width - GAP) / 2 : r.width;
  for (int n = 0; n < 3; ++n) {
    const auto row = rect(r.x, r.y + n * (rowHeight + GAP), leftWidth, rowHeight);
    panel(s, row);
    const int side = rowHeight - 24;
    framedIcon(s, rect(row.x + 12, row.y + 12, side, side), *MARKS[n]);
    const int divider = row.x + side + 28;
    s.target().line({int16_t(divider), int16_t(row.y + 12)}, {int16_t(divider), int16_t(row.bottom() - 12)}, 2, ink);
    text(s, rect(divider + 12, row.y + 5, row.right() - divider - 24, 28), I18N.get(LABELS[n]), 3, true);
    std::snprintf(w.label, sizeof(w.label), n == 0 ? tr(STR_QUEST_COLLECTION_POWER_POINTS) : tr(STR_QUEST_CAM_PLUS),
                  values[n]);
    text(s, rect(divider + 12, row.y + 31, row.right() - divider - 24, n == 0 && !wide ? 30 : row.height - 38), w.label,
         0, true);
    if (n == 0 && !wide) {
      std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_LEVEL_POWER),
                    questbalance::levelPower(quest::level(m.profile->xp)), unsigned(effect.power));
      text(s, rect(divider + 12, row.y + 64, row.right() - divider - 24, 22), w.label, 3);
    }
  }
  const auto limits = wide ? rect(r.x + leftWidth + GAP, r.y, r.width - leftWidth - GAP, std::min(242, int(r.height)))
                           : rect(r.x, r.y + 3 * (rowHeight + GAP) + 6, r.width, 250);
  panel(s, limits);
  icon(s, rect(limits.x + 12, limits.y + 10, 26, 26), extra_information_32);
  text(s, rect(limits.x + 48, limits.y + 6, limits.width - 60, 32), tr(STR_QUEST_COLLECTION_LIMITS_TITLE), 0, true);
  const int cellWidth = (limits.width - 40) / 3;
  for (int n = 0; n < 3; ++n) {
    const auto cap = rect(limits.x + 12 + n * (cellWidth + 8), limits.y + 48, cellWidth, 90);
    s.target().stroke(cap, ink, 2, 10);
    text(s, rect(cap.x + 4, cap.y + 5, cap.width - 8, 46),
         I18N.get(n == 0   ? StrId::STR_QUEST_COLLECTION_TIME_FLOOR
                  : n == 1 ? StrId::STR_QUEST_CAM_EXP_DETAIL_XP
                           : StrId::STR_QUEST_CAM_EXP_DETAIL_COINS),
         3, true, fui::TextAlign::Center, false, 2);
    std::snprintf(w.label, sizeof(w.label), n == 0 ? tr(STR_QUEST_COLLECTION_PERCENT) : tr(STR_QUEST_CAM_PLUS),
                  n == 0   ? 40u
                  : n == 1 ? questbalance::XP_CAP
                           : questbalance::COIN_CAP);
    text(s, rect(cap.x + 4, cap.y + 55, cap.width - 8, 30), w.label, 0, true, fui::TextAlign::Center);
  }
  text(s,
       rect(limits.x + 14, limits.y + 148, limits.width - 28,
            std::max(2 * s.target().lineHeight(0), int(limits.height) - 160)),
       tr(STR_QUEST_COLLECTION_POWER_RULE), wide ? 3 : 0, false, fui::TextAlign::Left, false, 4);
}
void confirmEnd(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const int width = std::min(430, int(r.width) - 12), height = std::min(330, int(r.height) - 12);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  panel(s, box);
  text(s, rect(box.x + 18, box.y + 18, box.width - 36, 54), tr(STR_QUEST_VICTORY_NEW_GAME_CONFIRM), 0, true,
       fui::TextAlign::Left, false, 2);
  text(s, rect(box.x + 18, box.y + 84, box.width - 36, 86), tr(STR_QUEST_VICTORY_NEW_GAME_WARNING), 3, false,
       fui::TextAlign::Left, false, 4);
  button(s, w, rect(box.x + 18, box.bottom() - 124, box.width - 36, 48), tr(STR_CANCEL), Back);
  button(s, w, rect(box.x + 18, box.bottom() - 62, box.width - 36, 48), tr(STR_QUEST_VICTORY_NEW_GAME), Restart, 0,
         true, m.writable);
}
void ending(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool won = quest::hasWon(*m.profile), wide = s.device().width > s.device().height;
  pageTitle(s, r, won ? tr(STR_QUEST_CAM_JOURNEY2_EPILOGUE_TITLE) : tr(STR_QUEST_CAM_UNFINISHED));
  const int controlsHeight = won ? 108 : 54;
  const auto controls = rect(wide ? r.x + r.width * 53 / 100 + GAP : r.x, r.bottom() - controlsHeight,
                             wide ? r.width - r.width * 53 / 100 - GAP : r.width, controlsHeight);
  const char* closing = won ? tr(STR_QUEST_CAM_JOURNEY2_EPILOGUE) : tr(STR_QUEST_CAM_EXP_CLOSED_TEXT);
  fui::TextStyle copyStyle;
  copyStyle.font = 3;
  const int artWidth = wide ? r.width * 53 / 100 : r.width;
  const int copyWidth = artWidth - 8;
  const int lines =
      fui::textAreaWalk(s.target(), copyWidth, closing, copyStyle, [](uint32_t, const fui::TextAreaLine&) {});
  const int proseHeight = lines * s.target().lineHeight(copyStyle.font);
  const auto picture =
      wide ? rect(r.x, r.y, artWidth, r.height - proseHeight - 12)
           : rect(r.x, r.y, r.width, std::max(90, int(r.height) - controlsHeight - proseHeight - (won ? 118 : 32)));
  panel(s, picture);
  const auto art = picture.inset({6, 6, 6, 6});
  if (won && m.assets && m.assets->ready()) {
    questavatar::draw(s.target(), art, *m.profile, *m.assets, quest::kKingBackground, 0, questavatar::Framing::Contain,
                      m.rewardAssets, questavatar::Cast::HeroOnly, m.outfitAssets);
  } else {
    scene(s, m, art, -1, questavatar::Cast::HeroOnly);
  }
  const auto copy = wide ? rect(controls.x, r.y, controls.width, controls.y - r.y - 8)
                         : rect(r.x, picture.bottom() + 10, r.width, controls.y - picture.bottom() - 18);
  text(s, rect(picture.x + 4, picture.bottom() + 10, copyWidth, proseHeight), closing, 3, false, fui::TextAlign::Left,
       false, lines);
  if (won) {
    const int giftTop = (wide ? 0 : proseHeight) + 32;
    text(s, rect(copy.x, copy.y + giftTop - 28, copy.width, 24), tr(STR_QUEST_CAM_EXP_VICTORY_COLLECTION), 3, false);
    static constexpr int ITEMS[] = {quest::kKingBody, quest::kKingBackground, quest::kKingPet, quest::kKingDecoration};
    const int width = (copy.width - 3 * 8) / 4;
    const int height = int(copy.height) - giftTop;
    for (int n = 0; n < 4; ++n) {
      const auto gift = rect(copy.x + n * (width + 8), copy.y + giftTop, width, height);
      const bool focused = button(s, w, gift, "", EndingContinue, n + 2);
      s.target().fill(gift.inset({4, 4, 4, 4}), paper, RADIUS - 4);
      if (focused) s.target().stroke(gift, ink, 4, RADIUS);
      sprite(s, m, ITEMS[n], rect(gift.x + 4, gift.y + 4, gift.width - 8, gift.height - 28));
      text(s, rect(gift.x + 2, gift.bottom() - 24, gift.width - 4, 22), I18N.get(CATEGORIES[n]), 3, false,
           fui::TextAlign::Center);
    }
    button(s, w, rect(controls.x, controls.y, controls.width, 46), tr(STR_QUEST_CAM_EXP_VIEW_GIFTS), EndingContinue, 1);
  }
  button(s, w, rect(controls.x, controls.bottom() - 54, controls.width, 54), tr(STR_QUEST_CAM_EXP_CONTINUE_APP),
         EndingContinue, 0, true);
}
}  // namespace
const char* rankTitle(uint32_t level) {
  static constexpr StrId TITLES[] = {StrId::STR_QUEST_APPRENTICE,      StrId::STR_QUEST_RANK_CURIOUS,
                                     StrId::STR_QUEST_RANK_EXPLORER,   StrId::STR_QUEST_RANK_TRAVELER,
                                     StrId::STR_QUEST_BOOKKEEPER,      StrId::STR_QUEST_RANK_SEEKER,
                                     StrId::STR_QUEST_RANK_CHRONICLER, StrId::STR_QUEST_RANK_SAGE,
                                     StrId::STR_QUEST_RANK_WEAVER,     StrId::STR_QUEST_RANK_KEEPER,
                                     StrId::STR_QUEST_MASTER,          StrId::STR_QUEST_RANK_SAGA,
                                     StrId::STR_QUEST_RANK_WORLDS,     StrId::STR_QUEST_RANK_ARCHITECT,
                                     StrId::STR_QUEST_RANK_FORGER,     StrId::STR_QUEST_RANK_SOVEREIGN,
                                     StrId::STR_QUEST_RANK_ORACLE,     StrId::STR_QUEST_RANK_CELESTIAL,
                                     StrId::STR_QUEST_RANK_LEGEND,     StrId::STR_QUEST_RANK_DEITY,
                                     StrId::STR_QUEST_RANK_KNOWLEDGE,  StrId::STR_QUEST_RANK_DREAMS,
                                     StrId::STR_QUEST_RANK_PAGES,      StrId::STR_QUEST_RANK_DESTINY,
                                     StrId::STR_QUEST_RANK_INFINITY,   StrId::STR_QUEST_RANK_ETERNAL_SAGE,
                                     StrId::STR_QUEST_RANK_LIGHT,      StrId::STR_QUEST_RANK_LEGACY,
                                     StrId::STR_QUEST_RANK_BOOK_HERO,  StrId::STR_QUEST_RANK_ETERNAL_LEGEND};
  static_assert(sizeof(TITLES) / sizeof(TITLES[0]) == quest::TITLE_COUNT);
  return I18N.get(TITLES[std::min<unsigned>(quest::TITLE_COUNT - 1,
                                            (std::max<uint32_t>(1u, level) - 1) / quest::LEVELS_PER_TITLE)]);
}
void buildLevelUp(toybox::Screen& s, const quest::Profile& p, bool saveFailed, Work& w) {
  const auto r = widget::content(s);
  const int width = std::min(440, int(r.width) - 16), height = std::min(320, int(r.height) - 8);
  const auto box = rect(r.x + (r.width - width) / 2, r.y + (r.height - height) / 2, width, height);
  panel(s, box);
  icon(s, rect(box.x + (box.width - 48) / 2, box.y + 16, 48, 48), extra_trophy_64);
  text(s, rect(box.x + 12, box.y + 74, box.width - 24, 32), tr(STR_QUEST_LEVEL_UP), 0, true, fui::TextAlign::Center);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_LEVEL), int(quest::level(p.xp)));
  text(s, rect(box.x + 12, box.y + 112, box.width - 24, 32), w.label, 0, true, fui::TextAlign::Center);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_COLLECTION_LEVEL_GAIN),
                2u * (quest::level(p.xp) - p.noticedLevel));
  text(s, rect(box.x + 12, box.y + 148, box.width - 24, 24), w.label, 3, true, fui::TextAlign::Center);
  std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_LEVEL_GIFT), quest::pendingLevelCoins(p));
  text(s, rect(box.x + 12, box.y + 178, box.width - 24, 28), w.label, 0, false, fui::TextAlign::Center);
  text(s, rect(box.x + 16, box.y + 212, box.width - 32, 36),
       saveFailed ? tr(STR_TOOLS_SAVE_FAILED) : tr(STR_QUEST_KEEP_READING), 4, false, fui::TextAlign::Center, false, 2);
  button(s, w, rect(box.x + 12, box.bottom() - 66, box.width - 24, 54), tr(STR_QUEST_CONTINUE), LevelContinue, 0, true);
}

void build(toybox::Screen& s, const Model& m, Work& w) {
  using namespace widget;
  auto body = content(s);
  s.setContentMarginAbsolute(
      {body.y, int16_t(s.device().width - body.right()), int16_t(s.device().height - body.bottom()), body.x});
  navigation(s, m, w);
  if ((m.assets && !m.assets->ready()) || (m.storyAssets && !m.storyAssets->ready()) ||
      (m.rewardAssets && !m.rewardAssets->ready()) || (m.enemyAssets && !m.enemyAssets->ready()) ||
      (m.outfitAssets && !m.outfitAssets->ready()) || (m.celebrationAssets && !m.celebrationAssets->ready())) {
    requiredresources::build(s, "/apps/reading-adventure-v2/", Back, w.button);
    return;
  }
  if (m.loading) {
    text(s, body, tr(STR_READING_LOADING), 0, false, fui::TextAlign::Center);
    return;
  }
  if (!m.profile || !m.summary) return;
  if (m.failed || m.denied || m.storeNotice != quest::StoreNotice::None) {
    panel(s, rect(body.x, body.y, body.width, 42));
    text(s, rect(body.x + 8, body.y + 4, body.width - 16, 34),
         m.failed   ? tr(STR_TOOLS_SAVE_FAILED)
         : m.denied ? tr(STR_QUEST_LOCKED)
                    : tr(STR_QUEST_COLLECTION_BACKUP_RECOVERED),
         3, true, fui::TextAlign::Center);
    body.y += 54;
    body.height -= 54;
  }
  switch (m.view) {
    case View::StoryPages:
      storyPages(s, m, w, body);
      break;
    case View::TryOut:
      tryOut(s, m, w, body);
      break;
    case View::Celebration:
      celebration(s, m, w, body);
      break;
    case View::ReadingReturn:
      switch (m.returnPanel) {
        case questreturn::Panel::Reading:
          readingResult(s, m, w, body);
          break;
        case questreturn::Panel::Victory:
          battleOutcome(s, m, w, body, true);
          break;
        case questreturn::Panel::Request:
          buildJourney(s, m, w, body);
          break;
      }
      break;
    case View::Hero:
      dashboard(s, m, w, body);
      break;
    case View::Experience:
      dashboard(s, m, w, body);
      experienceDetails(s, m, w, body);
      break;
    case View::Missions:
      missions(s, m, w, body);
      break;
    case View::Gear:
    case View::Shop:
      catalogue(s, m, w, body);
      break;
    case View::Profile:
      profile(s, m, w, body);
      break;
    case View::Info:
      info(s, m, w, body);
      break;
    case View::Setup:
      setup(s, m, w, body);
      break;
    case View::Name:
      naming(s, m, w, body);
      break;
    case View::Settings:
      settings(s, m, w, body);
      break;
    case View::Campaign:
      campaign(s, m, w, body);
      break;
    case View::ReadingData:
      readingData(s, m, w, body);
      break;
    case View::Plan:
      plan(s, m, w, body);
      break;
    case View::EndConfirm:
      confirmEnd(s, m, w, body);
      break;
    case View::Ending:
      ending(s, m, w, body);
      break;
    case View::Book:
      bookDetail(s, m, w, body);
      break;
    case View::Lore:
      lore(s, m, w, body);
      break;
    case View::PurchaseConfirm:
      purchaseConfirm(s, m, w, body);
      break;
    case View::AtlasPlace:
      buildJourney(s, m, w, body);
      break;
    case View::Atlas:
    case View::AtlasRegion:
    case View::Requests:
    case View::HuntRewards:
    case View::RequestCancelConfirm:
    case View::RequestSwitchConfirm:
    case View::Request:
    case View::Thanks:
    case View::Recap:
      buildJourney(s, m, w, body);
      break;
  }
}
}  // namespace questui

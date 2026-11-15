#pragma once

#include "QuestActionIcons.h"
#include "QuestBestiaryText.h"
#include "QuestCatalog.h"
#include "QuestExpansionArt.h"
#include "QuestExpansionText.h"
#include "QuestJourneyScreens.h"
#include "QuestKeepsakeArt.h"
#include "QuestRequestData.h"
#include "QuestRequests.h"
#include "QuestWidgets.h"

namespace questui::requestui {
inline const char* rank(int id) {
  switch (quest::huntRank(id)) {
    case quest::HuntRank::Basic:
      return tr(STR_QUEST_HUNTS_RANK_BASIC);
    case quest::HuntRank::Elite:
      return tr(STR_QUEST_HUNTS_RANK_ELITE);
    case quest::HuntRank::Legendary:
      return tr(STR_QUEST_HUNTS_RANK_LEGENDARY);
    case quest::HuntRank::Secret:
      return tr(STR_QUEST_HUNTS_RANK_SECRET);
  }
  return tr(STR_QUEST_HUNTS_RANK_BASIC);
}
inline const char* outfitName(int id) {
  if (quest::bestiaryHunt(id)) return I18N.get(questbestiary::OUTFIT[id - quest::kBestiaryHuntStart]);
  switch (id) {
    case quest::kStormHunt:
      return tr(STR_QUEST_HUNTS_STORM_OUTFIT);
    case quest::kCrystalHunt:
      return tr(STR_QUEST_HUNTS_CRYSTAL_OUTFIT);
    case quest::kPhoenixHunt:
      return tr(STR_QUEST_HUNTS_PHOENIX_OUTFIT);
    case quest::kInkHunt:
      return tr(STR_QUEST_HUNTS_INK_OUTFIT);
    default:
      return huntTitle(id);
  }
}
inline const freeink::Icon& goalIcon(quest::RequestGoal goal) {
  switch (goal) {
    case quest::RequestGoal::Minutes:
      return extra_clock_32;
    case quest::RequestGoal::Pages:
      return icon_wiki_contents_32;
    case quest::RequestGoal::Sessions:
      return questaction::dailyMission;
    case quest::RequestGoal::Books:
      return extra_reading_book_32;
    case quest::RequestGoal::Days:
      return extra_calendar_32;
  }
  return extra_clock_32;
}
inline void goalProgressText(Work& w, quest::RequestGoal goal, unsigned value, unsigned target, bool compact = false) {
  if (goal == quest::RequestGoal::Minutes && target >= 180 && target % 60 == 0) {
    std::snprintf(w.label, sizeof(w.label),
                  compact ? tr(STR_QUEST_HUNTS_SHORT_HOURS_PROGRESS) : tr(STR_QUEST_HUNTS_HOURS_PROGRESS), value / 60,
                  value % 60, target / 60);
    return;
  }
  static constexpr StrId values[] = {
      StrId::STR_QUEST_CAM_ORDER_PROGRESS_MINUTES, StrId::STR_QUEST_CAM_ORDER_PROGRESS_PAGES,
      StrId::STR_QUEST_CAM_ORDER_PROGRESS_SESSIONS, StrId::STR_QUEST_CAM_ORDER_PROGRESS_BOOKS,
      StrId::STR_QUEST_CAM_ORDER_PROGRESS_DAYS};
  std::snprintf(w.label, sizeof(w.label),
                compact && goal == quest::RequestGoal::Sessions ? tr(STR_QUEST_CAM_ORDER_PROGRESS_SHORT_SESSIONS)
                                                                : I18N.get(values[unsigned(goal)]),
                value, target);
}
inline void goalText(Work& w, const Model& m, int id, bool progressValue, bool compact = false) {
  static constexpr StrId goals[] = {StrId::STR_QUEST_CAM_ORDER_MINUTES, StrId::STR_QUEST_CAM_ORDER_PAGES,
                                    StrId::STR_QUEST_CAM_ORDER_SESSIONS, StrId::STR_QUEST_CAM_ORDER_BOOKS,
                                    StrId::STR_QUEST_CAM_ORDER_DAYS};
  static constexpr StrId shortGoals[] = {StrId::STR_QUEST_CAM_ORDER_SHORT_MINUTES,
                                         StrId::STR_QUEST_CAM_ORDER_SHORT_PAGES,
                                         StrId::STR_QUEST_CAM_ORDER_SHORT_SESSIONS,
                                         StrId::STR_QUEST_CAM_ORDER_SHORT_BOOKS, StrId::STR_QUEST_CAM_ORDER_SHORT_DAYS};
  const auto goal = quest::requestGoal(*m.profile, id);
  const auto target = quest::requestTarget(*m.profile, id);
  if (progressValue)
    goalProgressText(w, goal, quest::requestValue(*m.profile, id, m.summary), target, compact);
  else if (goal == quest::RequestGoal::Minutes && target >= 180 && target % 60 == 0)
    std::snprintf(w.label, sizeof(w.label), tr(STR_QUEST_HUNTS_HOURS), target / 60);
  else
    std::snprintf(w.label, sizeof(w.label), I18N.get((compact ? shortGoals : goals)[unsigned(goal)]), target);
}
inline const char* rewardName(int id) {
  if (id == quest::kShadowHunt) return tr(STR_QUEST_CAM_JOURNEY2_SHADOW_REWARD);
  if (id == quest::kKingHunt) return tr(STR_QUEST_CAM_JOURNEY2_KING_REWARD);
  const int decoration = quest::requestDecoration(id);
  return quest::visibleItem(decoration) ? I18N.get(questexp::DECOR_NAMES[decoration - quest::kExpandedItemStart])
                                        : I18N.get(questrequests::TEXTS[id].item);
}
inline void rewardArt(toybox::Screen& s, const Model& m, freeink::ui::Rect art, int id) {
  const int decoration = quest::requestDecoration(id);
  if (quest::visibleItem(decoration) && m.assets)
    questextra::paint(s.target(), art, questextra::decoration(*m.assets, m.rewardAssets, decoration));
  else if (id >= quest::kLegacyRequestCount)
    widget::icon(s, art, extra_trophy_32);
  else
    questkeepsake::draw(s.target(), art, id);
}
inline const char* extraName(int item) {
  if (item == quest::kInkRelic) return tr(STR_QUEST_HUNTS_INK_RELIC);
  const auto category = quest::slot(item);
  const int index = item - quest::first(category);
  return category == quest::Slot::Pet ? I18N.get(questcatalog::pet[index]) : I18N.get(questcatalog::background[index]);
}
}  // namespace questui::requestui

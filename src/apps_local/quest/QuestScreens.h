#pragma once
#include <components/controls/header-clock.h>

#include "../ui/ToyboxScreen.h"
#include "../ui/WordleKeyboard.h"
#include "QuestAssets.h"
#include "QuestCore.h"
#include "QuestReadingReturn.h"
namespace questui {
inline constexpr freeink::ui::FontId NARRATIVE_FONT = freeink::ui::FONT_SLOT_SMALL;
inline constexpr freeink::ui::FontId COIN_VALUE_FONT = 6;
inline constexpr freeink::ui::FontId HUNT_LABEL_FONT = freeink::ui::FONT_SLOT_TITLE;
static_assert(COIN_VALUE_FONT != freeink::ui::HEADER_CLOCK_FONT && COIN_VALUE_FONT != NARRATIVE_FONT);
static_assert(NARRATIVE_FONT != freeink::ui::HEADER_CLOCK_FONT);
enum class View : uint8_t {
  Hero,
  Missions,
  Gear,
  Shop,
  Profile,
  Info,
  Setup,
  Name,
  Settings,
  Campaign,
  ReadingData,
  Plan,
  EndConfirm,
  Ending,
  Book,
  Lore,
  ReadingReturn,
  Atlas,
  AtlasRegion,
  AtlasPlace,
  Request,
  Thanks,
  Recap,
  Requests,
  StoryPages,
  RequestCancelConfirm,
  RequestSwitchConfirm,
  PurchaseConfirm,
  Celebration,
  TryOut,
  HuntRewards,
  Experience
};
enum class CampaignTab : uint8_t { Saga, Path, Encounter };
inline bool showsSagaOpening(const quest::Profile& p, CampaignTab tab) {
  return tab != CampaignTab::Saga && !quest::needsPrologue(p) && quest::needsIntroduction(p);
}
inline bool showsCampaignReader(const quest::Profile& p, CampaignTab tab) {
  return quest::needsPrologue(p) || showsSagaOpening(p, tab) ||
         (tab == CampaignTab::Encounter && p.campaign.phase == quest::BattlePhase::Story);
}
inline constexpr int sagaPageSize(bool landscape) { return landscape ? 2 : 4; }
inline constexpr int atlasPageSize(bool) { return 4; }
inline constexpr int atlasRegionPageSize(bool) { return 4; }
enum class RequestFilter : uint8_t { Available, Collection };
enum class InventoryFilter : uint8_t { Recent, Favorites };
inline constexpr int PROLOGUE_SECTIONS = 4;
enum : freeink::ui::ActionId {
  Tab = 1,
  Claim,
  Outfit,
  Read,
  Gender,
  Category,
  Page,
  Milestone,
  Preview,
  Back,
  InventoryView,
  LevelContinue,
  OpenView,
  Start,
  NameKey,
  ConfirmName,
  Rename,
  CampaignSection,
  BattleReward,
  StoryContinue,
  EndAdventure,
  ConfirmEnd,
  Restart,
  SelectBook,
  PlanMinutes,
  EncounterInfo,
  IntroductionPage,
  FinishPrologue,
  BeginEncounter,
  ReturnPath,
  ResumeJourney,
  DismissReadingReturn,
  StoryReward,
  AtlasBook,
  AtlasChapter,
  RequestInfo,
  BackToRequests,
  RecapContinue,
  RecallStory,
  ShowHuntRewards,
  AcceptRequest,
  CollectRequest,
  FilterRequests,
  ActiveRequest,
  EndingContinue,
  StoryPageOpen,
  StoryPageDigit,
  StoryPageErase,
  StoryPageApply,
  CancelRequest,
  ConfirmRequestChange,
  KeepRequest,
  FinishSagaIntro,
  InventoryFilterChange,
  FavoriteItem,
  ConfirmPurchase,
  MilestoneContinue,
  OpenHuntCollection,
  TryOutUse,
  TryOutStore,
  ShowExperience
};
inline bool isRequestConfirmation(View view) {
  return view == View::RequestCancelConfirm || view == View::RequestSwitchConfirm;
}
inline void focusDecision(toybox::Interactions& interactions, freeink::ui::ActionId keep, freeink::ui::ActionId confirm,
                          int steps = 0) {
  const int current = interactions.focusedIndex();
  const auto action = current >= 0 && size_t(current) < interactions.count() ? interactions.data()[current].action
                                                                             : freeink::ui::NO_ACTION;
  if (!interactions.hasButtonFocus() || (action != keep && action != confirm)) {
    interactions.setFocusedIndex(-1);
    freeink::ui::InputSnapshot initial;
    initial.focusNext = true;
    interactions.route(initial);
  }
  if (!(steps % 2)) return;
  freeink::ui::InputSnapshot move;
  move.focusNext = steps > 0;
  move.focusPrev = steps < 0;
  interactions.route(move);
}
inline void focusRequestConfirmation(toybox::Interactions& interactions, int steps = 0) {
  focusDecision(interactions, KeepRequest, ConfirmRequestChange, steps);
}
inline bool isDecisionConfirmation(View view) {
  return isRequestConfirmation(view) || view == View::PurchaseConfirm || view == View::TryOut ||
         view == View::EndConfirm;
}
inline void focusConfirmation(toybox::Interactions& interactions, View view, int steps = 0) {
  if (view == View::TryOut)
    focusDecision(interactions, TryOutUse, TryOutStore, steps);
  else if (view == View::EndConfirm)
    focusDecision(interactions, Back, Restart, steps);
  else if (view == View::PurchaseConfirm)
    focusDecision(interactions, Back, ConfirmPurchase, steps);
  else
    focusRequestConfirmation(interactions, steps);
}
struct Model {
  const quest::Profile* profile = nullptr;
  const readingstats::Summary* summary = nullptr;
  View view = View::Hero;
  int today = 0;
  bool loading = false, failed = false, denied = false;
  int category = 0, page = 0, preview = -1, questPage = 0;
  sdart::Pack* assets = nullptr;
  int chosenGender = -1;
  const char* nameDraft = "";
  bool writable = true;
  bool creating = false;
  CampaignTab campaignTab = CampaignTab::Saga;
  int selectedBook = 0, dailyMinutes = 30;
  int selectedEncounter = 0;
  int introductionPage = 0;
  int storyPageCount = 1, storyPageDraft = 0;
  int selectedRequest = -1;
  RequestFilter requestFilter = RequestFilter::Available;
  InventoryFilter inventoryFilter = InventoryFilter::Recent;
  quest::StoreNotice storeNotice = quest::StoreNotice::None;
  bool atlasArrival = false;
  bool recalling = false;
  const questreturn::Report* readingReturn = nullptr;
  questreturn::Phase attackPhase = questreturn::Phase::Result;
  questreturn::Panel returnPanel = questreturn::Panel::Reading;
  sdart::Pack* storyAssets = nullptr;
  sdart::Pack* rewardAssets = nullptr;
  sdart::Pack* outfitAssets = nullptr;
  sdart::Pack* enemyAssets = nullptr;
  sdart::Pack* celebrationAssets = nullptr;
};
struct Work {
  wordlekeys::Layout keys;
  freeink::ui::ButtonProps button;
  freeink::ui::TabItem tabs[quest::kSlotCount]{};
  int page = 0, pages = 1;
  uint16_t storyPosition = 0;
  char label[160]{}, name[96]{};
  char storyLine[224]{};
  char tabLabels[quest::kSlotCount][32]{};
};
const char* rankTitle(uint32_t level);
void buildLevelUp(toybox::Screen&, const quest::Profile&, bool saveFailed, Work&);
void build(toybox::Screen&, const Model&, Work&);
}  // namespace questui

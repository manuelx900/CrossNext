#include "ReadingQuestActivity.h"

#include <ChromeRegion.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

#include <cstdlib>
#include <ctime>

#include "QuestSimulatorReading.h"
#endif

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "QuestAvatar.h"
#include "QuestCampaign.h"
#include "QuestJourney.h"
#include "QuestMilestones.h"
#include "QuestRequests.h"
#include "components/SideButtonNavigation.h"
#include "fontIds.h"
#ifdef SIMULATOR
#include "../pet/PetGrayTarget.h"
#endif

namespace fui = freeink::ui;
#ifdef __EMSCRIPTEN__
namespace {
questsim::ReadingMailbox simulatorReading;
std::atomic<unsigned> simulatorPaints{0};
}  // namespace
extern "C" EMSCRIPTEN_KEEPALIVE int crossplay_quest_reading(int minutes) {
  return int(simulatorReading.request(minutes));
}
extern "C" EMSCRIPTEN_KEEPALIVE int crossplay_quest_reading_status() { return int(simulatorReading.result()); }
extern "C" EMSCRIPTEN_KEEPALIVE unsigned crossplay_quest_render_count() {
  return simulatorPaints.load(std::memory_order_relaxed);
}
bool ReadingQuestActivity::processSimulatorReading() {
  int minutes = 0;
  if (!simulatorReading.begin(minutes)) return false;
  using Result = questsim::ReadingResult;
  if (creating || !profile.campaign.initialized) {
    simulatorReading.finish(Result::NeedsHero);
    return true;
  }
  if (view == questui::View::ReadingReturn || view == questui::View::Name || view == questui::View::StoryPages ||
      view == questui::View::EndConfirm || view == questui::View::Celebration ||
      questui::isDecisionConfirmation(view)) {
    simulatorReading.finish(Result::FinishScreen);
    return true;
  }
  if (failed || !writable) {
    simulatorReading.finish(Result::StorageError);
    return true;
  }
  if (loading || !historyComplete || !ready || !artReady()) {
    simulatorReading.finish(Result::NotReady);
    return true;
  }
  savePendingChanges();
  if (failed) {
    simulatorReading.finish(Result::StorageError);
    return true;
  }
  const uint32_t seconds = unsigned(minutes) * 60;
  if (seconds && !readingstats::recordSession(questsim::READING_DOCUMENT, tr(STR_QUEST_SIM_READING_BOOK), "",
                                              std::time(nullptr) - seconds, seconds, 0, 0)) {
    simulatorReading.finish(Result::StorageError);
    return true;
  }
  // Reuse the normal history scan and return reports, keeping the current profile.
  simulatorReadingMode = true;
  simulatorPendingSeconds = 0;
  today = readingstats::today();
  summary.seconds = summary.pages = 0;
  summary.sessions = summary.completed = summary.totalDays = 0;
  std::fill_n(summary.dailySeconds, readingstats::kHistoryDays, 0u);
  summary.anchorDay = today;
  summary.lastReadDay = 0;
  summary.run = summary.longest = 0;
  firstDayTotal = 0;
  readingReturn = {};
  returnPanel = questreturn::Panel::Reading;
  readingClock.cancel();
  view = questui::View::Hero;
  introductionPage = page = 0;
  preview = -1;
  ready = historyComplete = false;
  navigation.clear();
  loading = repository.begin(false, historyReadAhead, sizeof(historyReadAhead));
  if (!loading) {
    failed = true;
    LOG_ERR("QUEST", "Cannot rescan simulated reading history");
    simulatorReading.finish(Result::StorageError);
  }
  requestUpdate();
  return true;
}
void ReadingQuestActivity::applySimulatorPreview() {
  if (view != questui::View::Hero || creating || failed || simulatorReadingMode) return;
  const char* panel = std::getenv("CROSSNEXT_QUEST_PANEL");
  if (!panel) return;
  using View = questui::View;
  struct Panel {
    const char* name;
    View view;
  };
  static constexpr Panel PANELS[] = {
      {"hero", View::Hero}, {"encounter", View::Campaign}, {"path", View::Campaign},     {"sagas", View::Campaign},
      {"saga", View::Book}, {"lore", View::Lore},          {"missions", View::Missions}, {"requests", View::Requests},
      {"gear", View::Gear}, {"shop", View::Shop},          {"atlas", View::Atlas},       {"request", View::Request}};
  for (const auto& option : PANELS) {
    if (std::strcmp(panel, option.name)) continue;
    view = option.view;
    introductionPage = view == View::Book ? -1 : 0;
    page = 0;
    preview = -1;
    selectedBook = quest::book(profile);
    selectedEncounter = profile.campaign.encounter;
    if (view == View::Campaign)
      showCampaign(!std::strcmp(panel, "path")    ? questui::CampaignTab::Path
                   : !std::strcmp(panel, "sagas") ? questui::CampaignTab::Saga
                                                  : questui::CampaignTab::Encounter);
    if (view == View::Requests) requestListReturn = View::Hero;
    if (view == View::Lore) infoReturn = View::Campaign;
    if (view == View::Request) {
      const char* value = std::getenv("CROSSNEXT_QUEST_REQUEST");
      const int id = value ? std::atoi(value) : 0;
      selectedRequest = quest::validRequest(profile.activeRequest)                       ? profile.activeRequest
                        : quest::validRequest(id) && quest::requestUnlocked(profile, id) ? id
                                                                                         : 0;
    }
    if (const char* value = std::getenv("CROSSNEXT_QUEST_CATEGORY")) category = std::clamp(std::atoi(value), 0, 3);
    if (const char* value = std::getenv("CROSSNEXT_QUEST_ITEM"); value && (view == View::Gear || view == View::Shop)) {
      const int item = std::atoi(value);
      if (quest::availableItem(item)) {
        preview = item;
        category = int(quest::slot(item));
      }
    }
    break;
  }
}
#endif
void ReadingQuestActivity::showCampaign(questui::CampaignTab tab) {
  if (tab == questui::CampaignTab::Path && profile.campaign.phase == quest::BattlePhase::Story &&
      quest::lastPathInSaga(quest::chapter(profile)))
    tab = questui::CampaignTab::Encounter;
  atlasArrival = false;
  view = questui::View::Campaign;
  campaignTab = tab;
  introductionPage = 0;
  preview = -1;
  page = tab == questui::CampaignTab::Saga
             ? quest::book(profile) / questui::sagaPageSize(renderer.getScreenWidth() > renderer.getScreenHeight())
         : tab == questui::CampaignTab::Path
             ? int(quest::chapter(profile) % 10) / (renderer.getScreenWidth() > renderer.getScreenHeight() ? 4 : 5)
             : 0;
}
void ReadingQuestActivity::enterName() {
  navigation.clear();
  if (!nameKeyboard.begin(I18N.getLanguage(), quest::NAME_BYTES - 1, quest::NAME_LIMIT, true, nameDraft)) {
    LOG_ERR("QUEST", "Cannot open inline name keyboard");
    view = questui::View::Setup;
  }
  ready = false;
  requestUpdate();
}

namespace {
constexpr questui::View SECTIONS[] = {questui::View::Hero, questui::View::Campaign, questui::View::Missions,
                                      questui::View::Gear, questui::View::Shop,     questui::View::Settings};
}  // namespace
std::unique_ptr<Activity> ReadingQuestActivity::create(GfxRenderer& r, MappedInputManager& i) {
  auto app = makeUniqueNoThrow<ReadingQuestActivity>(r, i);
  if (!app) LOG_ERR("QUEST", "OOM: %u bytes", unsigned(sizeof(ReadingQuestActivity)));
  return app;
}
void ReadingQuestActivity::onEnter() {
  Activity::onEnter();
#ifdef __EMSCRIPTEN__
  simulatorReading.activate();
  if (const char* value = std::getenv("CROSSNEXT_QUEST_ORIENTATION")) {
    renderer.setOrientation(std::strcmp(value, "1") == 0 ? GfxRenderer::Orientation::LandscapeClockwise
                                                         : GfxRenderer::Orientation::Portrait);
  }
#endif
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  if (!assets.openGrayscale(questassets::PATH, questassets::COUNT, ART_CACHE) ||
      !storyAssets.openGrayscale(questassets::STORY_PATH, questassets::STORY_COUNT, STORY_CACHE) ||
      !rewardAssets.openGrayscale(questassets::REWARDS_PATH, questassets::REWARDS_COUNT, MONSTER_CACHE) ||
      !enemyAssets.openGrayscale(questassets::GUARDIANS_PATH, questassets::GUARDIANS_COUNT, MONSTER_CACHE) ||
      !outfitAssets.openGrayscale(questassets::BESTIARY_OUTFITS_PATH, questassets::BESTIARY_OUTFITS_COUNT,
                                  OUTFIT_CACHE) ||
      !celebrationAssets.openGrayscale(questassets::CELEBRATIONS_PATH, questassets::CELEBRATIONS_COUNT)) {
    celebrationAssets.close();
    outfitAssets.close();
    enemyAssets.close();
    rewardAssets.close();
    storyAssets.close();
    assets.close();
    loading = false;
    requestUpdate();
    return;
  }
  today = readingstats::today();
  summary.anchorDay = today;
  bool found = false;
  failed = !quest::load(profile, writable, storeWork, &found);
  if (!failed && (!found || profile.campaign.state == quest::AdventureState::Setup)) {
    creating = true;
    view = questui::View::Setup;
  }
  if (!failed && questreturn::pendingEnding(profile)) view = questui::View::Ending;
  loading = repository.begin(false, historyReadAhead, sizeof(historyReadAhead));
  if (!loading) failed = true;
  requestUpdate();
}
void ReadingQuestActivity::onExit() {
#ifdef __EMSCRIPTEN__
  simulatorReading.deactivate();
#endif
  rememberStoryPosition();
  savePendingChanges();
  readingClock.cancel();
  nameKeyboard.close();
  repository.close();
  celebrationAssets.close();
  outfitAssets.close();
  enemyAssets.close();
  rewardAssets.close();
  storyAssets.close();
  assets.close();
  Activity::onExit();
}
bool ReadingQuestActivity::commitDraft() {
  if (!historyComplete) {
    failed = true;
    return false;
  }
  quest::acknowledgeLevel(draft);
  draft.savedDay = uint32_t(std::clamp(today, 0, 84005));
  failed = !quest::save(draft, writable, storeWork);
  if (!failed) {
    profile = draft;
    profileDirty = false;
  }
  return !failed;
}
void ReadingQuestActivity::rememberStoryPosition() {
  if (!ready || view != questui::View::Book || loading || !historyComplete || selectedBook < 0 ||
      selectedBook >= int(quest::CAMPAIGN_BOOKS))
    return;
  auto& position = profile.sagaPositions[selectedBook];
  if (position == work.storyPosition) return;
  position = work.storyPosition;
  markProfileDirty();
}
void ReadingQuestActivity::markProfileDirty() {
  profileDirty = true;
  profileChangedAt = millis();
}
void ReadingQuestActivity::runIdleWork() {
  if (profileDirty && uint32_t(millis() - profileChangedAt) >= 1500) savePendingChanges();
}
void ReadingQuestActivity::prepareForSleep() {
  rememberStoryPosition();
  savePendingChanges();
}
void ReadingQuestActivity::requestUpdate(bool immediate) {
  focusRedraw = false;
  pendingTouch.invalidate();
  Activity::requestUpdate(immediate);
}
void ReadingQuestActivity::savePendingChanges() {
  if (!profileDirty || failed || !historyComplete || !writable) return;
  const auto previousNotice = storeWork.notice;
  draft = profile;
  if (!commitDraft() || previousNotice != storeWork.notice) {
    ready = false;
    requestUpdate();
  }
}
void ReadingQuestActivity::dismissReadingReport() {
  readingClock.cancel();
  const auto next = questreturn::nextPanel(returnPanel, readingReturn, profile);
  if (next != returnPanel) {
    returnPanel = next;
    readingClock.start(next == questreturn::Panel::Request);
    introductionPage = 0;
    navigation.clear();
    return;
  }
  const bool huntReport = returnPanel == questreturn::Panel::Request;
  const bool victory = readingReturn.victory;
  const bool advance = victory && profile.campaign.phase == quest::BattlePhase::Story;
  draft = profile;
  bool changed = false;
  if (huntReport) {
    switch (questreturn::finishHuntReport(draft, readingReturn, summary, today)) {
      case questreturn::HuntAcknowledgement::Rejected:
        denied = true;
        return;
      case questreturn::HuntAcknowledgement::Progress:
      case questreturn::HuntAcknowledgement::Reward:
        changed = true;
        break;
      case questreturn::HuntAcknowledgement::Unchanged:
        break;
    }
  }
  if (advance && !quest::advanceBattle(draft)) return;
  if ((changed || advance) && !commitDraft()) return;
  readingReturn = {};
  returnPanel = questreturn::Panel::Reading;
  returnToMenu();
  introductionPage = 0;
  navigation.clear();
#ifdef __EMSCRIPTEN__
  applySimulatorPreview();
#endif
}
void ReadingQuestActivity::returnToMenu() {
  view = questreturn::pendingEnding(profile)                    ? questui::View::Ending
         : questmilestone::pending(profile) >= 0                ? questui::View::Celebration
         : quest::nextOutfitOffer(profile) != quest::kGearCount ? questui::View::TryOut
                                                                : questui::View::Hero;
  navigation.clear();
}
void ReadingQuestActivity::dismissCelebration() {
  draft = profile;
  if (!questmilestone::acknowledge(draft) || !commitDraft()) return;
  returnToMenu();
}
void ReadingQuestActivity::leaveEnding(int reward) {
  const bool gifts = quest::visibleItem(reward) && quest::owns(profile, reward) && quest::hasWon(profile);
  if (writable && historyComplete && (!profile.endingSeen || (gifts && quest::isNewItem(profile, reward)))) {
    draft = profile;
    draft.endingSeen = true;
    if (gifts) quest::markItemSeen(draft, reward);
    if (!commitDraft()) return;
  }
  view = gifts ? questui::View::Gear : questui::View::Hero;
  category = gifts ? int(quest::slot(reward)) : int(quest::Slot::Body);
  page = 0;
  preview = gifts ? reward : -1;
  if (!gifts) returnToMenu();
  navigation.clear();
#ifdef __EMSCRIPTEN__
  applySimulatorPreview();
#endif
}
void ReadingQuestActivity::action(fui::ActionId event, int value) {
  using View = questui::View;
  if (!artReady()) {
    if (event == questui::Back) shelf::leave(renderer, mappedInput);
    return;
  }
  if (loading) return;
  const int previousMilestone = questmilestone::pending(profile);
  rememberStoryPosition();
  if (view == View::Setup && event != questui::Gender && event != questui::Start && event != questui::Back) return;
  if (view == View::Name && event != questui::NameKey && event != questui::ConfirmName && event != questui::Back)
    return;
  if (view == View::TryOut && event != questui::TryOutUse && event != questui::TryOutStore && event != questui::Back)
    return;
  if (view == View::Celebration && event != questui::MilestoneContinue && event != questui::Back) return;
  if (view == View::Ending && event != questui::EndingContinue && event != questui::Back) return;
  if (view == View::EndConfirm && event != questui::ConfirmEnd && event != questui::Restart && event != questui::Back)
    return;
  if (questui::isRequestConfirmation(view) && event != questui::ConfirmRequestChange && event != questui::KeepRequest &&
      event != questui::Back)
    return;
  if (view == View::PurchaseConfirm && event != questui::Back && event != questui::ConfirmPurchase) return;
  if (view == View::Experience && event != questui::Back) return;
  if (view == View::ReadingReturn && event != questui::DismissReadingReturn && event != questui::IntroductionPage &&
      event != questui::ShowHuntRewards && event != questui::Back)
    return;
  if (view == View::ReadingReturn && readingClock.playing() && event == questui::DismissReadingReturn) return;
  denied = false;
  switch (event) {
    case questui::ShowExperience:
      if (view != View::Hero) return;
      view = View::Experience;
      break;
    case questui::TryOutUse:
    case questui::TryOutStore:
      if (view != View::TryOut || !writable) return;
      draft = profile;
      if (!quest::finishOutfitOffer(draft, quest::nextOutfitOffer(draft), event == questui::TryOutUse)) return;
      if (event == questui::TryOutUse) quest::settleBattle(draft);
      if (commitDraft()) {
        view = View::Hero;
        navigation.clear();
      }
      break;
    case questui::MilestoneContinue:
      if (view != View::Celebration) return;
      dismissCelebration();
      break;
    case questui::OpenHuntCollection:
      requestListReturn = view == View::Hero ? View::Hero : View::Missions;
      view = View::Requests;
      requestFilter = questui::RequestFilter::Collection;
      page = 0;
      break;
    case questui::EndingContinue:
      if (view != View::Ending || value < 0 || value > 5) return;
      {
        static constexpr int GIFTS[] = {quest::kKingBody, quest::kKingBackground, quest::kKingPet,
                                        quest::kKingDecoration};
        leaveEnding(value == 0 ? -1 : GIFTS[value == 1 ? 0 : value - 2]);
      }
      break;
    case questui::DismissReadingReturn:
      if (view != View::ReadingReturn) return;
      dismissReadingReport();
      break;
    case questui::RecapContinue:
      if (view != View::Recap || work.page + 1 != work.pages) return;
      view = View::Hero;
      [[fallthrough]];
    case questui::ResumeJourney:
      if (view != View::Hero && !(view == View::AtlasPlace && atlasArrival)) return;
      showCampaign(quest::needsPrologue(profile)         ? questui::CampaignTab::Saga
                   : quest::needsEncounterStory(profile) ? questui::CampaignTab::Path
                                                         : questui::CampaignTab::Encounter);
      break;
    case questui::OpenView:
      if (value < int(View::Hero) || value > int(View::Requests) || value == int(view)) return;
      if (value == int(View::Atlas) && !profile.campaign.prologueSeen) return;
      infoReturn = view;
      view = View(value);
      atlasArrival = false;
      preview = -1;
      page = 0;
      if (view == View::Requests) {
        requestListReturn = infoReturn == View::Hero ? View::Hero : View::Missions;
        requestFilter = questui::RequestFilter::Available;
      }
      if (view == View::Atlas)
        page = quest::book(profile) / questui::atlasPageSize(renderer.getScreenWidth() > renderer.getScreenHeight());
      introductionPage = 0;
      if (view == View::Campaign) {
        showCampaign(quest::hasWon(profile) ? questui::CampaignTab::Saga : questui::CampaignTab::Path);
      }
      if (view == View::Lore) selectedEncounter = profile.campaign.encounter;
      break;
    case questui::Tab:
      if (value < 0 || value >= int(sizeof(SECTIONS) / sizeof(SECTIONS[0])) || view == SECTIONS[value]) return;
      view = SECTIONS[value];
      preview = -1;
      page = 0;
      if (view == View::Campaign) showCampaign(questui::CampaignTab::Path);
      break;
    case questui::Category:
      if (value < 0 || value >= quest::kCategoryCount || (category == value && preview < 0)) return;
      category = value;
      page = 0;
      preview = -1;
      break;
    case questui::InventoryView:
      if (view == (value ? View::Gear : View::Shop) && preview < 0) return;
      view = value ? View::Gear : View::Shop;
      page = 0;
      preview = -1;
      break;
    case questui::Page:
      if (preview >= 0) return;
      if (std::clamp(work.page + value, 0, work.pages - 1) == work.page) return;
      page = std::clamp(work.page + value, 0, work.pages - 1);
      break;
    case questui::Preview:
      if ((view != View::Gear && view != View::Shop) || !quest::availableItem(value)) return;
      if (writable && quest::isNewItem(profile, value)) {
        quest::markItemSeen(profile, value);
        markProfileDirty();
      }
      preview = value;
      break;
    case questui::InventoryFilterChange:
      if (view != View::Gear || preview >= 0 || value < 0 || value > int(questui::InventoryFilter::Favorites)) return;
      if (inventoryFilter == questui::InventoryFilter(value)) return;
      inventoryFilter = questui::InventoryFilter(value);
      page = 0;
      break;
    case questui::FavoriteItem:
      if (view != View::Gear || preview != value || !writable || !quest::owns(profile, value)) return;
      if (quest::toggleFavorite(profile, value)) markProfileDirty();
      break;
    case questui::KeepRequest:
      if (!questui::isRequestConfirmation(view)) return;
      [[fallthrough]];
    case questui::Back:
      if (view == View::PurchaseConfirm) {
        view = View::Shop;
        break;
      }
      if (preview >= 0) {
        preview = -1;
        break;
      }
      if ((view == View::Request || view == View::Thanks || view == View::Recap) && introductionPage > 0) {
        --introductionPage;
        break;
      }
      switch (view) {
        case View::TryOut:
        case View::Celebration:
          view = View::Hero;
          navigation.clear();
          break;
        case View::PurchaseConfirm:
          view = View::Shop;
          break;
        case View::HuntRewards:
          view = rewardsReturn;
          break;
        case View::RequestCancelConfirm:
        case View::RequestSwitchConfirm:
          view = View::Request;
          break;
        case View::StoryPages:
          view = storyReturn;
          break;
        case View::ReadingReturn:
          readingClock.cancel();
          readingReturn = {};
          returnPanel = questreturn::Panel::Reading;
          view = View::Hero;
          break;
        case View::Recap:
          view = View::Hero;
          break;
        case View::Request:
          if (infoReturn == View::Hero || profile.activeRequest == selectedRequest) {
            view = View::Hero;
            page = 0;
            break;
          }
          [[fallthrough]];
        case View::Thanks:
          if (view == View::Thanks || profile.activeRequest == selectedRequest) page = 0;
          view = View::Requests;
          break;
        case View::Requests:
          view = requestListReturn;
          break;
        case View::Atlas:
          showCampaign(questui::CampaignTab::Saga);
          break;
        case View::AtlasRegion:
          view = View::Atlas;
          page = selectedBook / questui::atlasPageSize(renderer.getScreenWidth() > renderer.getScreenHeight());
          break;
        case View::AtlasPlace:
          page = (selectedEncounter / 6 % 10) /
                 questui::atlasRegionPageSize(renderer.getScreenWidth() > renderer.getScreenHeight());
          view = View::AtlasRegion;
          break;
        case View::Hero:
        case View::Setup:
          shelf::leave(renderer, mappedInput);
          return;
        case View::Ending:
          leaveEnding(-1);
          if (view == View::Ending) view = View::Hero;
          break;
        case View::Name:
          nameKeyboard.close();
          view = creating ? View::Setup : View::Profile;
          break;
        case View::EndConfirm:
          view = View::Settings;
          break;
        case View::Profile:
        case View::ReadingData:
          view = View::Settings;
          break;
        case View::Info:
        case View::Plan:
          view = infoReturn == View::Gear || infoReturn == View::Shop || infoReturn == View::Campaign ? infoReturn
                                                                                                      : View::Settings;
          break;
        case View::Book:
          view = View::Campaign;
          campaignTab = questui::CampaignTab::Saga;
          break;
        case View::Lore:
          if (infoReturn == View::Recap) {
            view = View::Recap;
            introductionPage = 0;
            break;
          }
          view = View::Campaign;
          campaignTab = questui::CampaignTab::Path;
          break;
        case View::Experience:
        case View::Missions:
        case View::Gear:
        case View::Shop:
        case View::Settings:
        case View::Campaign:
          view = View::Hero;
          break;
      }
      break;
    case questui::Start:
      if (view != View::Setup || chosenGender < 0 || chosenGender > 1) return;
      view = View::Name;
      enterName();
      break;
    case questui::Rename:
      if (view != View::Profile) return;
      chosenGender = int(profile.gender);
      std::memcpy(nameDraft, profile.name, sizeof(nameDraft));
      view = View::Name;
      enterName();
      break;
    case questui::NameKey:
      if (view != View::Name) return;
      if (value != wordlekeys::kEnter) {
        if (!(value == wordlekeys::kErase ? quest::eraseName(nameDraft) : quest::typeName(nameDraft, char(value))))
          return;
        break;
      }
      [[fallthrough]];
    case questui::ConfirmName:
      if (view != View::Name || !quest::finishName(nameDraft)) return;
      draft = profile;
      if (creating) {
        quest::restartAdventure(draft, summary, today);
        draft.gender = quest::Gender(chosenGender);
        draft.campaign.state = quest::AdventureState::Active;
      }
      std::memcpy(draft.name, nameDraft, sizeof(nameDraft));
      if (commitDraft()) {
        creating = false;
        view = View::Hero;
      }
      break;
    case questui::Gender:
      if (value < 0 || value > 1) return;
      if (view == View::Setup) {
        if (chosenGender == value) return;
        chosenGender = value;
        break;
      }
      if (view != View::Profile || int(profile.gender) == value) return;
      draft = profile;
      draft.gender = quest::Gender(value);
      commitDraft();
      break;
    case questui::ConfirmPurchase:
    case questui::Outfit:
      if (!quest::availableItem(value)) return;
      if (event == questui::ConfirmPurchase) {
        if (view != View::PurchaseConfirm || value != preview || quest::owns(profile, value) ||
            quest::storyExclusive(value))
          return;
      } else {
        if (view != View::Gear && view != View::Shop) return;
        if (!quest::owns(profile, value)) {
          if (view != View::Shop || !writable || quest::storyExclusive(value) || !quest::shopUnlocked(profile, value)) {
            denied = true;
            break;
          }
          preview = value;
          view = View::PurchaseConfirm;
          navigation.clear();
          break;
        }
      }
      draft = profile;
      if (!(quest::equipped(draft, value) && quest::slot(value) != quest::Slot::Body ? quest::removeItem(draft, value)
                                                                                     : quest::equip(draft, value))) {
        denied = true;
        break;
      }
      quest::settleBattle(draft);
      if (commitDraft()) {
        view = View::Hero;
        navigation.clear();
        category = int(quest::slot(value));
        page = 0;
        preview = -1;
      }
      break;
    case questui::Claim:
      if (view != View::Missions || value < 0 || value > int(quest::Mission::Weekly)) return;
      draft = profile;
      denied = !quest::claimMission(draft, summary, today, quest::Mission(value));
      if (!denied) commitDraft();
      break;
    case questui::RequestInfo:
      if ((view != View::Requests &&
           !(view == View::Hero && (value == profile.activeRequest || quest::hasWon(profile)))) ||
          !quest::validRequest(value) ||
          (!quest::requestUnlocked(profile, value) && !quest::requestClaimed(profile, value)))
        return;
      selectedRequest = value;
      if (view == View::Hero) requestListReturn = View::Hero;
      infoReturn = view;
      view = View::Request;
      introductionPage = 0;
      break;
    case questui::AcceptRequest:
    case questui::CollectRequest:
      if (view != View::Request || value != selectedRequest || work.page + 1 != work.pages) return;
      if (event == questui::AcceptRequest) {
        value = quest::replayHuntId(value);
        if (!quest::validRequest(value)) return;
        selectedRequest = value;
      }
      if (event == questui::AcceptRequest && quest::validRequest(profile.activeRequest)) {
        if (profile.activeRequest == value || !quest::requestUnlocked(profile, value)) return;
        if (quest::requestValue(profile, profile.activeRequest, &summary) >=
            quest::requestTarget(profile, profile.activeRequest)) {
          selectedRequest = profile.activeRequest;
          introductionPage = 0;
          break;
        }
        view = View::RequestSwitchConfirm;
        break;
      }
      draft = profile;
      denied = event == questui::AcceptRequest ? !quest::acceptRequest(draft, value)
                                               : !quest::collectRequest(draft, value, &summary, today);
      if (!denied && commitDraft()) {
        if (event == questui::CollectRequest) {
          requestFilter = questui::RequestFilter::Collection;
          returnToMenu();
        } else
          infoReturn = View::Hero;
        page = introductionPage = 0;
      }
      break;
    case questui::CancelRequest:
      if (view != View::Request || value != selectedRequest || profile.activeRequest != value) return;
      view = View::RequestCancelConfirm;
      break;
    case questui::ConfirmRequestChange:
      if ((view != View::RequestCancelConfirm && view != View::RequestSwitchConfirm) || value != selectedRequest)
        return;
      draft = profile;
      denied = view == View::RequestCancelConfirm ? !quest::cancelRequest(draft, selectedRequest, &summary)
                                                  : !quest::replaceRequest(draft, selectedRequest, &summary);
      if (!denied && commitDraft()) {
        view = profile.activeRequest == selectedRequest ? View::Request : View::Requests;
        if (view == View::Request) infoReturn = View::Hero;
        requestFilter = questui::RequestFilter::Available;
        page = introductionPage = 0;
      }
      break;
    case questui::ShowHuntRewards:
      if (view == View::ReadingReturn) {
        if (readingClock.playing() || returnPanel != questreturn::Panel::Request ||
            !quest::validRequest(readingReturn.requestId))
          return;
        selectedRequest = readingReturn.requestId;
      } else if (view != View::Request && view != View::Thanks)
        return;
      if (!quest::validRequest(selectedRequest) || quest::requestClaimed(profile, selectedRequest)) return;
      rewardsReturn = view;
      view = View::HuntRewards;
      break;
    case questui::ActiveRequest:
      if (view != View::Request || !quest::validRequest(profile.activeRequest)) return;
      selectedRequest = profile.activeRequest;
      introductionPage = 0;
      break;
    case questui::FilterRequests:
      if (view != View::Requests || value < 0 || value > int(questui::RequestFilter::Collection) ||
          value == int(requestFilter))
        return;
      requestFilter = questui::RequestFilter(value);
      page = 0;
      break;
    case questui::BackToRequests:
      if (view != View::Thanks && view != View::Request) return;
      if (view == View::Thanks || quest::validRequest(profile.activeRequest)) page = 0;
      view = View::Requests;
      introductionPage = 0;
      break;
    case questui::AtlasBook:
      if ((view != View::Atlas && view != View::AtlasPlace) || !profile.campaign.prologueSeen || value < 0 ||
          value > int(quest::book(profile)))
        return;
      selectedBook = value;
      page = 0;
      atlasArrival = false;
      view = View::AtlasRegion;
      introductionPage = 0;
      break;
    case questui::AtlasChapter:
      if (view != View::AtlasRegion || value < 0 || value >= int(quest::CAMPAIGN_CHAPTERS) ||
          !questjourney::canVisit(profile, value) || value / 10 != selectedBook)
        return;
      selectedEncounter = value * 6 + 5;
      atlasArrival = false;
      view = View::AtlasPlace;
      introductionPage = 0;
      break;
    case questui::RecallStory: {
      if (view != View::Recap) return;
      const auto context = questjourney::context(profile);
      if (context.prologue) {
        showCampaign(questui::CampaignTab::Saga);
        break;
      }
      selectedEncounter = questjourney::recallEncounter(context);
      infoReturn = View::Recap;
      view = View::Lore;
      introductionPage = 0;
      break;
    }
    case questui::CampaignSection:
      if (value < 0 || value > 2 || (view == View::Campaign && campaignTab == questui::CampaignTab(value))) return;
      showCampaign(questui::CampaignTab(value));
      break;
    case questui::StoryReward:
      if (view != View::Campaign || campaignTab != questui::CampaignTab::Encounter || (value != 0 && value != 1))
        return;
      draft = profile;
      if (quest::dismissStoryReward(draft, value == 1) && commitDraft()) introductionPage = 0;
      break;
    case questui::BattleReward:
      if (view != View::Campaign || campaignTab != questui::CampaignTab::Encounter) return;
      draft = profile;
      denied = !quest::collectBattleResult(draft);
      if (!denied) commitDraft();
      break;
    case questui::FinishPrologue:
      if (view != View::Campaign || work.page + 1 != work.pages) return;
      draft = profile;
      if (quest::acknowledgePrologue(draft)) {
        quest::normalizeBattles(draft);
        if (!commitDraft()) break;
        showCampaign(questui::CampaignTab::Path);
      }
      break;
    case questui::FinishSagaIntro:
      if (view != View::Campaign || !questui::showsSagaOpening(profile, campaignTab) || work.page + 1 != work.pages)
        return;
      draft = profile;
      if (quest::beginCampaign(draft) && commitDraft()) showCampaign(questui::CampaignTab::Path);
      break;
    case questui::BeginEncounter: {
      if (value != int(quest::battleAnchor(quest::chapter(profile))) || quest::needsPrologue(profile) ||
          quest::needsIntroduction(profile) || quest::chapter(profile) < quest::completedPaths(profile) ||
          profile.campaign.state != quest::AdventureState::Active)
        return;
      const bool path = view == View::Campaign && campaignTab == questui::CampaignTab::Path;
      const bool brief = view == View::Lore && infoReturn == View::Campaign && selectedEncounter == value &&
                         work.page + 1 == work.pages;
      if (!path && !brief) return;
      if (profile.campaign.phase != quest::BattlePhase::Reading ||
          (!quest::needsIntroduction(profile) && !quest::needsEncounterStory(profile) &&
           profile.campaign.encounter == unsigned(value))) {
        if (path) showCampaign(questui::CampaignTab::Encounter);
        break;
      }
      if (!brief) return;
      draft = profile;
      if (!quest::prepareBattle(draft)) return;
      if (commitDraft()) showCampaign(questui::CampaignTab::Encounter);
      break;
    }
    case questui::ReturnPath:
      if (view != View::Book && view != View::Lore) return;
      if (view == View::Lore && infoReturn == View::Recap) {
        view = View::Recap;
        introductionPage = 0;
        break;
      }
      showCampaign(questui::CampaignTab::Path);
      break;
    case questui::IntroductionPage:
      if ((view != View::Book && view != View::Lore && view != View::AtlasPlace && view != View::Request &&
           view != View::Thanks && view != View::Recap && view != View::ReadingReturn &&
           !(view == View::Campaign && questui::showsCampaignReader(profile, campaignTab))) ||
          (value != 1 && value != -1))
        return;
      if (std::clamp(work.page + value, 0, work.pages - 1) == work.page) return;
      introductionPage = std::clamp(work.page + value, 0, work.pages - 1);
      break;
    case questui::StoryPageOpen:
      if (view != View::Book && view != View::Lore && view != View::AtlasPlace &&
          !(view == View::Campaign && questui::showsCampaignReader(profile, campaignTab)))
        return;
      storyReturn = view;
      storyPageCount = work.pages;
      storyPageDraft = 0;
      introductionPage = work.page;
      view = View::StoryPages;
      break;
    case questui::StoryPageDigit:
      if (view != View::StoryPages || value < 0 || value > 9) return;
      if ((storyPageDraft == 0 && value == 0) || storyPageDraft > (storyPageCount - value) / 10 ||
          storyPageDraft * 10 + value > storyPageCount)
        return;
      storyPageDraft = storyPageDraft * 10 + value;
      break;
    case questui::StoryPageErase:
      if (view != View::StoryPages || storyPageDraft == 0) return;
      storyPageDraft /= 10;
      break;
    case questui::StoryPageApply:
      if (view != View::StoryPages || storyPageDraft < 1 || storyPageDraft > storyPageCount) return;
      introductionPage = storyPageDraft - 1;
      view = storyReturn;
      break;
    case questui::StoryContinue:
      if (view != View::Campaign || campaignTab != questui::CampaignTab::Encounter || work.page + 1 != work.pages)
        return;
      draft = profile;
      denied = !quest::advanceBattle(draft);
      if (!denied && commitDraft()) {
        const bool final = profile.campaign.phase == quest::BattlePhase::Complete;
        showCampaign(questui::CampaignTab::Path);
        if (final) {
          view = View::Ending;
        }
      }
      break;
    case questui::EndAdventure:
      if (view != View::Settings || !writable) return;
      view = View::EndConfirm;
      break;
    case questui::ConfirmEnd:
      if (view != View::EndConfirm) return;
      draft = profile;
      denied = !quest::finishAdventure(draft);
      if (!denied && commitDraft()) view = View::Ending;
      break;
    case questui::Restart:
      if (view != View::EndConfirm || !writable) return;
      draft = profile;
      quest::restartAdventure(draft, summary, today);
      if (commitDraft()) {
        creating = true;
        view = View::Setup;
        chosenGender = -1;
        nameDraft[0] = 0;
        category = 0;
        page = 0;
        preview = -1;
        campaignTab = questui::CampaignTab::Saga;
        introductionPage = 0;
      }
      break;
    case questui::SelectBook:
      if (view != View::Campaign || campaignTab != questui::CampaignTab::Saga || quest::needsPrologue(profile) ||
          value < 0 || value > int(quest::book(profile)))
        return;
      selectedBook = value;
      view = View::Book;
      introductionPage = -1;
      break;
    case questui::PlanMinutes:
      if (view != View::Plan || (value != 15 && value != 30 && value != 60)) return;
      dailyMinutes = value;
      break;
    case questui::EncounterInfo:
      if (view != View::Campaign || campaignTab != questui::CampaignTab::Path || quest::needsPrologue(profile) ||
          profile.campaign.state != quest::AdventureState::Active || quest::needsIntroduction(profile) ||
          profile.campaign.phase != quest::BattlePhase::Reading ||
          value != int(quest::battleAnchor(quest::chapter(profile))))
        return;
      selectedEncounter = value;
      infoReturn = View::Campaign;
      introductionPage = 0;
      view = View::Lore;
      break;
    case questui::Milestone:
    case questui::Read:
    case questui::LevelContinue:
      return;
    default:
      return;
  }
  if (!failed && previousMilestone < 0 && questmilestone::pending(profile) >= 0 && view != View::ReadingReturn &&
      !questreturn::pendingEnding(profile))
    view = View::Celebration;
  ready = false;
  requestUpdate();
}
void ReadingQuestActivity::loop() {
  if (view == questui::View::Name)
    nameKeyboard.capture(mappedInput);
  else if (!navigation.capture(mappedInput, millis()))
    LOG_ERR("APP-NAV", "Navigation queue full");
  int touchedX = 0, touchedY = 0;
  if (mappedInput.wasScreenTouchPressed(touchedX, touchedY)) pendingTouch.pressed();
  const bool touchThisFrame = mappedInput.wasScreenTapped(touchedX, touchedY);
  if (touchThisFrame) pendingTouch.capture(touchedX, touchedY);
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  rememberStoryPosition();
  if (profileDirty && !failed && !navigation.hasPending() && !pendingTouch.hasPending() &&
      uint32_t(millis() - profileChangedAt) >= 1500)
    activityManager.requestIdleWork(*this);
#ifdef __EMSCRIPTEN__
  if (processSimulatorReading()) return;
  if (simulatorReading.result() == questsim::ReadingResult::Scanning) navigation.clear();
#endif
  if (view == questui::View::Name && ready) {
    const auto key = nameKeyboard.poll(mappedInput, false);
    if (key != EmbeddedKeyboard::Event::None) {
      if (key == EmbeddedKeyboard::Event::Cancel) action(questui::Back, 0);
      if (key == EmbeddedKeyboard::Event::Changed) requestUpdate();
      if (key == EmbeddedKeyboard::Event::Submit) {
        std::snprintf(nameDraft, sizeof(nameDraft), "%s", nameKeyboard.text());
        action(questui::ConfirmName, 0);
        if (view != questui::View::Name) nameKeyboard.close();
      }
      return;
    }
    return;
  }

  sectionnav::Input::Event nav{};
  const bool hasNavigation = ((!loading && ready) || navigation.backPending()) && navigation.pop(nav);
  if (hasNavigation) pendingTouch.clear();
  if (hasNavigation && nav.kind == sectionnav::Input::Kind::Back) {
    if (loading)
      shelf::leave(renderer, mappedInput);
    else
      action(questui::Back, 0);
    return;
  }
  if (loading) {
    for (int n = 0; n < 32; ++n) {
      const auto result = repository.next(record);
      if (result == readingstats::ScanResult::Record) {
#ifdef __EMSCRIPTEN__
        const auto start = summary.seconds;
#endif
        summary.add(record);
#ifdef __EMSCRIPTEN__
        if (!std::strcmp(record.book.document, questsim::READING_DOCUMENT))
          simulatorPendingSeconds += questsim::unobservedPart(start, summary.seconds, profile.campaign.observedSeconds);
#endif
        if (profile.campaign.initialized)
          firstDayTotal =
              std::min<uint64_t>(86400, firstDayTotal + quest::recordSecondsOnDay(record, profile.campaign.startedDay));
      }
      if (result == readingstats::ScanResult::Error || result == readingstats::ScanResult::Finished) {
        loading = false;
        historyComplete = result == readingstats::ScanResult::Finished;
        repository.close();
        failed |= result == readingstats::ScanResult::Error;
        if (!failed) {
          const bool firstDayRead = firstDayTotal > profile.campaign.startDaySeconds;
          draft = profile;
          quest::ReadingCredit credit;
#ifdef __EMSCRIPTEN__
          credit.withoutReserve = simulatorPendingSeconds;
#endif
          const bool changed = quest::syncCampaign(draft, summary, today, credit);
          const bool firstDayChanged =
              draft.campaign.state == quest::AdventureState::Active && firstDayRead && !draft.campaign.firstDayRead;
          draft.campaign.firstDayRead |= firstDayChanged;
          const bool collected =
              draft.campaign.phase == quest::BattlePhase::Victory && quest::collectBattleResult(draft);
          const bool acknowledged = quest::acknowledgeLevel(draft);
          if (!creating) readingReturn = questreturn::capture(profile, draft, credit);
          const bool requestChanged = !creating && questreturn::captureRequest(profile, draft, summary, readingReturn);
          draft.savedDay = uint32_t(std::clamp(today, 0, 84005));
          if (!creating && (changed || collected || acknowledged || firstDayChanged || requestChanged))
            failed = !quest::save(draft, writable, storeWork);
          if (!failed) {
            profile = draft;
            if (readingReturn.kind != questreturn::Kind::None || readingReturn.requestId >= 0) {
              returnPanel =
                  questreturn::huntOnly(readingReturn) ? questreturn::Panel::Request : questreturn::Panel::Reading;
              view = questui::View::ReadingReturn;
              navigation.clear();
              readingClock.start(returnPanel == questreturn::Panel::Request ||
                                 readingReturn.kind == questreturn::Kind::Combat);
            } else if (!creating)
              returnToMenu();
#ifdef __EMSCRIPTEN__
            applySimulatorPreview();
#endif
          }
        }
        requestUpdate();
        break;
      }
    }
    return;
  }
  if (ready && artReady()) {
    if (questui::isDecisionConfirmation(view) && hasNavigation) {
      switch (nav.kind) {
        case sectionnav::Input::Kind::Step: {
          const int previous = interactions.focusedIndex();
          fui::Rect before, after;
          const bool hadFocus = interactions.buttonFocusRect(before);
          questui::focusConfirmation(interactions, view, nav.amount);
          if (interactions.focusedIndex() != previous && interactions.buttonFocusRect(after)) {
            const int left = hadFocus ? std::min(before.x, after.x) : after.x;
            const int top = hadFocus ? std::min(before.y, after.y) : after.y;
            const int right = hadFocus ? std::max(before.right(), after.right()) : after.right();
            const int bottom = hadFocus ? std::max(before.bottom(), after.bottom()) : after.bottom();
            focusArea =
                fui::makeRect(std::max(0, left - 6), std::max(0, top - 6), right - left + 12, bottom - top + 12);
            focusRedraw = true;
            Activity::requestUpdate();
          }
          return;
        }
        case sectionnav::Input::Kind::Confirm: {
          questui::focusConfirmation(interactions, view);
          fui::InputSnapshot confirm;
          confirm.confirm = true;
          const auto event = interactions.route(confirm);
          action(event.action, event.value);
          return;
        }
        case sectionnav::Input::Kind::Section:
        case sectionnav::Input::Kind::Back:
          return;
      }
    }
    if (view == questui::View::ReadingReturn && hasNavigation && nav.kind == sectionnav::Input::Kind::Confirm) {
      action(questui::DismissReadingReturn, 0);
      return;
    }
    if (view == questui::View::ReadingReturn && readingClock.advance(millis())) requestUpdate();
    const bool pageable =
        preview < 0 && (view == questui::View::Gear || view == questui::View::Shop || view == questui::View::Atlas ||
                        view == questui::View::AtlasRegion || view == questui::View::Requests ||
                        (view == questui::View::Campaign &&
                         (campaignTab == questui::CampaignTab::Saga || campaignTab == questui::CampaignTab::Path) &&
                         !quest::needsPrologue(profile) && !questui::showsSagaOpening(profile, campaignTab)));
    if (pageable) {
      const auto event = sidebuttons::pageSwipe(interactions, mappedInput);
      if (event.action) {
        action(event.action, event.value);
        return;
      }
    }
    const int direction =
        hasNavigation && (nav.kind == sectionnav::Input::Kind::Step || nav.kind == sectionnav::Input::Kind::Section)
            ? nav.amount
            : 0;
    if (direction && preview < 0) {
      interactions.setFocusedIndex(-1);
      if (view == questui::View::Name || view == questui::View::Ending || view == questui::View::EndConfirm ||
          view == questui::View::ReadingReturn || view == questui::View::Experience)
        return;
      if (view == questui::View::StoryPages) {
        const int next = std::clamp(storyPageDraft + (direction > 0 ? 1 : -1), 1, storyPageCount);
        if (next == storyPageDraft) return;
        storyPageDraft = next;
        ready = false;
        requestUpdate();
      } else if (view == questui::View::Setup)
        action(questui::Gender, chosenGender < 0 ? 0 : 1 - chosenGender);
      else if (pageable)
        action(questui::Page, direction);
      else if (view == questui::View::Book || view == questui::View::Lore || view == questui::View::AtlasPlace ||
               view == questui::View::Request || view == questui::View::Thanks || view == questui::View::Recap ||
               (view == questui::View::Campaign && questui::showsCampaignReader(profile, campaignTab)))
        action(questui::IntroductionPage, direction > 0 ? 1 : -1);
      else if (view == questui::View::Campaign)
        action(questui::CampaignSection, (int(campaignTab) + direction % 3 + 3) % 3);
      else {
        int section = 0;
        const auto sectionView = view == questui::View::Requests ? questui::View::Missions : view;
        for (int n = 0; n < int(sizeof(SECTIONS) / sizeof(SECTIONS[0])); ++n)
          if (sectionView == SECTIONS[n]) section = n;
        action(questui::Tab, (section + direction % 6 + 6) % 6);
      }
      return;
    }
  }
  int x, y;
  if (!ready) return;
  if (pendingTouch.hasPending()) {
    if (!pendingTouch.take(x, y)) return;
    interactions.setFocusedIndex(-1);
  } else if (touchThisFrame || !sidebuttons::poll(interactions, mappedInput, x, y)) {
    return;
  }
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = x;
  input.touchY = y;
  auto event = sidebuttons::route(interactions, input, mappedInput, *this);
  if (view == questui::View::Name && event.action == questui::NameKey && event.value == 0)
    event.value = wordlekeys::keyAt(work.keys, x, y);
  action(event.action, event.value);
}
void ReadingQuestActivity::render(RenderLock&&) {
#ifdef __EMSCRIPTEN__
  simulatorPaints.fetch_add(1, std::memory_order_relaxed);
#endif
  bool partial = focusRedraw && ready;
  focusRedraw = false;
  if (!partial) renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  if (partial) {
    target.setClipRect(focusArea);
    target.fill(focusArea, fui::Paint::solid(fui::Color::White));
  }
  target.setFont(0, UI_12_FONT_ID);
  target.setFont(1, UI_12_FONT_ID);
  target.setFont(questui::HUNT_LABEL_FONT, toybox::kProseFontId);
  target.setFont(3, SMALL_FONT_ID);
  target.setFont(4, UI_10_FONT_ID);
  target.setFont(questui::COIN_VALUE_FONT, toybox::kDisplayFontId);
  target.setFont(questui::NARRATIVE_FONT, UI_12_FONT_ID);
  target.setFont(fui::HEADER_CLOCK_FONT, toybox::kUiFontId);
  const fui::InputSnapshot input{};
  const auto status = toybox::headerStatus(renderer);
  if (view == questui::View::Name) {
    toybox::Frame frame(target, target.deviceContext(), input, interactions, nullptr, status);
    toybox::Screen screen(frame);
    EmbeddedKeyboard::NamePresentation name;
    name.prompt = tr(STR_QUEST_NAME_TITLE);
    name.subtitle = tr(STR_QUEST_NAME_INTRO);
    name.cardTitle = questui::rankTitle(quest::level(profile.xp));
    name.cardText = tr(STR_PET_NAME_COMPANION);
    name.fieldLabel = tr(STR_QUEST_NAME_LABEL);
    name.placeholder = tr(STR_QUEST_NAME_PLACEHOLDER);
    name.confirmLabel = creating ? tr(STR_PET_NAME_CONFIRM) : tr(STR_QUEST_SAVE_NAME);
    name.artContext = this;
    name.artId = profile.equipped[0];
    name.drawArt = [](void* context, fui::DrawTarget& surface, fui::Rect box, uint32_t id, bool) {
      auto& app = *static_cast<ReadingQuestActivity*>(context);
      const auto gender = app.creating ? quest::Gender(std::clamp(app.chosenGender, 0, 1)) : app.profile.gender;
      const auto art = questextra::figure(app.assets, &app.rewardAssets, id, gender, &app.outfitAssets);
      questextra::paint(surface, box, art);
      return art.pack && art.pack->ready();
    };
    name.enabled = writable && historyComplete;
    nameKeyboard.drawName(screen, tr(STR_READING_QUEST), name);
    chrome::framebufferLightHeader = chrome::lightHeader;
    ready = true;
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    nameKeyboard.didPaint();
    pendingTouch.painted();
    return;
  }

  questui::Model model;
  model.profile = &profile;
  model.summary = &summary;
  model.view = view;
  model.today = today;
  model.loading = loading;
  model.failed = failed;
  model.denied = denied;
  model.category = category;
  model.page = page;
  model.preview = preview;
  model.questPage = questPage;
  model.assets = &assets;
  model.storyAssets = &storyAssets;
  model.rewardAssets = &rewardAssets;
  model.outfitAssets = &outfitAssets;
  model.enemyAssets = &enemyAssets;
  model.chosenGender = chosenGender;
  model.nameDraft = nameDraft;
  model.creating = creating;
  model.writable = writable && historyComplete;
  model.campaignTab = campaignTab;
  model.selectedBook = selectedBook;
  model.selectedEncounter = selectedEncounter;
  model.dailyMinutes = dailyMinutes;
  model.introductionPage = introductionPage;
  model.storyPageCount = storyPageCount;
  model.storyPageDraft = storyPageDraft;
  model.selectedRequest = selectedRequest;
  model.requestFilter = requestFilter;
  model.inventoryFilter = inventoryFilter;
  model.storeNotice = storeWork.notice;
  model.atlasArrival = atlasArrival;
  model.recalling = view == questui::View::Lore && infoReturn == questui::View::Recap;
  model.readingReturn = &readingReturn;
  model.returnPanel = returnPanel;
  model.celebrationAssets = &celebrationAssets;
  model.attackPhase = readingClock.phase();
  const auto draw = [&](fui::DrawTarget& surface) {
    toybox::Frame frame(surface, target.deviceContext(), input, interactions, nullptr, status);
    toybox::Screen screen(frame);
    questui::build(screen, model, work);
  };
  draw(target);
  if (!artReady()) {
    partial = false;
    target.setClipRect(fui::makeRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight()));
    renderer.clearScreen();
    draw(target);
  }
  bool grayShown = false;
#ifdef SIMULATOR
  // PET and Quest share the same stateless absolute-gray plane adapter.
  if (!partial && artReady() && !(view == questui::View::ReadingReturn && readingClock.playing()) &&
      renderer.grayscaleCapabilities(HalDisplay::GrayscaleMode::Absolute).supported() &&
      renderer.displayGrayscaleBase(HalDisplay::GrayscaleMode::Absolute)) {
    for (const auto mode : {GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
      renderer.clearScreen();
      renderer.setRenderMode(mode);
      petui::GrayTarget plane(target, mode == GfxRenderer::GRAYSCALE_MSB);
      draw(plane);
      if (!artReady()) break;
      if (mode == GfxRenderer::GRAYSCALE_LSB)
        renderer.copyGrayscaleLsbBuffers();
      else {
        renderer.copyGrayscaleMsbBuffers();
        renderer.displayGrayBuffer();
        grayShown = true;
      }
    }
    renderer.setRenderMode(GfxRenderer::BW);
    renderer.clearScreen();
    draw(target);
    if (!artReady()) {
      renderer.clearScreen();
      draw(target);
      grayShown = false;
    }
    renderer.cleanupGrayscaleWithFrameBuffer();
  }
#endif
  ready = true;
  toybox::reportOverflow(interactions, "ReadingQuest", &renderer);
  target.setClipRect(fui::makeRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight()));
  if (!grayShown) {
    if (partial)
      renderer.displayWindow(focusArea.x, focusArea.y, focusArea.width, focusArea.height);
    else if (view == questui::View::ReadingReturn)
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    else
      renderer.displayBuffer();
  }
  pendingTouch.painted();
  if (questui::isDecisionConfirmation(view)) questui::focusConfirmation(interactions, view);
  if (view == questui::View::ReadingReturn) readingClock.painted(millis(), model.attackPhase);
#ifdef __EMSCRIPTEN__
  if (!loading)
    simulatorReading.finish(failed || !historyComplete ? questsim::ReadingResult::StorageError
                                                       : questsim::ReadingResult::Done);
#endif
}

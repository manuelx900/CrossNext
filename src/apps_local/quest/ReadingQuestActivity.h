#pragma once
#include "../../activities/Activity.h"
#include "../../activities/util/EmbeddedKeyboard.h"
#include "../reading/ReadingStatsStore.h"
#include "../ui/SectionNavigation.h"
#include "QuestPendingTouch.h"
#include "QuestScreens.h"
class ReadingQuestActivity final : public Activity {
 public:
  ReadingQuestActivity(GfxRenderer& r, MappedInputManager& i) : Activity("ReadingQuest", r, i) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void runIdleWork() override;
  void prepareForSleep() override;
  void requestUpdate(bool immediate = false) override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return loading; }

 private:
  sectionnav::Input navigation;
  questinput::PendingTouch pendingTouch;
  // Optional activity-lifetime PSRAM: 512 KiB total; no internal-heap fallback.
  static constexpr size_t ART_CACHE = 96 * 1024, STORY_CACHE = 64 * 1024, MONSTER_CACHE = 160 * 1024,
                          OUTFIT_CACHE = 32 * 1024;
  sdart::Pack assets, storyAssets, rewardAssets, enemyAssets, outfitAssets, celebrationAssets;
  quest::Profile profile, draft;
  quest::StoreWork storeWork;
  readingstats::Summary summary;
  readingstats::Repository repository;
  readingstats::Record record;
  uint64_t firstDayTotal = 0;
  // Eight sequential history records; activity-owned and reused during startup.
  uint8_t historyReadAhead[8 * readingstats::kRecordBytes]{};
  questui::Work work;
  toybox::Interactions interactions;
  freeink::ui::Rect focusArea{};
  bool focusRedraw = false;
  questui::View view = questui::View::Hero, infoReturn = questui::View::Hero;
  questui::View requestListReturn = questui::View::Missions;
  questui::View rewardsReturn = questui::View::Request;
  int today = 0, category = 0, page = 0, preview = -1, questPage = 0, chosenGender = -1;
  int selectedBook = 0, dailyMinutes = 30;
  int selectedEncounter = 0;
  int introductionPage = 0;
  int storyPageCount = 1, storyPageDraft = 0;
  questui::View storyReturn = questui::View::Book;
  bool profileDirty = false;
  uint32_t profileChangedAt = 0;
  int selectedRequest = -1;
  questui::RequestFilter requestFilter = questui::RequestFilter::Available;
  questui::InventoryFilter inventoryFilter = questui::InventoryFilter::Recent;
  bool atlasArrival = false;
  questui::CampaignTab campaignTab = questui::CampaignTab::Saga;
  questreturn::Report readingReturn;
  questreturn::Clock readingClock;
  questreturn::Panel returnPanel = questreturn::Panel::Reading;
  char nameDraft[quest::NAME_BYTES]{};
  bool creating = false;
  bool loading = true, failed = false, denied = false, ready = false, writable = true, historyComplete = false;
  void action(freeink::ui::ActionId, int);
  bool commitDraft();
  void enterName();
  void showCampaign(questui::CampaignTab);
  bool artReady() const {
    return assets.ready() && storyAssets.ready() && rewardAssets.ready() && enemyAssets.ready() &&
           outfitAssets.ready() && celebrationAssets.ready();
  }
  void leaveEnding(int reward);
  void dismissReadingReport();
  void returnToMenu();
  void dismissCelebration();
  void rememberStoryPosition();
  void savePendingChanges();
  void markProfileDirty();
#ifdef __EMSCRIPTEN__
  bool simulatorReadingMode = false;
  uint64_t simulatorPendingSeconds = 0;
  bool processSimulatorReading();
  void applySimulatorPreview();
#endif
  EmbeddedKeyboard nameKeyboard;
};

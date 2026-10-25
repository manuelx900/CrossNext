#pragma once
#include "../../activities/Activity.h"
#include "../reading/ReadingStatsStore.h"
#include "../ui/SectionNavigation.h"
#include "QuestScreens.h"
class ReadingQuestActivity final : public Activity {
 public:
  ReadingQuestActivity(GfxRenderer& r, MappedInputManager& i) : Activity("ReadingQuest", r, i) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return loading; }

 private:
  sectionnav::Input navigation;
  sdart::Pack assets;
  quest::Profile profile, draft;
  quest::StoreWork storeWork;
  readingstats::Summary summary;
  readingstats::Repository repository;
  readingstats::Record record;
  // Eight sequential history records; activity-owned and reused during startup.
  uint8_t historyReadAhead[8 * readingstats::kRecordBytes]{};
  questui::Work work;
  toybox::Interactions interactions;
  int tab = 0, today = 0, category = 0, page = 0, preview = -1, questPage = 0;
  bool ownedGear = false, levelSaveFailed = false;
  bool loading = true, failed = false, denied = false, ready = false, writable = true;
  void action(freeink::ui::ActionId, int);
};

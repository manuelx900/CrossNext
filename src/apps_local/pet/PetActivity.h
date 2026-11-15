#pragma once
#include <atomic>

#include "../../activities/Activity.h"
#include "../../activities/util/EmbeddedKeyboard.h"
#include "../reading/ReadingStatsStore.h"
#include "../ui/SdBitmapPack.h"
#include "../ui/SectionNavigation.h"
#include "PetPendingTap.h"
#include "PetReadingTimeline.h"
#include "PetScreens.h"
#include "PetSleepAnimation.h"
#include "PetStore.h"

class PetActivity final : public Activity {
 public:
  PetActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("Pet", renderer, input) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return loading; }
  bool allowsEdgeBack() const override {
    return view != pet::View::Projects && (view != pet::View::Play || game.kind != pet::Game::Puzzle);
  }

 private:
  // Reused source sprites in PSRAM, not a display buffer; released with each pack on exit.
  static constexpr size_t ART_PIXEL_CACHE_BYTES = 64 * 1024;
  // All portrait or landscape source tiles fit together; this is packed artwork, not a framebuffer.
  static constexpr size_t ROOM_PIXEL_CACHE_BYTES = 96 * 1024;
  static constexpr size_t COLLECTION_PIXEL_CACHE_BYTES = 16 * 1024;
  pet::Profile profile, draft;
  pet::GameState game;
  pet::Store store;
  pet::History history, historyDraft;
  pet::HistoryStore historyStore;
  petui::Work work;
  petui::Model model;
  pet::Affection affection;
  sdart::Pack art, homeArt, habitatArt, sleepArt, farewellArt, collectionArt;
  petsleep::Clock sleepClock;
  petfeed::Clock feedingClock;
  petcurious::Clock curiosityClock;
  petreact::Clock reactionClock;
  petcompanion::Clock companionClock;
  petcompanion::Kind nextTouchPose = petcompanion::Kind::Sit;
  petrest::Clock restClock;
  petrest::Kind restPending = petrest::Kind::None;
  bool restArtAvailable = false, discoveryPending = false;
  bool curiosityAvailable = false, curiosityOnEntry = true;
  petreact::Art reactionArt = petreact::Art::None;
  uint8_t selectedFood = 0;
  bool choosingFood = false;
  petfeed::Art feedingArt = petfeed::Art::Legacy;
  bool animationRebasePending = true;
  petinput::PendingTap pendingTap;
  petinput::PendingSlide pendingSlide;
  uint32_t viewRevision = 1;
  std::atomic<uint32_t> paintedRevision{0};
  readingstats::Repository readings;
  readingstats::Summary summary;
  readingstats::Record record;
  pet::ReadingTimeline readingTimeline;
  uint8_t readAhead[readingstats::kRecordBytes * 8]{};
  sectionnav::Input navigation;
  toybox::Interactions interactions;
  pet::View view = pet::View::New, returnView = pet::View::New, noticeReturn = pet::View::Dashboard;
  pet::View affectionReturn = pet::View::Care;
  pet::View memoryReturn = pet::View::Memories, lifeReturn = pet::View::Lives;
  pet::Tab tab = pet::Tab::Pet;
  pet::Mode selectedMode = pet::Mode::Relaxed;
  pet::Species chosenSpecies = pet::Species::Silva;
  bool historyWritable = true;
  bool farewellArtTried = false, collectionArtTried = false;
  bool contentsAvailable = false;
  bool pettingHandAvailable = false;
  bool portraitBackgroundsAvailable = false;
  bool fullRoomBackgroundsAvailable = false;
  uint32_t careCheckedAt = 0;
  pet::Game selectedGame = pet::Game::Pairs;
  petui::CatalogFilter catalogFilter = petui::CatalogFilter::Unowned;
  petui::ReadingDisplay readingDisplay = petui::ReadingDisplay::Project;
  uint8_t previewItem = pet::OBJECT_COUNT;
  petui::FeedResult feedResult;
  petui::CareResult careResult;
  petui::GameResult gameResult;
  int64_t lastReadingAt = 0, scanNow = 0;
  bool scanChanged = false;
  int today = 0, todaySeconds = 0, weekDays = 0, page = 0, memory = 0, projectSlot = 0;
  bool loading = true, ready = false, writable = true, loaded = true, historyChanged = false, reconciled = false;
  const char* message = nullptr;
  petui::NoticeKind noticeKind = petui::NoticeKind::Information;
  char nameDraft[pet::NAME_BYTES]{};
  void createCompanion();
  void scan();
  void finishScan(bool failed);
  bool updateHabits();
  bool refreshDay();
  bool archiveDraft();
  bool updateCare();
  void action(freeink::ui::ActionId, int);
  bool wakeBeforeCare(freeink::ui::ActionId);
  void swipeCatalog(int dx, int dy);
  void swipePages(int dx, int dy);
  void beginRestTransition();
  void show(pet::View);
  void enterName();
  EmbeddedKeyboard nameKeyboard;
  void back();
  void changeTab(int);
  int nextTab(int) const;
  bool commit();
  void notice(const char*, pet::View, petui::NoticeKind = petui::NoticeKind::Information, petui::CareResult = {});
  void acknowledge(bool collectProject = false);
  void react(petreact::Kind);
  bool openResources();
  bool resourcesReady() const;
  void syncFarewellArt();
  void syncCollectionArt();
  void acknowledgeDeath(bool openHistory);
  void saveSettings(bool acceptExpiry);
  static bool drawArt(void*, freeink::ui::DrawTarget&, freeink::ui::Rect, uint32_t, bool);
  static bool drawHomeArt(void*, freeink::ui::DrawTarget&, freeink::ui::Rect, uint32_t, bool);
  static bool drawPetArt(void*, freeink::ui::DrawTarget&, freeink::ui::Rect, uint32_t, bool);
};

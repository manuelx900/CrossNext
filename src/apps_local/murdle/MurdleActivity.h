#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "MurdleCampaign.h"
#include "MurdleCore.h"
#include "MurdleScreens.h"

class MurdleActivity final : public Activity {
 public:
  MurdleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Murdle", renderer, mappedInput) {}
  ~MurdleActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  void prepareForSleep() override { flushSave(); }

 private:
  enum class View : uint8_t {
    Menu,
    Cases,
    Case,
    Accuse,
    Verdict,
    Settings,
    HowTo,
    ConfirmNew,
    ConfirmReset,
  };

  void goToMenu();
  void openCase();
  // The one door to a new case. Asks when an unsolved one is open, generates
  // otherwise. Nothing else may call generateCase().
  void requestNewCase();
  void generateCase();
  void resetCaseProgress();
  void resetAllProgress();
  void routeAction(int action, int value);
  void handleGridTap(int x, int y);
  void submitAccusation();

  bool saveState();
  bool loadState();
  bool flushSave();
  void recordVerdict(int outcome);

  murdle::Puzzle puzzle;
  murdle::Marks marks;
  // ~4.7KB of working room for the generator, held only while a case is being
  // made. Too big for the task stack, and not worth keeping in DRAM between
  // cases on a device with 380KB of it.
  std::unique_ptr<murdle::Scratch> scratch;

  // What a tap on the play surface means. See Activity::surfaceMeaning().
  uint32_t surfaceMeaning() const override;

  View view = View::Menu;
  murdleui::Face face = murdleui::Face::Clues;
  murdleui::GridLayout gridLayout;

  bool hasCase = false;
  bool solved = false;
  murdle::Tier tier = murdle::Tier::Elementary;
  uint32_t seed = 0;
  murdlecampaign::Progress campaign;
  murdle::Tier campaignTier = murdle::Tier::Elementary;
  int campaignCase = -1, pendingCampaign = -1, campaignPage = 0, campaignSelected = 0;
  int caseNumber = 0;
  int page = 0;
  uint32_t struck = 0;
  int wrongAccusations = 0;
  int gridPage = 0, infoCategory = 0;
  bool showSolution = false, undoable = false;
  murdleui::GridCell undoCell, lastCell;
  murdle::Mark undoMark = murdle::Mark::Unknown;
  murdle::Marks solutionMarks;
  // What the grid last refused to do, and why. Not saved: it is about the tap
  // just made, so it goes the moment anything else happens.
  char notice[96] = {};

  int solvedCount = 0;
  int wrongCount = 0;
  uint32_t record = 0;

  uint8_t picks[murdle::kMaxCats] = {};
  int howToPage = 0;

  // Generation takes long enough to be worth a frame of its own, so it is
  // deferred by one loop pass: the "DEDUCING" paint lands before the work
  // starts rather than after it.
  bool generatePending = false;

  bool dirty = false;
  bool saveFailed = false, saveNotice = false, saveWritable = true;
  unsigned long lastSaveAttempt = 0;
  bool flashOnNextPaint = false;
  bool interactionsReady = false;
  toybox::Interactions interactions;
};

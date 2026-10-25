#pragma once
#include "../../activities/Activity.h"
#include "DraughtsScreens.h"
class DraughtsActivity final : public Activity {
 public:
  DraughtsActivity(GfxRenderer& r, MappedInputManager& i) : Activity("Checkers", r, i) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void begin();
  void commitSettings();
  void save();
  void action(freeink::ui::ActionId, int);
  uint32_t surfaceMeaning() const override;
  // One fallible Activity allocation owns ~7 KiB of search and repetition state;
  // explicit DFS frames avoid recursive copies on the firmware task stack.
  draughts::Game game;
  draughts::Generator legal;
  draughtsui::View view = draughtsui::View::Menu;
  draughtsui::View newGameReturnView = draughtsui::View::Board;
  draughts::Rules rules = draughts::Rules::International;
  draughts::Rules pendingRules = draughts::Rules::International;
  int pendingDifficulty = 1;
  bool pendingLocal = false, pendingHumanBlack = false;
  bool humanBlack = false, gameHumanBlack = false;
  draughtsui::Outcome result = draughtsui::Outcome::Playing;
  toybox::Interactions interactions;
  uint32_t random = 1;
  int helpPage = 0;
  int difficulty = 1, selected = -1;
  int gameDifficulty = 1;
  bool local = false, gameLocal = false, active = false, ready = false, dirty = false, saveFailed = false,
       readOnly = false;
};

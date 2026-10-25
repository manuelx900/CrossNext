#pragma once
#include "../../activities/Activity.h"
#include "Game2048Screens.h"
#include "Game2048Store.h"
class Game2048Activity final : public Activity {
 public:
  Game2048Activity(GfxRenderer& renderer, MappedInputManager& input) : Activity("2048", renderer, input) {}
  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& input);
  bool allowsEdgeBack() const override { return view != game2048ui::View::Board; }
  void onEnter() override;
  void onExit() override;
  void prepareForSleep() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void save();
  void action(freeink::ui::ActionId action, int value);
  bool routeBoardSwipe();
  game2048::State state;
  game2048::Repository repository;
  uint64_t best = 0;
  game2048ui::View view = game2048ui::View::Menu;
  game2048ui::Work uiWork;
  toybox::Interactions interactions;
  unsigned long lastSave = 0;
  int selected = 0;
  bool dirty = false, saveFailed = false, ready = false;
};

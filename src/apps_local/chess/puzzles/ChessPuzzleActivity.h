#pragma once

#include "../../../activities/Activity.h"
#include "ChessPuzzleScreens.h"
#include "ChessPuzzleStore.h"

class ChessPuzzleActivity final : public Activity {
 public:
  ChessPuzzleActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("ChessPuzzles", renderer, input) {}
  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& input);
  void onEnter() override;
  void onExit() override;
  void prepareForSleep() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Bounded chess workspace and UI descriptors live with the activity, never on the render stack.
  chesspuzzle::Session session;
  // Search reuses five fixed move lists; too large for the loop task's stack.
  chess::SearchBuffers searchBuffers;
  chesspuzzle::Progress progress;
  chesspuzzle::Store store;
  chesspuzzleui::Work work;
  toybox::Interactions interactions;
  chesspuzzleui::View view = chesspuzzleui::View::Difficulty;
  int group = 0, index = 0, page = 0;
  chessstyle::Design pieceDesign = chessstyle::kDefault;
  bool ready = false, dirty = false, saveFailed = false;
  bool invalidNumber = false;
  int promotionFrom = -1, promotionTo = -1;
  unsigned long lastSave = 0;
  void save();
  void open(int next);
  void choose();
  void completeMove();
  void action(freeink::ui::ActionId action, int value);
  uint32_t surfaceMeaning() const override;
};

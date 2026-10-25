#pragma once
#include "../../activities/Activity.h"
#include "PocketGamesScreens.h"
#include "PocketGamesStore.h"

class PocketGamesActivity final : public Activity {
 public:
  PocketGamesActivity(pocketgames::Kind kind, GfxRenderer& renderer, MappedInputManager& input)
      : Activity(pocketui::name(kind), renderer, input), kind(kind) {}
  static std::unique_ptr<Activity> createOthello(GfxRenderer&, MappedInputManager&);
  bool allowsEdgeBack() const override { return true; }
  void onEnter() override;
  void onExit() override;
  void prepareForSleep() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  const pocketgames::Kind kind;
  // One bounded state, store/readback workspace and UI descriptor set per activity.
  pocketgames::State state;
  pocketgames::Store store;
  toybox::Interactions interactions;
  pocketui::View view = pocketui::View::Menu;
  unsigned long lastTick = 0, lastSave = 0;
  uint32_t revision = 0;
  int selected = -1, level = 0, defaultLevel = 0, guidePage = 0;
  int humanColour = 1, preferredColour = 1, savedColour = 1, savedLevel = 1;
  void saveOthelloPreferences();
  bool dirty = false, ready = false, saveFailed = false, invalid = false, restart = false;
  enum class BoardInput : uint8_t { AwaitFreshFrame, AwaitIdle, Armed };
  BoardInput othelloInput = BoardInput::AwaitFreshFrame;
  enum class BotMovePhase : uint8_t { Idle, AwaitPaint, Showing };
  BotMovePhase botMovePhase = BotMovePhase::Idle;
  int botSquare = -1;
  uint32_t botPreviewAt = 0;
  static std::unique_ptr<Activity> create(pocketgames::Kind, GfxRenderer&, MappedInputManager&);
  void action(freeink::ui::ActionId, int = 0);
  void save();
  void changed();
  void cancelBotMove();
  uint32_t surfaceMeaning() const override;
};

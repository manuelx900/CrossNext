#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "GoCore.h"
#include "GoFlow.h"

class GoActivity final : public Activity {
 public:
  GoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Go", renderer, mappedInput) {}
  ~GoActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;

  void loop() override;
  void render(RenderLock&&) override;

 private:
  void beginSoloGame();
  void takeComputerTurn();
  // The search, run where it has room. See the long note in GoActivity.cpp:
  // michi's deepest path does not fit the loop task's stack, so the move is
  // computed on a task of this app's own and the loop waits for it.
  int chooseComputerMove(const go::Game& snapshot);
#if defined(ARDUINO_ARCH_ESP32)
  void searchLoop();
  static void searchTrampoline(void* self);
#endif
  void goTo(go::Screen next);
  void clearAim();
  void handlePointActivated(int point);
  void toggleDeadAt(int point);
  void enterCounting();
  // The machine declined the count, so the board settles it.
  void resumeFromDisagreement();
  // Recomputes owner/blackHalves/whiteHalves from the position as it stands.
  // Every door into the counting screen has to pass through this: a resumed
  // game reached it with the three of them still zero, so the board showed no
  // territory at all and the header read B+0.0.
  void refreshCount();
  void finishCounting();
  void recordResult();
  void loadSave();
  void writeSave();
  bool myMove() const;
  bool computerToMove() const;

  uint32_t surfaceMeaning() const override;

  go::Screen screen = go::Screen::Menu;
  go::Game game{};
  int menuSelected = -1;
  int settingsSelected = -1;
  int helpPage = 0;

  go::Opponent opponent = go::Opponent::Computer;
  go::Level level = go::Level::Medium;
  uint8_t playAs = go::kBlack;
  // Stones the player is spotted, 0 or 2..kMaxHandicap. Its OWN setting, not a
  // property of the level: a level that silently spotted stones made "easy"
  // mean two different things at once.
  int handicap = 0;
  // The board the NEXT new game is played on. Not `game.size`, which is the
  // board the game in progress is already on: changing the setting under a
  // live game would have to reinterpret its stones.
  int boardSize = go::kSmallSize;

  uint8_t seat = go::kBlack;

  // The point a finger has chosen but not committed. The one piece of state
  // between two taps, and the reason a misplaced stone is recoverable.
  int aimed = go::kNothingAimed;
  go::Caution caution = go::Caution::None;

  // The machine refused the count and play resumed. Shown on the board until
  // the next stone goes down, because a game that reappears with no explanation
  // reads as a bug.
  bool disagreed = false;
  // You passed and the machine answered with a stone. Tracked because the board
  // cannot tell: `game.passes` is back to zero by the time it is drawn, so a
  // pass that was answered and a pass that was never made look identical.
  bool passWasPlayedThrough = false;

  // The computer's move is started one loop pass AFTER the repaint that shows
  // the human's, so the panel says THINKING before the search begins rather
  // than after it. Chess learned this the expensive way: requestUpdate() only
  // notifies the render task, so deferring by "one pass" without a flag runs
  // the search while the repaint is still in flight.
  bool thinking = false;

  // Whose area each point is, recomputed only when the count changes rather
  // than every paint: the render task must not do a flood fill of the board on
  // every frame.
  uint8_t owner[go::kMaxPoints] = {};
  int blackHalves = 0;
  int whiteHalves = 0;

  uint32_t seed = 0x9E3779B9u;

  bool hasHistory = false;
  uint8_t lastPoints[go::kMaxPoints] = {};
  uint8_t lastSize = go::kSmallSize;
  bool lastWon = false;
  int lastMarginHalves = 0;
  int wins = 0;
  int losses = 0;
  bool resultRecorded = false;
  // A game that is part-played and can be resumed from the front door.
  bool inProgress = false;

#if defined(ARDUINO_ARCH_ESP32)
  // Created in onEnter and ended in onExit, so the 28KB it costs is only spent
  // while this app is open. Null when the task could not be started, which
  // falls back to searching on the loop task.
  void* searchTask = nullptr;
  // Whether the search task's stack came from PSRAM (internal RAM had no
  // contiguous 32KB). Decides how onExit deletes it.
  bool searchStackExternal = false;
  void* searchWaiter = nullptr;
  go::Game searchBoard{};
  go::Level searchLevel = go::Level::Medium;
  int searchResult = go::kPass;
  volatile bool searchEnding = false;
#endif

  toybox::Interactions interactions;
  bool interactionsReady = false;
};

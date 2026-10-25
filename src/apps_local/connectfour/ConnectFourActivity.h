#include <I18n.h>
#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "ConnectFourCore.h"
#include "ConnectFourFlow.h"

class ConnectFourActivity final : public Activity {
 public:
  ConnectFourActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ConnectFour", renderer, mappedInput) {}
  ~ConnectFourActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

  void loop() override;
  void render(RenderLock&&) override;

 private:
  uint32_t surfaceMeaning() const override;
  void beginSoloGame();
  void takeOpponentTurn();
  void goTo(connectfour::Screen next);
  // The last finished game and the running tally, for the menu's ornament.
  // Written when a game ends, read once on entry -- knucklebones' pattern.
  void recordResult();
  void loadHistory();

  connectfour::Screen screen = connectfour::Screen::Menu;
  connectfour::Game game{};
  int howToPage = 0;
  int menuSelected = -1;

  // What the menu draws instead of being three rows and white space. Kept here
  // rather than in the core: the rules do not have a history, a device does.
  bool hasHistory = false;
  // 0 won, 1 lost, 2 draw, from this device's seat.
  int lastOutcome = 2;
  // Column-major, row 0 at the bottom, always from this device's perspective:
  // kLight is always the seat this device played.
  uint8_t lastCells[connectfour::kCells] = {};
  // Flat indices (column * kRows + row) of the four that ended it, -1s on a
  // draw.
  int lastLine[connectfour::kLine] = {-1, -1, -1, -1};
  int wins = 0;
  int losses = 0;
  int draws = 0;
  int streak = 0;
  int selectedColumn = 3;
  bool resultRecorded = false;
  uint8_t seat = connectfour::kLight, preferredSeat = connectfour::kLight;
  bool local = false, preferredLocal = false, savedLocal = false;
  uint8_t savedSeat = connectfour::kLight;
  uint8_t preferredLevel = 1, savedLevel = 1;
  void savePreferences();

  toybox::Interactions interactions;
  bool interactionsReady = false;
};

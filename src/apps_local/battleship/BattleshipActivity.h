#include <I18n.h>
#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../../components/UITheme.h"  // Rect
#include "../player/PlayerName.h"
#include "../ui/ToyboxScreen.h"
#include "BattleshipCore.h"
#include "BattleshipScreens.h"

// Battleship. Touch only, in the fork's usual three layers: BattleshipCore has
// the rules and the gunner, BattleshipScreens owns rendering, and this activity owns
// input, navigation and persistence.
//
// Two grids, one screen. The big one is their water and it is the only thing
// you can tap; the small one is your own fleet and it only ever reports. That
// asymmetry is the whole interface: there is nothing to do on your own board
// once the game starts, so it stops being a control and becomes a readout.
class BattleshipActivity final : public Activity {
 public:
  BattleshipActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Battleship", renderer, mappedInput) {}
  ~BattleshipActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;

  void loop() override;
  void render(RenderLock&&) override;

 private:
  toybox::Interactions interactions;
  bool interactionsReady = false;

  // Which screen owns the pass. Menu is the front door, Place is setting up,
  // Board is the game.
  enum class View : uint8_t { Menu, Place, Board, Help, Result };
  int helpPage = 0;

  // Where a grid landed. Derived once and used by both painting and hit
  // testing, so the drawn cells and the tappable cells cannot drift apart --
  // the rule that has caught more bugs in this fork than any other.
  struct GridGeometry {
    int originX = 0;
    int originY = 0;
    int cell = 0;
  };

  // Their water: the big grid, and the only surface that takes a tap.
  GridGeometry targetGrid() const;
  GridGeometry placeGrid() const;
  static int cellAt(const GridGeometry& grid, int x, int y);
  bshipui::Layout layout{};

  bshipui::StartModel startModel() const;
  bshipui::PlaceModel placeModel() const;
  bshipui::BoardModel boardModel();
  void drawStartMenu();
  void drawPlaceScreen();
  void drawBoardScreen();

  void goToMenu();
  void activateStartRow(bshipui::StartRow row);
  void routeStartMenu();
  void routePlacement();
  void routeBoard();

  // Setting up.
  void beginPlacement();
  void shuffleFleet();
  void selectShip(int shipIndex);
  void rotateSelected();
  void moveSelected(int cell);
  void handlePlaceTap(int cell);
  void commitFleet();
  void enterBoardView();

  // Playing.
  void handleTargetTap(int cell);
  // The single activation path for firing: the capsule and a second tap on the
  // aimed cell both land here. Two paths drift, and on a device with two ways
  // to do everything the drift is invisible until somebody uses the one that
  // was not tested.
  void fireAtAimed();
  void playComputerShot();
  void finishGame();
  void startNewGame();

  int opponentSide() const { return mySide ^ 1; }
  bool myTurn() const { return (game.turn & 1) == mySide; }
  bool gameOver() const { return bship::over(game); }

  // The single door for "the player asked for a new game": solo it starts one,
  // in a match it asks the other device. A new door cannot pick the wrong
  // branch because there is no branch left at the call site.
  void requestNewGame();

  void reportShot(bool theirs);
  // Fills `capsule` and returns it. The label is derived at paint time from the
  // state rather than written at each transition, because there are five doors
  // into this screen and only one of them would have remembered.
  const char* capsuleLabel();
  // Not const: it fills `them` the way capsuleLabel() fills `capsule`, so the
  // returned pointer stays valid past the call.
  const char* themWord();

  void saveGame() const;
  bool loadGame();
  void saveStats() const;
  void loadStats();

  bship::Game game;
  // Your fleet while you are still arranging it, before it is handed to the
  // game. It has to live outside `game`: adopting a state from the other device
  // overwrites the whole struct, so a fleet parked in there would be wiped by
  // the opponent's first packet. Committing it is what makes it real.
  bship::Fleet myFleet;
  uint8_t mySide = 0;
  uint32_t seed = 1;

  // What a tap on the play surface means. See Activity::surfaceMeaning().
  uint32_t surfaceMeaning() const override;

  View view = View::Menu;
  int startIndex = 0;
  bool hasSavedGame = false;

  // Placement.
  int selectedShip = -1;
  bool fleetCommitted = false;

  // Playing.
  int aimCell = -1;
  // The computer owes a shot. Deferred by one loop pass so the repaint showing
  // your own shot lands before it thinks, exactly as the chess engine is.
  bool computerThinking = false;
  bool showOwnFleet = false;
  // What just happened, in the player's words. One line, and it is the only
  // narration this game has. It doubles as the instruction line while you are
  // setting up, because both answer the same question: what now.
  char report[96] = {};
  char capsule[24] = {};
  // What to call the opponent inside a sentence: their first word. Held rather
  // than returned by value so themWord() can hand back a pointer.
  char them[player::kMaxShortNameLength + 1] = {};

  int played = 0;
  int won = 0;
  int streak = 0;
};

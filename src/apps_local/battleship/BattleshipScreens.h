#pragma once

// The Battleship screens, as freestanding builders over a model.
//
// No GfxRenderer, Activity or storage: builders draw the chrome and grids from
// read-only models, then publish the grid rectangles used by activity input.

#include "../ui/ToyboxScreen.h"
#include "BattleshipCore.h"

namespace bshipui {

namespace fui = freeink::ui;

// Semantic actions. Every input path resolves to one of these, so a control
// cannot behave differently depending on which input reached it. The 200s
// belong to the shared link screens; these stay below them.
enum : fui::ActionId {
  ActionStartRow = 1,
  ActionFire = 2,
  ActionShuffle = 3,
  ActionReady = 4,
  ActionPlayAgain = 5,
  ActionHelpNext = 6,
  ActionRotate,
  ActionSelectShip,
};

// The local game opens at its saved-board preview.
enum class StartRow : uint8_t { Continue, NewGame, HowToPlay, Count };

struct StartModel {
  // No game to continue means no CONTINUE row, rather than one that does
  // nothing.
  bool hasSavedGame = false;
  int played = 0;
  int won = 0;
  int streak = 0;
  int selected = 0;
  const bship::Fleet* fleet = nullptr;
};

int startRows(const StartModel& model);
StartRow startRowAt(const StartModel& model, int visibleIndex);
const char* startRowLabel(StartRow row);

// Saved fleet preview, statistics and menu actions.
fui::Rect buildStartMenu(toybox::Screen& screen, const StartModel& model);
void buildHowTo(toybox::Screen&, int page);

// Fleet placement with ship selection, rotation, shuffle and readiness controls.
struct PlaceModel {
  const char* status = "";
  // False while waiting for the other device, when READY has already been sent
  // and there is nothing left to press.
  bool canEdit = true;
  const bship::Fleet* fleet = nullptr;
  int selectedShip = -1;
};

struct Layout {
  fui::Rect target{}, place{}, own{}, ships[bship::kShipCount]{};
};
fui::Rect buildPlaceChrome(toybox::Screen& screen, const PlaceModel& model, Layout* layout = nullptr);

// Playing. The status capsule is the trigger: it says FIRE when a cell is
// aimed, reports what happened when it is not, and becomes PLAY AGAIN at the
// end. One control, three jobs, and it is the largest thing on the screen that
// is not a grid.
struct BoardModel {
  // Public feedback for the last shot, kept separate from the fire action.
  const char* report = "";
  const char* status = "";
  // A cell is aimed and it is your turn, so the capsule is armed.
  bool canFire = false;
  bool gameOver = false;
  const bship::Game* game = nullptr;
  int mySide = 0, aim = -1;
  bool myTurn = true;
};

fui::Rect buildBoardChrome(toybox::Screen& screen, const BoardModel& model, Layout* layout = nullptr);

}  // namespace bshipui

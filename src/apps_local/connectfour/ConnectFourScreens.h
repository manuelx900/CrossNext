#pragma once

// Connect Four on screen. Freestanding builders over plain models.

#include "../ui/ToyboxScreen.h"
#include "ConnectFourCore.h"
#include "ConnectFourFlow.h"

namespace c4ui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenuRow = 1,
  ActionHowToNext = 2,
  ActionAgain = 3,
  ActionDone = 4,
  ActionHowToPrevious = 5,
  ActionViewBoard = 6,
  ActionReturnResult = 7,
};

enum class MenuRow : int { Play = 0, HowTo, Settings, Count };

struct MenuModel {
  int selected = -1;
  // The record and the last game's final slab, for the front door's ornament.
  // Cells are column-major bottom-up with light normalised to this device's
  // seat; line holds the four flat indices that ended it, -1s on a draw.
  bool hasHistory = false;
  // 0 won, 1 lost, 2 draw, from this device's seat.
  int lastOutcome = 2;
  const uint8_t* lastCells = nullptr;
  const int* lastLine = nullptr;
  int wins = 0;
  int losses = 0;
  int draws = 0;
  int streak = 0;
};

struct HowToModel {
  int page = 0;
};

struct BoardModel {
  connectfour::Game game{};
  // Which columns will take a disc, from the rules rather than recomputed here.
  uint8_t open = 0;
  // Which colour this device plays. Light in a solo game; the coin toss
  // decides it in a match.
  uint8_t seat = connectfour::kLight;
  bool yourTurn = true;
  int selectedColumn = 3;
  bool review = false;
  bool local = false;
};

struct ResultModel {
  // The final board. The result screen shows the position, not a score:
  // Connect Four has no score, and the question at the end is "where was it".
  connectfour::Game game{};
  bool local = false;
  connectfour::Outcome outcome = connectfour::Outcome::Draw;
  uint8_t seat = connectfour::kLight;
  int wins = 0;
  int streak = 0;
  // The four cells that ended it, as flat indices, or -1 when it was a draw.
  // Taken from the rules' own search, so the marked cells ARE the winning line
  // rather than a second opinion about where it was.
  int line[connectfour::kLine] = {-1, -1, -1, -1};
};

// The drawing and arithmetic hit test share these bounds. Row zero is the
// bottom row; numbered buttons and the board both accept column drops.
fui::Rect cellRect(const fui::DeviceContext& device, int column, int row);
fui::Rect slotRect(const fui::DeviceContext& device, int column);
int columnAt(const fui::DeviceContext& device, int x, int y);

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildHowTo(toybox::Screen& screen, const HowToModel& model);
void buildBoard(toybox::Screen& screen, const BoardModel& model);
void buildResult(toybox::Screen& screen, const ResultModel& model);

int howToPages();

}  // namespace c4ui

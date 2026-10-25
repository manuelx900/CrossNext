#include <I18n.h>
#pragma once

// Wordle screens are free functions over plain models for host rendering.

#include <Icon.h>

#include "../ui/ToyboxScreen.h"
#include "../ui/WordleKeyboard.h"
#include "LocalWordle.h"
#include "WordleCore.h"

namespace wordleui {

namespace fui = freeink::ui;
using wordle::Mark;

// Above connectionsui's ids, which the reused calendar registers.
enum : fui::ActionId {
  ActionKeyboard = 101,  // one region for every key; keyAt() says which
  ActionMenu = 103,      // value: 0 play/resume, 1 how to play, 2 word length
  ActionArchive = 104,   // return to the game menu
  ActionLength = 106,
  ActionConfirm = 107,
  ActionApplyLength = 108,
  ActionNext = 105,  // a finished game: the newest day not yet finished
};

using KeyboardLayout = wordlekeys::Layout;
using wordlekeys::keyAt;
using wordlekeys::keyboardLayout;
inline constexpr char kEnter = wordlekeys::kEnter, kErase = wordlekeys::kErase;

struct GameModel {
  const wordle::Game* game = nullptr;
  const char* date = nullptr;     // chosen word length above the grid
  const char* message = nullptr;  // one line under the header, or null
  bool canNext = false;           // a finished game: is there a day left to play
};

// Returns the keyboard it drew, for keyAt(); an empty one once the game is
// over, when the keyboard gives way to the result and Menu / Play Again.
struct Work {
  char label[128]{};
};
KeyboardLayout buildGame(toybox::Screen& screen, const GameModel& model, Work& work);
inline KeyboardLayout buildGame(toybox::Screen& screen, const GameModel& model) {
  Work work;
  return buildGame(screen, model, work);
}

struct MenuModel {
  const localwordle::Record* record = nullptr;
  int letters = 5;
  bool resume = false;
  const char* date = nullptr;  // "28 SEP 2026", or null before the first download
  const char* state = "";
  wordle::Stats stats;
  int puzzles = 0;
  bool upToDate = false;
};

void buildMenu(toybox::Screen& screen, const MenuModel& model, Work& work);
inline void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  Work work;
  buildMenu(screen, model, work);
}
void buildLengths(toybox::Screen& screen, int selected, Work& work);
void buildConfirm(toybox::Screen& screen, Work& work);
void buildHowTo(toybox::Screen& screen);

}  // namespace wordleui

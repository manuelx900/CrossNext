#pragma once

// Picross on screen. Freestanding builders over plain models: these know
// FreeInkUI and Toybox tokens and nothing else, so host-tests/ui can build
// every screen against a fake draw target and ask what it drew and what it made
// tappable. See ToyboxScreen.h.

#include "../ui/ToyboxScreen.h"
#include "PicrossCore.h"

namespace picrossui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  // The whole grid is one target: a 10x10 is a hundred cells against a
  // twenty-four slot interaction buffer, so it is hit-tested arithmetically
  // through Layout below, from the same geometry that drew it -- the rule this
  // fork already keeps (Minesweeper does the same).
  ActionBoard = 1,
  ActionButton = 2,
  ActionPick = 3,  // value is a puzzle index, or -1 to resolve through the picker
  ActionMode = 4,  // value is Mode: which input mode a tap selects
  ActionPage = 5,  // value is a page index the picker should jump to
  ActionApplySize = 7,
  ActionHelpPage = 8,
  ActionTab = 6,  // value is a size-group index the picker should switch to
};

enum Button : int {
  ButtonPlay = 0,
  ButtonRestart = 1,
  ButtonPuzzles = 2,
  ButtonNext = 3,
  ButtonSizes,
  ButtonHelp,
  ButtonHome,
};

// Physical-button tool. FILL commits (a wrong fill locks as a mistake); MARK
// annotates freely. Side keys and the capsule select it. Touch always uses
// a short tap for FILL and a hold for MARK.
enum Mode : int { ModeFill = 0, ModeMark = 1 };

// Where the board grid was drawn. Filled by the builder as it lays out the
// cells, read by the activity to turn a tap into a cell -- hit-testing sharing
// the geometry that placed the pixels.
struct Layout {
  fui::Rect board = {};  // the NxN cell grid, excluding the clue gutters
  int16_t cell = 0;
  int16_t size = 0;

  // The cell under logical (x, y), or false if the tap missed the grid.
  bool cellAt(int x, int y, int& row, int& col) const;
};

struct BoardModel {
  const picross::Board* board = nullptr;
  int mode = ModeFill;  // which input mode is active, drawn on the capsule
  int solvedCount = 0;
  int total = 0;
};

// Numbered picker, with completion and in-progress marks.
struct MenuModel {
  const picross::Progress* progress = nullptr;
  // The puzzle PLAY would open: the one you last tapped, or the next unsolved.
  int selectedIndex = 0;
  // The in-progress puzzle (touched, not solved), or -1. Drawn with a resume
  // affordance so RESUME is discoverable from the grid.
  int inProgressIndex = -1;
  int solvedCount = 0;
  int total = 0;
  // RESUME rather than PLAY on the selected tile, when it is the in-progress one.
  bool hasProgress = false;
  // Which page of the picker to draw, and which size group is on screen. The
  // activity owns both and the picker clamps both, reporting back what it
  // actually drew in PickerLayout.
  int page = 0;
  int sizeTab = 0;
  // Ignore `page` AND `sizeTab` and open on the tab and page holding
  // `selectedIndex`. Set when the picker appears, so the highlighted tile is on
  // screen without the activity having to know how many tiles a page holds or
  // where one size ends and the next begins. Both numbers are derived from the
  // panel and the bank inside buildMenu, and a second copy of either elsewhere
  // is a copy that goes stale the next time the layout or the bank moves --
  // which is exactly what happened to the literal 16 this replaced.
  bool followSelection = false;
};

// Where the picker drew its tile grid. Same discipline as the board's Layout:
// filled while drawing, read to resolve a tap.
struct PickerLayout {
  fui::Rect grid = {};
  int16_t cell = 0;
  int16_t gap = 0;
  int16_t cols = 0;
  int16_t rows = 0;
  int16_t count = 0;         // tiles actually drawn on this page
  int16_t firstIndex = 0;    // global puzzle index of the first tile on the page
  int16_t rowHeight = 0;     // list layouts: the per-row pitch (0 for grids)
  int16_t pageCount = 1;     // total pages IN THE GROUP on screen
  int16_t pageOnScreen = 0;  // the page this layout drew
  int16_t tabOnScreen = 0;   // the size group this layout drew
  int16_t tabCount = 0;      // size groups the picker offered, one tab each
  // The puzzle index under logical (x, y), or -1.
  int indexAt(int x, int y) const;
};

struct WinModel {
  const picross::Puzzle* cleared = nullptr;
  int mistakes = 0;
  int solvedCount = 0;
  int total = 0;
  bool moreToPlay = true;
};

// The board. Fills `layout` as it draws.
void buildBoard(toybox::Screen& screen, const BoardModel& model, Layout& layout);

// Main menu and numbered picker. The picker fills its tap geometry.
void buildMenu(toybox::Screen& screen, const MenuModel& model, PickerLayout& layout);
void buildPicker(toybox::Screen& screen, const MenuModel& model, PickerLayout& layout);
void buildSizes(toybox::Screen& screen, const MenuModel& model);
void buildHelp(toybox::Screen& screen, int page = 0);
int groupFor(int index);
int groupFirst(int group);

// The payoff: the finished picture, revealed clean and named, graded by
// mistakes. A puzzle nobody has named yet is revealed without a name band.
void buildWin(toybox::Screen& screen, const WinModel& model);

// One page step, clamped to [0, pageCount). `delta` is -1 or +1.
//
// A free function, and the ONLY reason it is one is that it can be tested. The
// two physical side keys page the picker, and the simulator never runs
// InputManager and never sees a button at all -- so nothing about the key
// press itself is provable off-device. What IS provable is the decision the
// press feeds: this. Clamping rather than wrapping, because the keys are the
// case's moulded page-turn keys and the reader they were made for stops at the
// ends of a book.
int stepPage(int page, int pageCount, int delta);

}  // namespace picrossui

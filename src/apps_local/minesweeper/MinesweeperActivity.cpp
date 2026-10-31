#include "MinesweeperActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "MinesweeperScreens.h"
#include "components/SideButtonNavigation.h"

namespace ms = minesweeper;

namespace {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kHistoryPath[] = "/.crosspoint/minesweeper.sav";
#endif

// How long a finger must rest on a cell to plant a flag. Long enough that a
// deliberate dig never trips it, short enough that it does not feel broken on a
// panel whose own refresh is half a second.
constexpr unsigned long kFlagHoldMs = 400;
}  // namespace

std::unique_ptr<Activity> MinesweeperActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MinesweeperActivity>(renderer, mappedInput);
}

void MinesweeperActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  screen = ms::Screen::Menu;
  menuSelected = -1;
  loadHistory();
  HalFile config;
  if (Storage.openFileForRead("MINE", "/.crosspoint/mines-size.cfg", config)) {
    uint8_t size = 0;
    if (config.size() == 1 && config.read(&size, 1) == 1 && (size == 10 || size == 15 || size == 20)) boardSize = size;
  }
  requestUpdate();
}

void MinesweeperActivity::loadHistory() {
  {
    HalFile file;
    if (Storage.openFileForRead("MINE", "/.crosspoint/minesweeper-v2.bin", file)) {
      const int got = file.read(historyBytes, minehistory::kBytes);
      if (got >= 4 && !std::memcmp(historyBytes, "MSH", 3) && historyBytes[3] > '2') historyReadOnly = true;
      if (got == minehistory::kBytes && file.size() == minehistory::kBytes &&
          minehistory::decode(historyBytes, lastBoard, wins, losses)) {
        hasHistory = true;
        return;
      }
    }
  }
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.exists(kHistoryPath)) return;
  char buffer[512] = {};
  if (Storage.readFileToBuffer(kHistoryPath, buffer, sizeof(buffer)) == 0) return;

  // Parsed into locals and committed only if the whole line is good: a
  // truncated file leaves an empty menu rather than half a minefield. Losing
  // this costs an ornament, so it fails quietly.
  int values[2 + ms::kCells] = {};
  char* cursor = buffer;
  for (int i = 0; i < static_cast<int>(sizeof(values) / sizeof(values[0])); ++i) {
    char* next = nullptr;
    const long value = strtol(cursor, &next, 10);
    if (next == cursor) return;
    values[i] = static_cast<int>(value);
    cursor = next;
  }

  int at = 0;
  wins = values[at++];
  losses = values[at++];
  auto& board = lastBoard;
  board = {};
  for (int column = 0; column < ms::kColumns; ++column) {
    for (int row = 0; row < ms::kRows; ++row) board.cell[column][row] = static_cast<uint8_t>(values[at++]);
  }
  lastBoard = board;
  hasHistory = true;
#endif
}

void MinesweeperActivity::recordResult() {
  if (resultRecorded) return;
  resultRecorded = true;

  lastBoard = game;
  hasHistory = true;
  if (game.status == ms::Status::Won) ++wins;
  if (game.status == ms::Status::Lost) ++losses;

  if (historyReadOnly) return;
  minehistory::encode(lastBoard, wins, losses, historyBytes);
  {
    HalFile file;
    if (!Storage.openFileForWrite("MINE", "/.crosspoint/minesweeper-v2.part", file) ||
        file.write(historyBytes, minehistory::kBytes) != minehistory::kBytes) {
      LOG_ERR("MINE", "Could not save history");
      return;
    }
    file.flush();
  }
  if (!Storage.replaceFile("/.crosspoint/minesweeper-v2.part", "/.crosspoint/minesweeper-v2.bin"))
    LOG_ERR("MINE", "Could not publish history");
}

void MinesweeperActivity::goTo(const ms::Screen next) {
  screen = next;
  requestUpdate();
}

void MinesweeperActivity::beginGame() {
  // millis() is the only entropy that differs between two boots, and it enters
  // here and nowhere deeper: the core takes a seed and never reaches for a
  // clock, which is what keeps a board replayable from its seed.
  ms::start(game, static_cast<uint32_t>(millis()) * 2654435761u + 1u, boardSize, boardSize);
  holdColumn = -1;
  holdRow = -1;
  holdFired = false;
  flagMode = false;
  resultRecorded = false;
  goTo(ms::Screen::Board);
}

// What a tap on the grid means. Two things decide it and neither is in the
// interaction table.
//
// `screen`, because the grid geometry is only live on the board while the
// panel may still be showing the menu -- whose PLAY row sits exactly where a
// cell now is, so the tap that started the game would also dig.
//
// `flagMode`, because the FLAG capsule is registered with an identical rect,
// action, value and inputMask and flips only StateSelected, which the digest
// ignores as paint. The same cell digs or flags with nothing in the table to
// say which, and that is the whole worked example.
//
// Deliberately NOT here: the board itself. Revealing a cell repaints, and
// folding the position in would gate the next dig for a full refresh, which is
// the frozen-device failure. A change to the board is also always the player's
// own last tap -- there is no opponent here to move it underneath them.
uint32_t MinesweeperActivity::surfaceMeaning() const {
  const uint32_t withScreen = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(screen));
  const uint32_t withMode = paintclock::mixMeaning(withScreen, flagMode ? 1u : 0u);
  // The live/dead bit: a settled board keeps its pixels and stops accepting
  // moves, so the surface changes meaning without moving.
  return paintclock::mixMeaning(withMode, ms::over(game) ? 1u : 0u);
}

void MinesweeperActivity::loop() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (settings) {
      settings = false;
      screen = settingsReturn;
      requestUpdate();
      return;
    }
    if (ms::leavesApp(screen)) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    goTo(ms::back(screen));
    return;
  }

  // The rules decide when it is over; the record is written the moment they
  // do (latched by resultRecorded). The screen does NOT change: the settled
  // board stays, mines bared, wearing the verdict as its capsule. Navigating
  // away here was what made every ending anticlimactic -- the finished
  // minefield flashed for under a repaint and was gone.
  if (screen == ms::Screen::Board && ms::over(game)) recordResult();

  // Hold to flag, tap to dig.
  //
  // The grid is not in the interaction buffer -- eighty cells against a
  // twenty-four slot buffer -- so neither the tap nor the hold can go through
  // FreeInkUI's TouchHoldRouter, which routes against that buffer. Both are
  // done here against the same geometry that drew the pixels.
  //
  // swallowCurrentTouch() is the piece that makes a hold usable: it exists
  // precisely so a long press can fire while the finger is still down without
  // the ensuing lift also arriving as a tap. Without it, planting a flag would
  // immediately dig the cell it was planted on.
  if (screen == ms::Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    int hx = 0;
    int hy = 0;
    int column = 0;
    int row = 0;
    if (mappedInput.isScreenTouchHeld(hx, hy) && mineui::cellAt(device, hx, hy, column, row, game.columns, game.rows)) {
      if (!surfaceRevealed()) {
        // Void the contact instead of just ignoring it. Leaving the latch
        // running would let the hold fire the instant the gate opens, planting
        // a flag against a board the player had not seen yet -- the timer
        // would already have expired against the screen underneath.
        holdColumn = -1;
        holdRow = -1;
        holdFired = false;
        return;
      }
      if (column != holdColumn || row != holdRow) {
        // Moved to another cell: the hold starts again there, so sliding a
        // finger across the board never plants a flag you did not mean.
        holdColumn = column;
        holdRow = row;
        holdSinceMs = millis();
        holdFired = false;
      } else if (!holdFired && millis() - holdSinceMs >= kFlagHoldMs) {
        // Still a flag, in both modes, rather than "whatever the mode is not".
        // The symmetry was tempting and is a trap: in FLAG mode it would make
        // a held finger dig, and digging is the move that can end the game.
        // A hold you did not mean should never cost more than an extra tap to
        // undo. In FLAG mode it simply agrees with the tap.
        holdFired = true;
        if (ms::toggleFlag(game, column, row)) requestUpdate();
        // UNCONDITIONAL, and it must stay that way. Swallowing only when the
        // flag was actually planted looks harmless -- on a revealed cell the
        // hold does nothing, so why eat the contact? -- and it opens a way to
        // dig a cell the finger never rested on:
        //
        //   isScreenTouchHeld reports touchUpPoint, the LATEST contact sample,
        //   with no slop gate at all. wasScreenTapped reports touchDownPoint,
        //   the FIRST sample, and stays valid until motion passes the 59px
        //   release slop. A finger that lands on covered cell A, rolls 29-59px
        //   onto revealed cell B and rests there re-arms the hold at B; the
        //   hold fires at B, toggleFlag refuses because B is revealed, and the
        //   lift then taps A. Cells are 60px, so that window is about one cell
        //   wide, and the cell it digs can be a mine.
        //
        // Suppressing the contact is what makes those two positions unable to
        // disagree. The cost is that a press held past kFlagHoldMs on a number
        // does not chord -- chording is a quick tap, which is what it is
        // everywhere else -- and that is the cheap half of this trade.
        mappedInput.swallowCurrentTouch();
      }
      return;
    }
    if (holdColumn >= 0) {
      holdColumn = -1;
      holdRow = -1;
    }
  }

  if (interactionsReady && screen == ms::Screen::HowTo && surfaceRevealed()) {
    const int swipe = sidebuttons::swipeStep(mappedInput);
    if (const int direction = swipe ? swipe : sidebuttons::shortStep(mappedInput)) {
      const int next = std::clamp(howToPage + direction, 0, 4);
      if (next != howToPage) {
        howToPage = next;
        requestUpdate();
      }
      return;
    }
  }
  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  const bool hadButtonFocus = interactions.hasButtonFocus();
  if (sidebuttons::poll(interactions, mappedInput, tapX, tapY, !settings)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
    if (hadButtonFocus && !interactions.hasButtonFocus()) requestUpdate();
  }
  if (!input.touchReleased || !interactionsReady) return;

  if (screen == ms::Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    int column = 0;
    int row = 0;
    if (mineui::cellAt(device, tapX, tapY, column, row, game.columns, game.rows)) {
      // Guarded here rather than around the whole branch so a tap that misses
      // the grid still reaches route(), which has its own digest gate for the
      // chrome. See Activity::surfaceMeaning().
      if (!surfaceRevealed()) return;
      // FLAG mode does NOT chord, and that is a decision rather than an
      // oversight: chording is a dig, so it would make the FLAG tool able to
      // end the game. The hold branch above already refuses the same thing for
      // the same reason -- a move you did not mean should never cost more than
      // an extra tap to undo, and digging is the one move that can cost the
      // run. A player in FLAG mode taps the number again in DIG.
      //
      // The rules decide legality, and what the tap even MEANS: ms::dig digs a
      // covered cell and chords a revealed number that already carries its
      // flags. The choice between them is a rule, so it lives in the core
      // where the host suite can reach it -- there is nothing here that could
      // prove it, and the simulator never runs this file's input at all.
      const bool changed = flagMode ? ms::toggleFlag(game, column, row) : ms::dig(game, column, row);
      if (changed) requestUpdate();
      return;
    }
  }

  const fui::ActionEvent event = sidebuttons::route(interactions, input, mappedInput, *this, !settings);
  switch (event.action) {
    case mineui::ActionSettings:
      settingsReturn = screen;
      pendingSize = boardSize;
      settings = true;
      goTo(ms::Screen::Menu);
      return;
    case mineui::ActionBoardSize:
      if (settings && (event.value == 10 || event.value == 15 || event.value == 20)) {
        pendingSize = event.value;
        requestUpdate();
      }
      return;
    case mineui::ActionSaveSize:
      if (!settings) return;
      if (boardSize != pendingSize) {
        boardSize = pendingSize;
        HalFile config;
        const uint8_t size = boardSize;
        if (!Storage.openFileForWrite("MINE", "/.crosspoint/mines-size.cfg", config) || config.write(&size, 1) != 1)
          LOG_ERR("MINE", "Could not save board size");
      }
      settings = false;
      screen = settingsReturn;
      requestUpdate();
      return;
    case mineui::ActionHowToPage:
      if (screen == ms::Screen::HowTo) {
        {
          const int next = std::clamp(howToPage + event.value, 0, mineui::howToPages() - 1);
          if (next == howToPage) return;
          howToPage = next;
        }
        requestUpdate();
      }
      return;
    case mineui::ActionMenuRow:
      switch (static_cast<mineui::MenuRow>(event.value)) {
        case mineui::MenuRow::Play:
          beginGame();
          return;
        case mineui::MenuRow::HowTo:
          howToPage = 0;
          goTo(ms::Screen::HowTo);
          return;
        case mineui::MenuRow::Settings:
          settingsReturn = ms::Screen::Menu;
          pendingSize = boardSize;
          settings = true;
          requestUpdate();
          return;
        case mineui::MenuRow::Count:
          return;
      }
      return;

    case mineui::ActionHowToNext:
      if (howToPage + 1 < mineui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(ms::Screen::Menu);
      return;

    case mineui::ActionAgain:
      beginGame();
      return;

    case mineui::ActionDone:
      goTo(ms::Screen::Menu);
      return;

    case mineui::ActionToggleMode:
      flagMode = event.value == 0;
      requestUpdate();
      return;

    case mineui::ActionSeeResult:
      goTo(ms::Screen::Result);
      return;

    default:
      return;
  }
}

void MinesweeperActivity::render(RenderLock&&) {
  if (settings) interactions.setFocusedIndex(-1);
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen surface(frame);

  switch (screen) {
    case ms::Screen::Menu: {
      auto& model = menuModel;
      model = {};
      model.settings = settings;
      model.size = settings ? pendingSize : boardSize;
      model.selected = menuSelected;
      model.hasHistory = hasHistory;
      model.lastBoard = lastBoard;
      model.wins = wins;
      model.losses = losses;
      mineui::buildMenu(surface, model);
      break;
    }
    case ms::Screen::HowTo: {
      mineui::HowToModel model;
      model.page = howToPage;
      mineui::buildHowTo(surface, model);
      break;
    }
    case ms::Screen::Board: {
      auto& model = boardModel;
      model = {};
      model.game = game;
      model.showMines = ms::over(game);
      model.flagMode = flagMode;
      mineui::buildBoard(surface, model);
      break;
    }
    case ms::Screen::Result: {
      mineui::ResultModel model;
      model.cells = game.columns * game.rows;
      model.mines = game.mines;
      model.won = game.status == ms::Status::Won;
      int revealed = 0;
      int flagsRight = 0;
      for (int column = 0; column < game.columns; ++column) {
        for (int row = 0; row < game.rows; ++row) {
          const uint8_t cell = game.cell[column][row];
          if ((cell & ms::kRevealed) && !(cell & ms::kMine)) ++revealed;
          if ((cell & ms::kFlagged) && (cell & ms::kMine)) ++flagsRight;
        }
      }
      model.revealed = revealed;
      model.flagsRight = flagsRight;
      mineui::buildResult(surface, model);
      break;
    }
  }

  interactionsReady = true;
  if (screen == ms::Screen::Board) interactions.limitButtonActions({mineui::ActionToggleMode});
  toybox::reportOverflow(interactions, "Minesweeper", &renderer);

  const auto labels = mappedInput.mapLabels(tr(STR_QUEST_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

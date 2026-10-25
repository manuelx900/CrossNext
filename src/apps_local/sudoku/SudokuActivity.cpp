#include "SudokuActivity.h"

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
#include "SudokuLabels.h"
#include "components/SideButtonNavigation.h"

namespace sk = sudoku;

namespace {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kStatePath[] = "/.crosspoint/sudoku.sav";
constexpr char kPreferencesPath[] = "/.crosspoint/sudoku.cfg";
// Bumped when the layout of the line below changes. An older file is discarded
// rather than misread.
constexpr int kStateVersion = 1;
// Eighteen small integers, then 81 + 81 digits and 81 three-digit hex masks.
constexpr int kStateBytes = 768;
#endif

}  // namespace

std::unique_ptr<Activity> SudokuActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<SudokuActivity>(renderer, mappedInput);
}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  screen = sk::Screen::Menu;
  menuSelected = -1;
  notice = nullptr;
  lastTickMs = millis();
  // The only entropy that differs between two boots, and it enters here and
  // nowhere deeper: the rules take a seed and never reach for a clock, which is
  // what keeps a puzzle reproducible from one.
  rng = static_cast<uint32_t>(millis()) * 2654435761u + 1u;
  loadState();
  loadPreferences();
  requestUpdate();
}

void SudokuActivity::onExit() {
  // Runs on sleep as well as on leaving, which is the case that matters: the
  // player puts the device down by doing nothing at all.
  saveState();
  Activity::onExit();
}

void SudokuActivity::goTo(const sk::Screen next) {
  screen = next;
  if (next != sk::Screen::Board) entryTool = sk::EntryTool::Digit;
  interactionsReady = false;
  notice = nullptr;
  requestUpdate();
}

void SudokuActivity::beginGame() {
  generating = true;
  generateDeferred = true;
  generatingLevel = menuLevel;
  resultRecorded = false;
  newBest = false;
  holdCell = sk::kNoCell;
  holdFired = false;
  selectedCell = sk::kNoCell;
  notesMode = false;
  entryTool = sk::EntryTool::Digit;
  goTo(sk::Screen::Board);
}

void SudokuActivity::cancelCarve() {
  generating = false;
  generateDeferred = false;
  // beginGame() cleared this on the way in, so restore what the surviving game
  // actually is. Otherwise cancelling a new puzzle started over a SOLVED one
  // leaves solvedFlag set with resultRecorded false, and the board would record
  // that solve a second time. Unreachable today only because canResume()
  // refuses to re-open a solved game -- one menu row away from being live.
  resultRecorded = game.solvedFlag != 0;
}

bool SudokuActivity::handleHomeGesture() {
  cancelCarve();
  // saveState() is not needed here: onExit() runs on the way out and writes the
  // game this call just protected.
  return false;
}

void SudokuActivity::tickClock() {
  const unsigned long now = millis();
  const unsigned long since = now - lastTickMs;
  lastTickMs = now;
  if (screen != sk::Screen::Board || generating || game.solvedFlag != 0 || !hasGame) return;
  // The clock advances here and is painted with the next requested game frame.
  game.elapsedMs += static_cast<uint32_t>(since);
}

void SudokuActivity::recordResult() {
  if (resultRecorded) return;
  resultRecorded = true;
  const int index = static_cast<int>(game.puzzle.level);
  previousBestMs = record.bestMs[index];
  sk::recordSolve(record, game.puzzle.level, game.elapsedMs, game.hintsUsed);
  newBest = game.hintsUsed == 0 && record.bestMs[index] == game.elapsedMs && previousBestMs != game.elapsedMs;
  saveState();
}

void SudokuActivity::loadPreferences() {
  showMatches = true;
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  uint8_t bytes[2]{};
  HalFile file;
  if (Storage.openFileForRead("SUDOKU", kPreferencesPath, file) && file.read(bytes, sizeof(bytes)) == sizeof(bytes) &&
      bytes[0] == 1 && bytes[1] <= 1)
    showMatches = bytes[1] != 0;
#endif
}

bool SudokuActivity::savePreferences(const bool enabled) {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  const uint8_t bytes[] = {1, static_cast<uint8_t>(enabled)};
  HalFile file;
  if (!Storage.openFileForWrite("SUDOKU", kPreferencesPath, file) ||
      file.write(bytes, sizeof(bytes)) != sizeof(bytes)) {
    LOG_ERR("SUDOKU", "Could not save highlighting preference");
    return false;
  }
#else
  (void)enabled;
#endif
  return true;
}

void SudokuActivity::loadState() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.exists(kStatePath)) return;
  auto buffer = makeUniqueNoThrow<char[]>(kStateBytes);
  if (!buffer) {
    LOG_ERR("SUDOKU", "OOM: %d bytes for the save", kStateBytes);
    return;
  }
  std::memset(buffer.get(), 0, kStateBytes);
  if (Storage.readFileToBuffer(kStatePath, buffer.get(), kStateBytes) == 0) return;

  // Parsed into locals and committed only at the end: a truncated file leaves a
  // fresh app rather than half a puzzle.
  constexpr int kHeaderCount = 18;
  long header[kHeaderCount] = {};
  char* cursor = buffer.get();
  for (int i = 0; i < kHeaderCount; ++i) {
    char* next = nullptr;
    const long value = strtol(cursor, &next, 10);
    if (next == cursor) return;
    header[i] = value;
    cursor = next;
  }
  if (header[0] != kStateVersion) return;

  // Three fixed-width runs: the clues, the player's digits, their marks.
  auto takeRun = [&cursor](char* out, const int length) {
    while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r') ++cursor;
    for (int i = 0; i < length; ++i) {
      if (cursor[i] == '\0') return false;
      out[i] = cursor[i];
    }
    cursor += length;
    return true;
  };
  char clues[sk::kCells + 1] = {};
  char entries[sk::kCells + 1] = {};
  char marks[sk::kCells * 3 + 1] = {};
  if (!takeRun(clues, sk::kCells) || !takeRun(entries, sk::kCells) || !takeRun(marks, sk::kCells * 3)) return;

  sk::Puzzle puzzle;
  for (int cell = 0; cell < sk::kCells; ++cell) {
    const char c = clues[cell];
    if (c < '0' || c > '9') return;
    puzzle.given[cell] = static_cast<uint8_t>(c - '0');
  }

  // The answer is not saved, it is re-derived. The puzzle has exactly one
  // solution, so the clues determine it, and a save that cannot be solved is a
  // corrupt save rather than a puzzle with a surprising answer. This is also
  // what makes the file independent of the generator: a later generator changes
  // nothing about how an old grid reads.
  sk::Grid grid;
  if (!sk::load(grid, puzzle.given)) return;
  const sk::SolveReport report = sk::solve(grid, sk::ceilingFor(sk::Level::Expert));
  if (!report.solved || report.broken) return;
  for (int cell = 0; cell < sk::kCells; ++cell) puzzle.solution[cell] = grid.value[cell];
  puzzle.hardest = report.hardest;
  puzzle.level = sk::levelOf(report.hardest);
  puzzle.clues = 0;
  for (int cell = 0; cell < sk::kCells; ++cell) {
    if (puzzle.given[cell] != 0) ++puzzle.clues;
  }

  sk::Game restored{};
  restored.puzzle = puzzle;
  for (int cell = 0; cell < sk::kCells; ++cell) {
    const char c = entries[cell];
    if (c < '0' || c > '9') return;
    restored.entry[cell] = puzzle.given[cell] != 0 ? 0 : static_cast<uint8_t>(c - '0');
    char hex[4] = {marks[cell * 3], marks[cell * 3 + 1], marks[cell * 3 + 2], '\0'};
    restored.note[cell] = static_cast<sk::Mask>(strtol(hex, nullptr, 16) & sk::kAllDigits);
  }

  int at = 1;
  // Range-checked rather than reduced: header[] is a long, and a negative one
  // survives `% kLevelCount` as a negative, which becomes Level(255) and indexes
  // record.bestMs[255] off the end of a four-element array when the menu draws.
  const long savedLevel = header[at++];
  menuLevel = static_cast<sk::Level>(savedLevel >= 0 && savedLevel < sk::kLevelCount ? savedLevel : 0);
  restored.elapsedMs = static_cast<uint32_t>(header[at++]);
  restored.hintsUsed = static_cast<uint8_t>(header[at++]);
  const long armed = header[at++];
  restored.armed = static_cast<uint8_t>(armed >= 1 && armed <= sk::kSize ? armed : 1);
  const long hintCell = header[at++];
  restored.hintCell = static_cast<uint8_t>(hintCell >= 0 && hintCell < sk::kCells ? hintCell : sk::kNoCell);
  restored.hintDigit = static_cast<uint8_t>(header[at++]);
  restored.solvedFlag = header[at++] != 0 ? 1 : 0;
  const bool savedHasGame = header[at++] != 0;
  for (int i = 0; i < sk::kLevelCount; ++i) record.solved[i] = static_cast<uint16_t>(header[at++]);
  for (int i = 0; i < sk::kLevelCount; ++i) record.bestMs[i] = static_cast<uint32_t>(header[at++]);
  record.hintsTaken = static_cast<uint16_t>(header[at++]);

  game = restored;
  hasGame = savedHasGame;
  resultRecorded = game.solvedFlag != 0;

  // The menu opens on the level of the puzzle it is offering to resume, which
  // is a repair as much as a preference. `menuLevel` is a stored field and
  // `puzzle.level` is re-derived from the clues above, so a card written while
  // the player was merely BROWSING the difficulty row holds the two disagreeing
  // -- and the door reads what they say, so it would offer NEW PUZZLE over a
  // good grid. A level picked and never played is not worth carrying across a
  // restart; a half-solved puzzle is.
  if (hasGame && game.solvedFlag == 0) menuLevel = game.puzzle.level;
#endif
}

void SudokuActivity::saveState() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  auto buffer = makeUniqueNoThrow<char[]>(kStateBytes);
  if (!buffer) {
    LOG_ERR("SUDOKU", "OOM: %d bytes for the save", kStateBytes);
    return;
  }
  int used =
      std::snprintf(buffer.get(), kStateBytes, "%d %d %u %d %d %d %d %d %d", kStateVersion, static_cast<int>(menuLevel),
                    game.elapsedMs, game.hintsUsed, game.armed, game.hintCell >= sk::kCells ? -1 : game.hintCell,
                    game.hintDigit, game.solvedFlag, hasGame ? 1 : 0);
  for (int i = 0; i < sk::kLevelCount && used > 0 && used < kStateBytes; ++i) {
    used += std::snprintf(buffer.get() + used, static_cast<size_t>(kStateBytes - used), " %d", record.solved[i]);
  }
  for (int i = 0; i < sk::kLevelCount && used > 0 && used < kStateBytes; ++i) {
    used += std::snprintf(buffer.get() + used, static_cast<size_t>(kStateBytes - used), " %u", record.bestMs[i]);
  }
  if (used > 0 && used < kStateBytes) {
    used += std::snprintf(buffer.get() + used, static_cast<size_t>(kStateBytes - used), " %d\n", record.hintsTaken);
  }
  if (used <= 0 || used + 2 * sk::kCells + 3 * sk::kCells + 4 >= kStateBytes) {
    LOG_ERR("SUDOKU", "Save did not fit %d bytes", kStateBytes);
    return;
  }

  for (int cell = 0; cell < sk::kCells; ++cell) {
    buffer[used++] = static_cast<char>('0' + game.puzzle.given[cell]);
  }
  buffer[used++] = '\n';
  for (int cell = 0; cell < sk::kCells; ++cell) {
    buffer[used++] = static_cast<char>('0' + (game.entry[cell] <= 9 ? game.entry[cell] : 0));
  }
  buffer[used++] = '\n';
  for (int cell = 0; cell < sk::kCells; ++cell) {
    static const char kHex[] = "0123456789ABCDEF";
    const sk::Mask mask = game.note[cell];
    buffer[used++] = kHex[(mask >> 8) & 0xF];
    buffer[used++] = kHex[(mask >> 4) & 0xF];
    buffer[used++] = kHex[mask & 0xF];
  }
  buffer[used++] = '\n';
  buffer[used] = '\0';
  Storage.writeFile(kStatePath, String(buffer.get()));
#endif
}

// Navigation and availability change the meaning of the whole grid.
// The armed number and individual cell contents remain transient selections.
uint32_t SudokuActivity::surfaceMeaning() const {
  const uint32_t withScreen = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(screen));
  const uint32_t withGeneration = paintclock::mixMeaning(withScreen, generating ? 1u : 0u);
  const uint32_t withMode = paintclock::mixMeaning(withGeneration, notesMode ? 1u : 0u);
  const uint32_t withTool = paintclock::mixMeaning(withMode, static_cast<uint32_t>(entryTool));
  return paintclock::mixMeaning(withTool, game.solvedFlag != 0 ? 1u : 0u);
}

void SudokuActivity::loop() {
  namespace fui = freeink::ui;

  tickClock();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (sk::leavesApp(screen)) {
      saveState();
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (screen == sk::Screen::Board) {
      // Back off a board that is still being carved CANCELS the carve, and that
      // is not tidiness. The poll below does not look at `screen`, so an
      // abandoned carve kept running on the menu and landed through
      // startGame() -- replacing the saved game and rewriting the card from a
      // screen the player was only reading, with no tap at all. The window is
      // not a frame either: 24 attempts a pass carries roughly even odds at the
      // scarcer levels, so MAKING ONE can sit there for several passes.
      cancelCarve();
      saveState();
    }
    goTo(sk::back(screen));
    return;
  }

  if (screen == sk::Screen::HowTo) {
    const bool forward = mappedInput.wasReleased(MappedInputManager::Button::Down);
    const bool backward = mappedInput.wasReleased(MappedInputManager::Button::Up);
    if (forward || backward) {
      howToPage = (howToPage + (forward ? 1 : sudokuui::howToPages() - 1)) % sudokuui::howToPages();
      requestUpdate();
      return;
    }
  }

  // Carving runs one pass after the frame that announced it, so MAKING ONE is
  // on the panel before the main loop disappears for a while. The budget is per
  // pass rather than per puzzle: at the scarcer levels this can take a few
  // dozen attempts, and spending them all in one pass is what a freeze is.
  if (generating) {
    int x = 0, y = 0;
    if (interactionsReady && sidebuttons::poll(interactions, mappedInput, x, y)) {
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = x;
      tap.touchY = y;
      if (sidebuttons::route(interactions, tap, mappedInput, *this).action == sudokuui::ActionMenu) {
        cancelCarve();
        saveState();
        goTo(sk::Screen::Menu);
        return;
      }
    }
    if (generateDeferred) {
      generateDeferred = false;
      return;
    }
    sk::Puzzle puzzle;
    if (sk::generate(puzzle, generatingLevel, work, rng, 24)) {
      sk::startGame(game, puzzle);
      hasGame = true;
      generating = false;
      saveState();
      requestUpdate();
    }
    return;
  }

  if (screen == sk::Screen::Board && game.solvedFlag != 0 && !resultRecorded) {
    recordResult();
    goTo(sk::Screen::Result);
    return;
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (sidebuttons::poll(interactions, mappedInput, tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  if (screen == sk::Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    int cell = 0;
    int digit = 0;
    if (sudokuui::cellAt(device, tapX, tapY, cell)) {
      // Guarded inside the branch, so a tap that misses grid and pad still
      // reaches route() and its own digest gate for the chrome.
      if (!surfaceRevealed()) return;
      if (entryTool == sk::EntryTool::Check) return;
      notice = nullptr;
      if (!sk::applyEntryTool(game, cell, entryTool)) return;
      selectedCell = cell;
      requestUpdate();
      return;
    }
    if (sudokuui::padKeyAt(device, tapX, tapY, digit)) {
      if (!surfaceRevealed()) return;
      notice = nullptr;
      game.armed = static_cast<uint8_t>(digit);
      entryTool = sk::EntryTool::Digit;
      game.hintCell = sk::kNoCell;
      game.hintDigit = 0;
      requestUpdate();
      return;
    }
  }

  const fui::ActionEvent event = sidebuttons::route(interactions, input, mappedInput, *this);
  switch (event.action) {
    case sudokuui::ActionDigit:
      if (screen != sk::Screen::Board || event.value < 1 || event.value > 9) return;
      game.armed = event.value;
      entryTool = sk::EntryTool::Digit;
      requestUpdate();
      return;
    case sudokuui::ActionPlay:
      // Resuming and starting are the same door, because the difference is a
      // fact about the save rather than a choice the player should have to make.
      // Which one it is has exactly one definition, shared with the label the
      // player read before tapping: a second copy of it here is how a door does
      // something other than what it says.
      if (sk::canResume(game, hasGame, menuLevel)) {
        goTo(sk::Screen::Board);
        return;
      }
      beginGame();
      return;

    case sudokuui::ActionMenuRow:
      switch (static_cast<sudokuui::MenuRow>(event.value)) {
        case sudokuui::MenuRow::Settings:
          pendingLevel = menuLevel;
          pendingShowMatches = showMatches;
          settingsSaveFailed = false;
          goTo(sk::Screen::Settings);
          return;
        case sudokuui::MenuRow::HowTo:
          howToPage = 0;
          goTo(sk::Screen::HowTo);
          return;
        case sudokuui::MenuRow::Count:
          return;
      }
      return;

    case sudokuui::ActionHowToNext:
      if (howToPage + 1 < sudokuui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(sk::Screen::Menu);
      return;

    case sudokuui::ActionHowToPrevious:
      if (howToPage > 0) {
        --howToPage;
        requestUpdate();
      }
      return;

    case sudokuui::ActionPickLevel:
      if (event.value >= 0 && event.value < sk::kLevelCount) {
        pendingLevel = static_cast<sk::Level>(event.value);
        menuLevel = pendingLevel;
        saveState();
        requestUpdate();
      }
      return;

    case sudokuui::ActionToggleMatches: {
      if (screen != sk::Screen::Settings) return;
      const bool next = !pendingShowMatches;
      settingsSaveFailed = !savePreferences(next);
      if (!settingsSaveFailed) pendingShowMatches = showMatches = next;
      requestUpdate();
      return;
    }

    case sudokuui::ActionApplySettings:
      if (screen != sk::Screen::Settings) return;
      if (pendingShowMatches != showMatches && !savePreferences(pendingShowMatches)) {
        settingsSaveFailed = true;
        requestUpdate();
        return;
      }
      showMatches = pendingShowMatches;
      menuLevel = pendingLevel;
      saveState();
      goTo(sk::Screen::Menu);
      return;

    case sudokuui::ActionMenu:
      if (generating) cancelCarve();
      saveState();
      goTo(sk::Screen::Menu);
      return;

    case sudokuui::ActionErase:
      if (game.solvedFlag != 0 || entryTool == sk::EntryTool::Erase) return;
      entryTool = sk::EntryTool::Erase;
      game.hintCell = sk::kNoCell;
      notice = nullptr;
      requestUpdate();
      return;

    case sudokuui::ActionUndo:
      if (sk::undoOnce(game)) {
        notice = nullptr;
        requestUpdate();
      }
      return;

    case sudokuui::ActionHint:
      if (game.solvedFlag != 0 || entryTool == sk::EntryTool::Check) return;
      for (int cell = 0; cell < sk::kCells; ++cell) {
        if (!game.entry[cell]) continue;
        if (game.hintsUsed < 255) ++game.hintsUsed;
        break;
      }
      entryTool = sk::EntryTool::Check;
      selectedCell = sk::kNoCell;
      game.hintCell = sk::kNoCell;
      notice = nullptr;
      requestUpdate();
      return;

    case sudokuui::ActionSeeResult:
      goTo(sk::Screen::Result);
      return;

    case sudokuui::ActionNew:
      if (hasGame && !game.solvedFlag)
        goTo(sk::Screen::ConfirmNew);
      else
        beginGame();
      return;
    case sudokuui::ActionAgain:
      beginGame();
      return;

    case sudokuui::ActionDone:
      goTo(sk::Screen::Board);
      return;

    default:
      return;
  }
}

void SudokuActivity::render(RenderLock&&) {
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
    case sk::Screen::ConfirmNew:
      sudokuui::buildConfirmNew(surface);
      break;
    case sk::Screen::Menu: {
      sudokuui::MenuModel model;
      model.level = menuLevel;
      model.hasGame = hasGame;
      model.game = game;
      model.record = record;
      model.selected = menuSelected;
      sudokuui::buildMenu(surface, model);
      break;
    }
    case sk::Screen::Settings: {
      sudokuui::SettingsModel model;
      model.level = pendingLevel;
      model.showMatches = pendingShowMatches;
      model.saveFailed = settingsSaveFailed;
      sudokuui::buildSettings(surface, model);
      break;
    }
    case sk::Screen::HowTo: {
      sudokuui::HowToModel model;
      model.page = howToPage;
      sudokuui::buildHowTo(surface, model);
      break;
    }
    case sk::Screen::Board: {
      sudokuui::BoardModel model;
      model.game = game;
      model.holdCell = holdCell;
      model.generating = generating;
      model.selectedCell = selectedCell;
      model.notesMode = notesMode;
      model.entryTool = entryTool;
      model.showMatches = showMatches;
      model.notice = notice;
      sudokuui::buildBoard(surface, model);
      break;
    }
    case sk::Screen::Result: {
      sudokuui::ResultModel model;
      model.game = game;
      model.level = game.puzzle.level;
      model.hardest = game.puzzle.hardest;
      model.elapsedMs = game.elapsedMs;
      model.bestMs = record.bestMs[static_cast<int>(game.puzzle.level)];
      model.newBest = newBest;
      model.hintsUsed = game.hintsUsed;
      model.clues = game.puzzle.clues;
      model.solvedAtThisLevel = record.solved[static_cast<int>(game.puzzle.level)];
      model.solvedTotal = sk::totalSolved(record);
      sudokuui::buildResult(surface, model);
      break;
    }
  }

  interactionsReady = true;
  if (screen == sk::Screen::Board)
    interactions.limitButtonActions({sudokuui::ActionDigit, sudokuui::ActionErase, sudokuui::ActionHint});
  toybox::reportOverflow(interactions, "Sudoku", &renderer);

  const auto labels = mappedInput.mapLabels(tr(STR_QUEST_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

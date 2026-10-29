#include "PicrossActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "../../activities/ActivityManager.h"
#include "PicrossLegacyMap.h"
#include "components/SideButtonNavigation.h"
// GUI, for drawButtonHints. Included directly rather than inherited from
// Toybox.h: a header pulled in only for a macro reads as unused to clangd.
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"

namespace {

namespace fui = freeink::ui;
namespace ui = picrossui;

constexpr char kSavePath[] = "/.crosspoint/picross-v7.sav";
constexpr char kLegacyPath[] = "/.crosspoint/picross.sav";

// How many MARKS may go unwritten. A committing FILL is flushed immediately --
// it is irreversible and unrepeatable, so losing one to a sleep is the worst
// data loss this game has. Annotations are cheap to redo, so they batch.
constexpr int kSaveEvery = 12;

// After this many board changes with no full refresh, force one to clear the
// ghosting a dense 1-bit grid accumulates over a solve.
constexpr int kFlashEvery = 24;

}  // namespace

std::unique_ptr<Activity> PicrossActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<PicrossActivity>(renderer, mappedInput);
}

void PicrossActivity::onEnter() {
  toybox::ensureFonts(renderer);
  if (!loadState()) {
    progress = picross::Progress{};
    board.load(0);
  }
  // Open on whatever the save was in the middle of, so the picker points at the
  // resumable puzzle the moment the app appears.
  selected = (board.touched() && !board.solved()) ? board.index() : progress.nextUnsolved();
  view = View::Menu;
  syncPicker();
  recorded = false;
  requestUpdate();
}

void PicrossActivity::onExit() { flushSave(); }

void PicrossActivity::setMode(const int newMode) {
  if (view != View::Board || mode == newMode) return;
  mode = newMode;
  requestUpdate();
}

void PicrossActivity::openPuzzle(const int requested) {
  const int index = picross::isPlayable(requested) ? requested : 0;
  // Resume only when the tapped puzzle is the one already in hand and still
  // unfinished. A different puzzle, or a solved one tapped for a replay, starts
  // clean -- otherwise tapping RESUME would wipe the board it offered to resume,
  // and re-opening a finished picture would show it already done.
  const bool resume = index == board.index() && board.touched() && !board.solved();
  if (!resume) {
    board.load(index);
    unsaved = 1;
    flushSave();
  }
  recorded = false;
  // THE PICKER'S IDEA OF "THE CHOSEN PUZZLE" IS THIS ONE FROM NOW ON, and
  // forgetting to say so was a data-loss bug rather than an untidiness.
  //
  // `selected` drives two things. syncPicker() sets followSelection, so the
  // picker resolves the TAB and the PAGE from it -- open a 10x10, press Back,
  // and without this line the picker reopens on tab 0 page 0 with the tier you
  // were browsing gone. And buildMenu's `hasProgress` is
  // `inProgressIndex == selected`, so a stale `selected` makes the front-door
  // button read PLAY over an in-progress board; tapping it calls openPuzzle on
  // a DIFFERENT index, which takes the `!resume` branch above and load()s over
  // the marks it was offering to resume. RESUME therefore only ever worked on a
  // cold entry, and the tabs are what made the loss visible.
  selected = index;
  // Start physical-button activation with the fill tool.
  mode = ui::ModeFill;
  view = View::Board;
  // Full refresh on open clears whatever the last screen left behind.
  flashOnNextPaint = true;
  paintsSinceFlash = 0;
  requestUpdate();
}

void PicrossActivity::settleWin() {
  if (!board.solved() || recorded) return;
  recorded = true;
  lastCleared = board.index();
  lastMistakes = board.mistakes();
  progress.markSolved(board.index());
  // The finished board is not worth resuming, so the save carries the next
  // puzzle instead -- and carries the progress, which is what matters.
  //
  // NEXT STAYS IN THE TIER YOU ARE PLAYING. `nextUnsolved()` is the first
  // unsolved in BANK ORDER, and the bank is size-sorted: with one tier that was
  // progression, with four it means finishing any 10x10 hands you the lowest
  // unsolved 5x5 -- a third of the size, four tabs from where you were, and
  // `selected` follows it so the picker reopens there too. NEXT means "another
  // one of these"; changing tier is what the tabs are for. Falling back to the
  // whole bank only when this tier is finished, because a NEXT that does
  // nothing on the last 5x5 is worse than one that moves on.
  const int sameTier = progress.nextUnsolvedAtSize(board.puzzle().size);
  board.load(sameTier >= 0 ? sameTier : progress.nextUnsolved());
  selected = board.index();
  unsaved = 1;
  flushSave();
  view = View::Won;
  flashOnNextPaint = true;  // the full blink as punctuation for a finished picture
  requestUpdate();
}

// board.index() rather than a layout member: the layout is derived from the
// puzzle and hashing a member would stamp the previous frame's copy
// (surfaceMeaning runs before render). See Activity::surfaceMeaning.
//
// THE PICKER'S TAB AND PAGE ARE PART OF THE MEANING, and leaving them out was a
// hole this change widened. The picker registers ONE rect for the whole tile
// grid and resolves which tile a pixel is through `pickerLayout`, so a pixel's
// meaning is the geometry, not the rect -- which is the case SurfaceGate exists
// for. Switching tab or page replaces `pickerLayout.firstIndex` wholesale while
// leaving the board index untouched, so without these two the meaning is
// IDENTICAL across the swap, routable() falls through to `meaning ==
// shownMeaning_`, and a tap queued against the tiles you were looking at
// resolves against the twenty that replaced them. The page dots had the same
// hole; four tabs multiply it, and openPuzzle() load()s, so the cost is an
// in-progress board wiped by a puzzle nobody picked.
uint32_t PicrossActivity::surfaceMeaning() const {
  const uint32_t withView = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(view));
  const uint32_t withPuzzle = paintclock::mixMeaning(withView, static_cast<uint32_t>(board.index()));
  const uint32_t withTab = paintclock::mixMeaning(withPuzzle, static_cast<uint32_t>(menuTab));
  const uint32_t withPage = paintclock::mixMeaning(withTab, static_cast<uint32_t>(menuPage));
  return paintclock::mixMeaning(withPage, static_cast<uint32_t>(selected));
}

void PicrossActivity::routeBoardTap(const int x, const int y, const int inputMode) {
  // The board registers as one rect; which cell this pixel is comes from the
  // layout, and route() cannot see that. See Activity::surfaceMeaning().
  if (!surfaceRevealed()) return;
  int row = 0;
  int col = 0;
  if (!layout.cellAt(x, y, row, col)) return;

  bool changed = false;
  bool newMistake = false;
  if (inputMode == ui::ModeMark) {
    changed = board.mark(row, col);
  } else {
    const int before = board.mistakes();
    changed = board.fill(row, col);
    newMistake = board.mistakes() > before;
  }
  if (!changed) return;

  // A commit is persisted at once; a mark can wait.
  if (inputMode == ui::ModeMark)
    touchSave();
  else {
    unsaved = std::max(1, unsaved);
    flushSave();
  }

  if (board.solved()) {
    settleWin();
    return;
  }
  if (newMistake || ++paintsSinceFlash >= kFlashEvery) {
    flashOnNextPaint = true;
    paintsSinceFlash = 0;
  }
  requestUpdate();
}

// Land the picker on the page showing `selected`, so the highlighted tile is on
// screen the moment the grid appears.
//
// It sets a flag rather than working the page out, because the page depends on
// how many tiles fit the panel and that is decided in buildMenu. This used to
// divide by a literal 16 -- the number the picker happened to draw when a size-
// tab band sat above the grid -- and the moment that band went the two
// disagreed silently, opening the picker on the wrong page. The picker reports
// what it drew and menuPage is copied back from that.
void PicrossActivity::syncPicker() { menuFollowsSelection = true; }

// Show size group `tab`, from a tab tap. The page resets to the first of that
// group: keeping it would land on page 4 of a tier that has two, and the picker
// would clamp it to the last page rather than the first, which reads as the tab
// remembering somewhere the player has never been.
void PicrossActivity::showTab(const int tab) {
  if ((view != View::Picker && view != View::Sizes) || tab < 0 || tab >= picross::kSizeGroupCount) return;
  if (view == View::Sizes) {
    pendingSizeTab = tab;
    requestUpdate();
    return;
  }
  if (ui::groupFor(selected) != tab) selected = ui::groupFirst(tab);
  menuTab = tab;
  menuPage = 0;
  menuFollowsSelection = false;  // an explicit tab beats "open on the selection"
  flashOnNextPaint = true;       // a wholesale content swap earns a clean full refresh
  requestUpdate();
}

// Show picker page `page`, from a dot tap or a side key. A no-op when it is
// already the page on screen: an e-ink full refresh for nothing is a visible
// blink that says something happened when nothing did.
void PicrossActivity::showPage(const int page) {
  if (view != View::Picker || page == menuPage) return;
  menuPage = page;
  menuFollowsSelection = false;  // an explicit page beats "open on the selection"
  flashOnNextPaint = true;       // a wholesale page swap earns a clean full refresh
  requestUpdate();
}

void PicrossActivity::routeButton(const int button) {
  switch (button) {
    case ui::ButtonPlay:
      if (!surfaceRevealed()) break;
      openPuzzle(selected);
      break;
    case ui::ButtonRestart:
      // A new attempt at the same picture: clears the board and the mistake
      // count. Labelled and placed a row above the mode toggle, away from the
      // thumb, because it discards progress.
      board.load(board.index());
      mode = ui::ModeFill;
      unsaved = 1;
      flushSave();
      flashOnNextPaint = true;
      requestUpdate();
      break;
    case ui::ButtonPuzzles:
      flushSave();
      view = View::Picker;
      syncPicker();
      flashOnNextPaint = true;
      requestUpdate();
      break;
    case ui::ButtonSizes:
      pendingSizeTab = ui::groupFor(selected);
      view = View::Sizes;
      requestUpdate();
      break;
    case ui::ButtonHelp:
      helpPage = 0;
      view = View::Help;
      requestUpdate();
      break;
    case ui::ButtonHome:
      view = View::Menu;
      requestUpdate();
      break;
    case ui::ButtonNext:
      openPuzzle(progress.nextUnsolved());
      break;
    default:
      break;
  }
}

void PicrossActivity::loop() {
  // Keep touch sampling active while the panel refreshes.
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view == View::Menu) {
      shelf::leave(renderer, mappedInput);  // no app names its own destination
    } else {
      flushSave();
      view = View::Menu;
      syncPicker();
      flashOnNextPaint = true;
      requestUpdate();
    }
    return;
  }

  if (interactionsReady && interactions.routable()) {
    if (const int direction = sidebuttons::horizontalStep(mappedInput)) {
      if (view == View::Picker) {
        showPage(ui::stepPage(menuPage, pickerLayout.pageCount, direction));
        return;
      }
      if (view == View::Help) {
        helpPage = std::clamp(helpPage + direction, 0, 3);
        requestUpdate();
        return;
      }
    }
  }
  if (interactionsReady && view == View::Picker) {
    const int held = sidebuttons::longStep(mappedInput);
    if (const int direction = held ? held : sidebuttons::shortStep(mappedInput)) {
      showPage(ui::stepPage(menuPage, pickerLayout.pageCount, direction));
      return;
    }
  }
  if (interactionsReady && view == View::Help) {
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      helpPage = std::clamp(helpPage + direction, 0, 3);
      requestUpdate();
      return;
    }
  }

  int holdX = 0, holdY = 0;
  if (view == View::Board && mappedInput.wasScreenLongPress(holdX, holdY)) {
    if (interactionsReady) routeBoardTap(holdX, holdY, ui::ModeMark);
    return;
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (sidebuttons::poll(interactions, mappedInput, tapX, tapY, !(view == View::Sizes || view == View::Picker))) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  int touchX = 0, touchY = 0;
  const bool touch = mappedInput.wasScreenTapped(touchX, touchY);
  // Holding always marks; a short tap uses the tool selected on screen.
  const int inputMode = ui::tapMode(mode, touch && mappedInput.tapWasHeldLong());
  const fui::ActionEvent action =
      sidebuttons::route(interactions, input, mappedInput, *this, !(view == View::Sizes || view == View::Picker));
  switch (action.action) {
    case ui::ActionBoard:
      if (view == View::Board) routeBoardTap(tapX, tapY, inputMode);
      break;
    case ui::ActionButton:
      routeButton(action.value);
      break;
    case ui::ActionMode:
      setMode(action.value);
      break;
    case ui::ActionPick: {
      if (view != View::Picker) break;
      // The grid registers one target and sends -1; the layout that drew the
      // tiles resolves the tap, so the region cannot drift from the pixels.
      // Only the -1 branch is geometry; a real value is already digest-gated.
      if (action.value < 0 && !surfaceRevealed()) break;
      const int picked = action.value >= 0 ? action.value : pickerLayout.indexAt(tapX, tapY);
      if (picross::isPlayable(picked)) {
        openPuzzle(picked);
      }
      break;
    }
    case ui::ActionHelpPage:
      if (view == View::Help) {
        {
          const int next = std::clamp(helpPage + action.value, 0, 3);
          if (next == helpPage) return;
          helpPage = next;
        }
        requestUpdate();
      }
      break;
    case ui::ActionApplySize:
      if (view == View::Sizes) {
        if (ui::groupFor(selected) != pendingSizeTab) selected = ui::groupFirst(pendingSizeTab);
        menuTab = pendingSizeTab;
        menuPage = 0;
        menuFollowsSelection = true;
        view = View::Menu;
        requestUpdate();
      }
      break;
    case ui::ActionTab:
      showTab(action.value);
      break;
    case ui::ActionPage:
      if (view != View::Picker || action.value == menuPage) break;
      showPage(action.value);
      break;
    default:
      break;
  }
}

void PicrossActivity::render(RenderLock&&) {
  if (view == View::Sizes || view == View::Picker) interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(
      renderer, view == View::Board || view == View::Won ? toybox::toyboxFaces() : toybox::proseMenuFaces());
  pixelui::bindFonts(target, renderer);
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);

  switch (view) {
    case View::Board: {
      ui::BoardModel model;
      model.board = &board;
      model.mode = mode;
      model.solvedCount = progress.solvedCount();
      model.total = picross::kPuzzleCount;
      ui::buildBoard(screen, model, layout);
      break;
    }
    case View::Won: {
      ui::WinModel model;
      model.cleared = &picross::kPuzzles[lastCleared];
      model.mistakes = lastMistakes;
      model.solvedCount = progress.solvedCount();
      model.total = picross::kPuzzleCount;
      model.moreToPlay = progress.solvedCount() < picross::kPuzzleCount;
      ui::buildWin(screen, model);
      break;
    }
    case View::Menu:
    case View::Picker:
    case View::Sizes:
    case View::Help:
    default: {
      ui::MenuModel model;
      model.progress = &progress;
      model.selectedIndex = selected;
      model.inProgressIndex = (board.touched() && !board.solved()) ? board.index() : -1;
      model.solvedCount = progress.solvedCount();
      model.total = picross::kPuzzleCount;
      model.hasProgress = model.inProgressIndex == selected && model.inProgressIndex >= 0;
      model.page = menuPage;
      model.sizeTab = menuTab;
      model.followSelection = menuFollowsSelection;
      switch (view) {
        case View::Menu:
          ui::buildMenu(screen, model, pickerLayout);
          break;
        case View::Picker:
          ui::buildPicker(screen, model, pickerLayout);
          break;
        case View::Sizes:
          model.sizeTab = pendingSizeTab;
          ui::buildSizes(screen, model);
          break;
        case View::Help:
          ui::buildHelp(screen, helpPage);
          break;
        case View::Board:
        case View::Won:
          break;
      }
      // The picker clamped whatever it was handed, and may have resolved the
      // tab and page from `selected` instead; keep our copies in step so the
      // dots, the tabs, the side keys and the next tap all agree with what was
      // drawn.
      if (view == View::Picker) {
        menuPage = pickerLayout.pageOnScreen;
        menuTab = pickerLayout.tabOnScreen;
        menuFollowsSelection = false;
      }
      break;
    }
  }

  interactionsReady = true;
  if (view == View::Picker) interactions.limitButtonActions({ui::ActionPick});
  toybox::reportOverflow(interactions, "Picross", &renderer);

  const auto labels = mappedInput.mapLabels(tr(STR_QUEST_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(flashOnNextPaint ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  flashOnNextPaint = false;
}

void PicrossActivity::touchSave() {
  if (saveReadOnly) return;
  if (unsaved < kSaveEvery) ++unsaved;
  if (unsaved < kSaveEvery) return;
  flushSave();
}

void PicrossActivity::flushSave() {
  if (unsaved == 0) return;
  if (saveState()) unsaved = 0;
}

bool PicrossActivity::saveState() {
  if (saveReadOnly) return false;
  picrosssave::encode(board, progress, saveBytes);
  const uint32_t expected = picrosssave::get(saveBytes + 164);
  {
    HalFile file;
    if (!Storage.openFileForWrite("PICR", "/.crosspoint/picross-v7.part", file) ||
        file.write(saveBytes, sizeof(saveBytes)) != sizeof(saveBytes)) {
      LOG_ERR("PICR", "Could not write save");
      return false;
    }
    file.flush();
  }
  {
    HalFile verify;
    if (!Storage.openFileForRead("PICR", "/.crosspoint/picross-v7.part", verify) ||
        verify.size() != sizeof(saveBytes) || verify.read(saveBytes, sizeof(saveBytes)) != sizeof(saveBytes) ||
        picrosssave::get(saveBytes + 164) != expected || picrosssave::checksum(saveBytes) != expected) {
      LOG_ERR("PICR", "Save verification failed");
      return false;
    }
  }
  if (!Storage.replaceFile("/.crosspoint/picross-v7.part", kSavePath)) {
    LOG_ERR("PICR", "Could not publish save");
    return false;
  }
  return true;
}
bool PicrossActivity::loadState() {
  const bool canonical = Storage.exists(kSavePath);
  const char* paths[] = {kSavePath, "/.crosspoint/picross-v7.part", kLegacyPath};
  for (int candidate = 0; candidate < 3; ++candidate) {
    if (canonical && candidate > 0) break;
    const char* path = paths[candidate];
    if (!Storage.exists(path)) continue;
    HalFile file;
    if (!Storage.openFileForRead("PICR", path, file)) continue;
    uint8_t version = 0;
    if (file.read(&version, 1) != 1) continue;
    if (version > picrosssave::kVersion) {
      saveReadOnly = true;
      return false;
    }
    if (candidate == 1 && version != picrosssave::kVersion) continue;
    const size_t expected = version == 6 ? picrosssave::kLegacyBytes : picrosssave::kBytes;
    if (file.size() != expected || !file.seek(0) || file.read(saveBytes, expected) != static_cast<int>(expected) ||
        !picrosssave::decode(saveBytes, expected, board, progress)) {
      LOG_ERR("PICR", "Unsupported or damaged save version %d", version);
      continue;
    }
    if (candidate == 1) unsaved = 1;
    return true;
  }
  return false;
}

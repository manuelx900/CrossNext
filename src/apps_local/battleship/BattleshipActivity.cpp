#include "BattleshipActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"
#include "BattleshipLabels.h"
#include "components/SideButtonNavigation.h"

namespace {

constexpr char kSavePath[] = "/.crosspoint/bship.sav";
constexpr char kStatsPath[] = "/.crosspoint/bship.cfg";
constexpr int kSaveVersion = 1;

char hexDigit(const int value) { return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10); }

int hexValue(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

}  // namespace

std::unique_ptr<Activity> BattleshipActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<BattleshipActivity>(renderer, mappedInput);
}

// --- lifecycle --------------------------------------------------------------

void BattleshipActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  // Mixed from the clock, so two games in a row are not the same game. The
  // fleet you are given is the first thing you see, and a deterministic one
  // would be noticed by the second session.
  seed = toybox::seed();
  loadStats();
  hasSavedGame = loadGame();
  goToMenu();
}

void BattleshipActivity::onExit() {
  // Runs on sleep and on an app switch as well as on the way out, which is why
  // this is the door that matters: you press nothing to fall asleep.
  saveGame();
  Activity::onExit();
}

// --- geometry ---------------------------------------------------------------

BattleshipActivity::GridGeometry BattleshipActivity::targetGrid() const {
  return {layout.target.x, layout.target.y, layout.target.width / bship::kSize};
}
BattleshipActivity::GridGeometry BattleshipActivity::placeGrid() const {
  return {layout.place.x, layout.place.y, layout.place.width / bship::kSize};
}

int BattleshipActivity::cellAt(const GridGeometry& grid, const int x, const int y) {
  if (grid.cell <= 0) return -1;
  const int col = (x - grid.originX) / grid.cell;
  const int row = (y - grid.originY) / grid.cell;
  if (x < grid.originX || y < grid.originY) return -1;
  if (col < 0 || col >= bship::kSize || row < 0 || row >= bship::kSize) return -1;
  return bship::cellOf(row, col);
}

bshipui::StartModel BattleshipActivity::startModel() const {
  bshipui::StartModel model;
  model.hasSavedGame = hasSavedGame;
  model.played = played;
  model.won = won;
  model.streak = streak;
  model.selected = startIndex;
  model.fleet = hasSavedGame ? &game.side[mySide].fleet : nullptr;
  return model;
}

bshipui::PlaceModel BattleshipActivity::placeModel() const {
  bshipui::PlaceModel model;
  model.status = report;
  // Once the fleet is handed over there is nothing left to press, and the
  // buttons dim rather than vanish: a control that disappears takes its space
  // with it and the grid jumps.
  model.canEdit = !fleetCommitted;
  model.fleet = &myFleet;
  model.selectedShip = selectedShip;
  return model;
}

const char* BattleshipActivity::themWord() {
  // Naming them matters: you learn who you are playing on the searching screen
  // and would otherwise spend the whole match being told only that somebody
  // else had shot at you.
  //
  // Their first word, though. Every use of this is inside a sentence -- "%s
  // SANK YOUR %s", "WAITING FOR %s" -- and a full name is three words now, so
  // the sentence would be the part that got cut. See player::shortName.
  player::shortName(nullptr, them, sizeof(them));
  return them[0] != '\0' ? them : tr(STR_UI_THEY_723524);
}

const char* BattleshipActivity::capsuleLabel() {
  if (gameOver()) {
    std::snprintf(capsule, sizeof(capsule), "%s", tr(STR_WORDLE_PLAY_AGAIN));
    return capsule;
  }
  if (!myTurn()) {
    std::snprintf(capsule, sizeof(capsule), tr(STR_UI_INCOMING_21130F));
    return capsule;
  }
  if (aimCell < 0) {
    std::snprintf(capsule, sizeof(capsule), tr(STR_UI_TAP_A_TARGET_0B935C));
    return capsule;
  }
  char where[4] = {};
  bship::cellName(aimCell, where);
  std::snprintf(capsule, sizeof(capsule), tr(STR_UI_FIRE_AT_S_3F134A), where);
  return capsule;
}

bshipui::BoardModel BattleshipActivity::boardModel() {
  bshipui::BoardModel model;
  model.report = report;
  model.status = capsuleLabel();
  model.canFire = myTurn() && aimCell >= 0 && !gameOver();
  model.gameOver = gameOver();
  model.game = &game;
  model.mySide = mySide;
  model.aim = aimCell;
  model.myTurn = myTurn();
  model.ownFleet = showOwnFleet;
  return model;
}

void BattleshipActivity::drawStartMenu() {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  target.setFont(4, toybox::kLibraryFontId);
  target.setFont(3, toybox::kTileFontId);
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  bshipui::buildStartMenu(screen, startModel());
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Battleship start", &renderer);
}

void BattleshipActivity::drawPlaceScreen() {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  target.setFont(4, toybox::kLibraryFontId);
  target.setFont(3, toybox::kTileFontId);
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  bshipui::buildPlaceChrome(screen, placeModel(), &layout);
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Battleship placement", &renderer);
}

void BattleshipActivity::drawBoardScreen() {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  target.setFont(4, toybox::kLibraryFontId);
  target.setFont(3, toybox::kTileFontId);
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  if (view == View::Result)
    bshipui::buildResult(screen, boardModel());
  else
    bshipui::buildBoardChrome(screen, boardModel(), &layout);

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Battleship board", &renderer);
}

void BattleshipActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;
  switch (view) {
    case View::Help: {
      renderer.clearScreen();
      auto target = toybox::makeTarget(renderer);
      pixelui::bindFonts(target, renderer);
      target.setFont(4, toybox::kLibraryFontId);
      target.setFont(3, toybox::kTileFontId);
      const fui::InputSnapshot input{};
      toybox::Frame frame(target, target.deviceContext(), input, interactions, nullptr, toybox::headerStatus(renderer));
      toybox::Screen screen(frame);
      bshipui::buildHowTo(screen, helpPage);
      interactionsReady = true;
      break;
    }
    case View::Menu:
      drawStartMenu();
      break;
    case View::Place:
      drawPlaceScreen();
      break;
    case View::Result:
    case View::Board:
      drawBoardScreen();
      break;
  }
  renderer.displayBuffer();
}

// --- flow -------------------------------------------------------------------

void BattleshipActivity::goToMenu() {
  view = View::Menu;
  startIndex = 0;
  requestUpdate();
}

void BattleshipActivity::beginPlacement() {
  bship::reset(game);
  mySide = 0;
  fleetCommitted = false;
  selectedShip = 0;
  aimCell = -1;
  computerThinking = false;
  bship::randomFleet(myFleet, seed);
  std::snprintf(report, sizeof(report), tr(STR_UI_TAP_A_SHIP_TO_MOVE_IT_8AD98C));
  view = View::Place;
  requestUpdate();
}

void BattleshipActivity::startNewGame() {
  Storage.remove(kSavePath);
  hasSavedGame = false;
  beginPlacement();
}

void BattleshipActivity::activateStartRow(const bshipui::StartRow row) {
  switch (row) {
    case bshipui::StartRow::HowToPlay:
      view = View::Help;
      helpPage = 0;
      break;
    case bshipui::StartRow::Continue:
      // Through the one door, or the board opens with the narration line blank
      // -- which is the defect the screenshots caught on a fresh game and which
      // came straight back on the one door that is not a fresh game.
      enterBoardView();
      break;
    case bshipui::StartRow::NewGame:
      startNewGame();
      return;
    default:
      break;
  }
  requestUpdate();
}

void BattleshipActivity::routeStartMenu() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // See src/apps_local/Shelf.h: no app names its own destination.
    shelf::leave(renderer, mappedInput);
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

  const fui::ActionEvent event = sidebuttons::route(interactions, input, mappedInput, *this);
  if (event.action == bshipui::ActionStartRow) {
    startIndex = event.value;
    activateStartRow(bshipui::startRowAt(startModel(), event.value));
  }
}

// --- setting up -------------------------------------------------------------

void BattleshipActivity::shuffleFleet() {
  bship::randomFleet(myFleet, seed);
  selectedShip = 0;
  std::snprintf(report, sizeof(report), tr(STR_UI_TAP_A_SHIP_TO_MOVE_IT_8AD98C));
  requestUpdate();
}

void BattleshipActivity::selectShip(const int shipIndex) {
  selectedShip = shipIndex;
  std::snprintf(report, sizeof(report), tr(STR_UI_TAP_A_CELL_OR_THE_SHIP_72896B));
  requestUpdate();
}

void BattleshipActivity::rotateSelected() {
  if (selectedShip < 0) return;
  const bship::Ship current = myFleet.ships[selectedShip];
  bship::Ship candidate = current;
  candidate.horizontal = static_cast<uint8_t>(current.horizontal != 0 ? 0 : 1);
  if (bship::canPlace(myFleet, selectedShip, candidate)) {
    myFleet.ships[selectedShip] = candidate;
    requestUpdate();
    return;
  }
  // Turning at the edge would run the ship off the board. Walk the bow back
  // along the new axis rather than refusing, because a ship that will not turn
  // near a wall is the fiddliest part of every version of this game.
  const int length = bship::kShipLength[selectedShip];
  for (int back = 1; back < length; ++back) {
    bship::Ship shifted = candidate;
    const int row = bship::rowOf(current.bow) - (candidate.horizontal != 0 ? 0 : back);
    const int col = bship::colOf(current.bow) - (candidate.horizontal != 0 ? back : 0);
    if (row < 0 || col < 0) break;
    shifted.bow = static_cast<uint8_t>(bship::cellOf(row, col));
    if (!bship::canPlace(myFleet, selectedShip, shifted)) continue;
    myFleet.ships[selectedShip] = shifted;
    requestUpdate();
    return;
  }
  std::snprintf(report, sizeof(report), tr(STR_UI_IT_WILL_NOT_TURN_THERE_77BFB6));
  requestUpdate();
}

void BattleshipActivity::moveSelected(const int cell) {
  if (selectedShip < 0) return;
  bship::Ship candidate = myFleet.ships[selectedShip];
  candidate.bow = static_cast<uint8_t>(cell);
  if (!bship::canPlace(myFleet, selectedShip, candidate)) {
    std::snprintf(report, sizeof(report), tr(STR_UI_IT_WILL_NOT_FIT_THERE_E2B0B0));
    requestUpdate();
    return;
  }
  myFleet.ships[selectedShip] = candidate;
  std::snprintf(report, sizeof(report), tr(STR_UI_TAP_A_CELL_OR_IT_AGAIN_F0B654));
  requestUpdate();
}

void BattleshipActivity::handlePlaceTap(const int cell) {
  const int ship = bship::shipAt(myFleet, cell);
  if (ship >= 0) {
    if (ship == selectedShip) {
      rotateSelected();
      return;
    }
    selectShip(ship);
    return;
  }
  if (selectedShip < 0) {
    std::snprintf(report, sizeof(report), tr(STR_UI_TAP_A_SHIP_TO_MOVE_IT_8AD98C));
    requestUpdate();
    return;
  }
  // The tapped cell is where the bow goes; the ship runs right or down from it,
  // which is the direction its outline already shows.
  moveSelected(cell);
}

void BattleshipActivity::enterBoardView() {
  if (view == View::Board) return;
  view = View::Board;
  showOwnFleet = false;
  aimCell = -1;
  // Never inherit the line the previous screen was using. In a match this said
  // WAITING FOR MARIO for as long as it took somebody to fire, which is exactly
  // the state that had just ended.

  std::snprintf(report, sizeof(report), "%s", tr(STR_UI_YOUR_MOVE_AF6C56));
}

void BattleshipActivity::commitFleet() {
  if (fleetCommitted) return;
  fleetCommitted = true;
  selectedShip = -1;

  if (!bship::place(game, mySide, myFleet)) {
    LOG_ERR("BSHIP", "the fleet on screen was refused by the rules");
    return;
  }
  // Where your fleet ended up, so a drawing question ("is the small grid
  // showing the same board as the big one?") is answered by the log rather than
  // by counting dithered squares in a screenshot.
  for (int i = 0; i < bship::kShipCount; ++i) {
    const bship::Ship& ship = myFleet.ships[i];
    LOG_INF("BSHIP", "fleet %s at row %d col %d %s", battleshiplabels::shipName(i), bship::rowOf(ship.bow),
            bship::colOf(ship.bow), ship.horizontal != 0 ? tr(STR_UI_ACROSS_9A82BF) : tr(STR_UI_DOWN_77346D));
  }

  // Single player: the computer sets up in the same instant, because there is
  // nobody to wait for. In a match this is where the other device's fleet is
  // still on its way, and the wait is the screen's job rather than a branch.
  bship::Fleet theirs;
  bship::randomFleet(theirs, seed);
  if (!bship::place(game, opponentSide(), theirs)) {
    LOG_ERR("BSHIP", "the computer could not place its fleet");
    return;
  }
  // The line under the rule is never empty on this screen: an empty band at the
  // top reads as something that failed to draw.
  enterBoardView();
  saveGame();
  requestUpdate();
}

// `view` is not a screen label here, it is the grid arithmetic: placeGrid()
// and targetGrid() share an origin but not a cell size, because the target
// grid reserves the fleet band. Every pixel is a different cell on a different
// board across that switch -- and the switch is fired by the NETWORK, in
// takeOpponentState(), not by anything this player touched.
//
// The rest is the live/dead bit: whether a shot can be taken at all, which the
// opponent's arriving shot and the computer's turn both flip with no tap here.
//
// Absent on purpose: aimCell and selectedShip. Aim-then-fire is two taps on
// the same cell and the aim repaints between them, so hashing aimCell would
// drop the confirming tap -- the primary firing gesture. Arranging a fleet is
// fifteen consecutive taps with a repaint after each. Neither moves which cell
// a pixel is; both are the local, deliberate kind of change, not the uniform
// kind that reinterprets the whole surface under a resting finger.
uint32_t BattleshipActivity::surfaceMeaning() const {
  uint32_t meaning = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(view));
  meaning = paintclock::mixMeaning(meaning, fleetCommitted ? 1u : 0u);
  meaning = paintclock::mixMeaning(meaning, computerThinking ? 1u : 0u);
  meaning = paintclock::mixMeaning(meaning, showOwnFleet ? 1u : 0u);
  return paintclock::mixMeaning(meaning, (myTurn() && !gameOver()) ? 1u : 0u);
}

void BattleshipActivity::routePlacement() {
  if (interactionsReady && !fleetCommitted) {
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      selectShip((selectedShip + direction + bship::kShipCount) % bship::kShipCount);
      return;
    }
  }
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    goToMenu();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!sidebuttons::poll(interactions, mappedInput, tapX, tapY)) return;

  if (interactionsReady) {
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
    const fui::ActionEvent event = sidebuttons::route(interactions, input, mappedInput, *this);
    if (event.action == bshipui::ActionShuffle) {
      shuffleFleet();
      return;
    }
    if (event.action == bshipui::ActionRotate) {
      if (!fleetCommitted) rotateSelected();
      return;
    }
    if (event.action == bshipui::ActionSelectShip) {
      if (!fleetCommitted && event.value >= 0 && event.value < bship::kShipCount) selectShip(event.value);
      return;
    }
    if (event.action == bshipui::ActionReady) {
      commitFleet();
      return;
    }
  }

  // The grid is hit-tested against geometry and never reaches route().
  if (!surfaceRevealed()) return;
  const int cell = cellAt(placeGrid(), tapX, tapY);
  if (cell >= 0) {
    handlePlaceTap(cell);
    return;
  }
}

// --- playing ----------------------------------------------------------------

void BattleshipActivity::reportShot(const bool theirs) {
  if (game.lastShot == 0) {
    report[0] = '\0';
    return;
  }
  char where[4] = {};
  bship::cellName(game.lastShot - 1, where);
  const int sank = bship::lastShotSank(game);
  const bool hit = bship::lastShotHit(game);
  if (theirs) {
    if (sank >= 0) {
      std::snprintf(report, sizeof(report), tr(STR_UI_S_SANK_YOUR_S_09A635), themWord(),
                    battleshiplabels::shipName(sank));
      return;
    }
    std::snprintf(report, sizeof(report), hit ? tr(STR_UI_S_HIT_YOU_AT_S_9B0D7A) : tr(STR_UI_S_MISSED_AT_S_3D42A9),
                  themWord(), where);
    return;
  }
  if (sank >= 0) {
    std::snprintf(report, sizeof(report), tr(STR_UI_YOU_SANK_THEIR_S_361EE1), battleshiplabels::shipName(sank));
    return;
  }
  std::snprintf(report, sizeof(report), hit ? tr(STR_UI_S_HIT_0B7E53) : tr(STR_UI_S_MISS_1DBD94), where);
}

void BattleshipActivity::handleTargetTap(const int cell) {
  if (gameOver() || !myTurn()) return;
  if (bship::shotAt(game.side[opponentSide()], cell)) {
    std::snprintf(report, sizeof(report), tr(STR_UI_ALREADY_FIRED_THERE_253B10));
    requestUpdate();
    return;
  }
  if (cell == aimCell) {
    // A second tap on the cell you are aiming at is the same action as the
    // capsule, so both go through one function.
    fireAtAimed();
    return;
  }
  aimCell = cell;
  requestUpdate();
}

void BattleshipActivity::fireAtAimed() {
  if (gameOver() || !myTurn() || aimCell < 0) return;
  if (!bship::fire(game, aimCell)) {
    LOG_ERR("BSHIP", "the rules refused a shot the board offered");
    return;
  }
  reportShot(false);
  aimCell = -1;

  if (gameOver()) {
    finishGame();
    requestUpdate();
    return;
  }
  // Deferred a pass, so the repaint showing your shot lands before the gunner
  // thinks. It is instant either way, but the order is what makes it read as
  // your shot and then theirs rather than as both at once.
  computerThinking = true;
  requestUpdate();
}

void BattleshipActivity::playComputerShot() {
  computerThinking = false;
  const int cell = bship::chooseShot(game.side[mySide], seed);
  if (cell < 0 || !bship::fire(game, cell)) {
    LOG_ERR("BSHIP", "the gunner had nowhere left to shoot");
    return;
  }
  reportShot(true);
  if (gameOver()) finishGame();
  saveGame();
  requestUpdate();
}

void BattleshipActivity::finishGame() {
  ++played;
  const bool mine = bship::winner(game) == mySide;
  if (mine) {
    ++won;
    ++streak;
  } else {
    streak = 0;
  }
  // The result replaces the last shot's narration. Both are worth saying and
  // only one of them is worth saying twice.
  const int loser = mine ? opponentSide() : mySide;
  std::snprintf(report, sizeof(report),
                mine ? tr(STR_UI_YOU_WIN_IN_D_SHOTS_76D00A) : tr(STR_UI_THEY_WIN_IN_D_SHOTS_3F6520),
                bship::shotsTaken(game.side[loser]));
  saveStats();
  saveGame();
  hasSavedGame = false;
  computerThinking = false;
  view = View::Result;
  interactionsReady = false;
}

void BattleshipActivity::routeBoard() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    saveGame();
    hasSavedGame = !gameOver();
    goToMenu();
    return;
  }

  if (computerThinking) {
    playComputerShot();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!sidebuttons::poll(interactions, mappedInput, tapX, tapY, false)) return;

  if (interactionsReady) {
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
    const fui::ActionEvent event = sidebuttons::route(interactions, input, mappedInput, *this, false);
    if (event.action == bshipui::ActionFire) {
      fireAtAimed();
      return;
    }
    if (event.action == bshipui::ActionPlayAgain) {
      requestNewGame();
      return;
    }
    if (event.action == bshipui::ActionFleet || event.action == bshipui::ActionOwnBoard) {
      showOwnFleet = event.value == 0;
      interactionsReady = false;
      requestUpdate();
      return;
    }
    if (event.action == bshipui::ActionResultReview) {
      view = View::Board;
      showOwnFleet = false;
      interactionsReady = false;
      requestUpdate();
      return;
    }
    if (event.action == bshipui::ActionResultMenu) {
      goToMenu();
      return;
    }
  }

  // The grid is hit-tested against geometry and never reaches route().
  if (!surfaceRevealed() || showOwnFleet || view != View::Board) return;
  const int cell = cellAt(targetGrid(), tapX, tapY);
  if (cell >= 0) handleTargetTap(cell);
}

void BattleshipActivity::loop() {
  namespace fui = freeink::ui;
  switch (view) {
    case View::Help: {
      if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        goToMenu();
        return;
      }
      fui::InputSnapshot input;
      int x = 0, y = 0;
      if (sidebuttons::poll(interactions, mappedInput, x, y)) {
        input.touchReleased = true;
        input.touchX = x;
        input.touchY = y;
      }
      if ((!interactions.hasButtonFocus() && mappedInput.wasReleased(MappedInputManager::Button::Confirm)))
        input.confirm = true;
      if (sidebuttons::route(interactions, input, mappedInput, *this).action == bshipui::ActionHelpNext) {
        goToMenu();
      }
      return;
    }
    case View::Menu:
      routeStartMenu();
      return;
    case View::Place:
      routePlacement();
      return;
    case View::Result:
    case View::Board:
      routeBoard();
      return;
  }
}

void BattleshipActivity::requestNewGame() { startNewGame(); }

// --- persistence ------------------------------------------------------------

void BattleshipActivity::saveGame() const {
  // Nothing to keep before both fleets are down: a half-arranged board is not a
  // game, and restoring one would drop the player into a setup they thought
  // they had left.
  if (!bship::bothPlaced(game)) return;

  // 50 bytes of state as hex, on one line, after the version. Small enough to
  // build in a stack buffer and readable enough to diff when something is off.
  char line[8 + 2 * sizeof(bship::Game)] = {};
  int at = std::snprintf(line, sizeof(line), "%d %d ", kSaveVersion, static_cast<int>(mySide));
  const auto* bytes = reinterpret_cast<const uint8_t*>(&game);
  for (size_t i = 0; i < sizeof(bship::Game) && at + 2 < static_cast<int>(sizeof(line)); ++i) {
    line[at++] = hexDigit(bytes[i] >> 4);
    line[at++] = hexDigit(bytes[i] & 0x0F);
  }
  line[at] = '\0';
  Storage.writeFile(kSavePath, String(line));
}

bool BattleshipActivity::loadGame() {
  if (!Storage.exists(kSavePath)) return false;
  char buffer[8 + 2 * sizeof(bship::Game) + 4] = {};
  if (Storage.readFileToBuffer(kSavePath, buffer, sizeof(buffer)) == 0) return false;

  int version = 0;
  int side = 0;
  int consumed = 0;
  if (std::sscanf(buffer, "%d %d %n", &version, &side, &consumed) < 2) return false;
  if (version != kSaveVersion) {
    LOG_INF("BSHIP", "Ignoring save with unknown version");
    return false;
  }
  const char* hex = buffer + consumed;
  if (std::strlen(hex) < 2 * sizeof(bship::Game)) {
    LOG_ERR("BSHIP", "Corrupt save, starting fresh");
    return false;
  }

  bship::Game restored;
  auto* bytes = reinterpret_cast<uint8_t*>(&restored);
  for (size_t i = 0; i < sizeof(bship::Game); ++i) {
    const int high = hexValue(hex[2 * i]);
    const int low = hexValue(hex[2 * i + 1]);
    if (high < 0 || low < 0) {
      LOG_ERR("BSHIP", "Corrupt save, starting fresh");
      return false;
    }
    bytes[i] = static_cast<uint8_t>((high << 4) | low);
  }
  // A save whose fleets do not obey the rules is a save from a different build,
  // and adopting it would put an illegal board on screen rather than fail.
  for (int s = 0; s < 2; ++s) {
    if (restored.side[s].placed == 0) return false;
    for (int i = 0; i < bship::kShipCount; ++i) {
      if (!bship::canPlace(restored.side[s].fleet, i, restored.side[s].fleet.ships[i])) {
        LOG_ERR("BSHIP", "Save holds an illegal fleet, starting fresh");
        return false;
      }
    }
  }

  game = restored;
  mySide = static_cast<uint8_t>(side & 1);
  fleetCommitted = true;
  aimCell = -1;
  // A game saved while the gunner still owed a shot resumes with it owing one.
  // Back is checked before the deferred shot runs, so pressing it in that one
  // pass used to save a board whose turn belonged to a computer that would
  // never move again: the resumed game refused every tap and there was nothing
  // on screen to say why.
  computerThinking = !bship::over(restored) && (restored.turn & 1) != (side & 1);
  report[0] = '\0';
  LOG_INF("BSHIP", "Resumed a game, %d shots fired", bship::shotsTaken(game.side[opponentSide()]));
  return !bship::over(game);
}

void BattleshipActivity::saveStats() const {
  // "%d %d %d %d\n"
  constexpr int kLineChars =
      toybox::kIntChars + toybox::kIntChars + toybox::kIntChars + toybox::kIntChars + toybox::literalChars("   \n") + 1;
  char line[kLineChars];
  std::snprintf(line, sizeof(line), "%d %d %d %d\n", kSaveVersion, played, won, streak);
  Storage.writeFile(kStatsPath, String(line));
}

void BattleshipActivity::loadStats() {
  if (!Storage.exists(kStatsPath)) return;
  char buffer[48] = {};
  if (Storage.readFileToBuffer(kStatsPath, buffer, sizeof(buffer)) == 0) return;
  int version = 0;
  int storedPlayed = 0;
  int storedWon = 0;
  int storedStreak = 0;
  if (std::sscanf(buffer, "%d %d %d %d", &version, &storedPlayed, &storedWon, &storedStreak) < 4) return;
  if (version != kSaveVersion) return;
  played = storedPlayed;
  won = storedWon;
  streak = storedStreak;
}

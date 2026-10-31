#include "ConnectFourActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdlib>
#include <cstring>

#include "../Shelf.h"
#include "../ui/MatchSettings.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "ConnectFourBrain.h"
#include "ConnectFourScreens.h"
#include "components/SideButtonNavigation.h"

namespace c4 = connectfour;

std::unique_ptr<Activity> ConnectFourActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ConnectFourActivity>(renderer, mappedInput);
}

void ConnectFourActivity::savePreferences() {
  if (savedLocal == preferredLocal && savedSeat == preferredSeat && savedLevel == preferredLevel) return;
  const uint8_t bytes[] = {2, static_cast<uint8_t>(preferredLocal), preferredSeat, preferredLevel};
  HalFile file;
  if (!Storage.openFileForWrite("C4", "/.crosspoint/connectfour.cfg", file) ||
      file.write(bytes, sizeof(bytes)) != sizeof(bytes))
    LOG_ERR("C4", "Could not save preferences");
  else {
    savedLocal = preferredLocal;
    savedSeat = preferredSeat;
    savedLevel = preferredLevel;
  }
}
void ConnectFourActivity::onEnter() {
  {
    uint8_t bytes[4]{};
    HalFile file;
    if (Storage.openFileForRead("C4", "/.crosspoint/connectfour.cfg", file)) {
      const int count = file.read(bytes, sizeof(bytes));
      const bool validVersion = (bytes[0] == 1 && count == 3) || (bytes[0] == 2 && count == 4 && bytes[3] < 3);
      if (validVersion && bytes[1] < 2 && (bytes[2] == c4::kLight || bytes[2] == c4::kDark)) {
        preferredLocal = savedLocal = bytes[1];
        preferredSeat = savedSeat = bytes[2];
        preferredLevel = savedLevel = bytes[0] == 2 ? bytes[3] : 1;
      }
    }
  }
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  screen = c4::Screen::Menu;
  menuSelected = -1;
  loadHistory();
  requestUpdate();
}

// Beside the reader's own state and the player's name. A fork-local fact in a
// fork-local file, the pattern knucklebones.sav set.
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kHistoryPath[] = "/.crosspoint/connectfour.sav";
#endif

// wins losses draws outcome, the four winning cells as flat indices (-1s on a
// draw), then the 42 cells column-major bottom-up -- colours normalised so
// light is always the seat this device played.
constexpr int kHistoryValues = 4 + c4::kLine + c4::kCells;

void ConnectFourActivity::loadHistory() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.exists(kHistoryPath)) return;
  char buffer[256] = {};
  if (Storage.readFileToBuffer(kHistoryPath, buffer, sizeof(buffer)) == 0) return;

  // Parsed into locals and committed only if the whole line is good, so a
  // truncated file leaves an empty menu rather than half a board. Losing this
  // costs an ornament, so it fails quietly.
  int values[kHistoryValues] = {};
  char* cursor = buffer;
  for (int i = 0; i < kHistoryValues; ++i) {
    char* next = nullptr;
    const long value = strtol(cursor, &next, 10);
    if (next == cursor) return;
    values[i] = static_cast<int>(value);
    cursor = next;
  }

  int at = 0;
  wins = values[at++];
  losses = values[at++];
  draws = values[at++];
  lastOutcome = values[at++];
  for (int i = 0; i < c4::kLine; ++i) lastLine[i] = values[at++];
  for (int i = 0; i < c4::kCells; ++i) lastCells[i] = static_cast<uint8_t>(values[at++]);
  char* end = nullptr;
  const long savedStreak = strtol(cursor, &end, 10);
  streak = end != cursor && savedStreak >= 0 && savedStreak <= wins ? static_cast<int>(savedStreak) : 0;
  hasHistory = true;
#endif
}

void ConnectFourActivity::recordResult() {
  if (local) {
    resultRecorded = true;
    return;
  }
  if (resultRecorded) return;
  resultRecorded = true;

  const bool won = (seat == c4::kLight && game.outcome == c4::Outcome::LightWins) ||
                   (seat == c4::kDark && game.outcome == c4::Outcome::DarkWins);
  lastOutcome = 2;
  if (game.outcome != c4::Outcome::Draw) lastOutcome = won ? 0 : 1;
  if (game.outcome == c4::Outcome::Draw)
    ++draws;
  else if (won)
    ++wins;
  else
    ++losses;

  streak = won ? streak + 1 : 0;

  // Colours normalised so light is always this device's seat. The board never
  // rotates in Connect Four -- gravity reads the same from both chairs -- so a
  // colour swap is the whole normalisation.
  for (int column = 0; column < c4::kColumns; ++column) {
    for (int row = 0; row < c4::kRows; ++row) {
      uint8_t cell = game.cell[column][row];
      if (seat == c4::kDark && cell != c4::kEmpty) cell = c4::other(cell);
      lastCells[column * c4::kRows + row] = cell;
    }
  }
  for (int i = 0; i < c4::kLine; ++i) lastLine[i] = -1;
  if (game.outcome == c4::Outcome::LightWins)
    c4::winningLine(game, c4::kLight, lastLine);
  else if (game.outcome == c4::Outcome::DarkWins)
    c4::winningLine(game, c4::kDark, lastLine);
  hasHistory = true;

#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  char line[256];
  int used = snprintf(line, sizeof(line), "%d %d %d %d %d %d %d %d", wins, losses, draws, lastOutcome, lastLine[0],
                      lastLine[1], lastLine[2], lastLine[3]);
  for (int i = 0; i < c4::kCells && used > 0 && used < static_cast<int>(sizeof(line)); ++i) {
    used += snprintf(line + used, sizeof(line) - used, " %d", lastCells[i]);
  }
  if (used <= 0 || used >= static_cast<int>(sizeof(line))) {
    LOG_ERR("C4", "History line did not fit %d bytes", static_cast<int>(sizeof(line)));
    return;
  }
  const int tail = snprintf(line + used, sizeof(line) - used, " %d\n", streak);
  if (tail < 0 || tail >= static_cast<int>(sizeof(line)) - used) {
    LOG_ERR("C4", "History streak did not fit");
    return;
  }
  Storage.writeFile(kHistoryPath, String(line));
#endif
}

void ConnectFourActivity::goTo(const c4::Screen next) {
  screen = next;
  interactionsReady = false;
  requestUpdate();
}

void ConnectFourActivity::beginSoloGame() {
  c4::start(game);
  seat = preferredSeat;
  local = preferredLocal;
  selectedColumn = 3;
  resultRecorded = false;
  goTo(c4::Screen::Board);
}

void ConnectFourActivity::takeOpponentTurn() {
  // One drop per pass, not a loop to the end. The panel takes half a second to
  // repaint, so playing several of their moves before showing anything would
  // look like a freeze followed by a skip.
  if (local || c4::over(game) || game.turn == seat) return;
  const int column = c4::chooseColumn(game, preferredLevel);
  if (column == c4::kNoColumn) return;
  c4::drop(game, column);
  requestUpdate();
}

uint32_t ConnectFourActivity::surfaceMeaning() const {
  const auto withScreen = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(screen));
  return paintclock::mixMeaning(withScreen, static_cast<uint32_t>(c4::phaseFor(c4::over(game), game.turn == seat)));
}

void ConnectFourActivity::loop() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (c4::leavesApp(screen)) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (screen == c4::Screen::Settings) savePreferences();
    goTo(c4::back(screen));
    return;
  }

  // HOW TO PLAY pages on the two side keys. They are the device's only physical
  // buttons and the case labels them previous and next page; a page of a
  // how-to is a page. NEXT stays tappable -- a button is never the only route.
  if (screen == c4::Screen::HowTo) {
    const int swipe = sidebuttons::swipeStep(mappedInput);
    const int direction = swipe ? swipe : sidebuttons::shortStep(mappedInput);
    if (direction) {
      const int next = std::clamp(howToPage + direction, 0, c4ui::howToPages() - 1);
      if (next != howToPage) {
        howToPage = next;
        requestUpdate();
      }
      return;
    }
  }

  if (screen == c4::Screen::Board) {
    if (!local && !c4::over(game) && game.turn != seat) {
      takeOpponentTurn();
      return;
    } else if (c4::over(game)) {
      recordResult();
      goTo(c4::Screen::Result);
      return;
    }
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (sidebuttons::poll(interactions, mappedInput, tapX, tapY,
                        screen != c4::Screen::Board && screen != c4::Screen::Settings)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  // Forty-two cells and seven slots against a twenty-four slot interaction
  // buffer, so the board is hit-tested from the geometry that drew it. Tried
  // before the registered controls, because it covers most of the screen.
  if (screen == c4::Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    const int column = c4ui::columnAt(device, tapX, tapY);
    if (column != c4::kNoColumn) {
      if (!surfaceRevealed()) return;
      const bool mine = local || game.turn == seat;
      if (!mine || c4::over(game)) return;
      // A full column changes nothing, so it must not repaint. A refresh that
      // leaves the screen identical is worse than doing nothing: the panel
      // visibly blinks and comes back the same, which is what a bug looks like.
      if (!c4::drop(game, column)) return;
      selectedColumn = column;

      requestUpdate();
      return;
    }
  }

  const fui::ActionEvent event = sidebuttons::route(interactions, input, mappedInput, *this,
                                                    screen != c4::Screen::Board && screen != c4::Screen::Settings);
  switch (event.action) {
    case matchsettings::Opponent:
      preferredLocal = event.value == 1;
      requestUpdate();
      return;
    case matchsettings::Difficulty:
      preferredLevel = static_cast<uint8_t>(std::clamp(int(event.value), 0, 2));
      requestUpdate();
      return;
    case matchsettings::Colour:
      preferredSeat = event.value ? c4::kDark : c4::kLight;
      requestUpdate();
      return;
    case matchsettings::Done:
      savePreferences();
      goTo(c4::Screen::Menu);
      return;
    case c4ui::ActionMenuRow:
      switch (static_cast<c4ui::MenuRow>(event.value)) {
        case c4ui::MenuRow::Play:
          beginSoloGame();
          return;
        case c4ui::MenuRow::HowTo:
          howToPage = 0;
          goTo(c4::Screen::HowTo);
          return;
        case c4ui::MenuRow::Settings:
          goTo(c4::Screen::Settings);
          return;
        case c4ui::MenuRow::Count:
          return;
      }
      return;

    case c4ui::ActionHowToNext:
      if (howToPage + 1 < c4ui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(c4::Screen::Menu);
      return;

    case c4ui::ActionHowToPrevious:
      if (howToPage > 0) {
        --howToPage;
        requestUpdate();
      }
      return;

    case c4ui::ActionViewBoard:
      if (c4::over(game)) goTo(c4::Screen::Review);
      return;

    case c4ui::ActionReturnResult:
      goTo(c4::Screen::Result);
      return;

    case c4ui::ActionAgain:

      beginSoloGame();
      return;

    case c4ui::ActionDone:

      goTo(c4::Screen::Menu);
      return;

    default:
      return;
  }
}

void ConnectFourActivity::render(RenderLock&&) {
  if (screen == c4::Screen::Settings) interactions.setFocusedIndex(-1);
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
    case c4::Screen::Settings:
      matchsettings::build(
          surface, {matchsettings::Game::ConnectFour, preferredLocal, preferredSeat == c4::kDark, preferredLevel});
      break;
    case c4::Screen::Menu: {
      c4ui::MenuModel model;
      model.selected = menuSelected;
      model.hasHistory = hasHistory;
      model.wins = wins;
      model.losses = losses;
      model.draws = draws;
      model.streak = streak;
      model.lastOutcome = lastOutcome;
      model.lastCells = lastCells;
      model.lastLine = lastLine;
      c4ui::buildMenu(surface, model);
      break;
    }
    case c4::Screen::HowTo: {
      c4ui::HowToModel model;
      model.page = howToPage;
      c4ui::buildHowTo(surface, model);
      break;
    }
    case c4::Screen::Board:
    case c4::Screen::Review: {
      c4ui::BoardModel model;
      model.game = game;
      model.open = c4::openColumns(game);
      model.seat = seat;
      model.local = local;
      model.yourTurn = game.turn == seat;
      model.selectedColumn = selectedColumn;
      model.review = screen == c4::Screen::Review;
      c4ui::buildBoard(surface, model);
      break;
    }
    case c4::Screen::Result: {
      c4ui::ResultModel model;
      model.game = game;
      model.outcome = game.outcome;
      model.wins = wins;
      model.streak = streak;
      model.seat = seat;
      model.local = local;
      // The winning line comes from the rules' own search, so the cells marked
      // ARE the cells that won rather than a second opinion about where they
      // were.
      if (game.outcome == c4::Outcome::LightWins)
        c4::winningLine(game, c4::kLight, model.line);
      else if (game.outcome == c4::Outcome::DarkWins)
        c4::winningLine(game, c4::kDark, model.line);
      c4ui::buildResult(surface, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "ConnectFour", &renderer);

  const auto labels = mappedInput.mapLabels(tr(STR_QUEST_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

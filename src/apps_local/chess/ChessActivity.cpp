#include "ChessActivity.h"

#include <CrossPointSettings.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxScreen.h"
#include "../ui/ToyboxTheme.h"
#include "ChessNotation.h"
#include "ChessPieces.h"
#include "ChessScreens.h"
#include "ChessStyle.h"
#include "components/SideButtonNavigation.h"
#include "puzzles/ChessPuzzleActivity.h"

namespace {

constexpr int fileOf(const int square) { return square & 7; }
constexpr int rankOf(const int square) { return square >> 3; }

// Solid silhouette for black, outline for white. Colour reads from the drawing
// itself, so no background chip is needed and the board stays uncluttered.
// Draws a piece at `x,y`.
//
// The light pieces are the reason this exists. Drawing only their outline left
// them hollow, so on a dithered square the board's own texture showed straight
// through the piece: fine on a white square, muddy on a dark one, and wrong in
// every theme because it was never a theme problem. The fix is to knock the
// silhouette out to the page colour first and only then stroke the outline over
// it, which is what a white piece physically is.
void drawPieceAt(const GfxRenderer& renderer, const uint8_t piece, const int size, const int x, const int y,
                 const bool turned = false, chessstyle::Design design = chessstyle::kDefault) {
  (void)turned;
  auto target = toybox::makeTarget(renderer);
  chessstyle::draw(target, freeink::ui::makeRect(x, y, size, size), chess::pieceType(piece), chess::isBlack(piece),
                   design);
}

// Kings are never captured, so a strip only ever needs these five.

// Optical gap between captured pieces, measured from ink to ink.

// Next to the reader's own state, so a user clearing .crosspoint/ clears this
// too and there is one place to look.
constexpr const char* kSettingsPath = chessstyle::kSettingsPath;
constexpr char kSavePath[] = "/.crosspoint/chess.sav";
constexpr int kSaveVersion = 1;
constexpr int kGameSaveVersion = 3;

struct EngineTimeBudget {
  uint32_t started;
  uint32_t lastYield;
  uint32_t milliseconds;
};

bool pollEngineBudget(void* context) {
  auto& budget = *static_cast<EngineTimeBudget*>(context);
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - budget.started) >= budget.milliseconds) return true;
  if (static_cast<uint32_t>(now - budget.lastYield) >= 20u) {
    vTaskDelay(1);
    budget.lastYield = now;
  }
  return static_cast<uint32_t>(millis() - budget.started) >= budget.milliseconds;
}

}  // namespace

std::unique_ptr<Activity> ChessActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ChessActivity>(renderer, mappedInput);
}

void ChessActivity::resetGame() {
  Storage.remove(kSavePath);
  startedOpponent = opponent;
  startedHumanWhite = humanPlaysWhite;
  saveSettings();
  chess::setStartPosition(position);
  selectedSquare = -1;
  gameOver = false;
  engineThinking = false;
  historyCount = 0;
  losses = {};
  historyBase = 0;
  undoableFrom = 0;
  resetRepetition();
  refreshLegalMoves();
  // Playing Black means the engine opens.
  if (engineToMove() && !gameOver) engineThinking = true;
  requestUpdate();
}

void ChessActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadSettings();
  hasSavedGame = loadGame();
  if (!hasSavedGame) {
    resetGame();
  } else if (engineToMove() && !gameOver) {
    // A game saved mid-search resumes with the engine still owing a move.
    engineThinking = true;
  }
  goToMenu();
}

chessui::StartModel ChessActivity::startModel() const {
  chessui::StartModel model;
  model.hasSavedGame = hasSavedGame;
  model.canTakeBack = hasSavedGame && canTakeBack();
  model.continueDetail = continueDetail;
  model.selected = startIndex;
  return model;
}

void ChessActivity::drawStartMenu() {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  const fui::Rect slot = chessui::buildStartMenu(screen, startModel());
  drawMiniBoard(Rect{slot.x, slot.y, slot.width, slot.height});
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Chess start", &renderer);
}

void ChessActivity::drawMiniBoard(const Rect& slot) const {
  const int square = std::min(slot.width, slot.height) / 8;
  if (square < 1) return;
  const int side = square * 8, left = slot.x + (slot.width - side) / 2, top = slot.y + (slot.height - side) / 2;
  renderer.drawRect(left - 2, top - 2, side + 4, side + 4, 3, true);
  for (int index = 0; index < 64; ++index) {
    const int file = fileOf(index), rank = rankOf(index);
    const int x = left + file * square, y = top + (7 - rank) * square;
    renderer.fillRect(x, y, square, square, false);
    if (((file + rank) & 1) == 0) renderer.fillRectDither(x, y, square, square, LightGray);
    if (position.squares[index] == chess::Empty) continue;
    const int piece = std::max(1, square - 6), inset = (square - piece) / 2;
    drawPieceAt(renderer, position.squares[index], piece, x + inset, y + inset, false, pieceDesign);
  }
}

void ChessActivity::leaveBoard() {
  // One implementation for the chevron and the Back key, or the two drift and
  // only one of them remembers to tell the opponent.

  saveGame();
  hasSavedGame = true;
  goToMenu();
}

void ChessActivity::requestNewGame() {
  view = View::Board;
  resetGame();
}

void ChessActivity::refreshContinueDetail() {
  snprintf(
      continueDetail, sizeof(continueDetail), "%d %s", historyCount / 2 + 1,
      gameOver ? tr(STR_UI_OVER_CC1182) : (position.whiteToMove ? tr(STR_UI_WHITE_40D23F) : tr(STR_UI_BLACK_A9414A)));
}

void ChessActivity::goToMenu() {
  refreshContinueDetail();
  view = View::Menu;
  startIndex = 0;
  requestUpdate();
}

void ChessActivity::activateStartRow(const chessui::StartRow row) {
  switch (row) {
    case chessui::StartRow::Continue:
      view = gameOver ? View::Result : View::Board;
      break;
    case chessui::StartRow::NewGame:
      view = View::Board;
      requestNewGame();
      break;
    case chessui::StartRow::Puzzles: {
      auto puzzles = ChessPuzzleActivity::create(renderer, mappedInput);
      if (puzzles) startActivityForResult(std::move(puzzles), [this](const ActivityResult&) { goToMenu(); });
      return;
    }
    case chessui::StartRow::Settings:
      view = View::Board;
      view = View::Settings;
      settingsFrom = SettingsFrom::Menu;
      menuIndex = 0;
      break;
    default:
      break;
  }
  requestUpdate();
}

void ChessActivity::routeStartMenu() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // Chess does not know where it lives. shelf::leave() puts it back in the
    // folder it was opened from; see src/apps_local/Shelf.h for why no app
    // names its own destination.
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
  // No focusNext/focusPrev/confirm. Rows are tapped. Confirm does not exist on
  // this device, so a row selection only a button could open was a dead end.

  if (!input.touchReleased || !interactionsReady) return;

  const fui::ActionEvent event = sidebuttons::route(interactions, input, mappedInput, *this);
  if (event.action == chessui::ActionStartRow) {
    startIndex = event.value;
    activateStartRow(chessui::startRowAt(startModel(), event.value));
  }
}

void ChessActivity::onExit() {
  saveGame();
  Activity::onExit();
}

void ChessActivity::saveGame() const {
  // Written line by line rather than assembled in one buffer: a full history is
  // ~1.4KB, far past the 256-byte stack-local limit, and this is a cold path
  // where a few small writes cost nothing.
  HalFile file;
  if (!Storage.openFileForWrite(tr(STR_UI_CHESS_902609), kSavePath, file)) {
    LOG_ERR("CHESS", "Could not open %s for write", kSavePath);
    return;
  }
  // One buffer, three formats, so it is sized for the widest: "%s\n%d\n" with a
  // whole FEN in the %s. At 96 it held 95 of the 103 that format can print, and
  // the shortfall lands in the SAVE FILE rather than on the panel -- a cut FEN
  // is a position that will not load. Today's margin is one byte: a 89-char FEN
  // and a three-digit move number is 95.
  constexpr int kFenChars = 90;
  constexpr int kSaveLineChars = (kFenChars - 1) + toybox::kIntChars + toybox::literalChars("\n\n") + 1;
  char line[kSaveLineChars];
  snprintf(line, sizeof(line), "%d\n", kGameSaveVersion);
  file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  char fen[kFenChars];
  chess::positionToFen(position, fen);
  snprintf(line, sizeof(line), "%s\n%d\n", fen, historyBase);
  file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  losses.encode(line, sizeof(line));
  file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  snprintf(line, sizeof(line), "\n%d\n", static_cast<int>(drawReason));
  file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  for (int i = 0; i < historyCount; ++i) {
    snprintf(line, sizeof(line), "%s\n", history[i]);
    file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  }
}

void ChessActivity::saveSettings() const {
  // Version, strength, colour, hints, opponent and piece design.
  constexpr int kSettingsLineChars = toybox::kIntChars + toybox::kIntChars + toybox::kIntChars + toybox::kIntChars +
                                     toybox::kIntChars + toybox::kIntChars + toybox::kIntChars +
                                     toybox::literalChars("      \n") + 1;
  char line[kSettingsLineChars];
  snprintf(line, sizeof(line), "%d %d %d %d %d %d %d\n", kSaveVersion, static_cast<int>(level), humanPlaysWhite ? 1 : 0,
           showHints ? 1 : 0, static_cast<int>(opponent), int(chessstyle::kDefault), highlightSquares ? 1 : 0);
  Storage.writeFile(kSettingsPath, String(line));
}

void ChessActivity::loadSettings() {
  if (!Storage.exists(kSettingsPath)) return;
  char buffer[48] = {};
  if (Storage.readFileToBuffer(kSettingsPath, buffer, sizeof(buffer)) == 0) return;
  int version = 0;
  int storedLevel = 1;
  int white = 1;
  int hints = 1;
  int computer = 1;
  pieceDesign = chessstyle::parsePreference(buffer);
  // Files written before the opponent setting existed simply lack the field,
  // so accept four and default the fifth rather than discarding the lot.
  const int fields = sscanf(buffer, "%d %d %d %d %d", &version, &storedLevel, &white, &hints, &computer);
  if (fields < 4) return;
  if (version != kSaveVersion) return;
  level = static_cast<uint8_t>(storedLevel < 0 || storedLevel > 2 ? 1 : storedLevel);
  humanPlaysWhite = white != 0;
  showHints = hints != 0;
  int storedHighlight = hints;
  std::sscanf(buffer, "%*d %*d %*d %*d %*d %*d %d", &storedHighlight);
  highlightSquares = storedHighlight != 0;
  opponent = fields >= 5 && computer > 0 && computer <= static_cast<int>(chessui::Opponent::FaceToFace)
                 ? static_cast<chessui::Opponent>(computer)
                 : chessui::Opponent::Computer;
  if (opponent == chessui::Opponent::FaceToFace) opponent = chessui::Opponent::PassAndPlay;
  startedOpponent = opponent;
  startedHumanWhite = humanPlaysWhite;
}

bool ChessActivity::loadGame() {
  if (!Storage.exists(kSavePath)) return false;
  // 2KB: version + FEN + base + up to 128 SAN lines. Heap, not stack.
  auto buffer = makeUniqueNoThrow<char[]>(2048);
  if (!buffer) {
    LOG_ERR("CHESS", "OOM reading save");
    return false;
  }
  const size_t read = Storage.readFileToBuffer(kSavePath, buffer.get(), 2048);
  if (read == 0) return false;

  char* cursor = buffer.get();
  const auto nextLine = [&cursor]() -> char* {
    if (cursor == nullptr || *cursor == '\0') return nullptr;
    char* start = cursor;
    while (*cursor != '\0' && *cursor != '\n') ++cursor;
    if (*cursor == '\n') *cursor++ = '\0';
    return start;
  };

  const char* version = nextLine();
  if (version == nullptr || (atoi(version) < 1 || atoi(version) > kGameSaveVersion)) {
    LOG_INF("CHESS", "Ignoring save with unknown version");
    return false;
  }
  const char* fen = nextLine();
  const char* base = nextLine();
  chess::Position restored;
  if (fen == nullptr || base == nullptr || !chess::parseFen(fen, restored)) {
    LOG_ERR("CHESS", "Corrupt save, starting fresh");
    return false;
  }

  losses = {};
  if (atoi(version) >= 2) {
    if (!losses.decode(nextLine())) {
      LOG_ERR("CHESS", "Corrupt capture counts");
      return false;
    }
  } else
    losses.estimateLegacy(restored);
  DrawReason restoredReason = DrawReason::None;
  if (atoi(version) >= 3) {
    const char* reasonLine = nextLine();
    int reason = 0;
    char trailing;
    if (!reasonLine || std::sscanf(reasonLine, "%d %c", &reason, &trailing) != 1 || reason < 0 ||
        reason > static_cast<int>(DrawReason::FiftyMove)) {
      LOG_ERR("CHESS", "Corrupt terminal reason");
      return false;
    }
    restoredReason = static_cast<DrawReason>(reason);
  }
  position = restored;
  historyBase = atoi(base);
  historyCount = 0;
  for (const char* line = nextLine(); line != nullptr && historyCount < kMaxHistory; line = nextLine()) {
    if (*line == '\0') continue;
    snprintf(history[historyCount], sizeof(history[0]), "%s", line);
    ++historyCount;
  }

  selectedSquare = -1;
  engineThinking = false;
  // The plies just read back are notation only: the save stores the position
  // and the move sheet, never the undo data. Neither take-back nor repetition
  // may look behind this point.
  undoableFrom = historyCount;
  // Repetition starts again from the resumed position. The keys of the
  // positions before it are not recoverable from the save, and counting a
  // repetition against keys we do not have is the one outcome worth ruling
  // out: it would end a live game in a draw that never happened.
  resetRepetition();
  refreshLegalMoves();
  if (restoredReason != DrawReason::None) {
    drawReason = restoredReason;
    gameOver = true;
  }
  LOG_INF("CHESS", "Resumed saved game (%d plies)", historyCount);
  return true;
}

void ChessActivity::applyMove(const chess::Move& move) {
  {
    // The renderer runs on its own task and draws these squares directly, so
    // the board has to change under the same lock the render task holds.
    // Without it a repaint landing inside makeMove() shows the moving piece on
    // both squares or on neither.
    RenderLock lock;
    // recordMove appends and may shift the whole buffer down first, so the slot
    // to fill is only known afterwards.
    recordMove(move);
    chess::Undo undo;
    chess::makeMove(position, move, undo);
    losses.capture(undo.captured);
    historyMove[historyCount - 1] = move;
    historyUndo[historyCount - 1] = undo;
    // After the move, so the key is the position this ply produced. Before
    // refreshLegalMoves, which is what reads the window back.
    pushRepetitionKey();
    refreshLegalMoves();
  }
  if (gameOver) saveGame();
}

bool ChessActivity::canTakeBack() const {
  // Not against a remote opponent: their board has the move too, and taking it
  // back here would need their agreement, which is a conversation this design
  // deliberately has no way to have.

  // undoableFrom rather than 0: a resumed game has notation for its earlier
  // plies but no way to unmake them.
  return historyCount > undoableFrom && !engineThinking;
}

void ChessActivity::takeBack() {
  if (!canTakeBack()) return;
  // Undo back to your own turn, which is one ply if the engine has not replied
  // yet and two if it has. Anything else hands the move straight back to it.
  // Sharing the device, one ply is a whole turn and the board simply hands back
  // to the other player. Against the engine, undoing one ply would just let it
  // move again, so it takes back the pair.
  const int plies = vsComputer() ? (engineToMove() ? 1 : 2) : 1;
  {
    RenderLock lock;
    for (int i = 0; i < plies && historyCount > undoableFrom; ++i) {
      --historyCount;
      losses.undo(historyUndo[historyCount].captured);
      chess::unmakeMove(position, historyMove[historyCount], historyUndo[historyCount]);
      // One key per ply applied, so one key per ply undone. Never below the
      // position the window started from.
      if (repCount > 1) --repCount;
    }
    selectedSquare = -1;
    // gameOver is refreshLegalMoves()'s to set now that a draw can end the
    // game: clearing it here would have been overwritten a line later anyway.
    refreshLegalMoves();
  }
  saveGame();
  requestUpdate();
}

void ChessActivity::recordMove(const chess::Move& move) {
  if (historyCount == kMaxHistory) {
    // Drop the oldest ply and remember that we did, so printed move numbers
    // stay correct rather than restarting from 1.
    for (int i = 1; i < kMaxHistory; ++i) {
      std::memcpy(history[i - 1], history[i], sizeof(history[0]));
      historyMove[i - 1] = historyMove[i];
      historyUndo[i - 1] = historyUndo[i];
    }
    --historyCount;
    ++historyBase;
    if (undoableFrom > 0) --undoableFrom;
  }
  chess::moveToSan(position, move, history[historyCount]);
  ++historyCount;
}

ChessActivity::BoardGeometry ChessActivity::boardGeometry() const {
  const auto g = chessui::boardLayout(toybox::makeTarget(renderer).deviceContext());
  return {g.board.x, g.board.y, g.cell};
}

bool ChessActivity::whiteAtBottom() const {
  switch (opponent) {
    case chessui::Opponent::Computer:
      // The same person holds it all game, so it stays put.
      return humanPlaysWhite;
    case chessui::Opponent::PassAndPlay:
      return true;
    case chessui::Opponent::FaceToFace:
      // The device does not move, so neither does the board. Black reads it
      // upside down, which is what sitting opposite somebody means.
      return true;
  }
  return true;
}

int ChessActivity::screenColumnOf(const int square) const {
  return whiteAtBottom() ? fileOf(square) : 7 - fileOf(square);
}

int ChessActivity::screenRowOf(const int square) const { return whiteAtBottom() ? 7 - rankOf(square) : rankOf(square); }

bool ChessActivity::engineToMove() const { return vsComputer() && position.whiteToMove != humanPlaysWhite; }

int ChessActivity::squareAtPoint(const int x, const int y) const {
  const BoardGeometry geometry = boardGeometry();
  const int boardSize = geometry.squareSize * 8;
  if (x < geometry.originX || x >= geometry.originX + boardSize) return -1;
  if (y < geometry.originY || y >= geometry.originY + boardSize) return -1;
  const int column = (x - geometry.originX) / geometry.squareSize;
  const int row = (y - geometry.originY) / geometry.squareSize;
  // Inverse of screenColumnOf/screenRowOf, so the hit test follows the board
  // when it is flipped for playing Black.
  const int file = whiteAtBottom() ? column : 7 - column;
  const int rank = whiteAtBottom() ? 7 - row : row;
  return rank * 8 + file;
}

void ChessActivity::refreshLegalMoves() {
  chess::generateLegalMoves(position, legalMoves);
  drawReason = DrawReason::None;
  // Mate and stalemate first. Mate on the hundredth halfmove is mate, and a
  // position repeated for the third time with no move to make is stalemate --
  // in both cases the side to move having no move is the older, stronger fact.
  if (legalMoves.count == 0) {
    gameOver = true;
    if (view == View::Board) view = View::Result;
    return;
  }
  if (currentRepetitionCount() >= 3) {
    drawReason = DrawReason::Repetition;
  } else if (position.halfmoveClock >= 100) {
    // 100 halfmoves, not 50: the rule counts moves by both players.
    drawReason = DrawReason::FiftyMove;
  }
  gameOver = drawReason != DrawReason::None;
  if (gameOver && view == View::Board) view = View::Result;
}

void ChessActivity::resetRepetition() {
  repCount = 0;
  pushRepetitionKey();
}

void ChessActivity::pushRepetitionKey() {
  const uint64_t key = chess::positionKey(position);
  if (repCount > 0 && repKeys[repCount - 1] == key) return;
  if (repCount == kRepWindow) {
    // Unreachable while the fifty-move draw above fires at 100 halfmoves, and
    // here anyway: dropping the oldest key is the one safe way to lose
    // information, because a shorter window can only miss a repetition.
    for (int i = 1; i < kRepWindow; ++i) repKeys[i - 1] = repKeys[i];
    --repCount;
  }
  repKeys[repCount++] = key;
}

int ChessActivity::currentRepetitionCount() const {
  return chess::repetitionCount(repKeys, repCount, position.halfmoveClock);
}

bool ChessActivity::isLegalDestination(const int square) const {
  if (selectedSquare < 0) return false;
  for (int i = 0; i < legalMoves.count; ++i) {
    if (legalMoves.moves[i].from == selectedSquare && legalMoves.moves[i].to == square) return true;
  }
  return false;
}

bool ChessActivity::tryMove(const int from, const int to) {
  for (int i = 0; i < legalMoves.count; ++i) {
    const chess::Move& move = legalMoves.moves[i];
    if (move.from != from || move.to != to) continue;
    // Several entries share from/to when promoting; take the queen.
    if ((move.flags & chess::FlagPromotion) != 0 && move.promotion != chess::Queen) continue;
    applyMove(move);
    return true;
  }
  return false;
}

void ChessActivity::handleSquareActivated(const int square) {
  if (engineThinking) return;

  if (!gameOver && engineToMove()) return;
  // Confirm still restarts without aiming, for the button-only devices.
  if (gameOver) {
    resetGame();
    return;
  }
  if (square < 0) return;

  if (selectedSquare >= 0 && tryMove(selectedSquare, square)) {
    selectedSquare = -1;
    // Hand off to the engine on the next pass rather than searching here, so
    // this repaint shows the played move and "Thinking" before the search runs.
    // engineToMove() is the guard, not just !gameOver: without it the engine
    // answered in two-player mode too, which is what the first friend-mode
    // screenshot caught.
    if (!gameOver && engineToMove()) engineThinking = true;
    requestUpdate();
    return;
  }

  // Tapping your own piece selects it (and re-tapping deselects); tapping
  // anything else with a selection active clears it.
  const uint8_t piece = position.squares[square];
  if (chess::isColour(piece, position.whiteToMove)) {
    selectedSquare = selectedSquare == square ? -1 : square;
  } else {
    selectedSquare = -1;
  }
  requestUpdate();
}

void ChessActivity::playEngineMove() {
  engineThinking = false;
  // A COPY, and this is the whole of the flicker fix. search() makes and
  // unmakes thousands of moves on whatever position it is handed, and the
  // render task reads `position` straight out of this object while the main
  // task is inside the search. Handed the live board, a repaint drew the
  // middle of the search: a wrong position on screen for as long as the search
  // took, then the real one when the move landed.
  //
  // The one-pass deferral in handleSquareActivated() was written to prevent
  // exactly this and cannot, because requestUpdate() only NOTIFIES the render
  // task. The repaint it asks for is still in flight when the search starts,
  // so deferring by a pass moved the race rather than closing it.
  chess::Position scratch = position;
  const auto engineLevel = static_cast<chess::Level>(level);
  const uint32_t started = millis();
  EngineTimeBudget budget{started, started, engineLevel == chess::Level::Hard ? 2500u : 1200u};
  const chess::SearchControl control{repKeys, repCount, pollEngineBudget, &budget};
  const chess::SearchResult result = chess::search(scratch, engineLevel, searchBuffers, control);
  LOG_DBG("CHESS", "engine: %u ms, %u nodes, depth %u, score %d%s", static_cast<unsigned>(millis() - started),
          static_cast<unsigned>(result.nodes), static_cast<unsigned>(result.completedDepth), result.score,
          result.budgetExhausted ? " (budget hit)" : "");
  if (!result.hasMove) {
    gameOver = true;
    view = View::Result;
    requestUpdate();
    return;
  }
  applyMove(result.best);
  // Save on the completed pair rather than on every ply: it is the natural
  // resume point, and auto-sleep never runs onExit().
  saveGame();
  requestUpdate();
}

const char* ChessActivity::resultText() const {
  if (!gameOver) return "";
  // Named rather than a bare "DRAW". A game that stops on its own needs to say
  // why, or it reads as the app having given up: both of these arrive with no
  // move played and nothing on the board to point at.
  if (drawReason == DrawReason::Repetition) return tr(STR_UI_DRAW_REPETITION_7591CB);
  if (drawReason == DrawReason::FiftyMove) return tr(STR_UI_DRAW_50_MOVES_5B361E);
  if (chess::isInCheck(position))
    return position.whiteToMove ? tr(STR_UI_BLACK_WINS_6A6B94) : tr(STR_UI_WHITE_WINS_AC8744);
  return tr(STR_UI_STALEMATE_53A930);
}

const char* ChessActivity::statusText() const {
  // At game over the capsule stops reporting and starts acting: the result
  // moves to its own line and the capsule becomes the button.
  if (gameOver) return tr(STR_WORDLE_PLAY_AGAIN);
  if (engineThinking) return tr(STR_UI_THINKING_B6D843);
  // Whose check it is does not need saying: the board already shows the turn.
  if (chess::isInCheck(position)) return tr(STR_UI_CHECK_D37267);
  // Naming them matters most here. You learn who you are playing on the
  // searching screen and then, until now, spent the whole game being told only
  // that it was somebody else's move.

  if (!vsComputer()) return position.whiteToMove ? tr(STR_UI_WHITE_TO_MOVE_89B109) : tr(STR_UI_BLACK_TO_MOVE_6305D7);
  return engineToMove() ? tr(STR_UI_THINKING_B6D843) : tr(STR_UI_YOUR_MOVE_AF6C56);
}

uint32_t ChessActivity::surfaceMeaning() const {
  uint32_t meaning = paintclock::mixMeaning(paintclock::kMeaningSeed, uint32_t(view));
  meaning = paintclock::mixMeaning(meaning, whiteAtBottom() ? 1u : 0u);
  const bool live = !gameOver && !engineThinking && !engineToMove();
  return paintclock::mixMeaning(meaning, live ? 1u : 0u);
}

void ChessActivity::loop() {
  if (view == View::Menu) {
    routeStartMenu();
    return;
  }

  if (view == View::Settings) {
    routeSettings();
    return;
  }

  if (view == View::Result) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      leaveBoard();
      return;
    }
    int x, y;
    if (!interactionsReady || !sidebuttons::poll(interactions, mappedInput, x, y)) return;
    freeink::ui::InputSnapshot tap;
    tap.touchReleased = true;
    tap.touchX = x;
    tap.touchY = y;
    const auto e = sidebuttons::route(interactions, tap, mappedInput, *this);
    if (e.action == chessui::ActionPlayAgain)
      requestNewGame();
    else if (e.action == chessui::ActionResultMenu)
      leaveBoard();
    else if (e.action == chessui::ActionResultReview) {
      view = View::Board;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    leaveBoard();
    return;
  }

  // Deferred by one pass from the move that triggered it, so "THINKING" is on
  // screen before the search starts.
  if (engineThinking) {
    playEngineMove();
    return;
  }

  int touchX = 0;
  int touchY = 0;
  if (sidebuttons::poll(interactions, mappedInput, touchX, touchY)) {
    // Chrome first, from the table the last paint filled in. The board's own
    // squares are not components: 64 of them would be 64 interactions, and
    // squareAtPoint() already derives them from the geometry that drew them.
    if (interactionsReady) {
      freeink::ui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = static_cast<int16_t>(touchX);
      tap.touchY = static_cast<int16_t>(touchY);
      const freeink::ui::ActionEvent event = sidebuttons::route(interactions, tap, mappedInput, *this);
      if (event.action == chessui::ActionPlayAgain) {
        requestNewGame();
        return;
      }
    }
    // Everything below the chrome is the board. A stray tap on a finished game
    // does nothing, because only the button restarts it.
    if (!gameOver) {
      const int square = squareAtPoint(touchX, touchY);
      if (square >= 0) {
        // Sixty-four squares do not fit the interaction table, so this never
        // reaches route(). See Activity::surfaceMeaning().
        if (!surfaceRevealed()) return;
        handleSquareActivated(square);
      }
    }
    return;
  }

  // No button navigation. The board is pointing, and pointing is fingers.
  //
  // There was a D-pad cursor here, moved by Up/Down/Left/Right and committed
  // with Confirm. On the X4 Pro three of those four buttons do not exist --
  // left, right and confirm are PIN_UNASSIGNED in the board profile and are
  // never configured as inputs -- so the cursor could move up and down a file
  // and could never be acted on.
  //
  // It was worse than the shelf's version, which at least drew itself: this one
  // was guarded by `!mappedInput.hasTouch()`, so on a touch device it was
  // INVISIBLE, and Up/Down still moved it and still called requestUpdate(). A
  // guaranteed full-panel repaint that changed nothing on screen, which is
  // exactly what a bug looks like on e-ink.
  //
  // See docs/buttons.md. Up and Down are the two side page keys and they page;
  // they do not push things around a board.
}

void ChessActivity::drawSquareContents(const BoardGeometry& geometry, const int square) const {
  const int x = geometry.originX + screenColumnOf(square) * geometry.squareSize;
  const int y = geometry.originY + screenRowOf(square) * geometry.squareSize;
  const int size = geometry.squareSize;

  // Dark squares are dithered, never solid. The board repaints on every move,
  // and solid fills at this area are exactly what makes e-ink ghost. This is
  // the ink-budget rule from Toybox.h in its most literal form.
  // The light dither, not the heavy one: at 50% the squares carry nearly as much
  // ink as the pieces, and the board reads as texture competing with the
  // position instead of a surface it sits on.
  const bool darkSquare = ((fileOf(square) + rankOf(square)) & 1) == 0;
  if (darkSquare) renderer.fillRectDither(x, y, size, size, LightGray);

  const uint8_t piece = position.squares[square];
  if (piece != chess::Empty) {
    const int inset = 3;
    drawPieceAt(renderer, piece, size - 6, x + inset, y + inset, false, pieceDesign);
  }

  if (highlightSquares && isLegalDestination(square)) renderer.drawRect(x + 2, y + 2, size - 4, size - 4, 2, true);
  if (showHints && isLegalDestination(square)) {
    if (piece == chess::Empty) {
      // Empty square: a solid dot in the middle. Nothing to hide behind it.
      const int diameter = std::max(8, size / 4);
      renderer.fillRoundedRect(x + (size - diameter) / 2, y + (size - diameter) / 2, diameter, diameter, diameter / 2,
                               Black);
    } else {
      // Occupied square: mark the corners instead. A dot in the middle of a
      // capture sits on top of the piece, where it vanishes into a solid black
      // silhouette and looks like a blemish on a light one. The corners are
      // free of piece art at every size, so the mark reads on either colour and
      // you can still see what you are taking.
      toybox::cornerMarks(renderer, Rect{x, y, size, size}, std::max(10, size / 4), toybox::kFrame);
    }
  }
}

void ChessActivity::activateMenuRow(const MenuRow row) {
  switch (row) {
    case MenuRow::NewGame:
      view = View::Board;
      requestNewGame();
      return;
    case MenuRow::TakeBack:
      if (!canTakeBack()) return;
      view = View::Board;
      takeBack();
      return;
    case MenuRow::Level:
      level = static_cast<uint8_t>((level + 1) % 3);
      break;
    case MenuRow::PlayAs:
      // Edited in place. Closing the overlay is what starts the new game, and
      // the button down there says so, because a tap that silently threw away
      // your game and jumped back to the board left you wondering whether
      // anything had happened at all.
      humanPlaysWhite = !humanPlaysWhite;
      break;
    case MenuRow::Opponent:
      opponent = static_cast<chessui::Opponent>((static_cast<int>(opponent) + 1) % 2);
      // The visible row set shrinks against a person, so a selection sitting on
      // a row that just disappeared has to come back into range.
      if (menuIndex >= visibleMenuRows()) menuIndex = visibleMenuRows() - 1;
      break;
    case MenuRow::Hints:
      showHints = !showHints;
      break;
    default:
      return;
  }
  saveSettings();
  requestUpdate();
}

void ChessActivity::routeSettings() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    closeSettings();
    return;
  }

  // One snapshot, one table, one dispatch: touch, focus and Confirm all arrive
  // as the same semantic action, so a row cannot behave differently depending
  // on which input reached it.
  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (sidebuttons::poll(interactions, mappedInput, tapX, tapY, false)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  // No focusNext/focusPrev/confirm. Rows are tapped. Confirm does not exist on
  // this device, so a row selection only a button could open was a dead end.

  if (!input.touchReleased || !interactionsReady) return;

  const fui::ActionEvent event = sidebuttons::route(interactions, input, mappedInput, *this, false);
  if (event.action == chessui::ActionSettingValue) {
    switch (static_cast<chessui::SettingValue>(event.value)) {
      case chessui::SettingValue::Computer:
        opponent = chessui::Opponent::Computer;
        break;
      case chessui::SettingValue::Person:
        opponent = chessui::Opponent::PassAndPlay;
        break;
      case chessui::SettingValue::Easy:
        level = 0;
        break;
      case chessui::SettingValue::Normal:
        level = 1;
        break;
      case chessui::SettingValue::Hard:
        level = 2;
        break;
      case chessui::SettingValue::White:
        humanPlaysWhite = true;
        break;
      case chessui::SettingValue::Black:
        humanPlaysWhite = false;
        break;
      case chessui::SettingValue::Hints:
        showHints = !showHints;
        break;
      case chessui::SettingValue::Highlights:
        highlightSquares = !highlightSquares;
        break;
    }
    saveSettings();
    requestUpdate();
    return;
  }
  if (event.action == chessui::ActionMenuRow) {
    menuIndex = event.value;
    activateMenuRow(menuRowAt(event.value));
    return;
  }
  if (event.action == chessui::ActionCloseSettings) closeSettings();
}

bool ChessActivity::restartPending() const {
  return opponent != startedOpponent || (vsComputer() && humanPlaysWhite != startedHumanWhite);
}

void ChessActivity::closeSettings() {
  view = View::Board;
  if (!vsComputer()) startedHumanWhite = humanPlaysWhite;
  // A setting that cannot apply mid-game restarts it, which is what the close
  // button has been saying it would do.
  if (restartPending()) requestNewGame();
  if (settingsFrom == SettingsFrom::Menu) {
    goToMenu();
    return;
  }
  requestUpdate();
}

chessui::SettingsModel ChessActivity::settingsModel() const {
  chessui::SettingsModel model;
  model.fromMenu = settingsFrom == SettingsFrom::Menu;
  model.opponent = opponent;
  model.humanPlaysWhite = humanPlaysWhite;
  model.showHints = showHints;
  model.highlightSquares = highlightSquares;
  model.level = level;
  model.pieceDesign = pieceDesign;
  model.canTakeBack = canTakeBack();
  model.restartPending = restartPending();
  model.selected = menuIndex;
  return model;
}

void ChessActivity::drawSettings() {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  chessui::buildSettings(screen, settingsModel());
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Chess settings", &renderer);
}

// Puts the single-player game back and returns to the menu rather than dropping
// the player out to the apps hub, which is where they came from two taps ago.

void ChessActivity::render(RenderLock&&) {
  if (view == View::Settings) interactions.setFocusedIndex(-1);
  if (view == View::Menu) {
    drawStartMenu();
    renderer.displayBuffer();
    return;
  }

  if (view == View::Settings) {
    drawSettings();
    renderer.displayBuffer();
    return;
  }

  renderer.clearScreen();

  namespace fui = freeink::ui;
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);

  pixelui::bindFonts(target, renderer);
  if (view == View::Result) {
    char badge[96];
    static constexpr StrId levels[] = {StrId::STR_UI_EASY_493F11, StrId::STR_CHESS_NORMAL, StrId::STR_UI_HARD_A06A58};
    const bool draw = drawReason != DrawReason::None || !chess::isInCheck(position);
    std::snprintf(badge, sizeof(badge), tr(STR_CHESS_MATE_LEVEL), I18N.get(levels[level]));
    chessui::buildResult(screen, {draw, position.whiteToMove, !startedHumanWhite, draw ? resultText() : badge});
    interactionsReady = true;
    renderer.displayBuffer();
    return;
  }
  char status[80], last[32];
  if (gameOver || !vsComputer())
    std::snprintf(status, sizeof(status), "%s", gameOver ? resultText() : statusText());
  else
    std::snprintf(status, sizeof(status), "%s · %s", statusText(),
                  position.whiteToMove ? tr(STR_UI_WHITE_40D23F) : tr(STR_UI_BLACK_A9414A));
  if (historyCount) {
    char notation[10];
    chess::displaySan(history[historyCount - 1], I18N.getLanguage() == Language::ES, notation, sizeof(notation));
    std::snprintf(last, sizeof(last), "%d. %s", (historyBase + historyCount - 1) / 2 + 1, notation);
  } else
    std::snprintf(last, sizeof(last), "—");
  static constexpr StrId levels[] = {StrId::STR_UI_EASY_493F11, StrId::STR_CHESS_NORMAL, StrId::STR_UI_HARD_A06A58};
  chessui::BoardModel chrome;
  chrome.status = status;
  chrome.gameOver = gameOver;
  chrome.level = vsComputer() ? I18N.get(levels[level]) : tr(STR_CHESS_PERSON);
  chrome.lastMove = last;
  chrome.losses = losses.counts;
  chrome.whiteAtBottom = whiteAtBottom();
  chessui::buildBoardChrome(screen, chrome);

  const BoardGeometry geometry = boardGeometry();
  const int boardSize = geometry.squareSize * 8;
  for (int square = 0; square < 64; ++square) {
    drawSquareContents(geometry, square);
  }
  // The selection frame is drawn OUTSIDE its square, so it overlaps the
  // neighbours. That means it has to come after every square is painted:
  // drawn inside drawSquareContents it was overdrawn by whichever adjacent
  // square happened to be rendered later, leaving a broken rectangle.
  if (selectedSquare >= 0) {
    const int sx = geometry.originX + screenColumnOf(selectedSquare) * geometry.squareSize;
    const int sy = geometry.originY + screenRowOf(selectedSquare) * geometry.squareSize;
    renderer.drawRect(sx - toybox::kFrame, sy - toybox::kFrame, geometry.squareSize + 2 * toybox::kFrame,
                      geometry.squareSize + 2 * toybox::kFrame, toybox::kFrame, true);
  }

  // The frame sits outside the squares so it never eats a rank.
  renderer.drawRect(geometry.originX - 3, geometry.originY - 3, boardSize + 6, boardSize + 6, 3, true);

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Chess board", &renderer);

  renderer.displayBuffer();
}

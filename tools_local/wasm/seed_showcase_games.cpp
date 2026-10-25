// Host-only fixture generator. Every save uses the firmware's encoder/decoder.
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "../../src/apps_local/carnivoro/CarnivoroCore.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleCore.h"
#include "../../src/apps_local/game2048/Game2048Store.h"
#include "../../src/apps_local/intercept/InterceptCore.h"
#include "../../src/apps_local/murdle/MurdleSave.h"
#include "../../src/apps_local/picross/PicrossSave.h"
#include "../../src/apps_local/pomodoro/PomodoroCore.h"
#include "../../src/apps_local/wordle/LocalWordle.h"

namespace fs = std::filesystem;
static void write(const fs::path& dir, const char* name, const uint8_t* bytes, size_t size) {
  const auto path = dir / name;
  FILE* file = std::fopen(path.string().c_str(), "wb");
  if (!file || std::fwrite(bytes, 1, size, file) != size || std::fclose(file)) std::exit(2);
}
int main(int argc, char** argv) {
  if (argc != 2) return 1;
  const auto dir = fs::weakly_canonical(fs::path(argv[1]));
  if (dir.filename() != ".crosspoint" || !fs::exists(dir.parent_path() / "CROSSNEXT-SIMULATOR-CARD.json")) return 1;
  uint8_t bytes[carnivoro::kSaveBytes]{};
  uint32_t generation = 0;
  chesspuzzle::Progress chess, chessCopy;
  for (int n = 0; n < 31; ++n) chesspuzzle::markSolved(chess, n);
  for (int n = 400; n < 412; ++n) chesspuzzle::markSolved(chess, n);
  chess.cursor[0] = 31;
  chess.cursor[1] = 12;
  chess.lastIndex = 31;
  chesspuzzle::encode(chess, 1, bytes);
  if (!chesspuzzle::decode(bytes, chessCopy, generation)) return 3;
  write(dir, "chess-puzzles-a.bin", bytes, chesspuzzle::kProgressBytes);

  picross::Board board, boardCopy;
  picross::Progress progress, progressCopy;
  for (int n = 0; n < 28; ++n) progress.markSolved(n);
  board.load(28);
  for (int row = 0; row < 2; ++row)
    for (int col = 0; col < board.size(); ++col)
      if (board.solid(row, col)) board.fill(row, col);
  picrosssave::encode(board, progress, bytes);
  if (!picrosssave::decode(bytes, picrosssave::kBytes, boardCopy, progressCopy)) return 4;
  write(dir, "picross-v7.sav", bytes, picrosssave::kBytes);

  carnivoro::State carnivoro, carnivoroCopy;
  for (int n = 0; n < 18; ++n)
    carnivoro.status[carnivoro::caseAt(carnivoro::Difficulty::Easy, n)] = carnivoro::Status::Solved;
  carnivoro.caseId = carnivoro::caseAt(carnivoro::Difficulty::Easy, 18);
  carnivoro.status[carnivoro.caseId] = carnivoro::Status::Active;
  bool future = false;
  if (!carnivoro::encode(carnivoro, bytes, sizeof(bytes)) ||
      !carnivoro::decode(bytes, sizeof(bytes), carnivoroCopy, future))
    return 5;
  write(dir, "carnivoro.bin", bytes, sizeof(bytes));

  localwordle::State wordle, wordleCopy;
  wordle.language = 1;
  wordle.seed = 20261021;
  localwordle::start(wordle);
  const char* guess = localwordle::wordAt(5, 2, 1);
  for (int n = 0; n < 5; ++n) wordle.game.type(guess[n]);
  wordle.game.submit([](void*, const char* text) { return localwordle::contains(5, text, 1); }, nullptr);
  wordle.record = {42, 37, 6, 9, {2, 5, 11, 12, 5, 2}};
  localwordle::encode(wordle, 1, bytes);
  if (!localwordle::decode(bytes, wordleCopy, generation)) return 6;
  write(dir, "wordle-local-a.bin", bytes, localwordle::kSaveBytes);

  game2048::State game, gameCopy;
  game2048::start(game, 751);
  for (int n = 0; n < 140 && game2048::canMove(game); ++n)
    game2048::move(game, static_cast<game2048::Direction>(n % 4));
  game2048::encode(game, 18720, 1, bytes);
  uint64_t best = 0;
  if (!game2048::decode(bytes, gameCopy, best, generation)) return 7;
  write(dir, "2048-a.bin", bytes, game2048::kSaveBytes);

  pomodoro::State timer, timerCopy;
  timer.completed = 18;
  timer.stage = 2;
  timer.remainingMs = 17 * 60000;
  pomodoro::encode(timer, 1, bytes);
  if (!pomodoro::decode(bytes, timerCopy, generation)) return 8;
  write(dir, "pomodoro-a.bin", bytes, pomodoro::kSaveBytes);

  intercept::State story, storyCopy;
  for (int n = 0; n < 4; ++n) {
    bool moved = false;
    for (unsigned i = 0; i < intercept::nodes[story.node].count; ++i)
      if (intercept::choose(story, i)) {
        moved = true;
        break;
      }
    if (!moved) break;
  }
  intercept::encode(story, bytes);
  if (!intercept::decode(bytes, storyCopy)) return 9;
  write(dir, "intercept.dat", bytes, 32);

  murdlesave::State murdle{};
  murdle.caseNumber = 18;
  murdle.solvedCount = 17;
  for (int n = 0; n < 17; ++n) murdle.completed[n / 8] |= 1u << (n % 8);
  murdlesave::encode(murdle, bytes);
  murdlesave::State murdleCopy{};
  if (!murdlesave::decode(bytes, murdlesave::kBytes, murdleCopy)) return 10;
  write(dir, "murdle.sav", bytes, murdlesave::kBytes);
  std::puts("Eight game/app save formats round-trip correctly.");
}

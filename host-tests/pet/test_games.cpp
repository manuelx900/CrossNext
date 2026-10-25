#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../../src/apps_local/pet/PetPuzzleInput.h"
#include "../../src/apps_local/pet/PetState.h"

using Board = std::array<uint8_t, 9>;
unsigned rank(const uint8_t* board) {
  unsigned value = 0;
  for (unsigned i = 0; i < 9; ++i) {
    unsigned smaller = 0;
    for (unsigned j = i + 1; j < 9; ++j) smaller += board[j] < board[i];
    value = value * (9 - i) + smaller;
  }
  return value;
}
int main() {
  for (bool wide : {false, true}) {
    const auto layout = petinput::puzzleLayout(freeink::ui::makeRect(20, 30, wide ? 500 : 440, wide ? 240 : 370), wide);
    for (int blank = 0; blank < 9; ++blank) {
      uint8_t board[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
      board[blank] = 0;
      for (int from = 0; from < 9; ++from) {
        const auto box = layout.tile(unsigned(from));
        const int x = box.x + box.width / 2, y = box.y + box.height / 2;
        assert(layout.at(x, y) == from);
        for (int dx : {-1, 0, 1})
          for (int dy : {-1, 0, 1}) {
            const int row = from / 3 + dy, col = from % 3 + dx;
            const bool legal = from != blank && std::abs(dx) + std::abs(dy) == 1 && row >= 0 && row < 3 && col >= 0 &&
                               col < 3 && row * 3 + col == blank;
            assert(layout.slide(board, x, y, dx, dy) == (legal ? from : -1));
          }
        assert(layout.slide(board, box.right() + 1, y, 1, 0) == -1);
      }
    }
    assert(layout.at(layout.frame.x, layout.frame.y) == -1);
  }
  petinput::PendingSlide slide;
  int x = 0, y = 0, dx = 0, dy = 0;
  slide.begin(100, 200, 4);
  slide.end(1, 0, 4);
  slide.begin(300, 400, 4);
  slide.end(0, 1, 4);
  assert(slide.take(4, x, y, dx, dy) && x == 100 && y == 200 && dx == 1 && dy == 0);
  assert(!slide.take(4, x, y, dx, dy));
  slide.begin(100, 200, 4);
  slide.end(1, 0, 5);
  assert(!slide.take(5, x, y, dx, dy));
  slide.begin(100, 200, 4);
  slide.end(1, 0, 4);
  assert(!slide.take(5, x, y, dx, dy));
  slide.begin(100, 200, 4);
  slide.end(0, 0, 4);
  assert(!slide.take(4, x, y, dx, dy));
  slide.begin(100, 200, 4);
  slide.end(1, 0, 4);
  slide.clear();
  assert(!slide.take(4, x, y, dx, dy));
  static_assert(sizeof(pet::GameState) <= 64);
  // Reverse BFS independently proves reachability and supplies shortest solutions.
  static uint8_t solutionMove[362880];
  std::memset(solutionMove, 255, sizeof(solutionMove));
  std::vector<Board> reachable;
  reachable.reserve(181440);
  reachable.push_back({1, 2, 3, 4, 5, 6, 7, 8, 0});
  solutionMove[rank(reachable[0].data())] = 254;
  for (size_t n = 0; n < reachable.size(); ++n) {
    const Board board = reachable[n];
    unsigned blank = 0;
    while (board[blank]) ++blank;
    for (unsigned cell = 0; cell < 9; ++cell) {
      if (std::abs(int(cell / 3) - int(blank / 3)) + std::abs(int(cell % 3) - int(blank % 3)) != 1) continue;
      auto next = board;
      std::swap(next[cell], next[blank]);
      const auto id = rank(next.data());
      if (solutionMove[id] != 255) continue;
      solutionMove[id] = uint8_t(blank);
      reachable.push_back(next);
    }
  }
  assert(reachable.size() == 181440);
  for (unsigned seed = 0; seed < 256; ++seed) {
    pet::GameState game;
    game.begin(pet::Game::Puzzle, seed);
    const auto initial = game;
    pet::GameState repeat;
    repeat.begin(pet::Game::Puzzle, seed);
    assert(!std::memcmp(game.puzzle, repeat.puzzle, 9));
    unsigned puzzleCounts[9]{}, distance = 0, inversions = 0, blank = 0;
    for (unsigned i = 0; i < 9; ++i) {
      assert(game.puzzle[i] < 9);
      ++puzzleCounts[game.puzzle[i]];
      if (!game.puzzle[i]) {
        blank = i;
        continue;
      }
      const unsigned goal = game.puzzle[i] - 1;
      distance += unsigned(std::abs(int(i / 3) - int(goal / 3)) + std::abs(int(i % 3) - int(goal % 3)));
      for (unsigned j = i + 1; j < 9; ++j) inversions += game.puzzle[j] && game.puzzle[i] > game.puzzle[j];
    }
    for (auto count : puzzleCounts) assert(count == 1);
    assert(distance >= 12 && inversions % 2 == 0 && !game.finished() && !game.puzzleSolved());
    for (unsigned cell = 0; cell < 10; ++cell) {
      game = initial;
      game.select(cell);
      const bool legal =
          cell < 9 && std::abs(int(cell / 3) - int(blank / 3)) + std::abs(int(cell % 3) - int(blank % 3)) == 1;
      assert(game.moves == unsigned(legal));
      if (legal)
        assert(game.puzzle[cell] == 0 && game.puzzle[blank] == initial.puzzle[cell]);
      else
        assert(!std::memcmp(game.puzzle, initial.puzzle, 9));
      game.resetPuzzle();
      assert(!std::memcmp(game.puzzle, initial.puzzle, 9) && game.moves == 0);
    }
    for (unsigned step = 0; !game.finished() && step < 32; ++step) {
      const auto move = solutionMove[rank(game.puzzle)];
      assert(move < 9);
      game.select(move);
    }
    assert(game.finished() && game.puzzleSolved() && game.moves >= 12 && game.moves <= 31);
    const auto finished = game;
    game.select(7);
    game.resetPuzzle();
    game.advance();
    assert(game.finished() && !std::memcmp(game.puzzle, finished.puzzle, 9) && game.moves == finished.moves);
    game.begin(pet::Game::Sequence, seed);
    for (unsigned round = 0; round < 3; ++round) {
      assert(game.showing() && game.sequenceLength() == 5 + round);
      game.select(game.sequence[0]);
      assert(game.step == 0);
      game.advance();
      game.select(3);
      assert(game.step == 0);
      game.select((game.sequence[0] + 1) % 3);
      assert(game.showing() && game.step == 0 && game.mistakes == round + 1);
      game.advance();
      const unsigned length = game.sequenceLength();
      for (unsigned i = 0; i < length; ++i) game.select(game.sequence[i]);
      assert(game.rounds == round + 1);
      if (round < 2) {
        assert(game.phase == pet::GamePhase::RoundComplete);
        game.advance();
      } else
        assert(game.finished());
    }
    game.begin(pet::Game::Pairs, seed);
    unsigned counts[3]{};
    for (auto card : game.cards) {
      assert(card < 3);
      ++counts[card];
    }
    for (auto count : counts) assert(count == 2);
    game.select(0);
    game.select(0);
    game.advance();
    assert(game.picked == 0 && game.second == 255);
    unsigned different = 1;
    while (game.cards[different] == game.cards[0]) ++different;
    game.select(different);
    assert(game.mistakes == 1);
    game.select(5);
    assert(game.second == different);
    game.advance();
    assert(game.picked == 255 && game.second == 255);
    for (unsigned symbol = 0; symbol < 3; ++symbol)
      for (unsigned i = 0; i < 6; ++i)
        if (game.cards[i] == symbol) game.select(i);
    assert(game.finished() && game.matched == 63);
  }
  pet::GameState game;
  game.begin(pet::Game::Puzzle, 1);
  game.moves = UINT16_MAX;
  unsigned blank = 0;
  while (game.puzzle[blank]) ++blank;
  game.select(blank % 3 ? blank - 1 : blank + 1);
  assert(game.moves == UINT16_MAX);
  game.begin(pet::Game::Sequence, 1);
  game.mistakes = UINT16_MAX;
  game.advance();
  game.select((game.sequence[0] + 1) % 3);
  assert(game.mistakes == UINT16_MAX);
  std::printf(
      "PET games: all 181440 reachable puzzle boards; 256 seeds solved; three games and guards passed. State: %zu "
      "bytes\n",
      sizeof(game));
}

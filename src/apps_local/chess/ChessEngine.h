#pragma once

// Chess opponent: alpha-beta search over ChessCore.
//
// Freestanding like ChessCore: fixed buffers, no heap, clock or RTOS dependency.
// Node limits are deterministic; device callers can also provide a cooperative
// stop callback to enforce a time budget and yield to their scheduler.

#include <cstdint>

#include "ChessCore.h"

namespace chess {

// Scores are centipawns from the side-to-move's point of view.
constexpr int kMateScore = 30000;
// Beyond any plausible material score, so "mate in n" always beats material.
constexpr int kMateThreshold = kMateScore - 1000;

// Regular search plus a bounded tactical extension for captures and check evasions.
constexpr int kMaxSearchDepth = 4;
constexpr int kQuiescencePlies = 4;
constexpr int kOpponentMaxDepth = 6;
constexpr int kOpponentMaxPly = kOpponentMaxDepth + kQuiescencePlies;
constexpr int kMoveOrderEntries = 512;

enum class Level : uint8_t { Easy, Medium, Hard };

struct SearchLimits {
  int depth;
  uint32_t nodes;
};

constexpr SearchLimits limitsForLevel(const Level level) {
  switch (level) {
    case Level::Easy:
      return {1, 2500};
    case Level::Medium:
      return {5, 60000};
    case Level::Hard:
      return {6, 180000};
  }
  return {1, 2500};
}

// One MoveList per ply, ~1KB each. Far too big for a stack frame on the C3, so
// the caller owns it: the activity holds one as a member. Handing the search a
// pre-allocated buffer is also what keeps the engine allocation-free.
struct SearchBuffers {
  MoveList plies[kMaxSearchDepth + kQuiescencePlies + 1];
};

struct MoveOrderEntry {
  uint64_t key = 0;
  Move best{};
  uint8_t depth = 0;
};

// Only the normal opponent owns this larger workspace; Problems retains SearchBuffers.
// Entries reuse move ordering, never scores that depend on repetition history.
struct OpponentBuffers {
  MoveList plies[kOpponentMaxPly + 1];
  MoveOrderEntry order[kMoveOrderEntries];
  Move killers[kOpponentMaxPly + 1][2];
  uint64_t pathKeys[kOpponentMaxPly + 1] = {};
};
static_assert(sizeof(OpponentBuffers) <= 20000, "Chess opponent workspace exceeds its memory budget");
static_assert((kMoveOrderEntries & (kMoveOrderEntries - 1)) == 0, "Move ordering needs a power-of-two table");

struct SearchControl {
  // Oldest first, including the current board if it is present in the caller's history.
  const uint64_t* history = nullptr;
  int historyCount = 0;
  // Called synchronously, before searching and at most every 256 nodes.
  // The caller may yield here; true stops and retains the last complete iteration.
  bool (*shouldStop)(void*) = nullptr;
  void* context = nullptr;
};

struct SearchResult {
  Move best{};
  int score = 0;
  uint32_t nodes = 0;
  uint8_t completedDepth = 0;
  // A stopped iteration is discarded. With no complete iteration, best is a
  // legal fallback and completedDepth is zero.
  bool budgetExhausted = false;  // Node ceiling or cooperative time budget.
  bool hasMove = false;
};

// Static evaluation in centipawns, positive when the side to move is better.
int evaluate(const Position& position);
int evaluate(const Position& position, Level level);

// Searches to `depth` and returns the best move for the side to move.
// `nodeBudget` caps the work; pass 0 for no cap. Returns hasMove == false only
// when the position has no legal moves (checkmate or stalemate).
SearchResult search(Position& position, int depth, SearchBuffers& buffers, uint32_t nodeBudget = 0);
SearchResult search(Position& position, Level level, OpponentBuffers& buffers, const SearchControl& control = {});

}  // namespace chess

#include "ChessEngine.h"

#include <algorithm>
#include <cstdlib>

namespace chess {
namespace {

// Centipawns. The king's value only has to dominate every other term; mate is
// scored separately, so its exact size does not matter.
constexpr int kPieceValue[7] = {0, 100, 320, 330, 500, 900, 20000};

// Piece-square tables, written from White's point of view with rank 1 first, so
// index == square index. They are what stop the engine playing aimlessly at
// this depth: without them it shuffles, because material is equal in most
// early positions.
// clang-format off
constexpr int8_t kPawnTable[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
   5, 10, 10,-20,-20, 10, 10,  5,
   5, -5,-10,  0,  0,-10, -5,  5,
   0,  0,  0, 20, 20,  0,  0,  0,
   5,  5, 10, 25, 25, 10,  5,  5,
  10, 10, 20, 30, 30, 20, 10, 10,
  50, 50, 50, 50, 50, 50, 50, 50,
   0,  0,  0,  0,  0,  0,  0,  0,
};
constexpr int8_t kKnightTable[64] = {
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -40,-20,  0,  0,  0,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50,
};
constexpr int8_t kBishopTable[64] = {
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10,-10,-10,-10,-10,-20,
};
constexpr int8_t kRookTable[64] = {
   0,  0,  0,  5,  5,  0,  0,  0,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   5, 10, 10, 10, 10, 10, 10,  5,
   0,  0,  0,  0,  0,  0,  0,  0,
};
constexpr int8_t kQueenTable[64] = {
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  5,  0,  0,  0,  0,-10,
 -10,  5,  5,  5,  5,  5,  0,-10,
   0,  0,  5,  5,  5,  5,  0, -5,
  -5,  0,  5,  5,  5,  5,  0, -5,
 -10,  0,  5,  5,  5,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20,
};
// Midgame king table: rewards castling and staying behind pawns. An endgame
// table would want the opposite, but at this depth it is not worth the phase
// detection.
constexpr int8_t kKingTable[64] = {
  20, 30, 10,  0,  0, 10, 30, 20,
  20, 20,  0,  0,  0,  0, 20, 20,
 -10,-20,-20,-20,-20,-20,-20,-10,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
};
// clang-format on

const int8_t* tableFor(const uint8_t type) {
  switch (type) {
    case Pawn:
      return kPawnTable;
    case Knight:
      return kKnightTable;
    case Bishop:
      return kBishopTable;
    case Rook:
      return kRookTable;
    case Queen:
      return kQueenTable;
    case King:
      return kKingTable;
    default:
      return nullptr;
  }
}

// Most Valuable Victim minus Least Valuable Attacker. Trying PxQ before QxP
// prunes far more of the tree, which matters much more than the ordering being
// clever: alpha-beta's whole benefit depends on searching good moves first.
int moveScore(const Position& position, const Move& move) {
  int score = 0;
  if ((move.flags & FlagCapture) != 0) {
    // static_cast keeps both arms uint8_t: Piece is an enum and pieceType()
    // returns the plain type, which -Wextra flags as a mixed conditional.
    const uint8_t victim =
        (move.flags & FlagEnPassant) != 0 ? static_cast<uint8_t>(Pawn) : pieceType(position.squares[move.to]);
    const uint8_t attacker = pieceType(position.squares[move.from]);
    score += 10000 + kPieceValue[victim] * 10 - kPieceValue[attacker];
  }
  if ((move.flags & FlagPromotion) != 0) score += 9000 + kPieceValue[move.promotion];
  return score;
}

// Selection sort of the single best remaining move. Cheaper than sorting the
// whole list, because a beta cutoff usually happens in the first few moves.
int orderedMoveScore(const Position& position, const Move& move, const Move* preferred, const Move* killers) {
  if (preferred != nullptr && move == *preferred) return 500000;
  if (killers != nullptr && !(move.flags & (FlagCapture | FlagPromotion))) {
    if (move == killers[0]) return 9000;
    if (move == killers[1]) return 8000;
  }
  return moveScore(position, move);
}

void pickBestMoveTo(const Position& position, MoveList& list, const int index, const Move* preferred = nullptr,
                    const Move* killers = nullptr) {
  int bestIndex = index;
  int bestScore = orderedMoveScore(position, list.moves[index], preferred, killers);
  for (int i = index + 1; i < list.count; ++i) {
    const int score = orderedMoveScore(position, list.moves[i], preferred, killers);
    if (score > bestScore) {
      bestScore = score;
      bestIndex = i;
    }
  }
  if (bestIndex != index) {
    const Move temp = list.moves[index];
    list.moves[index] = list.moves[bestIndex];
    list.moves[bestIndex] = temp;
  }
}

struct SearchContext {
  MoveList* plies;
  uint32_t nodes;
  uint32_t nodeBudget;
  bool budgetExhausted;
  int maxPly;
  OpponentBuffers* opponent;
  const SearchControl* control;
  int historyCount;
};

bool stopRequested(SearchContext& context) {
  if (context.nodeBudget != 0 && context.nodes >= context.nodeBudget) {
    context.budgetExhausted = true;
  } else if (context.opponent != nullptr && context.control != nullptr && context.control->shouldStop != nullptr &&
             (context.nodes & 255u) == 0 && context.control->shouldStop(context.control->context)) {
    context.budgetExhausted = true;
  }
  return context.budgetExhausted;
}

int staticScore(const Position& position, const SearchContext& context) {
  return context.opponent != nullptr ? evaluate(position, Level::Medium) : evaluate(position);
}

bool repeatedThreeTimes(const Position& position, const int ply, const SearchContext& context) {
  const uint64_t key = context.opponent->pathKeys[ply];
  int count = 1;
  for (int i = ply - 1; i >= 0 && ply - i <= position.halfmoveClock; --i) {
    if (context.opponent->pathKeys[i] == key && ++count == 3) return true;
  }
  if (context.control == nullptr || context.control->history == nullptr) return false;
  for (int i = context.historyCount - 1, distance = ply + 1; i >= 0 && distance <= position.halfmoveClock;
       --i, ++distance) {
    if (context.control->history[i] == key && ++count == 3) return true;
  }
  return false;
}

int negamax(Position& position, const int depth, int alpha, const int beta, const int ply, SearchContext& context) {
  if (stopRequested(context)) return staticScore(position, context);
  ++context.nodes;

  MoveList& list = context.plies[ply];
  generateLegalMoves(position, list);
  if (list.count == 0) {
    // Mate scores shrink with distance so the search prefers the quicker mate,
    // and prefers the slower one when it is getting mated.
    return isInCheck(position) ? -kMateScore + ply : 0;
  }

  const bool tactical = depth <= 0;
  const bool checked = isInCheck(position);
  uint64_t key = 0;
  Move preferred{};
  if (context.opponent != nullptr) {
    key = positionKey(position);
    context.opponent->pathKeys[ply] = key;
    if (position.halfmoveClock >= 100 || repeatedThreeTimes(position, ply, context)) return 0;
    const MoveOrderEntry& entry = context.opponent->order[key & (kMoveOrderEntries - 1)];
    if (entry.depth != 0 && entry.key == key) preferred = entry.best;
  }
  // Even at the extension cap, mate and stalemate have already been resolved.
  if (depth <= -kQuiescencePlies || ply >= context.maxPly) return staticScore(position, context);
  int best = -kMateScore * 2;
  Move bestMove = list.moves[0];
  if (tactical && !checked) {
    best = staticScore(position, context);
    if (best >= beta) return best;
    if (best > alpha) alpha = best;
  }
  const int count = list.count;
  for (int i = 0; i < count; ++i) {
    pickBestMoveTo(position, list, i, context.opponent != nullptr ? &preferred : nullptr,
                   context.opponent != nullptr ? context.opponent->killers[ply] : nullptr);
    // The move must be copied out: the recursive call reuses deeper plies, but
    // this ply's list stays put, and re-reading it after unmake is fine only
    // because nothing below touches index `i`.
    const Move move = list.moves[i];
    if (tactical && !checked && !(move.flags & (FlagCapture | FlagPromotion))) continue;
    Undo undo;
    makeMove(position, move, undo);
    int score;
    if (context.opponent != nullptr && !tactical && i != 0) {
      score = -negamax(position, depth - 1, -alpha - 1, -alpha, ply + 1, context);
      if (!context.budgetExhausted && score > alpha && score < beta) {
        score = -negamax(position, depth - 1, -beta, -alpha, ply + 1, context);
      }
    } else {
      score = -negamax(position, depth - 1, -beta, -alpha, ply + 1, context);
    }
    unmakeMove(position, move, undo);

    if (context.budgetExhausted) break;
    if (score > best) {
      best = score;
      bestMove = move;
    }
    if (best > alpha) alpha = best;
    if (alpha >= beta) {
      if (context.opponent != nullptr && !tactical && !(move.flags & (FlagCapture | FlagPromotion))) {
        Move* killers = context.opponent->killers[ply];
        if (!(killers[0] == move)) {
          killers[1] = killers[0];
          killers[0] = move;
        }
      }
      break;
    }
  }
  if (context.opponent != nullptr && !tactical && !context.budgetExhausted) {
    MoveOrderEntry& entry = context.opponent->order[key & (kMoveOrderEntries - 1)];
    if (entry.key == key || depth >= entry.depth) entry = {key, bestMove, static_cast<uint8_t>(depth)};
  }
  return best;
}

int distance(const int a, const int b) { return std::max(std::abs((a & 7) - (b & 7)), std::abs((a >> 3) - (b >> 3))); }

int mobility(const Position& position, const int square, const uint8_t type, const bool white) {
  struct Delta {
    int8_t file, rank;
  };
  static constexpr Delta knight[] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
  static constexpr Delta rays[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  int result = 0;
  const int file = square & 7, rank = square >> 3;
  if (type == Knight) {
    for (const auto d : knight) {
      const int f = file + d.file, r = rank + d.rank;
      if (f >= 0 && f < 8 && r >= 0 && r < 8 && !isColour(position.squares[r * 8 + f], white)) ++result;
    }
    return result * 4;
  }
  for (int direction = 0; direction < 8; ++direction) {
    if ((type == Bishop && direction < 4) || (type == Rook && direction >= 4)) continue;
    const auto d = rays[direction];
    for (int f = file + d.file, r = rank + d.rank; f >= 0 && f < 8 && r >= 0 && r < 8; f += d.file, r += d.rank) {
      const uint8_t piece = position.squares[r * 8 + f];
      if (isColour(piece, white)) break;
      ++result;
      if (piece != Empty) break;
    }
  }
  return result * (type == Bishop ? 4 : type == Rook ? 2 : 1);
}

bool passedPawn(const Position& position, const int square, const bool white) {
  const int file = square & 7, step = white ? 1 : -1;
  for (int rank = (square >> 3) + step; rank >= 0 && rank < 8; rank += step) {
    for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); ++f) {
      const uint8_t piece = position.squares[rank * 8 + f];
      if (pieceType(piece) == Pawn && isColour(piece, !white)) return false;
    }
  }
  return true;
}

int strategicEvaluation(const Position& position) {
  static constexpr int phaseWeight[] = {0, 0, 1, 1, 2, 4, 0};
  static constexpr int passedBonus[] = {0, 5, 12, 24, 44, 75, 120, 0};
  uint8_t pawns[2][8] = {}, bishops[2] = {};
  int kings[2] = {-1, -1}, material[2] = {};
  int phase = 0, middle = 0, end = 0;
  for (int square = 0; square < 64; ++square) {
    const uint8_t piece = position.squares[square], type = pieceType(piece);
    if (piece == Empty) continue;
    const int colour = isBlack(piece) ? 1 : 0;
    phase += phaseWeight[type];
    if (type == Pawn) ++pawns[colour][square & 7];
    if (type == Bishop) ++bishops[colour];
    if (type == King)
      kings[colour] = square;
    else
      material[colour] += kPieceValue[type];
  }
  phase = std::min(24, phase);
  for (int square = 0; square < 64; ++square) {
    const uint8_t piece = position.squares[square], type = pieceType(piece);
    if (piece == Empty) continue;
    const bool white = !isBlack(piece);
    const int colour = white ? 0 : 1, sign = white ? 1 : -1;
    const int file = square & 7, rank = white ? square >> 3 : 7 - (square >> 3);
    const int8_t* table = tableFor(type);
    int mg = kPieceValue[type] + (table != nullptr ? table[rank * 8 + file] : 0);
    int eg = mg;
    switch (type) {
      case Pawn: {
        const bool isolated =
            (file == 0 || pawns[colour][file - 1] == 0) && (file == 7 || pawns[colour][file + 1] == 0);
        const int penalty = (pawns[colour][file] - 1) * 8 + (isolated ? 12 : 0);
        mg -= penalty;
        eg = kPieceValue[Pawn] + rank * 8 - penalty;
        if (passedPawn(position, square, white)) {
          mg += passedBonus[rank] / 2;
          eg += passedBonus[rank];
        }
        if (kings[colour] >= 0) eg += (7 - distance(kings[colour], square)) * 3;
        break;
      }
      case King: {
        eg = kPieceValue[King] - 30 + 12 * (std::min(file, 7 - file) + std::min(rank, 7 - rank));
        const int ahead = (square >> 3) + (white ? 1 : -1);
        int shield = 0;
        if (ahead >= 0 && ahead < 8) {
          for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); ++f) {
            if (position.squares[ahead * 8 + f] == (Pawn | (white ? 0 : BlackFlag))) ++shield;
          }
        }
        mg += shield * 9 - 18;
        break;
      }
      case Rook:
        if (pawns[colour][file] == 0) {
          mg += 18 + (pawns[1 - colour][file] == 0 ? 12 : 0);
          eg += 12;
        }
        [[fallthrough]];
      case Knight:
      case Bishop:
      case Queen: {
        const int activity = mobility(position, square, type, white);
        mg += activity;
        eg += activity;
        break;
      }
      case Empty:
        break;
    }
    middle += sign * mg;
    end += sign * eg;
  }
  for (int colour = 0; colour < 2; ++colour) {
    const int sign = colour == 0 ? 1 : -1;
    if (bishops[colour] >= 2) {
      middle += sign * 30;
      end += sign * 45;
    }
    if (phase <= 4 && material[colour] >= 500 && material[1 - colour] == 0 && kings[0] >= 0 && kings[1] >= 0) {
      const int defender = kings[1 - colour];
      const int edge =
          std::min(std::min(defender & 7, 7 - (defender & 7)), std::min(defender >> 3, 7 - (defender >> 3)));
      end += sign * ((3 - edge) * 20 + (7 - distance(kings[0], kings[1])) * 8);
    }
  }
  const int score = (middle * phase + end * (24 - phase)) / 24;
  return (position.whiteToMove ? score : -score) + 10;
}

}  // namespace

int evaluate(const Position& position) {
  int score = 0;
  for (int square = 0; square < 64; ++square) {
    const uint8_t piece = position.squares[square];
    if (piece == Empty) continue;
    const uint8_t type = pieceType(piece);
    const bool white = !isBlack(piece);
    // Black reads the same table from the mirrored square.
    const int tableIndex = white ? square : ((7 - (square >> 3)) * 8 + (square & 7));
    const int8_t* table = tableFor(type);
    const int value = kPieceValue[type] + (table != nullptr ? table[tableIndex] : 0);
    score += white ? value : -value;
  }
  return position.whiteToMove ? score : -score;
}

int evaluate(const Position& position, const Level level) {
  return level == Level::Easy ? evaluate(position) : strategicEvaluation(position);
}

namespace {

SearchResult runSearch(Position& position, int depth, MoveList* plies, const uint32_t nodeBudget, const int maxDepth,
                       OpponentBuffers* opponent, const SearchControl* control) {
  SearchResult result{};
  if (depth < 1) depth = 1;
  if (depth > maxDepth) depth = maxDepth;

  MoveList& root = plies[0];
  generateLegalMoves(position, root);
  if (root.count == 0) return result;

  SearchContext context{plies, 0, nodeBudget, false, maxDepth + kQuiescencePlies, opponent, control, 0};
  if (opponent != nullptr) {
    for (auto& entry : opponent->order) entry.depth = 0;
    for (auto& killers : opponent->killers) {
      killers[0] = {};
      killers[1] = {};
    }
    opponent->pathKeys[0] = positionKey(position);
    if (control != nullptr && control->history != nullptr) {
      context.historyCount = std::max(0, control->historyCount);
      if (context.historyCount != 0 && control->history[context.historyCount - 1] == opponent->pathKeys[0]) {
        --context.historyCount;
      }
    }
  }
  pickBestMoveTo(position, root, 0);
  result.best = root.moves[0];
  result.hasMove = true;
  result.score = staticScore(position, context);
  const int count = root.count;
  for (int iteration = 1; iteration <= depth; ++iteration) {
    // Search the previous complete iteration's best move first.
    for (int i = 0; i < count; ++i) {
      if (root.moves[i].from == result.best.from && root.moves[i].to == result.best.to &&
          root.moves[i].promotion == result.best.promotion) {
        const Move temp = root.moves[0];
        root.moves[0] = root.moves[i];
        root.moves[i] = temp;
        break;
      }
    }
    int alpha = -kMateScore * 2;
    const int beta = kMateScore * 2;
    Move best = result.best;
    for (int i = 0; i < count; ++i) {
      if (i) pickBestMoveTo(position, root, i);
      const Move move = root.moves[i];
      Undo undo;
      makeMove(position, move, undo);
      int score;
      if (opponent != nullptr && i != 0) {
        score = -negamax(position, iteration - 1, -alpha - 1, -alpha, 1, context);
        if (!context.budgetExhausted && score > alpha) {
          score = -negamax(position, iteration - 1, -beta, -alpha, 1, context);
        }
      } else {
        score = -negamax(position, iteration - 1, -beta, -alpha, 1, context);
      }
      unmakeMove(position, move, undo);
      if (context.budgetExhausted) break;
      if (score > alpha) {
        best = move;
        alpha = score;
      }
    }
    if (context.budgetExhausted) break;
    result.best = best;
    result.score = alpha;
    result.completedDepth = static_cast<uint8_t>(iteration);
    if (alpha >= kMateThreshold) break;
  }

  result.nodes = context.nodes;
  result.budgetExhausted = context.budgetExhausted;
  return result;
}

}  // namespace

SearchResult search(Position& position, const int depth, SearchBuffers& buffers, const uint32_t nodeBudget) {
  return runSearch(position, depth, buffers.plies, nodeBudget, kMaxSearchDepth, nullptr, nullptr);
}

SearchResult search(Position& position, const Level level, OpponentBuffers& buffers, const SearchControl& control) {
  const SearchLimits limits = limitsForLevel(level);
  return runSearch(position, limits.depth, buffers.plies, limits.nodes,
                   level == Level::Easy ? kMaxSearchDepth : kOpponentMaxDepth,
                   level == Level::Easy ? nullptr : &buffers, &control);
}

}  // namespace chess

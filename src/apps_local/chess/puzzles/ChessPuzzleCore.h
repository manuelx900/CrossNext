#pragma once

#include <cstddef>
#include <cstdint>

#include "../ChessCore.h"
#include "../ChessEngine.h"

namespace chesspuzzle {
constexpr int kCount = 1000;
constexpr int kGroupCounts[] = {400, 350, 250};
constexpr int kGroupStarts[] = {0, 400, 750};
constexpr size_t kProgressBytes = 152;
struct Puzzle {
  uint8_t board[32];
  uint16_t moves[5];
  char id[6];
  uint16_t rating;
  uint8_t castling;
  int8_t epSquare;
  uint8_t plies;
  bool white;
};
const Puzzle* puzzle(int index);
enum class Status : uint8_t { Playing, Thinking, Correct, Illegal, Solved, Lost, Draw, DataError };
struct Session {
  chess::Position position;
  chess::MoveList legal;
  const Puzzle* source = nullptr;
  Status status = Status::DataError;
  int ply = 0;
  uint16_t playerMoves = 0;
  bool onLine = true;
  int selected = -1;
  int lastFrom = -1, lastTo = -1;
};
struct Progress {
  uint16_t cursor[3]{};
  uint8_t solved[(kCount + 7) / 8]{};
  uint16_t lastIndex = kCount;
};
bool load(Session& session, int index);
bool play(Session& session, int from, int to, int promotion = chess::Queen);
bool reply(Session& session, chess::SearchBuffers& buffers);
bool terminal(const Session& session);
int goal(const Session& session);
int remaining(const Session& session);
bool solved(const Progress& progress, int index);
bool markSolved(Progress& progress, int index);
int solvedCount(const Progress& progress, int group);
void encode(const Progress& progress, uint32_t generation, uint8_t* bytes);
bool decode(const uint8_t* bytes, Progress& progress, uint32_t& generation);
}  // namespace chesspuzzle

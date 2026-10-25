#pragma once
#include <cstddef>
#include <cstdint>

namespace pocketgames {
enum class Kind : uint8_t { Othello = 1 };
constexpr size_t kSaveBytes = 256;
struct Othello {
  uint8_t board[64]{};
  uint8_t turn = 1;
  bool over = false, passed = false;
};
struct State {
  Kind kind = Kind::Othello;
  uint8_t level = 0;
  uint8_t humanColour = 1;
  uint32_t seed = 1;
  Othello othello;
};
uint32_t random(uint32_t& state);
void start(State& state, Kind kind, uint32_t seed, int level = 0);
int flips(const Othello& game, int square, int colour);
bool hasMove(const Othello& game, int colour);
bool play(Othello& game, int square);
int chooseBotMove(const Othello& game, int level = 1);
bool bot(Othello& game, int level = 1);
int disks(const Othello& game, int colour);
bool finished(const State& state);
void encode(const State& state, uint32_t generation, uint8_t* output);
bool decode(const uint8_t* input, Kind kind, State& state, uint32_t& generation);
}  // namespace pocketgames

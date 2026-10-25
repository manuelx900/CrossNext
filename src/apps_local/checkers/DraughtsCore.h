#pragma once
#include <cstdint>

namespace draughts {
enum class Rules : uint8_t { International, Brazilian, Spanish, English };
constexpr int kMaxOptions = 800;
constexpr uint8_t kTaken = 8;
struct Move {
  uint8_t from = 0, to = 0;
  int8_t over = -1;
};
struct Option {
  Move move;
  uint16_t score = 0;
};
struct Game {
  uint8_t cells[100]{};
  Rules rules = Rules::International;
  uint8_t size = 10, turn = 0, idle = 0, endLimit = 0;
  uint8_t endRemaining[2]{}, unchanged[2]{};
  uint32_t material = 0;
  int8_t forced = -1;
  bool manMoved = false, captured = false, draw = false;
  uint64_t history[81]{};
  uint8_t historyCount = 0, historyNext = 0;
};
int owner(uint8_t piece);
bool king(uint8_t piece);
void start(Game&, Rules = Rules::International);
bool nextCapture(const Game&, int from, int& cursor, Move&);
bool nextQuiet(const Game&, int from, int& cursor, Move&);
// Activity-owned iterative search yields after a bounded number of nodes. It
// never truncates capture chains or legal choices to meet a timing budget.
class Generator {
 public:
  void begin(const Game&);
  bool tick(int nodes = 512);
  bool ready() const { return complete; }
  int count() const { return complete ? used : 0; }
  const Option& at(int index) const { return options[index]; }
  bool permits(int from, int to) const;

 private:
  struct Frame {
    Move incoming;
    uint8_t pos = 0, cursor = 0;
    uint16_t score = 0;
  };
  Game source, trial;
  Option options[kMaxOptions]{};
  Frame stack[21]{};
  int used = 0, current = -1, depth = 0;
  uint16_t best = 0;
  bool complete = true, captures = false;
  void enterRoot();
};
bool play(Game&, const Generator&, int from, int to);
int choose(const Game&, const Generator&, int difficulty, uint32_t& random);
}  // namespace draughts

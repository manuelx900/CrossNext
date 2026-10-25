#include "DraughtsCore.h"

#include <algorithm>

namespace draughts {
int owner(uint8_t piece) { return piece ? ((piece & 7) - 1) % 2 : -1; }
bool king(uint8_t piece) { return (piece & 7) >= 3; }
namespace {
bool inside(const Game& g, int x, int y) { return x >= 0 && y >= 0 && x < g.size && y < g.size; }
bool promotion(const Game& g, int pos) { return pos / g.size == (g.turn == 0 ? 0 : g.size - 1); }
bool stopsOnCrown(const Game& g, int pos) {
  return !king(g.cells[pos]) && promotion(g, pos) && (g.rules == Rules::English || g.rules == Rules::Spanish);
}
uint16_t weight(const Game& g, int over) { return 32 + (g.rules == Rules::Spanish && king(g.cells[over]) ? 1 : 0); }
void push(Game& g, Move m) {
  g.cells[m.to] = g.cells[m.from];
  g.cells[m.from] = 0;
  if (m.over >= 0) g.cells[m.over] |= kTaken;
}
void pop(Game& g, Move m) {
  g.cells[m.from] = g.cells[m.to];
  g.cells[m.to] = 0;
  if (m.over >= 0) g.cells[m.over] &= ~kTaken;
}
uint64_t hash(const Game& g) {
  uint64_t value = 1469598103934665603ULL ^ g.turn;
  for (int n = 0; n < g.size * g.size; ++n) value = (value ^ g.cells[n]) * 1099511628211ULL;
  return value;
}
void remember(Game& g) {
  const auto value = hash(g);
  int repeats = 0;
  for (int n = 0; n < g.historyCount; ++n)
    if (g.history[n] == value) ++repeats;
  g.draw |= repeats >= 2;
  g.history[g.historyNext] = value;
  g.historyNext = (g.historyNext + 1) % 81;
  g.historyCount = std::min(81, int(g.historyCount) + 1);
}
void finish(Game& g, int pos) {
  for (int n = 0; n < g.size * g.size; ++n)
    if (g.cells[n] & kTaken) g.cells[n] = 0;
  const bool crowned = !king(g.cells[pos]) && promotion(g, pos);
  if (crowned) g.cells[pos] += 2;
  if (g.captured || g.manMoved) {
    g.idle = 0;
    g.historyCount = g.historyNext = 0;
  } else
    ++g.idle;
  g.draw |= g.idle >= (g.rules == Rules::International ? 50
                       : g.rules == Rules::Brazilian   ? 30
                       : g.rules == Rules::Spanish     ? 40
                                                       : 80);
  int men[2]{}, kings[2]{};
  for (int n = 0; n < g.size * g.size; ++n)
    if (g.cells[n]) {
      if (king(g.cells[n]))
        ++kings[owner(g.cells[n])];
      else
        ++men[owner(g.cells[n])];
    }
  int limit = 0;
  for (int side = 0; side < 2; ++side)
    if (kings[side] == 1 && !men[side]) {
      const int other = 1 - side, total = kings[other] + men[other];
      if (g.rules == Rules::International && kings[other]) {
        if (total <= 2)
          limit = 5;
        else if (total == 3)
          limit = 16;
      } else if (g.rules == Rules::Brazilian && kings[other]) {
        bool longDiagonal = false;
        for (int n = 0; n < 64; ++n)
          if (owner(g.cells[n]) == side && n / 8 + n % 8 == 7) longDiagonal = true;
        if (total <= 2 || (total == 3 && longDiagonal))
          limit = 5;
        else if (kings[other] >= 3)
          limit = 15;
      } else if (g.rules == Rules::Spanish && kings[other] == 3 && !men[other]) {
        for (int n = 0; n < 64; ++n)
          if (owner(g.cells[n]) == other && n / 8 + n % 8 == 7) limit = 13;
      }
    }
  if (limit) {
    for (int side = 0; side < 2; ++side) {
      if (g.endLimit && side == g.turn && g.endRemaining[side]) --g.endRemaining[side];
      g.endRemaining[side] = g.endLimit ? std::min<int>(g.endRemaining[side], limit) : limit;
    }
    g.endLimit = limit;
    g.draw |= g.endRemaining[0] == 0 && g.endRemaining[1] == 0;
  } else {
    g.endLimit = 0;
    g.endRemaining[0] = g.endRemaining[1] = 0;
  }
  const uint32_t material = men[0] + 21 * men[1] + 441 * kings[0] + 9261 * kings[1];
  if (g.captured || crowned || g.material != material)
    g.unchanged[0] = g.unchanged[1] = 0;
  else if (g.unchanged[g.turn] < 120)
    ++g.unchanged[g.turn];
  g.material = material;
  if (g.rules == Rules::Brazilian && kings[0] && kings[1]) {
    const int total = men[0] + men[1] + kings[0] + kings[1];
    const int moves = total <= 5 ? 30 : total <= 7 ? 60 : 0;
    if (moves) g.draw |= g.unchanged[0] >= moves && g.unchanged[1] >= moves;
  }
  g.turn = 1 - g.turn;
  g.forced = -1;
  g.manMoved = g.captured = false;
  remember(g);
}
}  // namespace
void start(Game& g, Rules rules) {
  g = {};
  g.rules = rules;
  g.size = rules == Rules::International ? 10 : 8;
  for (int y = 0; y < g.size; ++y)
    for (int x = 0; x < g.size; ++x)
      if ((x + y) % 2) {
        if (y < g.size / 2 - 1) g.cells[y * g.size + x] = 2;
        if (y > g.size / 2) g.cells[y * g.size + x] = 1;
      }
  remember(g);
}
bool nextCapture(const Game& g, int from, int& cursor, Move& out) {
  if (from < 0 || from >= g.size * g.size || !g.cells[from] || (g.cells[from] & kTaken)) return false;
  const bool flying = king(g.cells[from]) && g.rules != Rules::English;
  const int forward = owner(g.cells[from]) == 0 ? -1 : 1;
  for (; cursor < 4 * g.size;) {
    const int code = cursor++, dir = code / g.size, distance = code % g.size + 1;
    const int dx = dir % 2 ? 1 : -1, dy = dir / 2 ? 1 : -1;
    if (distance < 2 || (!flying && distance != 2)) continue;
    if (!king(g.cells[from]) && (g.rules == Rules::English || g.rules == Rules::Spanish) && dy != forward) continue;
    const int x = from % g.size + dx * distance, y = from / g.size + dy * distance;
    if (!inside(g, x, y) || g.cells[y * g.size + x]) continue;
    int over = -1;
    bool blocked = false;
    for (int d = 1; d < distance; ++d) {
      const int at = (from / g.size + dy * d) * g.size + from % g.size + dx * d;
      if (!g.cells[at]) continue;
      if (over >= 0 || (g.cells[at] & kTaken) || owner(g.cells[at]) == owner(g.cells[from])) {
        blocked = true;
        break;
      }
      over = at;
    }
    if (!blocked && over >= 0) {
      out = {uint8_t(from), uint8_t(y * g.size + x), int8_t(over)};
      return true;
    }
  }
  return false;
}
bool nextQuiet(const Game& g, int from, int& cursor, Move& out) {
  if (!g.cells[from] || (g.cells[from] & kTaken)) return false;
  const bool flying = king(g.cells[from]) && g.rules != Rules::English;
  const int forward = owner(g.cells[from]) == 0 ? -1 : 1;
  for (; cursor < 4 * g.size;) {
    const int code = cursor++, dir = code / g.size, distance = code % g.size + 1;
    const int dx = dir % 2 ? 1 : -1, dy = dir / 2 ? 1 : -1;
    if ((!flying && distance != 1) || (!king(g.cells[from]) && dy != forward)) continue;
    const int x = from % g.size + dx * distance, y = from / g.size + dy * distance;
    if (!inside(g, x, y)) continue;
    bool clear = true;
    for (int d = 1; d <= distance; ++d)
      if (g.cells[(from / g.size + dy * d) * g.size + from % g.size + dx * d]) {
        clear = false;
        break;
      }
    if (clear) {
      out = {uint8_t(from), uint8_t(y * g.size + x), -1};
      return true;
    }
  }
  return false;
}
void Generator::begin(const Game& g) {
  source = g;
  used = 0;
  current = -1;
  complete = false;
  captures = false;
  for (int n = 0; n < g.size * g.size; ++n)
    if (owner(g.cells[n]) == g.turn && (g.forced < 0 || n == g.forced)) {
      int cursor = 0;
      Move m;
      while (nextCapture(g, n, cursor, m)) {
        options[used++] = {m, 0};
        captures = true;
      }
    }
  if (!captures) {
    for (int n = 0; n < g.size * g.size; ++n)
      if (owner(g.cells[n]) == g.turn) {
        int cursor = 0;
        Move m;
        while (nextQuiet(g, n, cursor, m)) options[used++] = {m, 0};
      }
    complete = true;
    return;
  }
  enterRoot();
}
void Generator::enterRoot() {
  ++current;
  if (current >= used) {
    if (source.rules != Rules::English) {
      uint16_t maximum = 0;
      for (int n = 0; n < used; ++n) maximum = std::max(maximum, options[n].score);
      int kept = 0;
      for (int n = 0; n < used; ++n)
        if (options[n].score == maximum) options[kept++] = options[n];
      used = kept;
    }
    complete = true;
    return;
  }
  trial = source;
  depth = 0;
  best = 0;
  const auto move = options[current].move;
  stack[0] = {move, move.to, 0, weight(trial, move.over)};
  push(trial, move);
}
bool Generator::tick(int nodes) {
  while (!complete && nodes-- > 0) {
    auto& at = stack[depth];
    int cursor = at.cursor;
    Move next;
    if (!stopsOnCrown(trial, at.pos) && nextCapture(trial, at.pos, cursor, next)) {
      at.cursor = cursor;
      const auto score = uint16_t(at.score + weight(trial, next.over));
      push(trial, next);
      ++depth;
      stack[depth] = {next, next.to, 0, score};
    } else {
      best = std::max(best, at.score);
      if (depth == 0) {
        options[current].score = best;
        enterRoot();
      } else {
        pop(trial, at.incoming);
        --depth;
      }
    }
  }
  return complete;
}
bool Generator::permits(int from, int to) const {
  if (!complete) return false;
  for (int n = 0; n < used; ++n)
    if (options[n].move.from == from && options[n].move.to == to) return true;
  return false;
}
bool play(Game& g, const Generator& legal, int from, int to) {
  if (g.draw || !legal.permits(from, to)) return false;
  Move move;
  for (int n = 0; n < legal.count(); ++n)
    if (legal.at(n).move.from == from && legal.at(n).move.to == to) {
      move = legal.at(n).move;
      break;
    }
  g.manMoved |= !king(g.cells[from]);
  g.captured |= move.over >= 0;
  push(g, move);
  int cursor = 0;
  Move next;
  if (move.over >= 0 && !stopsOnCrown(g, to) && nextCapture(g, to, cursor, next))
    g.forced = to;
  else
    finish(g, to);
  return true;
}
int choose(const Game& g, const Generator& legal, int difficulty, uint32_t& random) {
  if (!legal.count()) return -1;
  random ^= random << 13;
  random ^= random >> 17;
  random ^= random << 5;
  if (difficulty == 0) return random % legal.count();
  int best = -100000, selected = 0;
  for (int n = 0; n < legal.count(); ++n) {
    const auto& option = legal.at(n);
    const int x = option.move.to % g.size, y = option.move.to / g.size;
    int score = option.score * 8 + (g.turn == 0 ? g.size - y : y) + std::min(x, g.size - 1 - x);
    if (!king(g.cells[option.move.from]) && promotion(g, option.move.to)) score += 100;
    if (difficulty >= 2) {
      // Prefer protected squares and avoid immediate diagonal captures.
      for (int dy : {-1, 1})
        for (int dx : {-1, 1}) {
          const int nx = x + dx, ny = y + dy, bx = x - dx, by = y - dy;
          if (!inside(g, nx, ny)) continue;
          const auto piece = g.cells[ny * g.size + nx];
          if (owner(piece) == g.turn) score += 8;
          if (piece && owner(piece) != g.turn && inside(g, bx, by) &&
              (!g.cells[by * g.size + bx] || by * g.size + bx == option.move.from))
            score -= 120;
        }
      if (!x || x == g.size - 1) score += 12;
    }
    if (score > best) {
      best = score;
      selected = n;
    }
  }
  return selected;
}
}  // namespace draughts

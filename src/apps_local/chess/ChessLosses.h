#pragma once
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "ChessCore.h"
namespace chessui {
struct Losses {
  uint8_t counts[2][7]{};
  void capture(uint8_t piece) {
    const auto type = chess::pieceType(piece);
    if (type >= chess::Pawn && type <= chess::Queen) {
      auto& count = counts[chess::isBlack(piece) ? 1 : 0][type];
      if (count < 15) ++count;
    }
  }
  void undo(uint8_t piece) {
    const auto type = chess::pieceType(piece);
    if (type >= chess::Pawn && type <= chess::Queen) {
      auto& count = counts[chess::isBlack(piece) ? 1 : 0][type];
      if (count) --count;
    }
  }
  void estimateLegacy(const chess::Position& p) {
    static constexpr int initial[] = {0, 8, 2, 2, 2, 1, 1};
    for (int c = 0; c < 2; ++c)
      for (int type = 1; type <= 5; ++type) {
        int present = 0;
        for (auto piece : p.squares)
          if (piece != chess::Empty && int(chess::pieceType(piece)) == type && chess::isBlack(piece) == bool(c))
            ++present;
        counts[c][type] = std::max(0, initial[type] - present);
      }
  }
  bool decode(const char* line) {
    if (!line) return false;
    Losses trial;
    for (int c = 0; c < 2; ++c)
      for (int type = 1; type <= 5; ++type) {
        char* end = nullptr;
        const long n = std::strtol(line, &end, 10);
        if (end == line || n < 0 || n > 15) return false;
        trial.counts[c][type] = uint8_t(n);
        line = end;
      }
    while (*line == ' ') ++line;
    if (*line) return false;
    *this = trial;
    return true;
  }
  void encode(char* out, size_t size) const {
    std::snprintf(out, size, "%u %u %u %u %u %u %u %u %u %u", counts[0][1], counts[0][2], counts[0][3], counts[0][4],
                  counts[0][5], counts[1][1], counts[1][2], counts[1][3], counts[1][4], counts[1][5]);
  }
};
}  // namespace chessui

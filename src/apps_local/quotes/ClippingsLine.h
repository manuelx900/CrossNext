#pragma once
#include <cstdint>

namespace quotes {
// Only separator recognition needs to survive between bounded scan batches.
class ClippingsLine {
 public:
  bool consume(char c) {
    if (c == '\r') return false;
    if (c == '\n') {
      const bool separator = equals && length == 10;
      length = 0;
      equals = true;
      return separator;
    }
    equals &= c == '=';
    if (length < 11) ++length;
    return false;
  }

 private:
  uint8_t length = 0;
  bool equals = true;
};
}  // namespace quotes

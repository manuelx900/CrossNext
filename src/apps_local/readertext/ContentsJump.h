#pragma once
#include <cstdint>

namespace readertext {
struct ContentsPosition {
  int spine = 0;
  uint32_t page = 0, offset = UINT32_MAX;
};
class ContentsJump {
 public:
  void remember(ContentsPosition position) {
    source = position;
    active = true;
    settled = false;
  }
  bool rendered(ContentsPosition position) {
    if (!active) return false;
    if (!settled) {
      target = position;
      settled = true;
    } else if (position.spine != target.spine || position.page != target.page) {
      clear();
    }
    return active;
  }
  void clear() { active = settled = false; }
  ContentsPosition origin() const { return source; }

 private:
  ContentsPosition source, target;
  bool active = false, settled = false;
};
}  // namespace readertext

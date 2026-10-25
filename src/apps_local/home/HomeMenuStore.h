#pragma once

#include "HomeMenuCore.h"

namespace homemenu {
class Store {
 public:
  static Store& instance();
  void load();
  const Config& get() const { return current; }
  bool save(const Config& next);

 private:
  Config current;
  Config scratch;
  uint8_t buffer[kSaveBytes]{};
  uint32_t generation = 0;
  int slot = -1;
  bool loaded = false;
  bool readOnly = false;
  size_t readLength = 0;
  bool read(int index);
};
const Config& config();
}  // namespace homemenu

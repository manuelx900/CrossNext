#pragma once
#include "PomodoroCore.h"
namespace pomodoro {
class Store {
 public:
  bool load(State& state);
  bool save(const State& state);

 private:
  uint8_t buffer[kSaveBytes]{};
  State scratch;
  uint32_t generation = 0, savedHash = 0;
  int slot = -1;
  bool readOnly = false;
  bool read(int index);
  uint32_t hash(const State& state);
};
}  // namespace pomodoro

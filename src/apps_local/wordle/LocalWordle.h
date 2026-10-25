#pragma once
#include <cstddef>
#include <cstdint>

#include "WordleCore.h"

namespace localwordle {
constexpr size_t kSaveBytes = 256;
constexpr uint8_t kVocabularyVersion = 2;
struct Record {
  uint32_t played = 0, won = 0, streak = 0, maximum = 0;
  uint32_t distribution[6]{};
};
struct State {
  wordle::Game game;
  Record record;
  uint32_t seed = 1;
  uint16_t next[3]{};
  uint8_t letters = 5, language = 0, gameLanguage = 0;
  bool active = false, recorded = false;
};
int wordCount(int letters, int language = 0);
const char* wordAt(int letters, int index, int language = 0);
bool contains(int letters, const char* word, int language = 0);
void start(State& state);
void record(State& state, bool abandoned = false);
void encode(const State& state, uint32_t generation, uint8_t* bytes);
bool decode(const uint8_t* bytes, State& state, uint32_t& generation);

class Store {
 public:
  bool load(State& state);
  bool save(const State& state);

 private:
  bool read(int slot);
  void importLegacy(State& state);
  uint8_t buffer[kSaveBytes]{};
  State scratch;
  uint32_t generation = 0, savedHash = 0;
  int slot = -1;
  bool readOnly = false;
};
}  // namespace localwordle

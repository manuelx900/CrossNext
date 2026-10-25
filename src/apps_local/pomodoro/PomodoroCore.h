#pragma once
#include <cstddef>
#include <cstdint>

namespace pomodoro {
enum class Phase : uint8_t { Focus, ShortBreak, LongBreak };
struct Config {
  uint16_t focus = 25, shortBreak = 5, longBreak = 15;
  uint8_t rounds = 4;
  bool autoBreak = false, autoFocus = false;
};
struct State {
  Config config;
  Phase phase = Phase::Focus;
  uint8_t stage = 0;
  uint32_t remainingMs = 25 * 60000, completed = 0;
  bool running = false, notice = false;
};
constexpr size_t kSaveBytes = 64;
uint32_t duration(const Config& config, Phase phase);
bool valid(const State& state);
bool advance(State& state, uint32_t elapsedMs);
Phase phaseAt(const Config& config, int stage);
void selectStage(State& state, int stage);
void skip(State& state);
void reset(State& state);
void configure(State& state, int field, int delta);
void encode(const State& state, uint32_t generation, uint8_t* data);
bool decode(const uint8_t* data, State& state, uint32_t& generation);
}  // namespace pomodoro

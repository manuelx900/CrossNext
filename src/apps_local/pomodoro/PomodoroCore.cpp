#include "PomodoroCore.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace pomodoro {
namespace {
void put(uint8_t* p, uint32_t v) {
  for (int n = 0; n < 4; ++n) p[n] = static_cast<uint8_t>(v >> (8 * n));
}
uint32_t get(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint32_t checksum(const uint8_t* data) {
  uint32_t value = 2166136261u;
  for (size_t n = 0; n < kSaveBytes - 4; ++n) value = (value ^ data[n]) * 16777619u;
  return value;
}
void next(State& state, bool earned) {
  if (earned && state.phase == Phase::Focus && state.completed < std::numeric_limits<uint32_t>::max())
    ++state.completed;
  state.stage = (state.stage + 1) % (2 * state.config.rounds);
  state.phase = phaseAt(state.config, state.stage);
  state.remainingMs = duration(state.config, state.phase);
  state.running = earned && (state.phase == Phase::Focus ? state.config.autoFocus : state.config.autoBreak);
  state.notice = earned;
}
}  // namespace
Phase phaseAt(const Config& config, int stage) {
  return stage % 2 == 0 ? Phase::Focus : stage == 2 * config.rounds - 1 ? Phase::LongBreak : Phase::ShortBreak;
}
void selectStage(State& state, int stage) {
  if (stage < 0 || stage >= 2 * state.config.rounds) return;
  state.stage = static_cast<uint8_t>(stage);
  state.phase = phaseAt(state.config, stage);
  reset(state);
}
uint32_t duration(const Config& config, Phase phase) {
  switch (phase) {
    case Phase::Focus:
      return config.focus * 60000u;
    case Phase::ShortBreak:
      return config.shortBreak * 60000u;
    case Phase::LongBreak:
      return config.longBreak * 60000u;
  }
  return 0;
}
bool valid(const State& state) {
  const auto& c = state.config;
  return static_cast<int>(state.phase) <= 2 && c.focus >= 1 && c.focus <= 180 && c.shortBreak >= 1 &&
         c.shortBreak <= 60 && c.longBreak >= 1 && c.longBreak <= 120 && c.rounds >= 1 && c.rounds <= 12 &&
         state.stage < 2 * c.rounds && state.phase == phaseAt(c, state.stage) && state.remainingMs > 0 &&
         state.remainingMs <= duration(c, state.phase);
}
bool advance(State& state, uint32_t elapsedMs) {
  if (!state.running || !elapsedMs) return false;
  if (elapsedMs < state.remainingMs) {
    state.remainingMs -= elapsedMs;
    return false;
  }
  next(state, true);
  return true;
}
void skip(State& state) { next(state, false); }
void reset(State& state) {
  state.running = state.notice = false;
  state.remainingMs = duration(state.config, state.phase);
}
void configure(State& state, int field, int delta) {
  if (state.running) return;
  const auto oldDuration = duration(state.config, state.phase);
  auto& c = state.config;
  switch (field) {
    case 0:
      c.focus = static_cast<uint16_t>(std::clamp(int(c.focus) + delta, 1, 180));
      break;
    case 1:
      c.shortBreak = static_cast<uint16_t>(std::clamp(int(c.shortBreak) + delta, 1, 60));
      break;
    case 2:
      c.longBreak = static_cast<uint16_t>(std::clamp(int(c.longBreak) + delta, 1, 120));
      break;
    case 3:
      c.rounds = static_cast<uint8_t>(std::clamp(int(c.rounds) + delta, 1, 12));
      break;
    case 4:
      c.autoBreak = !c.autoBreak;
      break;
    case 5:
      c.autoFocus = !c.autoFocus;
      break;
    default:
      return;
  }
  state.stage = std::min<int>(state.stage, 2 * c.rounds - 1);
  const auto phase = phaseAt(c, state.stage);
  if (phase != state.phase) {
    state.phase = phase;
    reset(state);
  }
  if (oldDuration != duration(c, state.phase)) reset(state);
}
void encode(const State& state, uint32_t generation, uint8_t* data) {
  std::memset(data, 0, kSaveBytes);
  std::memcpy(data, "POM2", 4);
  put(data + 4, generation);
  data[8] = static_cast<uint8_t>(state.config.focus);
  data[9] = static_cast<uint8_t>(state.config.shortBreak);
  data[10] = static_cast<uint8_t>(state.config.longBreak);
  data[11] = state.config.rounds;
  data[12] = state.config.autoBreak;
  data[13] = state.config.autoFocus;
  data[14] = static_cast<uint8_t>(state.phase);
  data[15] = state.stage;
  put(data + 16, state.remainingMs);
  put(data + 20, state.completed);
  put(data + kSaveBytes - 4, checksum(data));
}
bool decode(const uint8_t* data, State& state, uint32_t& generation) {
  const bool legacy = !std::memcmp(data, "POM1", 4);
  if ((!legacy && std::memcmp(data, "POM2", 4)) || get(data + kSaveBytes - 4) != checksum(data) || data[12] > 1 ||
      data[13] > 1)
    return false;
  State draft;
  draft.config = {data[8], data[9], data[10], data[11], data[12] != 0, data[13] != 0};
  draft.phase = static_cast<Phase>(data[14]);
  draft.remainingMs = get(data + 16);
  draft.completed = get(data + 20);
  if (legacy) {
    if (draft.config.rounds == 0 || draft.config.rounds > 12) return false;
    draft.stage = draft.phase == Phase::LongBreak ? 2 * draft.config.rounds - 1
                  : draft.phase == Phase::Focus
                      ? 2 * (draft.completed % draft.config.rounds)
                      : 2 * ((draft.completed ? draft.completed - 1 : 0) % draft.config.rounds) + 1;
    if (draft.phase == Phase::ShortBreak && draft.stage == 2 * draft.config.rounds - 1) draft.stage = 1;
    // Legacy Skip could choose a short break after the last round.
    if (draft.phase != phaseAt(draft.config, draft.stage)) {
      draft.phase = phaseAt(draft.config, draft.stage);
      draft.remainingMs = std::min(draft.remainingMs, duration(draft.config, draft.phase));
    }
  } else
    draft.stage = data[15];
  if (!valid(draft)) return false;
  state = draft;
  generation = get(data + 4);
  return true;
}
}  // namespace pomodoro

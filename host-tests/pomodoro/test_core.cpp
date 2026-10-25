#include <HalStorage.h>

#include <cstdio>
#include <cstring>

#include "../../src/apps_local/pomodoro/PomodoroCore.h"
#include "../../src/apps_local/pomodoro/PomodoroStore.h"
namespace p = pomodoro;
namespace {
int checks = 0, failures = 0;
}
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
int main() {
  p::State s;
  CHECK(p::valid(s) && s.remainingMs == 1500000 && !s.running);
  CHECK(!p::advance(s, 60000) && s.remainingMs == 1500000);
  s.running = true;
  CHECK(!p::advance(s, 12345) && s.remainingMs == 1487655);
  s.running = false;
  CHECK(!p::advance(s, 1000000) && s.remainingMs == 1487655);
  p::reset(s);
  CHECK(s.remainingMs == 1500000 && s.completed == 0);
  for (int cycle = 1; cycle <= 12; ++cycle) {
    s.running = true;
    CHECK(p::advance(s, s.remainingMs));
    CHECK(s.completed == static_cast<uint32_t>(cycle) && !s.running && s.notice);
    CHECK(s.phase == (cycle % 4 ? p::Phase::ShortBreak : p::Phase::LongBreak));
    const auto completed = s.completed;
    s.running = true;
    CHECK(p::advance(s, UINT32_MAX) && s.phase == p::Phase::Focus && !s.running && s.completed == completed);
  }
  s.config.autoBreak = s.config.autoFocus = true;
  s.running = true;
  CHECK(p::advance(s, s.remainingMs) && s.running && s.phase == p::Phase::ShortBreak);
  CHECK(p::advance(s, s.remainingMs) && s.running && s.phase == p::Phase::Focus);
  const auto completed = s.completed;
  p::skip(s);
  CHECK(!s.running && !s.notice && s.phase == p::Phase::ShortBreak && s.completed == completed);
  p::skip(s);
  CHECK(s.phase == p::Phase::Focus && s.completed == completed);
  for (int field = 0; field < 4; ++field) {
    p::configure(s, field, -1000);
    CHECK(p::valid(s));
    p::configure(s, field, 1000);
    CHECK(p::valid(s));
  }
  CHECK(s.config.focus == 180 && s.config.shortBreak == 60 && s.config.longBreak == 120 && s.config.rounds == 12);
  s.running = true;
  p::configure(s, 0, -10);
  CHECK(s.config.focus == 180);
  uint8_t bytes[p::kSaveBytes];
  p::encode(s, UINT32_MAX, bytes);
  p::State restored;
  uint32_t generation = 0;
  CHECK(p::decode(bytes, restored, generation) && generation == UINT32_MAX && !restored.running);
  CHECK(restored.remainingMs == s.remainingMs && restored.config.autoFocus && restored.completed == completed);
  for (size_t n = 0; n < p::kSaveBytes; ++n) {
    bytes[n] ^= 1;
    CHECK(!p::decode(bytes, restored, generation));
    bytes[n] ^= 1;
  }
  for (int phase = 0; phase < 3; ++phase) {
    p::selectStage(s, phase == 0 ? 0 : phase == 1 ? 1 : 2 * s.config.rounds - 1);
    p::reset(s);
    s.running = true;
    p::advance(s, 99001);
    p::encode(s, 13, bytes);
    CHECK(p::decode(bytes, restored, generation) && restored.phase == s.phase && restored.remainingMs == s.remainingMs);
  }
  for (int rounds = 1; rounds <= 12; ++rounds) {
    s = {};
    s.config.rounds = rounds;
    for (int stage = 0; stage < 2 * rounds; ++stage) {
      const auto count = s.completed;
      p::selectStage(s, stage);
      CHECK(p::valid(s) && !s.running && s.completed == count && s.stage == stage);
      CHECK(s.phase == p::phaseAt(s.config, stage));
      p::encode(s, 42, bytes);
      CHECK(p::decode(bytes, restored, generation) && restored.stage == stage);
      p::skip(s);
      CHECK(p::valid(s) && s.stage == (stage + 1) % (2 * rounds) && s.completed == count);
    }
  }
  // Real POM1 records migrate into a paused, valid stage without changing earned credit.
  for (int rounds = 1; rounds <= 12; ++rounds)
    for (int oldPhase = 0; oldPhase < 3; ++oldPhase) {
      p::State old;
      old.config.rounds = rounds;
      p::selectStage(old, oldPhase == 0 ? 0 : oldPhase == 1 ? 1 : 2 * rounds - 1);
      old.phase = static_cast<p::Phase>(oldPhase);
      old.remainingMs = p::duration(old.config, old.phase) - 1234;
      old.completed = rounds * 2;
      p::encode(old, 91, bytes);
      bytes[3] = '1';
      bytes[15] = 0;
      uint32_t hash = 2166136261u;
      for (size_t n = 0; n < p::kSaveBytes - 4; ++n) hash = (hash ^ bytes[n]) * 16777619u;
      for (int n = 0; n < 4; ++n) bytes[p::kSaveBytes - 4 + n] = uint8_t(hash >> (8 * n));
      CHECK(p::decode(bytes, restored, generation));
      CHECK(p::valid(restored) && !restored.running && restored.completed == old.completed && generation == 91);
      CHECK(restored.remainingMs <= p::duration(restored.config, restored.phase));
    }
  testStorage = {};
  p::Store store;
  CHECK(store.load(restored));
  CHECK(store.save(s));
  const auto writes = testStorage.writes;
  CHECK(store.save(s) && testStorage.writes == writes);
  p::Store reload;
  CHECK(reload.load(restored) && restored.remainingMs == s.remainingMs);
  s.remainingMs -= 5000;
  testStorage.writeLimit = 8;
  CHECK(!reload.save(s));
  testStorage.writeLimit = -1;
  p::Store recovery;
  CHECK(recovery.load(restored) && restored.remainingMs == s.remainingMs + 5000);
  testStorage.files["/.crosspoint/pomodoro-b.bin"] = {'P', 'O', 'M', '3'};
  p::Store future;
  CHECK(!future.load(restored));
  const auto snapshot = testStorage.files;
  CHECK(!future.save(s) && testStorage.files == snapshot);
  testStorage = {};
  testStorage.files["/.crosspoint/pomodoro-a.bin"] = {1, 2, 3};
  p::Store damaged;
  CHECK(!damaged.load(restored) && !damaged.save(s));
  testStorage = {};
  testStorage.available = false;
  p::Store missing;
  CHECK(missing.load(restored) && !missing.save(s));
  std::printf("Pomodoro: %d checks, %d failed; state=%zu store=%zu bytes\n", checks, failures, sizeof(p::State),
              sizeof(p::Store));
  return failures ? 1 : 0;
}

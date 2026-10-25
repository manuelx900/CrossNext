#pragma once
#include <I18n.h>

#include <cstdint>
namespace intercept {
enum Flag : uint16_t { Component = 1, Cooperative = 2, Lied = 4, BrokenWindow = 8, Proof = 16, Returned = 32 };
enum class Ending : uint8_t { None, Free, Suspicion, Captured, Escape, Betrayed };
struct Choice {
  StrId label;
  uint16_t next, require, forbid, set, clear;
};
struct Node {
  StrId title, text;
  Choice choices[3];
  uint8_t count, scene;
  Ending ending;
};
extern const Node nodes[];
extern const uint16_t nodeCount;
struct State {
  uint32_t sequence = 0;
  uint16_t node = 0, flags = 0;
  uint32_t choices = 0;
};
bool allowed(const State&, unsigned choice);
bool choose(State&, unsigned choice);
void encode(const State&, uint8_t*);
bool decode(const uint8_t*, State&);
bool load(State&, bool& writable);
bool save(State&, bool writable);
}  // namespace intercept

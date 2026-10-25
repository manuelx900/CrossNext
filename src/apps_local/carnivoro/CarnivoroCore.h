#pragma once
#include <cstddef>
#include <cstdint>
namespace carnivoro {
inline constexpr int kCases = 1470, kAnimals = 7;
enum class Difficulty : uint8_t { Easy, Normal, Advanced, Expert };
struct DifficultyRules {
  uint8_t minAnimals, maxAnimals, minLiars, maxLiars, minCulprits, maxCulprits;
};
enum class Claim : uint8_t {
  Guilty,
  Innocent,
  Either,
  Both,
  Truth,
  Lie,
  SameTruth,
  DifferentTruth,
  Carnivore,
  Herbivore,
  Taller,
  Shorter,
  Neighbour
};
struct Statement {
  Claim kind = Claim::Guilty;
  uint8_t a = 0, b = 0;
};
struct Case {
  uint8_t count = 3, culprits = 1, lies = 1;
  Statement statements[kAnimals]{};
};
struct Solution {
  uint8_t guilty = 0, truth = 0;
  int worlds = 0;
};
enum class Note : uint8_t { Unknown, Truth, Lie };
enum class GuiltNote : uint8_t { Unknown, Suspect, Innocent };
enum class Status : uint8_t { New, Active, Solved, Exhausted };
inline constexpr uint16_t kNoCase = 0xFFFF;
inline constexpr size_t kSaveBytes = 20 + 2 * kCases + 4;
inline constexpr uint8_t kSaveVersion = 5;
struct State {
  uint16_t caseId = kNoCase;
  Status status[kCases]{};
  uint8_t misses[kCases]{};
  Note notes[kAnimals]{};
  uint8_t picks = 0;
  uint8_t innocents = 0;
  Difficulty difficulty = Difficulty::Easy;
  uint8_t replayMisses = 0;
  Status replayStatus = Status::New;
  bool replaying = false;
  uint8_t reserved = 0;
};
static_assert(sizeof(State) == 2 * kCases + kAnimals + 9, "State comparison must not include padding");
GuiltNote guiltNote(const State&, int suspect);
bool setGuiltNote(State&, int suspect, GuiltNote);
Status caseStatus(const State&, int id);
bool closed(const State&, int id);
int nextCase(const State&);
bool restartCase(State&, int id);
void resetProgress(State&);
Difficulty difficulty(int id);
int caseCount(Difficulty);
int caseAt(Difficulty, int index);
int caseIndex(int id);
// One-based progressive order by difficulty; save IDs remain stable.
int caseNumber(int id);
const DifficultyRules& rules(Difficulty);
int lives(Difficulty);
int population(uint8_t);
const Case* getCase(int id);
bool evaluate(const Case&, int speaker, uint8_t guilty, uint8_t truth);
Solution solve(const Case&);
bool available(const State&, int id, bool campaign);
int remaining(const State&, int id);
// A checked accusation consumes a durable attempt; the activity commits before revealing it.
bool accuse(State&, int id, uint8_t picks);
size_t encode(const State&, uint8_t*, size_t);
bool decode(const uint8_t*, size_t, State&, bool& future);
}  // namespace carnivoro

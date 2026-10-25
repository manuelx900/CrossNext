#include "CarnivoroCore.h"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "CarnivoroCases.h"
namespace carnivoro {
namespace {
constexpr int heights[] = {180, 165, 195, 172, 205, 188, 198};
constexpr uint8_t carnivores = (1 << 0) | (1 << 2), herbivores = (1 << 5) | (1 << 6);
struct CaseRange {
  uint16_t first, count;
};
constexpr CaseRange catalog[][3] = {{{0, 20}, {1000, 200}, {0, 0}},
                                    {{20, 30}, {100, 200}, {1200, 90}},
                                    {{50, 30}, {300, 350}, {0, 0}},
                                    {{80, 20}, {650, 350}, {1290, 180}}};
constexpr DifficultyRules collectRules(const CaseRange (&ranges)[3]) {
  DifficultyRules result{kAnimals, 0, kAnimals, 0, kAnimals, 0};
  for (const auto& range : ranges)
    for (int id = range.first; id < range.first + range.count; ++id) {
      const auto& c = kCaseData[id];
      result.minAnimals = std::min(result.minAnimals, c.count);
      result.maxAnimals = std::max(result.maxAnimals, c.count);
      result.minLiars = std::min(result.minLiars, c.lies);
      result.maxLiars = std::max(result.maxLiars, c.lies);
      result.minCulprits = std::min(result.minCulprits, c.culprits);
      result.maxCulprits = std::max(result.maxCulprits, c.culprits);
    }
  return result;
}
constexpr DifficultyRules ruleTable[] = {collectRules(catalog[0]), collectRules(catalog[1]), collectRules(catalog[2]),
                                         collectRules(catalog[3])};
struct CaseOrder {
  uint16_t ids[kCases]{}, rank[kCases]{}, first[std::size(catalog) + 1]{};
};
constexpr CaseOrder makeCaseOrder() {
  CaseOrder result;
  int position = 0;
  for (unsigned tier = 0; tier < std::size(catalog); ++tier) {
    result.first[tier] = position;
    const auto& rules = ruleTable[tier];
    for (int culprits = rules.minCulprits; culprits <= rules.maxCulprits; ++culprits)
      for (int liars = rules.minLiars; liars <= rules.maxLiars; ++liars)
        for (int animals = rules.minAnimals; animals <= rules.maxAnimals; ++animals)
          for (const auto& range : catalog[tier])
            for (int id = range.first; id < range.first + range.count; ++id) {
              const auto& c = kCaseData[id];
              if (c.culprits != culprits || c.lies != liars || c.count != animals) continue;
              result.ids[position] = id;
              result.rank[id] = position++;
            }
  }
  result.first[std::size(catalog)] = position;
  return result;
}
// Compile-time lookup tables keep progression separate from persistent case IDs.
constexpr CaseOrder caseOrder = makeCaseOrder();
static_assert(caseOrder.first[std::size(catalog)] == kCases, "Progression must include every case");
constexpr bool validCaseOrder() {
  for (int position = 0; position < kCases; ++position)
    if (caseOrder.rank[caseOrder.ids[position]] != position) return false;
  return true;
}
static_assert(validCaseOrder(), "Progression must preserve unique case IDs");
uint32_t hash(const uint8_t* p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}
}  // namespace
Difficulty difficulty(int id) {
  if (id >= 1200) return id < 1290 ? Difficulty::Normal : Difficulty::Expert;
  if (id >= 1000) return Difficulty::Easy;
  if (id < 100)
    return id < 20   ? Difficulty::Easy
           : id < 50 ? Difficulty::Normal
           : id < 80 ? Difficulty::Advanced
                     : Difficulty::Expert;
  return id < 300 ? Difficulty::Normal : id < 650 ? Difficulty::Advanced : Difficulty::Expert;
}
int caseCount(Difficulty tier) {
  const auto& ranges = catalog[static_cast<unsigned>(tier)];
  int count = 0;
  for (const auto& range : ranges) count += range.count;
  return count;
}
int caseAt(Difficulty tier, int index) {
  if (index < 0 || index >= caseCount(tier)) return -1;
  return caseOrder.ids[caseOrder.first[static_cast<unsigned>(tier)] + index];
}
int caseIndex(int id) {
  if (!getCase(id)) return -1;
  return caseOrder.rank[id] - caseOrder.first[static_cast<unsigned>(difficulty(id))];
}
int caseNumber(int id) { return getCase(id) ? caseOrder.rank[id] + 1 : 0; }
const DifficultyRules& rules(Difficulty tier) { return ruleTable[static_cast<unsigned>(tier)]; }
Status caseStatus(const State& state, int id) {
  if (id < 0 || id >= kCases) return Status::New;
  return state.replaying && state.caseId == id ? state.replayStatus : state.status[id];
}
bool closed(const State& state, int id) {
  const auto status = caseStatus(state, id);
  return status == Status::Solved || status == Status::Exhausted;
}
int nextCase(const State& state) {
  int first = state.caseId < kCases ? caseOrder.rank[state.caseId] : 0;
  if (closed(state, caseOrder.ids[first])) ++first;
  for (int position = first; position < kCases; ++position) {
    const int id = caseOrder.ids[position];
    if (!closed(state, id) && available(state, id, true)) return id;
  }
  for (int position = 0; position < first; ++position) {
    const int id = caseOrder.ids[position];
    if (!closed(state, id) && available(state, id, true)) return id;
  }
  return -1;
}
bool restartCase(State& state, int id) {
  if (id < 0 || id >= kCases || (state.status[id] != Status::Solved && state.status[id] != Status::Exhausted))
    return false;
  state.caseId = id;
  state.replaying = true;
  state.replayMisses = 0;
  state.replayStatus = Status::Active;
  state.picks = 0;
  state.innocents = 0;
  for (auto& note : state.notes) note = Note::Unknown;
  return true;
}
void resetProgress(State& state) {
  std::fill(std::begin(state.status), std::end(state.status), Status::New);
  std::fill(std::begin(state.misses), std::end(state.misses), 0);
  std::fill(std::begin(state.notes), std::end(state.notes), Note::Unknown);
  state.caseId = kNoCase;
  state.picks = 0;
  state.innocents = 0;
  state.difficulty = Difficulty::Easy;
  state.replaying = false;
  state.replayMisses = 0;
  state.replayStatus = Status::New;
  state.reserved = 0;
}
int lives(Difficulty d) { return d == Difficulty::Easy || d == Difficulty::Normal ? 2 : 1; }
GuiltNote guiltNote(const State& state, int suspect) {
  if (suspect < 0 || suspect >= kAnimals) return GuiltNote::Unknown;
  const uint8_t bit = 1 << suspect;
  if (state.picks & bit) return GuiltNote::Suspect;
  return state.innocents & bit ? GuiltNote::Innocent : GuiltNote::Unknown;
}
bool setGuiltNote(State& state, int suspect, GuiltNote note) {
  const auto* c = getCase(state.caseId);
  if (note > GuiltNote::Innocent) return false;
  if (!c || suspect < 0 || suspect >= c->count || closed(state, state.caseId)) return false;
  const uint8_t bit = 1 << suspect;
  switch (note) {
    case GuiltNote::Unknown:
      state.picks &= ~bit;
      state.innocents &= ~bit;
      break;
    case GuiltNote::Suspect:
      state.picks |= bit;
      state.innocents &= ~bit;
      break;
    case GuiltNote::Innocent:
      state.picks &= ~bit;
      state.innocents |= bit;
      break;
  }
  return true;
}
int population(uint8_t n) {
  int count = 0;
  for (; n; n &= n - 1) ++count;
  return count;
}
const Case* getCase(int id) { return id >= 0 && id < kCases ? &kCaseData[id] : nullptr; }
bool evaluate(const Case& c, int speaker, uint8_t guilty, uint8_t truth) {
  if (speaker < 0 || speaker >= c.count) return false;
  const auto& s = c.statements[speaker];
  const bool a = guilty & (1 << s.a), b = guilty & (1 << s.b);
  const bool ta = truth & (1 << s.a), tb = truth & (1 << s.b);
  switch (s.kind) {
    case Claim::Guilty:
      return a;
    case Claim::Innocent:
      return !a;
    case Claim::Either:
      return a || b;
    case Claim::Both:
      return a && b;
    case Claim::Truth:
      return ta;
    case Claim::Lie:
      return !ta;
    case Claim::SameTruth:
      return ta == tb;
    case Claim::DifferentTruth:
      return ta != tb;
    case Claim::Carnivore:
      return (guilty & carnivores) != 0;
    case Claim::Herbivore:
      return (guilty & herbivores) != 0;
    case Claim::Taller:
    case Claim::Shorter:
      for (int i = 0; i < c.count; ++i)
        if ((guilty & (1 << i)) &&
            (s.kind == Claim::Taller ? heights[i] > heights[speaker] : heights[i] < heights[speaker]))
          return true;
      return false;
    case Claim::Neighbour:
      return (guilty & ((1 << ((speaker + 1) % c.count)) | (1 << ((speaker + c.count - 1) % c.count)))) != 0;
  }
  return false;
}
Solution solve(const Case& c) {
  Solution out;
  if (c.count < 3 || c.count > kAnimals || !c.culprits || c.culprits >= c.count || c.lies > c.count) return out;
  for (unsigned guilty = 1; guilty < (1u << c.count); ++guilty) {
    if (population(guilty) != c.culprits) continue;
    for (unsigned truth = 0; truth < (1u << c.count); ++truth) {
      if (population(truth) != c.count - c.lies) continue;
      bool fits = true;
      for (int i = 0; i < c.count; ++i)
        if (evaluate(c, i, guilty, truth) != bool(truth & (1 << i))) {
          fits = false;
          break;
        }
      if (fits) {
        out.guilty = guilty;
        out.truth = truth;
        ++out.worlds;
      }
    }
  }
  return out;
}
bool available(const State&, int id, bool) {
  // Catalog selection is independent of completion and campaign order.
  return getCase(id) != nullptr;
}
int remaining(const State& s, int id) {
  return getCase(id) ? lives(difficulty(id)) - (s.replaying && s.caseId == id ? s.replayMisses : s.misses[id]) : 0;
}
bool accuse(State& s, int id, uint8_t picks) {
  const auto* c = getCase(id);
  if (!c || closed(s, id) || remaining(s, id) <= 0 || (picks >> c->count) || population(picks) != c->culprits)
    return false;
  const auto solution = solve(*c);
  if (solution.worlds != 1) return false;
  auto& status = s.replaying && s.caseId == id ? s.replayStatus : s.status[id];
  auto& misses = s.replaying && s.caseId == id ? s.replayMisses : s.misses[id];
  if (picks == solution.guilty) {
    status = Status::Solved;
    if (s.replaying && s.caseId == id && s.status[id] != Status::Solved) {
      s.status[id] = Status::Solved;
      s.misses[id] = misses;
    }
  } else {
    ++misses;
    status = remaining(s, id) == 0 ? Status::Exhausted : Status::Active;
  }
  return true;
}
size_t encode(const State& s, uint8_t* out, size_t cap) {
  if (cap < kSaveBytes) return 0;
  std::memset(out, 0, kSaveBytes);
  std::memcpy(out, "CARN", 4);
  out[4] = kSaveVersion;
  out[5] = static_cast<uint8_t>(s.difficulty);
  out[6] = s.picks;
  out[7] = s.innocents;
  out[8] = s.caseId & 255;
  out[9] = s.caseId >> 8;
  for (int i = 0; i < kAnimals; ++i) out[10 + i] = static_cast<uint8_t>(s.notes[i]);
  out[17] = s.replaying;
  out[18] = s.replayMisses;
  out[19] = static_cast<uint8_t>(s.replayStatus);
  for (int i = 0; i < kCases; ++i) {
    out[20 + i] = static_cast<uint8_t>(s.status[i]);
    out[20 + kCases + i] = s.misses[i];
  }
  const auto crc = hash(out, kSaveBytes - 4);
  for (int i = 0; i < 4; ++i) out[kSaveBytes - 4 + i] = crc >> (8 * i);
  return kSaveBytes;
}
bool decode(const uint8_t* in, size_t n, State& s, bool& future) {
  future = n >= 5 && !std::memcmp(in, "CARN", 4) && in[4] > kSaveVersion;
  if (future || n < 5 || std::memcmp(in, "CARN", 4)) return false;
  const bool legacy = in[4] == 1;
  size_t size = 0;
  int count = 0;
  switch (in[4]) {
    case 1:
      size = 220;
      count = 100;
      break;
    case 2:
    case 3:
      size = 2024;
      count = 1000;
      break;
    case 4:
      size = 2424;
      count = 1200;
      break;
    case kSaveVersion:
      size = kSaveBytes;
      count = kCases;
      break;
    default:
      return false;
  }
  if (n != size) return false;
  uint32_t crc = 0;
  for (int i = 0; i < 4; ++i) crc |= uint32_t(in[size - 4 + i]) << (8 * i);
  if (hash(in, size - 4) != crc) return false;
  const int id = legacy ? (in[5] == 255 ? kNoCase : in[5]) : int(in[8]) + (int(in[9]) << 8);
  const int tier = in[legacy ? 6 : 5], picks = in[legacy ? 7 : 6], notes = legacy ? 8 : 10, status = legacy ? 16 : 20,
            misses = legacy ? 116 : 20 + count;
  if (tier > 3 || (id != kNoCase && id >= count) || (id == kNoCase ? picks != 0 : picks >> getCase(id)->count))
    return false;
  const uint8_t innocents = in[4] >= 3 ? in[7] : 0;
  if ((innocents & picks) || (id == kNoCase ? innocents != 0 : innocents >> getCase(id)->count)) return false;
  for (int i = 0; i < kAnimals; ++i)
    if (in[notes + i] > 2) return false;
  for (int i = 0; i < count; ++i)
    if (in[status + i] > 3 || in[misses + i] > lives(difficulty(i)) ||
        (in[status + i] == static_cast<uint8_t>(Status::Exhausted)) != (in[misses + i] == lives(difficulty(i))))
      return false;
  if (!legacy) {
    if (in[17] > 1 || in[19] > 3) return false;
    if (in[17]) {
      if (id == kNoCase) return false;
      const int limit = lives(difficulty(id));
      const auto replay = static_cast<Status>(in[19]), history = static_cast<Status>(in[status + id]);
      if (replay == Status::New || (history != Status::Solved && history != Status::Exhausted) || in[18] > limit ||
          (replay == Status::Exhausted) != (in[18] == limit))
        return false;
    }
  }
  resetProgress(s);
  s.caseId = id;
  s.difficulty = static_cast<Difficulty>(tier);
  s.picks = picks;
  s.innocents = innocents;
  for (int i = 0; i < kAnimals; ++i) s.notes[i] = static_cast<Note>(in[notes + i]);
  for (int i = 0; i < count; ++i) {
    s.status[i] = static_cast<Status>(in[status + i]);
    s.misses[i] = in[misses + i];
  }
  if (!legacy) {
    s.replaying = in[17];
    s.replayMisses = in[18];
    s.replayStatus = static_cast<Status>(in[19]);
  }
  return true;
}
}  // namespace carnivoro

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <set>
#include <string>

#include "CarnivoroCore.h"
using namespace carnivoro;
int checks = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                               \
    }                                             \
  } while (0)
uint32_t checksum(const uint8_t* p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}
void sign(uint8_t* p, size_t n) {
  auto h = checksum(p, n - 4);
  for (int i = 0; i < 4; ++i) p[n - 4 + i] = h >> (8 * i);
}
void roundtrip(const State& s) {
  uint8_t wire[kSaveBytes];
  State read;
  bool future = false;
  CHECK(encode(s, wire, sizeof(wire)) == kSaveBytes);
  CHECK(decode(wire, sizeof(wire), read, future));
  CHECK(!future && !std::memcmp(&s, &read, sizeof(s)));
}
int orderedCase(int position) {
  if (position < 0) return -1;
  for (auto tier : {Difficulty::Easy, Difficulty::Normal, Difficulty::Advanced, Difficulty::Expert}) {
    if (position < caseCount(tier)) return caseAt(tier, position);
    position -= caseCount(tier);
  }
  return -1;
}
int main() {
  State state, read;
  uint8_t wire[kSaveBytes];
  bool future = false;
  CHECK(kCases == 1470 && nextCase(state) == 0);
  bool seen[kCases]{};
  int total = 0;
  for (auto tier : {Difficulty::Easy, Difficulty::Normal, Difficulty::Advanced, Difficulty::Expert}) {
    const auto& rule = rules(tier);
    CHECK(caseAt(tier, -1) == -1 && caseAt(tier, caseCount(tier)) == -1);
    for (int index = 0; index < caseCount(tier); ++index) {
      const int id = caseAt(tier, index);
      CHECK(id >= 0 && id < kCases && !seen[id]);
      seen[id] = true;
      ++total;
      CHECK(difficulty(id) == tier && caseIndex(id) == index);
      CHECK(caseNumber(id) == total);
      const auto& c = *getCase(id);
      if (index > 0) {
        const auto& previous = *getCase(caseAt(tier, index - 1));
        CHECK(previous.culprits <= c.culprits);
        if (previous.culprits == c.culprits) {
          CHECK(previous.lies <= c.lies);
          if (previous.lies == c.lies) CHECK(previous.count <= c.count);
        }
      }
      CHECK(c.count >= rule.minAnimals && c.count <= rule.maxAnimals);
      CHECK(c.lies >= rule.minLiars && c.lies <= rule.maxLiars);
      CHECK(c.culprits >= rule.minCulprits && c.culprits <= rule.maxCulprits);
    }
    CHECK(rule.maxLiars == rule.maxAnimals - 1);
  }
  CHECK(total == kCases && caseIndex(-1) == -1 && caseIndex(kCases) == -1);
  CHECK(caseNumber(-1) == 0 && caseNumber(kCases) == 0);
  CHECK(caseNumber(caseAt(Difficulty::Normal, 0)) == 221);
  CHECK(caseNumber(caseAt(Difficulty::Advanced, 0)) == 541);
  CHECK(caseNumber(caseAt(Difficulty::Expert, 0)) == 921);
  CHECK(caseCount(Difficulty::Normal) == 320 && caseCount(Difficulty::Expert) == 550);
  CHECK(rules(Difficulty::Normal).minCulprits == 1 && rules(Difficulty::Normal).maxCulprits == 2);
  CHECK(rules(Difficulty::Expert).minCulprits == 1 && rules(Difficulty::Expert).maxCulprits == 2);
  CHECK(rules(Difficulty::Easy).maxAnimals < rules(Difficulty::Normal).maxAnimals);
  CHECK(rules(Difficulty::Normal).maxAnimals < rules(Difficulty::Advanced).maxAnimals);
  CHECK(rules(Difficulty::Advanced).maxAnimals < rules(Difficulty::Expert).maxAnimals);

  CHECK(!getCase(-1) && !getCase(kCases));
  CHECK(!restartCase(state, 0));
  uint32_t legacyHash = 2166136261u, original1000Hash = 2166136261u;
  std::set<std::string> meanings;
  for (int id = 0; id < kCases; ++id) {
    const auto& c = *getCase(id);
    const auto sol = solve(c);
    if (id < 1000) {
      for (auto byte : {int(c.count), int(c.culprits), int(c.lies)})
        original1000Hash = (original1000Hash ^ byte) * 16777619u;
      for (int speaker = 0; speaker < c.count; ++speaker)
        for (auto byte : {int(c.statements[speaker].kind), int(c.statements[speaker].a), int(c.statements[speaker].b)})
          original1000Hash = (original1000Hash ^ byte) * 16777619u;
    }
    if (id >= 1000 && id < 1200) {
      CHECK(difficulty(id) == Difficulty::Easy && c.count == 3 && c.culprits == 1 && c.lies >= 1 && c.lies <= 2);
      for (const auto& clue : c.statements) CHECK(clue.kind < Claim::Truth || clue.kind > Claim::DifferentTruth);
    }
    if (id >= 1200 && id < 1290)
      CHECK(difficulty(id) == Difficulty::Normal && c.count == 4 && c.culprits == 2 && c.lies >= 1 && c.lies <= 3);
    if (id >= 1290)
      CHECK(difficulty(id) == Difficulty::Expert && c.count == 7 && c.culprits == 1 && c.lies >= 1 && c.lies <= 6);
    std::string meaning;
    meaning += char(c.count);
    meaning += char(c.culprits);
    meaning += char(c.lies);
    if (id < 100) {
      legacyHash = (legacyHash ^ c.count) * 16777619u;
      legacyHash = (legacyHash ^ c.culprits) * 16777619u;
      legacyHash = (legacyHash ^ c.lies) * 16777619u;
    }
    for (int i = 0; i < c.count; ++i) {
      const auto claim = c.statements[i];
      int kind = int(claim.kind), a = claim.a, b = claim.b;
      if (id < 100)
        for (auto byte : {kind, a, b}) legacyHash = (legacyHash ^ byte) * 16777619u;
      if (kind >= 8)
        a = b = 0;
      else if (kind == 0 || kind == 1 || kind == 4 || kind == 5)
        b = 0;
      else if (a > b)
        std::swap(a, b);
      meaning += char(kind);
      meaning += char(a);
      meaning += char(b);
    }
    CHECK(meanings.insert(meaning).second);
    CHECK(sol.worlds == 1);
    CHECK(population(sol.guilty) == c.culprits);
    CHECK(population(sol.truth) == c.count - c.lies);
    if (!id) CHECK(sol.guilty == 4 && sol.truth == 3);
    for (int i = 0; i < c.count; ++i) CHECK(evaluate(c, i, sol.guilty, sol.truth) == bool(sol.truth & (1 << i)));
    resetProgress(state);
    state.caseId = id;
    CHECK(available(state, id, false));
    CHECK(available(state, id, true));
    const int position = caseNumber(id) - 1;
    for (int i = 0; i < position; ++i) state.status[orderedCase(i)] = Status::Solved;
    CHECK(nextCase(state) == id);
    for (int picks = 0; picks < (1 << c.count); ++picks) {
      State attempt = state, before = state;
      const bool accepted = accuse(attempt, id, picks);
      CHECK(accepted == (population(picks) == c.culprits));
      if (!accepted)
        CHECK(!std::memcmp(&attempt, &before, sizeof(attempt)));
      else {
        CHECK(attempt.status[id] == (picks == sol.guilty          ? Status::Solved
                                     : lives(difficulty(id)) == 1 ? Status::Exhausted
                                                                  : Status::Active));
        CHECK(attempt.misses[id] == (picks == sol.guilty ? 0 : 1));
      }
    }
    uint8_t wrong = 1;
    while (population(wrong) != c.culprits || wrong == sol.guilty) ++wrong;
    for (int n = 0; n < lives(difficulty(id)); ++n) CHECK(accuse(state, id, wrong));
    CHECK(remaining(state, id) == 0 && closed(state, id));
    CHECK(!accuse(state, id, sol.guilty));
    CHECK(nextCase(state) == orderedCase(position + 1));
    roundtrip(state);
    CHECK(restartCase(state, id));
    CHECK(nextCase(state) == id);
    CHECK(remaining(state, id) == lives(difficulty(id)));
    CHECK(state.status[id] == Status::Exhausted);
    if (position + 1 < kCases) CHECK(available(state, orderedCase(position + 1), true));
    CHECK(accuse(state, id, sol.guilty));
    CHECK(state.status[id] == Status::Solved);
    roundtrip(state);
    CHECK(restartCase(state, id));
    for (int n = 0; n < lives(difficulty(id)); ++n) CHECK(accuse(state, id, wrong));
    CHECK(caseStatus(state, id) == Status::Exhausted && state.status[id] == Status::Solved);
    roundtrip(state);
  }
  CHECK(legacyHash == 2550420864u);  // Stable original 100-case content and save identities.
  for (int id : {0, 99, 255, 256, 999}) {
    resetProgress(state);
    state.caseId = id;
    state.status[id] = Status::Active;
    roundtrip(state);
    CHECK(encode(state, wire, sizeof(wire)) == kSaveBytes);
    for (size_t b = 0; b < kSaveBytes; ++b) {
      wire[b] ^= 1;
      CHECK(!decode(wire, kSaveBytes, read, future));
      wire[b] ^= 1;
    }
    for (size_t n = 0; n < kSaveBytes; ++n) CHECK(!decode(wire, n, read, future));
    wire[4] = kSaveVersion + 1;
    CHECK(!decode(wire, kSaveBytes, read, future) && future);
  }
  // The v1 migration must retain every original result, note and spent life.
  for (int current = -1; current < 100; ++current) {
    uint8_t legacy[220]{};
    std::memcpy(legacy, "CARN", 4);
    legacy[4] = 1;
    legacy[5] = current < 0 ? 255 : current;
    legacy[6] = 3;
    legacy[7] = current < 0 ? 0 : 1;
    for (int i = 0; i < 7; ++i) legacy[8 + i] = i % 3;
    for (int i = 0; i < 100; ++i) {
      legacy[16 + i] = uint8_t(i % 2 ? Status::Solved : Status::Exhausted);
      legacy[116 + i] = i % 2 ? 0 : lives(difficulty(i));
    }
    sign(legacy, sizeof(legacy));
    CHECK(decode(legacy, sizeof(legacy), read, future));
    CHECK(!future);
    CHECK(read.caseId == (current < 0 ? kNoCase : current));
    CHECK(read.difficulty == Difficulty::Expert);
    for (int i = 0; i < 100; ++i) {
      CHECK(uint8_t(read.status[i]) == legacy[16 + i]);
      CHECK(read.misses[i] == legacy[116 + i]);
    }
    for (int i = 100; i < kCases; ++i) CHECK(read.status[i] == Status::New && !read.misses[i]);
    for (int i = 0; i < 7; ++i) CHECK(uint8_t(read.notes[i]) == i % 3);
    CHECK(!read.replaying);
    roundtrip(read);
  }
  CHECK(original1000Hash == 1728623557u);
  CHECK(caseCount(Difficulty::Easy) == 220);
  // Non-contiguous v1 progress retains its current attempt and reached cases.
  uint8_t sparse[220]{};
  std::memcpy(sparse, "CARN", 4);
  sparse[4] = 1;
  sparse[5] = 20;
  sparse[7] = 2;
  sparse[8] = uint8_t(Note::Truth);
  sparse[16 + 20] = uint8_t(Status::Active);
  sparse[116 + 20] = 1;
  sign(sparse, sizeof(sparse));
  CHECK(decode(sparse, sizeof(sparse), read, future));
  CHECK(nextCase(read) == 20 && available(read, 20, true));
  CHECK(read.notes[0] == Note::Truth && read.picks == 2 && remaining(read, 20) == 1);
  for (int i = 0; i <= 20; ++i) CHECK(available(read, i, true));
  CHECK(available(read, 21, true));
  CHECK(accuse(read, 20, solve(*getCase(20)).guilty));
  CHECK(nextCase(read) == orderedCase(caseNumber(20)) && available(read, nextCase(read), true));
  roundtrip(read);
  resetProgress(state);
  encode(state, wire, sizeof(wire));
  wire[17] = 1;
  wire[19] = 1;
  sign(wire, sizeof(wire));
  CHECK(!decode(wire, sizeof(wire), read, future));
  resetProgress(state);
  state.caseId = orderedCase(5);
  for (int i = 0; i <= 5; ++i) state.status[orderedCase(i)] = Status::Solved;
  CHECK(nextCase(state) == orderedCase(6));
  for (int i = 0; i < kCases; ++i) state.status[i] = Status::Solved;
  state.caseId = orderedCase(kCases - 1);
  state.status[orderedCase(2)] = Status::New;
  CHECK(nextCase(state) == orderedCase(2));
  state.status[orderedCase(2)] = Status::Solved;
  CHECK(nextCase(state) == -1);
  resetProgress(state);
  CHECK(nextCase(state) == 0);
  roundtrip(state);
  resetProgress(state);
  state.caseId = 999;
  state.status[999] = Status::Active;
  for (int suspect = 0; suspect < 7; ++suspect) {
    CHECK(setGuiltNote(state, suspect, GuiltNote::Innocent));
    CHECK(guiltNote(state, suspect) == GuiltNote::Innocent);
    CHECK(!(state.picks & (1 << suspect)));
    roundtrip(state);
    CHECK(setGuiltNote(state, suspect, GuiltNote::Suspect));
    CHECK(guiltNote(state, suspect) == GuiltNote::Suspect);
    CHECK(!(state.innocents & (1 << suspect)));
    CHECK(setGuiltNote(state, suspect, GuiltNote::Unknown));
    CHECK(guiltNote(state, suspect) == GuiltNote::Unknown);
  }
  CHECK(!setGuiltNote(state, -1, GuiltNote::Innocent));
  CHECK(!setGuiltNote(state, 7, GuiltNote::Innocent));
  CHECK(!setGuiltNote(state, 0, static_cast<GuiltNote>(255)));
  setGuiltNote(state, 1, GuiltNote::Innocent);
  setGuiltNote(state, 2, GuiltNote::Suspect);
  encode(state, wire, sizeof(wire));
  // Real prior wire layout: 1000 results, followed by 1000 miss counts.
  uint8_t oldWire[2024]{};
  for (int version : {2, 3}) {
    std::memcpy(oldWire, wire, 20 + 1000);
    std::memcpy(oldWire + 20 + 1000, wire + 20 + kCases, 1000);
    oldWire[4] = version;
    sign(oldWire, sizeof(oldWire));
    CHECK(decode(oldWire, sizeof(oldWire), read, future));
    CHECK(read.innocents == (version == 2 ? 0 : state.innocents) && read.picks == state.picks);
    CHECK(read.caseId == state.caseId && read.difficulty == state.difficulty);
    CHECK(!std::memcmp(read.notes, state.notes, sizeof(state.notes)));
    for (int id = 0; id < 1000; ++id) CHECK(read.status[id] == state.status[id] && read.misses[id] == state.misses[id]);
    for (int id = 1000; id < kCases; ++id) CHECK(read.status[id] == Status::New && read.misses[id] == 0);
    roundtrip(read);
    oldWire[8] = 1000 & 255;
    oldWire[9] = 1000 >> 8;
    sign(oldWire, sizeof(oldWire));
    CHECK(!decode(oldWire, sizeof(oldWire), read, future));
  }
  uint8_t version4[2424]{};
  for (int current : {999, 1199}) {
    State prior = state;
    prior.caseId = current;
    prior.status[current] = Status::Active;
    encode(prior, wire, sizeof(wire));
    std::memcpy(version4, wire, 20 + 1200);
    std::memcpy(version4 + 20 + 1200, wire + 20 + kCases, 1200);
    version4[4] = 4;
    sign(version4, sizeof(version4));
    CHECK(decode(version4, sizeof(version4), read, future) && !future);
    CHECK(read.caseId == prior.caseId && read.picks == prior.picks && read.innocents == prior.innocents);
    CHECK(!std::memcmp(read.notes, prior.notes, sizeof(prior.notes)));
    for (int id = 0; id < 1200; ++id) CHECK(read.status[id] == prior.status[id] && read.misses[id] == prior.misses[id]);
    for (int id = 1200; id < kCases; ++id) CHECK(read.status[id] == Status::New && !read.misses[id]);
    roundtrip(read);
  }
  version4[8] = 1200 & 255;
  version4[9] = 1200 >> 8;
  sign(version4, sizeof(version4));
  CHECK(!decode(version4, sizeof(version4), read, future));
  wire[4] = kSaveVersion;
  wire[7] = wire[6];
  sign(wire, sizeof(wire));
  CHECK(!decode(wire, sizeof(wire), read, future));
  wire[7] = 128;
  sign(wire, sizeof(wire));
  CHECK(!decode(wire, sizeof(wire), read, future));
  state.status[999] = Status::Solved;
  CHECK(!setGuiltNote(state, 0, GuiltNote::Innocent));
  CHECK(restartCase(state, 999));
  CHECK(!state.innocents && !state.picks);
  state.replayMisses = 0;
  state.notes[0] = Note::Lie;
  setGuiltNote(state, 1, GuiltNote::Innocent);
  for (int id = 0; id < 999; ++id) {
    state.status[id] = id % 2 ? Status::Solved : Status::Exhausted;
    state.misses[id] = id % 2 ? 0 : lives(difficulty(id));
  }
  encode(state, wire, sizeof(wire));
  std::memcpy(oldWire, wire, 20 + 1000);
  std::memcpy(oldWire + 20 + 1000, wire + 20 + kCases, 1000);
  oldWire[4] = 3;
  sign(oldWire, sizeof(oldWire));
  CHECK(decode(oldWire, sizeof(oldWire), read, future));
  CHECK(read.replaying && read.replayStatus == state.replayStatus && read.replayMisses == state.replayMisses);
  CHECK(read.notes[0] == Note::Lie && read.innocents == state.innocents && read.caseId == 999);
  for (int id = 0; id < 1000; ++id) CHECK(read.status[id] == state.status[id] && read.misses[id] == state.misses[id]);
  roundtrip(read);
  std::memcpy(version4, wire, 20 + 1200);
  std::memcpy(version4 + 20 + 1200, wire + 20 + kCases, 1200);
  version4[4] = 4;
  sign(version4, sizeof(version4));
  CHECK(decode(version4, sizeof(version4), read, future) && !future);
  CHECK(read.replaying && read.replayStatus == state.replayStatus && read.replayMisses == state.replayMisses);
  CHECK(read.notes[0] == state.notes[0] && read.innocents == state.innocents && read.caseId == state.caseId);
  roundtrip(read);
  std::printf("%d core checks, 0 failures; %d unique solutions; state=%zu bytes, wire=%zu\n", checks, kCases,
              sizeof(State), kSaveBytes);
}

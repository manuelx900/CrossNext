#include "LocalWordle.h"

#include <algorithm>
#include <cstring>
#include <numeric>

#include "SpanishWords.generated.h"
#include "WordleWords.generated.h"

namespace localwordle {
namespace {
uint32_t get(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void put(uint8_t* p, uint32_t value) {
  for (int n = 0; n < 4; ++n) p[n] = value >> (n * 8);
}
uint32_t checksum(const uint8_t* p) {
  uint32_t value = 2166136261u;
  for (size_t n = 0; n < kSaveBytes - 4; ++n) value = (value ^ p[n]) * 16777619u;
  return value;
}
}  // namespace
namespace {
struct Bank {
  const char* answers;
  int count;
  const char* guesses;
  int accepted;
};
Bank bankFor(int letters, int language) {
  switch (letters) {
    case 4:
      return language ? Bank{&data::eswords4[0][0], int(sizeof(data::eswords4) / 5), &data::esguesses4[0][0],
                             int(sizeof(data::esguesses4) / 5)}
                      : Bank{&data::words4[0][0], int(sizeof(data::words4) / 5), &data::guesses4[0][0],
                             int(sizeof(data::guesses4) / 5)};
    case 5:
      return language ? Bank{&data::eswords5[0][0], int(sizeof(data::eswords5) / 6), &data::esguesses5[0][0],
                             int(sizeof(data::esguesses5) / 6)}
                      : Bank{&data::words5[0][0], int(sizeof(data::words5) / 6), &data::guesses5[0][0],
                             int(sizeof(data::guesses5) / 6)};
    case 6:
      return language ? Bank{&data::eswords6[0][0], int(sizeof(data::eswords6) / 7), &data::esguesses6[0][0],
                             int(sizeof(data::esguesses6) / 7)}
                      : Bank{&data::words6[0][0], int(sizeof(data::words6) / 7), &data::guesses6[0][0],
                             int(sizeof(data::guesses6) / 7)};
    default:
      return {nullptr, 0, nullptr, 0};
  }
}
}  // namespace
int wordCount(int letters, int language) { return bankFor(letters, language).count; }
const char* wordAt(int letters, int index, int language) {
  const auto bank = bankFor(letters, language);
  return index >= 0 && index < bank.count ? bank.answers + index * (letters + 1) : "";
}
bool contains(int letters, const char* word, int language) {
  if (!word) return false;
  const auto bank = bankFor(letters, language);
  int low = 0, high = bank.accepted;
  while (low < high) {
    const int middle = (low + high) / 2, result = std::memcmp(bank.guesses + middle * (letters + 1), word, letters);
    if (!result) return true;
    if (result < 0)
      low = middle + 1;
    else
      high = middle;
  }
  return false;
}
void start(State& state) {
  const int count = wordCount(state.letters, state.language);
  if (!count) return;
  const int bank = state.letters - 4;
  const uint32_t seed = state.seed ^ (0x9e3779b9u * state.letters);
  int stride = seed % count + 1;
  while (std::gcd(stride, count) != 1) stride = (stride + 1) % count;
  const int index = ((state.next[bank] % count) * stride + seed / count % count) % count;
  state.next[bank] = (state.next[bank] + 1) % count;
  state.game.start(0, wordAt(state.letters, index, state.language), state.letters);
  state.gameLanguage = state.language;
  state.active = true;
  state.recorded = false;
}
void record(State& state, bool abandoned) {
  if (!state.active || state.recorded || (!state.game.over() && !abandoned)) return;
  auto& r = state.record;
  if (r.played < 1000000) {
    ++r.played;
    if (!abandoned && state.game.status() == wordle::Game::Status::Won) {
      ++r.won;
      ++r.streak;
      r.maximum = std::max(r.maximum, r.streak);
      ++r.distribution[state.game.guesses() - 1];
    } else
      r.streak = 0;
  }
  state.recorded = true;
  if (abandoned) {
    state.active = false;
    state.recorded = false;
  }
}
void encode(const State& state, uint32_t generation, uint8_t* bytes) {
  std::memset(bytes, 0, kSaveBytes);
  std::memcpy(bytes, "WDL1", 4);
  put(bytes + 4, generation);
  bytes[8] = state.letters;
  bytes[9] = state.active;
  bytes[10] = state.recorded;
  bytes[11] = kVocabularyVersion;
  bytes[22] = state.language;
  bytes[23] = state.gameLanguage;
  put(bytes + 12, state.seed);
  for (int n = 0; n < 3; ++n) {
    bytes[16 + n * 2] = state.next[n];
    bytes[17 + n * 2] = state.next[n] >> 8;
  }
  put(bytes + 24, state.record.played);
  put(bytes + 28, state.record.won);
  put(bytes + 32, state.record.streak);
  put(bytes + 36, state.record.maximum);
  for (int n = 0; n < 6; ++n) put(bytes + 40 + n * 4, state.record.distribution[n]);
  if (state.active) {
    const auto text = state.game.save();
    bytes[64] = state.game.letters();
    bytes[65] = text.size();
    std::memcpy(bytes + 66, text.data(), std::min<size_t>(text.size(), 49));
    bytes[116] = state.game.typed();
    std::memcpy(bytes + 117, state.game.typing(), state.game.typed());
  }
  put(bytes + kSaveBytes - 4, checksum(bytes));
}
bool decode(const uint8_t* bytes, State& state, uint32_t& generation) {
  if (std::memcmp(bytes, "WDL1", 4) || get(bytes + 252) != checksum(bytes) || bytes[8] < 4 || bytes[8] > 6 ||
      bytes[9] > 1 || bytes[10] > 1 || (bytes[11] != 1 && bytes[11] != kVocabularyVersion) ||
      (bytes[11] >= 2 && (bytes[22] > 1 || bytes[23] > 1)))
    return false;
  state = {};
  state.letters = bytes[8];
  state.language = bytes[11] == 1 ? 0 : bytes[22];
  state.gameLanguage = bytes[11] == 1 ? 0 : bytes[23];
  state.active = bytes[9];
  state.recorded = bytes[10];
  state.seed = get(bytes + 12);
  for (int n = 0; n < 3; ++n) {
    state.next[n] = uint16_t(bytes[16 + n * 2]) | uint16_t(bytes[17 + n * 2]) << 8;
    if (state.next[n] >= std::max(wordCount(n + 4), wordCount(n + 4, 1))) return false;
  }
  auto& r = state.record;
  r.played = get(bytes + 24);
  r.won = get(bytes + 28);
  r.streak = get(bytes + 32);
  r.maximum = get(bytes + 36);
  if (r.played > 1000000 || r.won > r.played || r.streak > r.maximum || r.maximum > r.won) return false;
  uint64_t total = 0;
  for (int n = 0; n < 6; ++n) {
    r.distribution[n] = get(bytes + 40 + n * 4);
    total += r.distribution[n];
  }
  if (total != r.won) return false;
  if (state.active) {
    const int letters = bytes[64], length = bytes[65], typed = bytes[116];
    if (letters < 4 || letters > 6 || length < letters || length > letters + 6 * (letters + 1) || typed > letters ||
        !state.game.load(0, std::string(reinterpret_cast<const char*>(bytes + 66), length), letters))
      return false;
    if (state.recorded != state.game.over() || (state.game.over() && typed)) return false;
    for (int n = 0; n < typed; ++n)
      if (!state.game.type(bytes[117 + n])) return false;
  } else if (state.recorded)
    return false;
  generation = get(bytes + 4);
  return true;
}
}  // namespace localwordle

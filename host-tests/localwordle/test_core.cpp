#include <HalStorage.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/apps_local/wordle/LocalWordle.h"
namespace w = localwordle;
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
bool allow(void*, const char*) { return true; }
void type(wordle::Game& game, const char* text) {
  for (int n = 0; n < game.letters(); ++n) CHECK(game.type(text[n]));
}
int main() {
  // Repeated-letter scoring uses each answer occurrence at most once at every length.
  for (int length = 4; length <= 6; ++length) {
    const char* answer = length == 4 ? "AREA" : length == 5 ? "LEVEL" : "BANANA";
    const char* guess = length == 4 ? "AAAA" : length == 5 ? "EEEEE" : "AAAAAA";
    wordle::Mark marks[wordle::kMaxLetters]{};
    wordle::score(guess, answer, marks, length);
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
      int available = 0, marked = 0;
      for (int n = 0; n < length; ++n) {
        available += answer[n] == letter;
        marked += guess[n] == letter && (marks[n] == wordle::Mark::Correct || marks[n] == wordle::Mark::Present);
        CHECK((marks[n] == wordle::Mark::Correct) == (guess[n] == answer[n]));
      }
      CHECK(marked <= available);
    }
    wordle::Game game;
    game.start(0, answer, length);
    type(game, guess);
    CHECK(game.submit(allow, nullptr) == wordle::Game::Submit::Scored);
    wordle::Game copy;
    CHECK(copy.load(0, game.save(), length) && copy.letters() == length && copy.guesses() == 1);
    for (int n = 0; n < length; ++n) CHECK(copy.marks(0)[n] == marks[n]);
  }
  int total = 0;
  for (int length = 4; length <= 6; ++length) {
    const int count = w::wordCount(length);
    total += count;
    CHECK(count > 500);
    std::vector<bool> seen;
    seen.resize(count, false);
    w::State s;
    s.letters = length;
    s.seed = 987234u;
    for (int n = 0; n < count; ++n) {
      w::start(s);
      CHECK(s.game.letters() == length && w::contains(length, s.game.answer()));
      int index = 0;
      while (index < count && std::strcmp(w::wordAt(length, index), s.game.answer())) ++index;
      CHECK(index < count && !seen[index]);
      seen[index] = true;
      type(s.game, s.game.answer());
      CHECK(s.game.submit(allow, nullptr) == wordle::Game::Submit::Won);
      w::record(s);
      CHECK(s.record.played == unsigned(n + 1) && s.record.won == s.record.played &&
            s.record.maximum == s.record.played);
      w::record(s);
      CHECK(s.record.played == unsigned(n + 1));
    }
    CHECK(s.next[length - 4] == 0);
    w::start(s);
    CHECK(w::contains(length, s.game.answer()));
  }
  CHECK(total == 2465 && !w::contains(3, "CAT") && !w::contains(6, "ZZZZZZ"));
  w::State s;
  s.letters = 6;
  w::start(s);
  CHECK(s.game.type('A') && s.game.type('B'));
  uint8_t bytes[w::kSaveBytes];
  w::encode(s, UINT32_MAX, bytes);
  w::State restored;
  uint32_t generation = 0;
  CHECK(w::decode(bytes, restored, generation) && generation == UINT32_MAX && restored.game.typed() == 2);
  CHECK(!std::strcmp(restored.game.answer(), s.game.answer()) && !std::strcmp(restored.game.typing(), "AB"));
  for (size_t n = 0; n < w::kSaveBytes; ++n) {
    bytes[n] ^= 1;
    CHECK(!w::decode(bytes, restored, generation));
    bytes[n] ^= 1;
  }
  w::record(s, true);
  CHECK(s.record.played == 1 && s.record.won == 0 && s.record.streak == 0 && !s.active);
  w::encode(s, 1, bytes);
  CHECK(w::decode(bytes, restored, generation));
  s.letters = 4;
  w::start(s);
  type(s.game, s.game.answer());
  s.game.submit(allow, nullptr);
  w::record(s);
  CHECK(s.record.played == 2 && s.record.won == 1 && s.record.maximum == 1);
  testStorage = {};
  w::Store store;
  CHECK(store.load(restored));
  CHECK(store.save(s));
  const auto writes = testStorage.writes;
  CHECK(store.save(s) && testStorage.writes == writes);
  w::Store reload;
  CHECK(reload.load(restored) && restored.record.played == 2 && restored.game.status() == wordle::Game::Status::Won);
  w::start(s);
  testStorage.writeLimit = 12;
  CHECK(!reload.save(s));
  testStorage.writeLimit = -1;
  w::Store recover;
  CHECK(recover.load(restored) && restored.record.played == 2 && restored.game.over());
  w::encode(s, 200, bytes);
  bytes[11] = w::kVocabularyVersion + 1;
  uint32_t vocabularyHash = 2166136261u;
  for (size_t n = 0; n < w::kSaveBytes - 4; ++n) vocabularyHash = (vocabularyHash ^ bytes[n]) * 16777619u;
  for (int n = 0; n < 4; ++n) bytes[w::kSaveBytes - 4 + n] = uint8_t(vocabularyHash >> (8 * n));
  const auto priorSlot = testStorage.files["/.crosspoint/wordle-local-b.bin"];
  testStorage.files["/.crosspoint/wordle-local-b.bin"] = {bytes, bytes + w::kSaveBytes};
  w::Store futureVocabulary;
  const auto bothVersions = testStorage.files;
  CHECK(!futureVocabulary.load(restored) && !futureVocabulary.save(s) && testStorage.files == bothVersions);
  testStorage.files["/.crosspoint/wordle-local-b.bin"] = priorSlot;
  testStorage.files["/.crosspoint/wordle-local-b.bin"] = {'W', 'D', 'L', '2'};
  w::Store future;
  CHECK(!future.load(restored));
  const auto saved = testStorage.files;
  CHECK(!future.save(s) && testStorage.files == saved);
  testStorage = {};
  testStorage.files["/.crosspoint/wordle.res"] = {
      uint8_t(wordle::kOnTheDay | 2), uint8_t(wordle::kOnTheDay | wordle::kLost), uint8_t(wordle::kOnTheDay | 1)};
  const char legacy[] = "10 PLANT CRANE\n11 APPLE\n";
  testStorage.files["/.crosspoint/wordle.games"] = {legacy, legacy + sizeof(legacy) - 1};
  w::Store imported;
  restored = {};
  CHECK(imported.load(restored));
  CHECK(restored.record.played == 3 && restored.record.won == 2 && restored.record.streak == 1 && restored.active &&
        !std::strcmp(restored.game.answer(), "APPLE"));
  CHECK(imported.save(restored));
  w::Store twice;
  w::State second;
  CHECK(twice.load(second) && second.record.played == 3);
  CHECK(testStorage.files["/.crosspoint/wordle.games"].size() == sizeof(legacy) - 1);
  std::printf("Local Wordle: %d checks, %d failed; state=%zu store=%zu bytes\n", checks, failures, sizeof(w::State),
              sizeof(w::Store));
  return failures ? 1 : 0;
}

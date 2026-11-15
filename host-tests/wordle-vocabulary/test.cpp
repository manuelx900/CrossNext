#include <HalStorage.h>
#include <Utf8.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../../src/apps_local/wordle/NativeWordle.h"
#include "ExpansionCases.h"

static std::vector<uint8_t> read(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  assert(file);
  return {std::istreambuf_iterator<char>(file), {}};
}
static uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
static uint32_t digest(const uint8_t* p, size_t n) {
  uint32_t value = 2166136261u;
  while (n--) value = (value ^ *p++) * 16777619u;
  return value;
}
int main(int argc, char** argv) {
  assert(argc == 3);
  unsigned banks = 0, examples = 0, resumes = 0;
  size_t maxOpenReads = 0, maxLookupReads = 0;
  for (unsigned n = 0; n < unsigned(Language::_COUNT); ++n) {
    if (Language(n) == Language::OOK) continue;
    const std::string name = std::string(LANGUAGE_CODES[n]) + ".cpwb";
    const std::string card = "/languages/wordle/" + name;
    const auto oldBytes = read(std::string(argv[1]) + "/" + name);
    auto newBytes = read(std::string(argv[2]) + "/" + name);
    assert(digest(newBytes.data(), 52) == u32(newBytes.data() + 52));
    assert(digest(newBytes.data() + 64, newBytes.size() - 64) == u32(newBytes.data() + 48));
    for (size_t at = 64; at < newBytes.size(); at += 108)
      assert(digest(newBytes.data() + at, 104) == u32(newBytes.data() + at + 104));
    testStorage.files[card] = oldBytes;
    wordle::NativeWordle old;
    assert(old.open(Language(n)));
    assert(old.start(5, 0));
    char answer[97];
    std::strcpy(answer, old.word(6));
    unsigned first = 0;
    for (unsigned i = 0; i < 3; ++i) first += u32(oldBytes.data() + 8 + 4 * i);
    const char* previousGuess = reinterpret_cast<const char*>(oldBytes.data() + 64 + first * 108);
    if (!std::strcmp(previousGuess, answer)) previousGuess += 108;
    assert(old.submit(previousGuess) && old.active() && !old.over() && old.guesses() == 1);
    char guess[97];
    std::strcpy(guess, old.word(0));
    const auto played = old.statistics().played;
    old.close();
    testStorage.files[card] = std::move(newBytes);
    wordle::NativeWordle updated;
    auto reads = testStorage.readCalls;
    assert(updated.open(Language(n)));
    // Loading a saved row also performs bounded lookups. Measure a clean open
    // separately below so save restoration is not confused with header I/O.
    assert(updated.active() && !updated.over() && updated.guesses() == 1);
    assert(!std::strcmp(updated.word(6), answer) && !std::strcmp(updated.word(0), guess));
    assert(updated.statistics().played == played);
    ++resumes;
    for (const auto& example : EXPANSION_CASES) {
      if (example.language != Language(n)) continue;
      reads = testStorage.readCalls;
      if (!updated.accepts(example.units, example.word)) {
        std::printf("Rejected new word: %s %s\n", LANGUAGE_CODES[n], example.word);
        std::fflush(stdout);
        assert(false);
      }
      maxLookupReads = std::max(maxLookupReads, testStorage.readCalls - reads);
      assert(testStorage.readCalls - reads <= 40);
      ++examples;
    }
    assert(!updated.accepts(5, "12345") && !updated.accepts(5, "!@#$%"));
    assert(updated.submit(answer) && updated.victory());
    updated.close();
    const std::string save = std::string("/.crosspoint/wordle/") + LANGUAGE_CODES[n] + ".bin";
    testStorage.files.erase(save);
    reads = testStorage.readCalls;
    assert(updated.open(Language(n)));
    maxOpenReads = std::max(maxOpenReads, testStorage.readCalls - reads);
    assert(testStorage.readCalls - reads <= 4);
    updated.close();
    testStorage.files.erase(card);
    ++banks;
  }
  assert(banks == 38 && resumes == 38 && examples > 38);
  std::printf(
      "Expansion: %u banks, %u new-word forms accepted, %u old games resumed; max open %zu reads, lookup %zu reads; "
      "all body/record checksums valid\n",
      banks, examples, resumes, maxOpenReads, maxLookupReads);
}

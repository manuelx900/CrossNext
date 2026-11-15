#include <Utf8.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "../../src/apps_local/ui/UnicodeName.h"
#include "../../src/apps_local/wordle/NativeWordle.h"
#include "WordleCoverage.h"

int main() {
  assert(utf8ComposeNfc(u8"か\u3099") == u8"が");
  assert(utf8ComposeNfc(u8"ハ\u309A") == u8"パ");
  assert(utf8ComposeNfc(u8"ウ\u3099") == u8"ヴ");
  assert(utf8ComposeNfc(u8"e\u0301") == u8"é");
  assert(utf8ComposeNfc(u8"ا\u0653ب") == u8"آب");
  char arabic[] = u8"ا\u0654 و\u0654 ا\u0655 ي\u0654 ە\u0654 ہ\u0654 ے\u0654";
  utf8ComposeNfcInPlace(arabic);
  assert(!std::strcmp(arabic, u8"أ ؤ إ ئ ۀ ۂ ۓ"));
  assert(unicode_name::valid(u8"มะลิ", 25, 12, false));
  assert(unicode_name::valid(u8"小花", 25, 12, false));
  assert(unicode_name::valid(u8"Катя", 25, 12, false));
  assert(!unicode_name::valid("\xc0\xaf", 25, 12, false));
  unsigned banks = 0, games = 0;
  for (unsigned code = 0; code < unsigned(Language::_COUNT); ++code) {
    const std::string local = std::string("assets_local/wordle-languages/") + LANGUAGE_CODES[code] + ".cpwb";
    std::ifstream in(local, std::ios::binary);
    if (!in) continue;
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    const auto card = std::string("/languages/wordle/") + LANGUAGE_CODES[code] + ".cpwb";
    testStorage.files[card] = bytes;
    wordle::NativeWordle game;
    const auto openingReads = testStorage.readCalls;
    assert(game.open(Language(code)));
    assert(testStorage.readCalls - openingReads <= 4);
    for (const auto& example : COMMON_WORDS) {
      if (example.language != Language(code)) continue;
      const auto reads = testStorage.readCalls;
      if (!game.accepts(example.letters, example.word)) {
        std::printf("Missing common word %s: %s\n", LANGUAGE_CODES[code], example.word);
        std::fflush(stdout);
        assert(false);
      }
      assert(testStorage.readCalls - reads <= 42);
    }
    assert(!game.accepts(0, "calor"));
    assert(!game.accepts(7, "calor"));
    assert(!game.accepts(5, "12345"));
    assert(!game.accepts(5, nullptr));
    if (Language(code) == Language::ES) {
      assert(game.acceptsBuiltin(5, "CALOR", 1));
      assert(game.acceptsBuiltin(4, "NI[O", 1));
      assert(!game.acceptsBuiltin(5, "CALOR", 0));
    }
    if (Language(code) == Language::EN) {
      assert(game.acceptsBuiltin(5, "WATER", 0));
      assert(!game.acceptsBuiltin(4, "NI[O", 0));
    }
    ++banks;
    for (unsigned n = 2; n <= 6; ++n) {
      if (!game.supports(n)) continue;
      char sample[97]{};
      uint8_t boundaries[6]{};
      const auto sampleReads = testStorage.readCalls;
      assert(game.sample(n, sample, boundaries));
      assert(testStorage.readCalls - sampleReads <= 48);
      const auto number = [&bytes](unsigned at) {
        return uint32_t(bytes[at]) | uint32_t(bytes[at + 1]) << 8 | uint32_t(bytes[at + 2]) << 16 |
               uint32_t(bytes[at + 3]) << 24;
      };
      unsigned firstAnswer = 0;
      for (unsigned i = 0; i < 5; ++i) firstAnswer += number(8 + i * 4);
      for (unsigned i = 2; i < n; ++i) firstAnswer += number(20 + i * 4);
      bool isAnswer = false;
      for (unsigned i = 0; i < number(20 + n * 4); ++i)
        isAnswer |= !std::memcmp(sample, bytes.data() + 64 + (firstAnswer + i) * 108, sizeof(sample));
      assert(isAnswer);
      bool varied = false;
      for (unsigned i = 1; i < n; ++i) {
        const unsigned from = boundaries[i - 1], length = boundaries[i] - from;
        varied |= length != boundaries[0] || std::memcmp(sample, sample + from, length);
        if (i < 2) continue;
        const unsigned previous = boundaries[i - 2], before = i > 2 ? boundaries[i - 3] : 0;
        assert(length != from - previous || length != previous - before ||
               std::memcmp(sample + from, sample + previous, length) ||
               std::memcmp(sample + previous, sample + before, length));
      }
      assert(varied);
      assert(game.start(n, 0));
      assert(game.active() && !game.over());
      char answer[97];
      std::strcpy(answer, game.word(6));
      char input[97];
      std::strcpy(input, answer);
      if (Language(code) != Language::TR && Language(code) != Language::AZ)
        for (char* c = input; *c; ++c)
          if (*c >= 'a' && *c <= 'z') *c -= 32;
      assert(!game.submit("12345"));
      assert(game.guesses() == 0);
      const auto reads = testStorage.readCalls;
      assert(game.submit(input));
      assert(game.victory());
      assert(game.statistics().won > 0);
      assert(testStorage.readCalls - reads <= 46);
      for (unsigned c = 0; c < n; ++c) {
        assert(game.mark(0, c) == wordle::Mark::Correct);
        char key[16]{};
        game.unit(0, c, key);
        assert(game.keyMark(key) == wordle::Mark::Correct);
      }
      wordle::NativeWordle restored;
      assert(restored.open(Language(code)));
      assert(restored.victory() && restored.guesses() == 1);
      assert(!std::strcmp(restored.word(6), answer));
      assert(restored.statistics().won == game.statistics().won);
      ++games;
    }
    unsigned size = 2;
    while (!game.supports(size)) ++size;
    assert(game.start(size, 0));
    // Vocabulary expansion keeps an unfinished game when its words still exist.
    auto& updated = testStorage.files[card];
    updated[48] ^= 1;
    uint32_t checksum = 2166136261u;
    for (unsigned i = 0; i < 52; ++i) checksum = (checksum ^ updated[i]) * 16777619u;
    for (unsigned i = 0; i < 4; ++i) updated[52 + i] = uint8_t(checksum >> (8 * i));
    wordle::NativeWordle expanded;
    assert(expanded.open(Language(code)) && expanded.active() && expanded.guesses() == 0);
    assert(!std::strcmp(expanded.word(6), game.word(6)));
    assert(expanded.statistics().played == game.statistics().played);
    updated = bytes;
    char winning[97];
    std::strcpy(winning, game.word(6));
    testStorage.writeLimit = 32;
    assert(game.submit(winning) && game.persistenceFailed());
    testStorage.writeLimit = -1;
    wordle::NativeWordle beforeFailure;
    assert(beforeFailure.open(Language(code)) && beforeFailure.guesses() == 0);
    const std::string savePath = std::string("/.crosspoint/wordle/") + LANGUAGE_CODES[code] + ".bin";
    auto& saved = testStorage.files[savePath];
    saved[2] = '9';
    saved[3] = '9';
    const auto protectedBytes = saved;
    wordle::NativeWordle future;
    assert(future.open(Language(code)) && future.persistenceFailed());
    assert(future.start(size, 0) && future.persistenceFailed());
    assert(testStorage.files[savePath] == protectedBytes);
    game.close();
    testStorage.files[card][40] ^= 1;
    assert(!game.open(Language(code)));
    testStorage.files.erase(card);
    testStorage.files.erase(savePath);
  }
  assert(banks == 38);
  std::printf("Native Wordle: %u banks, %u win/save/restore/lookup cases; Unicode names and Kana NFC passed\n", banks,
              games);
}

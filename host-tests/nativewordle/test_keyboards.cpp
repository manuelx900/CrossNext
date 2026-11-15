#include <HalStorage.h>
#include <Utf8.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>

#include "../../src/activities/util/KeyboardLayoutSet.h"
#include "../../src/activities/util/LanguageKeyboard.h"
#include "../../src/activities/util/PinyinInput.h"
#include "../../src/apps_local/ui/KeyboardAlternates.h"

static void install(const std::string& from, const std::string& to) {
  std::ifstream f(from, std::ios::binary);
  assert(f);
  testStorage.files[to] = std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
int main() {
  unsigned layouts = 0;
  for (unsigned n = 0; n < unsigned(Language::_COUNT); ++n) {
    const auto local = std::string("assets_local/language-keyboards/") + LANGUAGE_CODES[n] + ".cpkbd";
    if (!std::ifstream(local)) continue;
    const auto path = std::string("/languages/keyboards/") + LANGUAGE_CODES[n] + ".cpkbd";
    install(local, path);
    LanguageKeyboard keyboard;
    assert(keyboard.load(Language(n)));
    for (bool shift : {false, true}) {
      const auto& layout = keyboard.layout(shift);
      assert(layout.rowCount <= 6);
      unsigned keys = 0;
      for (unsigned r = 0; r < layout.rowCount; ++r) keys += layout.rows[r].count;
      assert(keys <= 80);
    }
    keyboard.clear();
    assert(!keyboard.available());
    testStorage.files[path][16] ^= 1;
    assert(!keyboard.load(Language(n)));
    install(local, path);
    ++layouts;
  }
  assert(layouts == 30);
  unsigned testedWords = 0;
  bool complete = true;
  for (unsigned n = 0; n < unsigned(Language::_COUNT); ++n) {
    const auto local = std::string("assets_local/wordle-languages/") + LANGUAGE_CODES[n] + ".cpwb";
    std::ifstream bank(local, std::ios::binary);
    if (!bank) continue;
    if (Language(n) == Language::ZH_CN || Language(n) == Language::ZH_TW) continue;
    LanguageKeyboard external;
    const bool optional = external.load(Language(n));
    auto builtin = freeink::ui::KeyboardLayoutId::QwertyEn;
    for (const auto& entry : keyboard_layouts::ALL)
      if (entry.language == Language(n)) builtin = entry.id;
    std::set<std::string> outputs;
    std::set<uint32_t> reachable;
    std::set<uint32_t> missing;
    for (bool shifted : {false, true}) {
      const auto& layout = optional ? external.layout(shifted)
                                    : freeink::ui::builtinKeyboardLayout(builtin, shifted, false, false, true);
      for (unsigned row = 0; row < layout.rowCount; ++row)
        for (unsigned col = 0; col < layout.rows[row].count; ++col) {
          const auto& k = layout.rows[row].keys[col];
          if (k.output && *k.output) outputs.insert(k.output);
          if (k.alt && *k.alt) outputs.insert(k.alt);
          keyboardview::Alternates alternatives;
          if (alternatives.open(k, Language(n), freeink::ui::makeRect(0, 500, 44, 52)))
            for (unsigned i = 0; i < alternatives.count(); ++i) outputs.insert(alternatives.choice(i));
        }
    }
    for (const auto& output : outputs) {
      const auto* at = reinterpret_cast<const unsigned char*>(output.c_str());
      while (*at) reachable.insert(utf8NextCodepoint(&at));
      for (const auto& tail : outputs) {
        const auto* cursor = reinterpret_cast<const unsigned char*>(tail.c_str());
        const auto cp = utf8NextCodepoint(&cursor);
        if (!utf8IsCombiningMark(cp) && cp != 0x3099 && cp != 0x309a) continue;
        const auto composed = utf8ComposeNfc(output + tail);
        cursor = reinterpret_cast<const unsigned char*>(composed.c_str());
        while (*cursor) reachable.insert(utf8NextCodepoint(&cursor));
      }
    }
    char header[64];
    bank.read(header, 8);
    const bool current = header[4] == 2;
    bank.seekg(current ? 64 : 32);
    char record[108];
    while (bank.read(record, current ? 108 : 104)) {
      const auto* at = reinterpret_cast<const unsigned char*>(record);
      while (*at) {
        const auto cp = utf8NextCodepoint(&at);
        if (!reachable.count(cp) && missing.insert(cp).second) {
          std::printf("Unreachable %s U+%04X in %s\n", LANGUAGE_CODES[n], unsigned(cp), record);
          std::fflush(stdout);
          complete = false;
        }
      }
      ++testedWords;
    }
  }
  assert(complete);
  std::printf("Actual keyboard outputs cover %u native-bank words\n", testedWords);
  for (const auto language : {Language::ZH_CN, Language::ZH_TW}) {
    const char* name = language == Language::ZH_CN ? "sc" : "tc";
    const auto local = std::string("assets_local/language-keyboards/pinyin-") + name + ".bin";
    const auto path = std::string("/languages/keyboards/pinyin-") + name + ".bin";
    install(local, path);
    PinyinInput p;
    assert(p.begin(language));
    assert(!p.canPage(-1) && !p.canPage(1));
    assert(p.type('n') && p.type('i'));
    assert(std::strlen(p.candidate(0)) == 3);
    p.clear();
    assert(p.type('s') && p.type('h') && p.type('i'));
    assert(!p.canPage(-1) && p.canPage(1));
    assert(p.page(1));
    assert(p.page(-1));
    assert(!p.page(-1));
    assert(p.erase());
    p.clear();
    assert(!*p.spelling());
    p.close();
    assert(!p.available());
  }
  std::puts("30 runtime layouts and both pinyin candidate dictionaries passed");
}

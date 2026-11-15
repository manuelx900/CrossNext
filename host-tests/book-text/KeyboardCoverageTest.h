#pragma once

#include <Utf8.h>

#include <cassert>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>

#include "expected-keyboard-outputs.h"

static void auditKeyboardCoverage(const std::string& proof) {
  namespace fui = freeink::ui;
  using Outputs = std::set<std::string>;
  auto collect = [](Outputs& outputs, const fui::KeyboardLayout& layout, Language language, bool legacy) {
    for (unsigned row = 0; row < layout.rowCount; ++row)
      for (unsigned col = 0; col < layout.rows[row].count; ++col) {
        const auto& key = layout.rows[row].keys[col];
        if (key.output && *key.output) outputs.insert(key.output);
        if (legacy) {
          if (key.alt && *key.alt) outputs.insert(key.alt);
          continue;
        }
        keyboardview::Alternates popup;
        if (popup.open(key, language, fui::makeRect(20, 500, 44, 52))) {
          assert(popup.count() >= 2 && popup.count() <= keyboardview::Alternates::Capacity);
          for (unsigned n = 0; n < popup.count(); ++n) outputs.insert(popup.choice(n));
        }
      }
  };
  auto installFile = [](const std::string& source, const std::string& destination) {
    std::ifstream file(source, std::ios::binary);
    if (!file) return false;
    Storage.files[destination] = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    return true;
  };
  std::ofstream report(proof + "/language-coverage.tsv");
  report << "language\toutputs\tmissing_legacy\tmissing_pack\tmissing_ascii_symbols\n";
  unsigned languages = 0, packs = 0;
  for (unsigned index = 0; index < unsigned(Language::_COUNT); ++index) {
    const auto language = Language(index);
    const std::string code = LANGUAGE_CODES[index];
    Outputs actual, expected;
    LanguageKeyboard pack;
    const bool optional =
        installFile("assets_local/language-keyboards/" + code + ".cpkbd", "/languages/keyboards/" + code + ".cpkbd") &&
        pack.load(language);
    if (optional) {
      ++packs;
      for (bool shift : {false, true}) collect(actual, pack.layout(shift), language, false);
    }
    for (const auto& builtin : keyboard_layouts::ALL)
      if (builtin.language == language || language == Language::OOK) {
        for (bool shift : {false, true}) {
          const auto& letters = fui::builtinKeyboardLayout(builtin.id, shift, false, true, true);
          collect(expected, letters, language, true);
          if (!optional)
            collect(actual, fui::builtinKeyboardLayout(builtin.id, shift, false, false, true), language, false);
          collect(expected, fui::builtinKeyboardLayout(builtin.id, shift, true, false, true), language, true);
        }
        if (language == Language::OOK) break;
      }
    for (bool second : {false, true}) collect(actual, keyboardview::symbols(second, true), language, false);
    unsigned missingLegacy = 0, missingPack = 0, missingSymbols = 0;
    for (const auto& output : expected)
      if (!actual.count(output)) {
        ++missingLegacy;
        std::printf("Lost legacy output %s %s\n", code.c_str(), output.c_str());
      }
    for (const auto& entry : expectedOutputs)
      if (entry.language == language && !actual.count(entry.output)) {
        ++missingPack;
        std::printf("Lost pack output %s %s\n", code.c_str(), entry.output);
      }
    for (char c = 32; c < 127; ++c)
      if (!std::isalpha(static_cast<unsigned char>(c)) && !actual.count(std::string(1, c))) ++missingSymbols;
    assert(!missingLegacy && !missingPack && !missingSymbols);
    // Every documented common variant must survive locale merging/capacity.
    const auto* common = reinterpret_cast<const unsigned char*>("¿¡–—−…·‘’‚‹›“”„«»°ºª‰±×÷≠≈∞≤≥€£¥¢₽₹₩₺`^´¨");
    while (*common) {
      const auto* begin = common;
      utf8NextCodepoint(&common);
      assert(actual.count(std::string(reinterpret_cast<const char*>(begin), common - begin)));
    }
    if (language == Language::ES)
      for (const char* s : {"¿", "¡", "º", "ª", "°"}) assert(actual.count(s));
    if (language == Language::CA || language == Language::CAV) assert(actual.count("l·l"));
    if (language == Language::AR)
      for (const char* s : {"؟", "،", "؛", "٪", "ـ", "َ", "ِ", "ُ", "ً", "ٌ", "ٍ", "ّ", "ْ"}) assert(actual.count(s));
    if (language == Language::HE)
      for (const char* s : {"ּ", "ׁ", "ׂ"}) assert(actual.count(s));
    if (language == Language::VI)
      for (const char* s : {"ạ", "ả", "ậ", "ẫ", "ẳ", "ặ", "ệ", "ễ", "ị", "ĩ", "ộ", "ợ", "ỡ", "ự", "ữ", "ỵ", "Ỵ"})
        assert(actual.count(s));
    if (language == Language::AR || language == Language::FA || language == Language::TH) {
      const uint32_t zero = language == Language::AR ? 0x660 : language == Language::FA ? 0x6f0 : 0xe50;
      for (unsigned digit = 0; digit < 10; ++digit) {
        char text[8];
        *utf8AppendCodepoint(text, zero + digit) = 0;
        assert(actual.count(text));
      }
    }
    report << code << '\t' << actual.size() << '\t' << missingLegacy << '\t' << missingPack << '\t' << missingSymbols
           << '\n';
    ++languages;
  }
  assert(languages == 39 && packs == 30);
  std::printf(
      "Reachability audit: %u languages, %u packs, all shipped letters/alts and ASCII symbols preserved; locale "
      "punctuation/digits/tones passed\n",
      languages, packs);
}

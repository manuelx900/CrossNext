#pragma once

#include "../../src/apps_local/ui/UnicodeName.h"
#include "mobile-keyboard-repertoire.h"

// Product reference rows are independent of the layout generator.
static void auditMobileKeyboards() {
  namespace fui = freeink::ui;
  struct Reference {
    Language language;
    const char* first;
    const char* second;
    const char* third;
  };
  static constexpr Reference references[] = {
      {Language::EN, "qwertyuiop", "asdfghjkl", "zxcvbnm"},
      {Language::ES, "qwertyuiop", "asdfghjklñ", "zxcvbnm"},
      {Language::FR, "azertyuiop", "qsdfghjklm", "wxcvbn"},
      {Language::DA, "qwertyuiopå", "asdfghjklæø", "zxcvbnm"},
      {Language::FI, "qwertyuiopå", "asdfghjklöä", "zxcvbnm"},
      {Language::SV, "qwertyuiopå", "asdfghjklöä", "zxcvbnm"},
      {Language::NB, "qwertyuiopå", "asdfghjkløæ", "zxcvbnm"},
      {Language::CS, "qwertzuiop", "asdfghjkl", "yxcvbnm"},
      {Language::HU, "qwertzuiop", "asdfghjkl", "yxcvbnm"},
      {Language::BS, "qwertzuiop", "asdfghjkl", "yxcvbnm"},
      {Language::SK, "qwertzuiop", "asdfghjkl", "yxcvbnm"},
      {Language::SI, "qwertzuiop", "asdfghjkl", "yxcvbnm"},
      {Language::TR, "qwertyuıopğü", "asdfghjklşi", "zxcvbnmöç"},
      {Language::AZ, "qüertyuiopöğ", "asdfghjklıə", "zxcvbnmçş"},
      {Language::UK, "йцукенгшщзхї", "фівапролджє", "ячсмитьбю"},
      {Language::BG, "явертъуиопч", "асдфгхйклшщ", "зьцжбнмю"},
      {Language::AR, "ضصثقفغعهخحج", "شسيبلاتنمكط", "ذءؤرىةوزظد"},
      {Language::HE, "קראטוןםפ", "שדגכעיחלךף", "זסבהנמצתץ"},
      {Language::VI, "qwertyuiop", "asdfghjkl", "zxcvbnm"},
      {Language::JA, "あかさ", "たなは", "まやら"},
  };
  unsigned preserved = 0;
  for (unsigned n = 0; n < unsigned(Language::_COUNT); ++n) {
    const auto language = Language(n);
    LanguageKeyboard pack;
    const bool optional = pack.load(language);
    auto id = fui::KeyboardLayoutId::QwertyEn;
    bool builtin = language == Language::OOK;
    for (const auto& item : keyboard_layouts::ALL)
      if (item.language == language) {
        builtin = true;
        id = item.id;
      }
    CHECK(optional || builtin);
    if (!optional && !builtin) continue;
    std::set<std::string> outputs;
    for (bool upper : {false, true}) {
      const auto& layout = builtin ? fui::builtinKeyboardLayout(id, upper, false, false, true) : pack.layout(upper);
      CHECK(optional || builtin);
      for (bool symbols : {false, true}) {
        const auto& page = symbols ? keyboardview::symbols(upper, true) : layout;
        for (unsigned r = 0; r < page.rowCount; ++r)
          for (unsigned c = 0; c < page.rows[r].count; ++c) {
            const auto& key = page.rows[r].keys[c];
            if (key.output) outputs.insert(key.output);
            keyboardview::Alternates popup;
            if (popup.open(key, language, fui::makeRect(10, 500, 44, 52)))
              for (unsigned i = 0; i < popup.count(); ++i) outputs.insert(popup.choice(i));
          }
      }
      if (upper) continue;
      for (const auto& reference : references) {
        if (reference.language != language) continue;
        const char* expected[] = {reference.first, reference.second, reference.third};
        for (unsigned r = 0; r < 3; ++r) {
          std::string letters;
          for (unsigned c = 0; c < layout.rows[r].count; ++c) {
            const auto& key = layout.rows[r].keys[c];
            if (key.kind == fui::KeyKind::Normal && key.output) letters += key.output;
          }
          CHECK(letters == expected[r]);
        }
      }
      if (!builtin) {
        const auto& lastLetters = layout.rows[layout.rowCount - 2];
        CHECK(lastLetters.keys[0].kind == fui::KeyKind::Shift);
        CHECK(lastLetters.keys[lastLetters.count - 1].kind == fui::KeyKind::Delete);
        CHECK(layout.rows[layout.rowCount - 1].count == 4);
        if (language != Language::TH && language != Language::JA) CHECK(layout.rowCount == 4);
      }
      if (builtin && optional) {
        const auto& card = pack.layout(false);
        CHECK(card.rowCount == layout.rowCount);
        for (unsigned r = 0; r < layout.rowCount; ++r) {
          std::string native, disk;
          for (unsigned c = 0; c < layout.rows[r].count; ++c)
            if (layout.rows[r].keys[c].kind == fui::KeyKind::Normal) native += layout.rows[r].keys[c].output;
          for (unsigned c = 0; c < card.rows[r].count; ++c)
            if (card.rows[r].keys[c].kind == fui::KeyKind::Normal) disk += card.rows[r].keys[c].output;
          CHECK(native == disk);
        }
      }
    }
    for (const auto& expected : legacyKeyboardOutputs)
      if (expected.language == language) {
        if (!outputs.count(expected.output))
          std::printf("Missing pre-change repertoire %s: %s\n", LANGUAGE_CODES[n], expected.output);
        CHECK(outputs.count(expected.output));
        ++preserved;
      }
  }
  // The native alphabets are independent of the pre-change output fixture.
  for (const auto language : {Language::UK, Language::TH}) {
    std::set<uint32_t> actual;
    LanguageKeyboard pack;
    const bool external = language == Language::TH;
    if (external) CHECK(pack.load(language));
    for (unsigned layer = 0; layer < 4; ++layer) {
      const bool upper = (layer & 1) != 0;
      const auto& layout =
          layer >= 2 ? keyboardview::symbols(upper, true)
          : external ? pack.layout(upper)
                     : fui::builtinKeyboardLayout(fui::KeyboardLayoutId::CyrillicUk, upper, false, false, true);
      for (unsigned r = 0; r < layout.rowCount; ++r)
        for (unsigned c = 0; c < layout.rows[r].count; ++c) {
          const auto& key = layout.rows[r].keys[c];
          if (!key.output) continue;
          const auto collect = [&](const char* text) {
            const auto* at = reinterpret_cast<const uint8_t*>(text);
            uint32_t cp;
            while ((cp = utf8NextCodepoint(&at))) actual.insert(cp);
          };
          collect(key.output);
          keyboardview::Alternates alternates;
          if (alternates.open(key, language, {}))
            for (unsigned n = 0; n < alternates.count(); ++n) collect(alternates.choice(n));
        }
    }
    if (language == Language::TH) {
      for (uint32_t cp = 0x0e01; cp <= 0x0e5b; ++cp)
        if (cp <= 0x0e3a || cp >= 0x0e3f) CHECK(actual.count(cp));
    } else {
      const auto* at =
          reinterpret_cast<const uint8_t*>("абвгґдеєжзиіїйклмнопрстуфхцчшщьюяАБВГҐДЕЄЖЗИІЇЙКЛМНОПРСТУФХЦЧШЩЬЮЯʼ");
      uint32_t cp;
      while ((cp = utf8NextCodepoint(&at))) CHECK(actual.count(cp));
    }
  }
  keyboardview::Alternates persian;
  persian.open(fui::KeyboardKey{"ه", "ه", fui::KeyKind::Normal}, Language::FA, {});
  bool ezafeh = false;
  for (unsigned n = 0; n < persian.count(); ++n) ezafeh = ezafeh || !std::strcmp(persian.choice(n), "هٔ");
  CHECK(ezafeh);
  const fui::KeyboardKey minus{"-", "-", fui::KeyKind::Normal};
  for (unsigned n = 0; n < getLanguageCount(); ++n) {
    for (bool text : {false, true}) {
      keyboardview::Alternates alternatives;
      alternatives.open(minus, Language(n), {}, false, true, text);
      bool halfSpace = false;
      for (unsigned k = 0; k < alternatives.count(); ++k)
        halfSpace = halfSpace || !std::strcmp(alternatives.choice(k), "\xe2\x80\x8c");
      CHECK(halfSpace == (text && Language(n) == Language::FA));
    }
  }
  CHECK(unicode_name::valid("กิกิกิกิกิกิกิกิกิกิกิกิ", 97, 12, false));
  CHECK(!unicode_name::valid("กิกิกิกิกิกิกิกิกิกิกิกิกิ", 97, 12, false));
  CHECK(name_units::measure("กิกิกิ", 97).count == 3);
  CHECK(name_units::measure("กิกิกิ", 97).last == 12);
  CHECK(name_units::measure("بَبَ", 97).count == 2);
  CHECK(
      !unicode_name::valid("\xcc\x81"
                           "a",
                           97, 12, false));
  CHECK(unicode_name::valid("José", 97, 12, false));
  std::printf("Independent mobile references, fixed controls, and %u pre-change outputs checked\n", preserved);
}

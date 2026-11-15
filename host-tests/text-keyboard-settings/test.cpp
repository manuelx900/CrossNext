#include <CrossPointSettings.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cassert>
#include <cstdio>
#include <set>

#include "src/activities/util/KeyboardLayoutSet.h"
I18n& I18n::getInstance() {
  static I18n instance;
  return instance;
}
void I18n::setLanguage(Language language) { _language = language; }
using namespace keyboard_layouts;
void install(Language language, bool valid = true) {
  char path[80];
  std::snprintf(path, sizeof(path), "/languages/keyboards/%s.cpkbd", LANGUAGE_CODES[unsigned(language)]);
  auto& bytes = testStorage.files[path];
  bytes.assign(32, 0);
  std::memcpy(bytes.data(), valid ? "CPKB" : "XXXX", 4);
  bytes[4] = 1;
}
int main() {
  I18N.setLanguage(Language::ES);
  assert(startingSelection(0).language == Language::ES);
  install(Language::JA);
  install(Language::TH);
  install(Language::ZH_TW);
  install(Language::FR, false);
  const auto card = scanCardLayouts();
  assert(card == (languageBit(Language::JA) | languageBit(Language::TH) | languageBit(Language::ZH_TW)));
  SETTINGS.keyboardLanguageLayouts =
      languageBit(Language::JA) | languageBit(Language::TH) | languageBit(Language::ZH_TW);
  std::set<unsigned> seen;
  auto choice = startingSelection(card);
  for (unsigned n = 0; n < 20; ++n) {
    seen.insert(unsigned(choice.language));
    choice = nextSelection(choice, card);
  }
  assert(seen.size() == 5 && seen.count(unsigned(Language::ZH_TW)));
  SETTINGS.keyboardLanguageLayouts = 0;
  I18N.setLanguage(Language::JA);
  assert(startingSelection(card).external);
  choice = nextSelection(startingSelection(card), card);
  assert(choice.language == Language::EN && !choice.external);
  choice = nextSelection(choice, card);
  assert(choice.language == Language::JA && choice.external);
  // Missing files never enter the ring, even when an old save enables them.
  SETTINGS.keyboardLanguageLayouts = ~uint64_t{0};
  I18N.setLanguage(Language::EN);
  choice = startingSelection(0);
  for (unsigned n = 0; n < 100; ++n) {
    choice = nextSelection(choice, 0);
    assert(choice.language == Language::EN && !choice.external);
  }
  // Each UI language is a starting choice, with a Latin escape for credentials.
  for (unsigned n = 0; n < getLanguageCount(); ++n) {
    const auto language = static_cast<Language>(n);
    I18N.setLanguage(language);
    SETTINGS.keyboardLayouts = uint16_t(1u << 4);
    SETTINGS.keyboardLanguageLayouts = ~uint64_t{0};
    assert(startingSelection(~uint64_t{0}).language == language);
    seen.clear();
    choice = startingSelection(~uint64_t{0});
    for (unsigned k = 0; k < 100; ++k) {
      seen.insert(unsigned(choice.language));
      choice = nextSelection(choice, ~uint64_t{0});
    }
    bool latin = false;
    for (unsigned k = 0; k < 4; ++k) latin = latin || seen.count(unsigned(ALL[k].language));
    assert(seen.count(unsigned(language)) && latin);
    assert(languageBit(language) != 0);
  }
  testStorage.available = false;
  assert(scanCardLayouts() == 0);
  std::puts("Keyboard catalog: 39 UI languages, optional layouts, auto selection, missing cards and cycling passed");
}

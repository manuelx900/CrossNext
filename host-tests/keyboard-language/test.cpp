#include <CrossPointSettings.h>

#include <cstdio>
#include <set>

#include "../../src/activities/util/KeyboardLayoutSet.h"
#include "../../src/apps_local/wordle/WordleScreens.h"
int main() {
  int checks = 0, failed = 0;
#define CHECK(v)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(v)) {                                   \
      ++failed;                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #v); \
    }                                             \
  } while (0)
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (uint16_t mask : {uint16_t(0), uint16_t(1), uint16_t(8), uint16_t(16), uint16_t(65535)}) {
      SETTINGS.keyboardLayouts = mask;
      CHECK(keyboard_layouts::startingLayout() == (language == Language::ES ? freeink::ui::KeyboardLayoutId::SpanishEs
                                                                            : freeink::ui::KeyboardLayoutId::QwertyEn));
    }
    const auto k = wordleui::keyboardLayout(freeink::ui::makeRect(0, 0, 480, 800), 560);
    std::set<char> letters;
    for (int y = 560; y < 760; y += 4)
      for (int x = 0; x < 480; x += 4) {
        const char key = wordleui::keyAt(k, x, y);
        if ((key >= 'A' && key <= 'Z') || key == '[') letters.insert(key);
      }
    CHECK(letters.size() == (language == Language::ES ? 27 : 26));
    CHECK((letters.count('[') > 0) == (language == Language::ES));
  }
  std::printf("Keyboard language: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}

#include <CrossPointSettings.h>

#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "../../src/activities/util/KeyboardLayoutSet.h"
int main() {
  unsigned checks = 0;
  for (const auto& entry : keyboard_layouts::ALL) {
    I18N.setLanguage(entry.language);
    for (const uint16_t mask : {uint16_t(0), uint16_t(1), uint16_t(8), uint16_t(16), uint16_t(512), uint16_t(65535)}) {
      SETTINGS.keyboardLayouts = mask;
      assert(keyboard_layouts::startingLayout() == entry.id);
      assert(keyboard_layouts::enabled() & keyboard_layouts::LATIN_BITS);
      auto next = entry.id;
      bool foundLatin = false;
      for (unsigned i = 0; i < keyboard_layouts::COUNT; ++i) {
        next = keyboard_layouts::next(next);
        for (unsigned n = 0; n < keyboard_layouts::COUNT; ++n)
          if (keyboard_layouts::ALL[n].id == next && (keyboard_layouts::LATIN_BITS & keyboard_layouts::bitAt(n)))
            foundLatin = true;
      }
      assert(foundLatin);
      ++checks;
    }
  }
  std::printf("Automatic system keyboards: %u cases passed\n", checks);
}

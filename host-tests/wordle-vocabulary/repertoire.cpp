#include <FreeInkUI.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#define private public
#include "src/activities/util/EmbeddedKeyboard.h"
#undef private
#include "src/apps_local/ui/KeyboardAlternates.h"
static void install(const std::string& from, const std::string& to) {
  std::ifstream f(from, std::ios::binary);
  if (f) testStorage.files[to] = {std::istreambuf_iterator<char>(f), {}};
}
int main() {
  for (auto which : {"sc", "tc"})
    install(std::string("assets_local/language-keyboards/pinyin-") + which + ".bin",
            std::string("/languages/keyboards/pinyin-") + which + ".bin");
  for (unsigned n = 0; n < getLanguageCount(); n++) {
    auto language = Language(n);
    testI18n.language = language;
    const std::string code = LANGUAGE_CODES[n];
    install("assets_local/language-keyboards/" + code + ".cpkbd", "/languages/keyboards/" + code + ".cpkbd");
    EmbeddedKeyboard k;
    if (!k.begin(language)) return 1;
    k.wordGame(5);
    std::set<std::string> outputs;
    for (unsigned layer = 0; layer < 2; layer++) {
      const auto& layout = k.gameLayout();
      bool shift = false;
      for (unsigned r = 0; r < layout.rowCount; r++)
        for (unsigned c = 0; c < layout.rows[r].count; c++) {
          const auto& key = layout.rows[r].keys[c];
          shift |= key.kind == freeink::ui::KeyKind::Shift;
          if (key.kind != freeink::ui::KeyKind::Normal || !key.enabled || key.value >= 2000 || !key.output ||
              !*key.output)
            continue;
          outputs.insert(key.output);
          keyboardview::Alternates popup;
          if (popup.open(key, language, {}))
            for (unsigned i = 0; i < popup.count(); i++) outputs.insert(popup.choice(i));
        }
      if (!shift) break;
      k.activate(freeink::ui::QWERTY_KEY_SHIFT, false);
    }
    for (const auto& word : outputs) {
      std::printf("%s\t", code.c_str());
      for (unsigned char c : word) std::printf("%02x", c);
      std::puts("");
    }
  }
}

#include <HalStorage.h>

#include <cassert>

#include "FontTarget.h"
#include "src/activities/util/EmbeddedKeyboard.h"
#include "src/apps_local/ui/KeyboardAlternates.h"
#include "src/apps_local/ui/ToyboxScreen.h"
#include "src/apps_local/ui/WordleKeyboard.h"
#include "src/apps_local/ui/fonts/toybox_10.h"
#include "src/apps_local/ui/fonts/toybox_14.h"
#include "src/apps_local/ui/fonts/toybox_20.h"
const char* I18n::getLanguageName(Language l) const { return LANGUAGE_NAMES[unsigned(l)]; }
static void install(const std::string& file, const std::string& path) { Storage.files[path] = read(file); }
static void save(const std::vector<uint8_t>& pixels, int w, int h, const std::string& name) {
  std::ofstream f(outputDir + "/" + name + ".pbm", std::ios::binary);
  f << "P4\n" << w << " " << h << "\n";
  for (auto b : pixels) f.put(char(~b));
}
int main(int argc, char** argv) {
  assert(argc == 3);
  const std::string root = argv[1];
  outputDir = argv[2];
  std::ofstream deleteBounds(outputDir + "/classic-delete-rects.jsonl");
  report = std::fopen((outputDir + "/glyph-issues.jsonl").c_str(), "w");
  for (int g = 0; g < 3; g++)
    for (int n = 0; n < 3; n++)
      sd[g * 3 + n].load(root + "/assets_local/language-fonts/Interface" +
                         (g == 1   ? "_SC"
                          : g == 2 ? "_TC"
                                   : "") +
                         "_" +
                         std::to_string(n == 0   ? 8
                                        : n == 1 ? 10
                                                 : 12) +
                         ".cpfont");
  for (auto code : {"sc", "tc"})
    install(root + "/assets_local/language-keyboards/pinyin-" + code + ".bin",
            std::string("/languages/keyboards/pinyin-") + code + ".bin");
  for (currentLanguage = 0; currentLanguage < 39; currentLanguage++) {
    auto b = read(outputDir + "/" + std::to_string(currentLanguage) + ".bin");
    catalogue.assign(b.begin(), b.end());
    labels.clear();
    for (size_t p = 0; p < catalogue.size();) {
      labels.push_back(catalogue.data() + p);
      p += std::strlen(catalogue.data() + p) + 1;
    }
    I18N.setLanguage(Language(currentLanguage));
    bind(currentLanguage);
    static EpdFont button(&toybox_14), body(&toybox_20), tile(&toybox_10);
    static EpdFontFamily buttonFace(&button), bodyFace(&body), tileFace(&tile);
    fonts.insert_or_assign(0, buttonFace);
    fonts.insert_or_assign(1, bodyFace);
    fonts.insert_or_assign(3, tileFace);
    fallbacks[0] = 101;
    const auto code = std::string(LANGUAGE_CODES[currentLanguage]);
    const auto pack = root + "/assets_local/language-keyboards/" + code + ".cpkbd";
    if (std::ifstream(pack)) install(pack, "/languages/keyboards/" + code + ".cpkbd");
    for (int rotation = 0; rotation < 4; rotation++) {
      const int w = rotation % 2 ? 800 : 480, h = rotation % 2 ? 480 : 800;
      fui::DeviceContext device;
      device.width = w;
      device.height = h;
      device.hasTouch = true;
      device.safeArea = {0, 0, 0, 0};
      for (int state = 0; state < 2; state++) {
        currentCase = code + "-" + std::to_string(rotation) + "-" + std::to_string(state);
        std::vector<uint8_t> pixels((w + 7) / 8 * h, 255);
        FontTarget target(pixels, w, h, true);
        EmbeddedKeyboard keyboard;
        assert(keyboard.begin(Language(currentLanguage)));
        keyboard.wordGame(5);
        const int keyboardWidth = w > h ? (w - 24) * 54 / 100 - 12 : w - 24;
        const auto box = fui::makeRect(w - keyboardWidth - 12, h - keyboard.preferredHeight() - 12, keyboardWidth,
                                       keyboard.preferredHeight());
        keyboard.drawGame(target, device, box, &state, [](const void* ctx, const char* value) {
          if (!*static_cast<const int*>(ctx)) return wordlekeys::Tone::Normal;
          const unsigned x = uint8_t(value[0]);
          return x % 3 == 0   ? wordlekeys::Tone::Correct
                 : x % 3 == 1 ? wordlekeys::Tone::Present
                              : wordlekeys::Tone::Absent;
        });
        save(pixels, w, h, currentCase);
        frames++;
      }
      if (currentLanguage == 0 || currentLanguage == 11) {
        currentCase = "classic-" + code + "-" + std::to_string(rotation);
        std::vector<uint8_t> pixels((w + 7) / 8 * h, 255);
        FontTarget target(pixels, w, h, true);
        toybox::Interactions hits;
        const fui::InputSnapshot input;
        toybox::Frame frame(target, device, input, hits);
        toybox::Screen screen(frame);
        auto box = fui::makeRect(12, h - 216, w - 24, 204);
        auto keys = wordlekeys::keyboardLayout(box, box.y, currentLanguage == 11);
        const auto erase = wordlekeys::eraseRect(keys);
        deleteBounds << "{\"file\":\"" << currentCase << ".pbm\",\"rect\":[" << erase.x << "," << erase.y << ","
                     << erase.width << "," << erase.height << "]}\n";
        wordlekeys::Tone tones[27]{};
        tones[0] = wordlekeys::Tone::Present;
        tones[1] = wordlekeys::Tone::Correct;
        tones[2] = wordlekeys::Tone::Absent;
        wordlekeys::draw(screen, box, keys, 1700, tones);
        save(pixels, w, h, currentCase);
        frames++;
      }
      const auto language = Language(currentLanguage);
      const char* letter =
          language == Language::AR   ? "ا"
          : language == Language::FA ? "ه"
          : language == Language::HE ? "א"
          : language == Language::TH ? "ํ"
          : language == Language::JA ? "か"
          : language == Language::UK ? "г"
          : language == Language::BG ? "и"
          : language == Language::KK ? "а"
          : language == Language::RU || language == Language::BE || language == Language::BG || language == Language::KK
              ? "е"
          : language == Language::ZH_CN || language == Language::ZH_TW ? "?"
                                                                       : "a";
      for (bool game : {false, true}) {
        currentCase = std::string(game ? "game-popup-" : "typing-popup-") + code + "-" + std::to_string(rotation);
        std::vector<uint8_t> pixels((w + 7) / 8 * h, 255);
        FontTarget target(pixels, w, h, true);
        fui::InteractionBuffer<80> hits;
        const fui::InputSnapshot input;
        fui::Frame<80> frame(target, device, input, hits);
        keyboardview::Alternates popup;
        fui::KeyboardKey key;
        key.label = key.output = letter;
        assert(popup.open(key, language, fui::makeRect(w / 2, h - 72, 44, 52)));
        popup.draw(frame, pixelui::geometry(device).body, 1700, game ? pixelui::kBody : 4, game);
        save(pixels, w, h, currentCase);
        frames++;
      }
    }
  }
  std::fclose(report);
  std::printf("Wordle: %d actual SDK/font frames, %d glyph/overflow observations.\n", frames, issues);
  assert(!issues);
}

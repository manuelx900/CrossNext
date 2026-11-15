#include "../../.cache/locale-ui/fixtures/centered.inc"

#include <I18n.h>

#include "Trace.h"
int main(int argc, char** argv) {
  if (argc < 3) return 2;
  outputDir = argv[2];
  report = std::fopen((outputDir + "/centered-issues.jsonl").c_str(), "w");
  for (int group = 0; group < 3; ++group)
    for (int n = 0; n < 3; ++n)
      sd[group * 3 + n].load(std::string(argv[1]) + "/assets_local/language-fonts/Interface" +
                             (group == 1   ? "_SC"
                              : group == 2 ? "_TC"
                                           : "") +
                             "_" +
                             std::to_string(n == 0   ? 8
                                            : n == 1 ? 10
                                                     : 12) +
                             ".cpfont");
  for (currentLanguage = 0; currentLanguage < 39; ++currentLanguage) {
    auto bytes = read(std::string(argv[1]) + "/.cache/locale-ui/fixtures/" + std::to_string(currentLanguage) + ".bin");
    catalogue.assign(bytes.begin(), bytes.end());
    labels.clear();
    for (size_t i = 0; i < catalogue.size();) {
      labels.push_back(catalogue.data() + i);
      i += std::strlen(catalogue.data() + i) + 1;
    }
    I18N.setLanguage(Language(currentLanguage));
    pixelui::turkicCase = I18N.getLanguage() == Language::TR || I18N.getLanguage() == Language::AZ;
    pixelui::preserveTranslatedCase = I18N.getLanguage() != Language::EN && I18N.getLanguage() != Language::ES;
    bind(currentLanguage);
    std::vector<uint8_t> pixels(48000, 255);
    Target target(pixels, 480, 800, false);
    currentCase = "centered-system-messages";
    for (const auto& item : centered) {
      fui::TextStyle style;
      style.font = item.font;
      style.bold = item.bold;
      style.maxLines = item.lines;
      target.text(fui::makeRect(20, 200, 440, item.lines * target.lineHeight(item.font)), I18N.get(item.key), style);
      ++frames;
    }
  }
  std::fclose(report);
  std::printf("System messages: %d cases, %d text issues\n", frames, issues);
  return issues ? 1 : 0;
}

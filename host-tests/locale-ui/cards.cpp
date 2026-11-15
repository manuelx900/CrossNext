#include "../../src/apps_local/hearts/HeartsScreens.h"
#include "../../src/apps_local/solitaire/SolitaireScreens.h"
#include "Trace.h"

enum class CardView { SolitaireHelp, HeartsHelp, SolitaireSetup, SolitaireSettings, HeartsSetup, HeartsSettings };

static void draw(int width, int height, CardView view, int page) {
  currentCase = std::to_string(static_cast<int>(view)) + "-" + std::to_string(page);
  std::vector<uint8_t> pixels((width + 7) / 8 * height, 255);
  Target target(pixels, width, height, false);
  auto device = target.base.deviceContext();
  device.hasTouch = true;
  device.safeArea = width < height ? fui::Insets{10, 1, 0, 1} : fui::Insets{1, 10, 1, 0};
  fui::InputSnapshot input;
  toybox::Interactions interactions;
  toybox::Frame frame(target, device, input, interactions);
  toybox::Screen screen(frame);
  switch (view) {
    case CardView::SolitaireHelp:
      solitaireui::buildHelp(screen, page);
      break;
    case CardView::HeartsHelp:
      heartsui::buildHowTo(screen, {page});
      break;
    case CardView::SolitaireSetup:
      solitaireui::buildSetup(screen, page != 0, true);
      break;
    case CardView::SolitaireSettings:
      solitaireui::buildSettings(screen, {});
      break;
    case CardView::HeartsSetup:
      heartsui::buildSetup(screen, {}, true);
      break;
    case CardView::HeartsSettings:
      heartsui::buildSettings(screen, {});
      break;
  }
  ++frames;
}
int main(int argc, char** argv) {
  if (argc != 3) return 2;
  outputDir = argv[2];
  report = std::fopen((outputDir + "/issues.jsonl").c_str(), "w");
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
    for (auto size : {std::pair{480, 800}, std::pair{800, 480}}) {
      for (int page = 0; page < solitaireui::kHelpPages; ++page)
        draw(size.first, size.second, CardView::SolitaireHelp, page);
      for (int page = 0; page < heartsui::howToPages(); ++page)
        draw(size.first, size.second, CardView::HeartsHelp, page);
      for (auto view :
           {CardView::SolitaireSetup, CardView::SolitaireSettings, CardView::HeartsSetup, CardView::HeartsSettings})
        draw(size.first, size.second, view, 0);
    }
    std::printf("%s: %d frames, %d issues\n", localeCodes[currentLanguage], frames, issues);
  }
  std::fclose(report);
  std::printf("TOTAL %d card UI frames, %d issues\n", frames, issues);
  return issues ? 1 : 0;
}

#include "../../src/apps_local/study/StudyScreens.h"
#include "../../src/apps_local/ui/fonts/reading_serif_14.h"
#include "../../src/apps_local/ui/fonts/reading_serif_bold_14.h"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#include "../../src/apps_local/ui/fonts/ui_prose_bold_14.h"
#include "Trace.h"
static EpdFont tileFace(&toybox_10), uiFace(&toybox_20), proseFace(&ui_prose_14), proseBold(&ui_prose_bold_14),
    serifFace(&reading_serif_14), serifBold(&reading_serif_bold_14);
static EpdFontFamily tileFamily(&tileFace), uiFamily(&uiFace), proseFamily(&proseFace, &proseBold),
    serifFamily(&serifFace, &serifBold);

enum class Appearance { Pixel, Classic, Sans, Serif };
enum class Scenario { MissingDate, UnavailableDecks, OtherDecks, SyncRunning, Unchanged, Stopped };
static void bindAppearance(Appearance appearance) {
  fonts.at(0) = appearance == Appearance::Pixel ? tileFamily : f8;
  fallbacks[0] = 100;
  switch (appearance) {
    case Appearance::Pixel:
      fonts.at(1) = uiFamily;
      fonts.at(2) = coinValueFamily;
      break;
    case Appearance::Classic:
      fonts.at(1) = f12;
      fonts.at(2) = f12;
      break;
    case Appearance::Sans:
      fonts.at(1) = proseFamily;
      fonts.at(2) = proseFamily;
      break;
    case Appearance::Serif:
      fonts.at(1) = serifFamily;
      fonts.at(2) = serifFamily;
      break;
  }
}

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  outputDir = argv[2];
  report = std::fopen((outputDir + "/study-issues.jsonl").c_str(), "w");
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
    for (auto appearance : {Appearance::Pixel, Appearance::Classic, Appearance::Sans, Appearance::Serif}) {
      bindAppearance(appearance);
      for (auto size : {std::pair{480, 800}, std::pair{800, 480}}) {
        for (auto variant : {Scenario::MissingDate, Scenario::UnavailableDecks, Scenario::OtherDecks,
                             Scenario::SyncRunning, Scenario::Unchanged, Scenario::Stopped}) {
          currentCase = "style-" + std::to_string(int(appearance)) + "-study-" + std::to_string(int(variant)) + "-" +
                        std::to_string(size.first);
          std::vector<uint8_t> pixels(48000, 255);
          Target target(pixels, size.first, size.second, false);
          auto device = target.base.deviceContext();
          device.hasTouch = true;
          fui::InputSnapshot input;
          toybox::Interactions interactions;
          toybox::Frame frame(target, device, input, interactions);
          toybox::Screen screen(frame);
          if (variant == Scenario::MissingDate || variant == Scenario::UnavailableDecks ||
              variant == Scenario::OtherDecks) {
            studyui::DeckModel model;
            model.name = "Vocabulary";
            model.deckCount = 3;
            model.due = 10;
            model.fresh = 5;
            model.total = 100;
            model.clockUnset = variant == Scenario::MissingDate;
            model.decksOverCap = variant == Scenario::UnavailableDecks ? 8 : 0;
            model.otherWaiting = variant == Scenario::OtherDecks ? 123456 : 0;
            studyui::buildDeck(screen, model);
          } else {
            studyui::SyncFlowModel model;
            model.leaveSafe = true;
            model.safety = studyui::SyncSafety::ReviewsSafe;
            model.stages[2] = studyui::SyncStageState::Active;
            if (variant != Scenario::SyncRunning) {
              model.verdict =
                  variant == Scenario::Stopped ? studyui::SyncVerdictKind::Neutral : studyui::SyncVerdictKind::Success;
              std::snprintf(model.title, sizeof(model.title), "%s",
                            variant == Scenario::Stopped ? tr(STR_UI_STOPPED_A59E35) : tr(STR_UI_DONE_E782E7));
              std::snprintf(model.body, sizeof(model.body), "%s",
                            tr(STR_UI_YOUR_DECKS_ARE_UNCHANGED_CHANGE_THEM_47AD38));
              model.factCount = 3;
              std::snprintf(model.factLines[0], sizeof(model.factLines[0]), tr(STR_UI_D_REVIEW_S_SENT_0360CA), 123456,
                            "S");
              std::snprintf(model.factLines[1], sizeof(model.factLines[1]),
                            tr(STR_UI_D_DECK_S_REBUILT_WITH_YOUR_E08002), 8, "S");
              std::snprintf(model.factLines[2], sizeof(model.factLines[2]), tr(STR_UI_D_DECK_S_UPDATED_7E2BCA), 8, "S");
              if (variant == Scenario::Stopped)
                std::snprintf(model.body, sizeof(model.body), "%s", tr(STR_UI_STOPPED_THE_DECKS_ON_THE_CARD_76C699));
            }
            studyui::buildSyncFlow(screen, model);
          }
          ++frames;
        }
      }
    }
  }
  std::fclose(report);
  std::printf("Study: %d frames, %d text issues\n", frames, issues);
  return issues ? 1 : 0;
}

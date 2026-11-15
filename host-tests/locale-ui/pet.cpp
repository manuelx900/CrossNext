#include "../../src/apps_local/pet/PetScreens.h"
#include "Trace.h"

static void frame(petui::Model& model, int width, int height, const std::string& name, bool shot) {
  currentCase = name;
  std::vector<uint8_t> pixels((width + 7) / 8 * height, 255);
  Target target(pixels, width, height, shot);
  auto device = target.base.deviceContext();
  device.hasTouch = true;
  device.safeArea = width < height ? fui::Insets{10, 1, 0, 1} : fui::Insets{1, 10, 1, 0};
  fui::InputSnapshot input;
  toybox::Interactions interactions;
  toybox::HeaderStatus status;
  std::snprintf(status.clock, sizeof(status.clock), "23:59");
  petui::Work work;
  petui::render(target, device, input, interactions, status, model, work);
  for (size_t i = 0; i < interactions.count(); ++i) {
    const auto& hit = interactions.data()[i];
    if (hit.rect.x < 0 || hit.rect.y < 0 || hit.rect.right() > width || hit.rect.bottom() > height)
      target.note("hit-outside-screen", hit.rect, "", {}, 0);
  }
  ++frames;
  if (shot) {
    auto file = outputDir + "/" + localeCodes[currentLanguage] + "-" + name + "-" + std::to_string(width) + ".pbm";
    FILE* out = std::fopen(file.c_str(), "wb");
    std::fprintf(out, "P4\n%d %d\n", width, height);
    for (auto byte : pixels) std::fputc(byte ^ 255, out);
    std::fclose(out);
  }
}
int main(int argc, char** argv) {
  if (argc < 3) return 2;
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
    for (auto size : {std::pair{480, 800}, std::pair{800, 480}})
      for (unsigned species = 0; species < 3; ++species)
        for (unsigned age = 0; age < 5; ++age) {
          pet::Profile profile;
          profile.created = true;
          profile.species = pet::Species(species);
          profile.readingSeconds = pet::stageStart(pet::Stage(age));
          profile.memories = 0xffff;
          std::snprintf(profile.name, sizeof(profile.name), "MAXIMILIANO");
          for (unsigned item = 0; item < pet::OBJECT_COUNT; ++item) pet::own(profile, item);
          pet::History history;
          pet::GameState game;
          game.begin(pet::Game::Pairs, 123);
          petui::Model model;
          model.profile = &profile;
          model.history = &history;
          model.game = &game;
          model.today = 20770;
          model.now = 1794614400;
          model.notice = tr(STR_PET_FIRST_STROKE_TEXT);
          model.nameDraft = profile.name;
          for (unsigned view = 0; view <= unsigned(pet::View::SettingsConfirm); ++view) {
            model.view = pet::View(view);
            frame(model, size.first, size.second, "view-" + std::to_string(view),
                  species == 0 && age == 4 && (view == 3 || view == 7 || view == 13 || view == 15));
          }
          for (auto illness : {pet::Illness::Cold, pet::Illness::Malaise}) {
            profile.illness = illness;
            model.view = pet::View::Health;
            frame(model, size.first, size.second, "illness-" + std::to_string(unsigned(illness)), false);
          }
          profile.illness = pet::Illness::Healthy;
          if (species == 0 && age == 4) {
            for (auto view : {pet::View::Info, pet::View::Rules, pet::View::Projects, pet::View::Memories}) {
              model.view = view;
              for (model.page = 1; model.page < 6; ++model.page)
                frame(model, size.first, size.second,
                      "pages-" + std::to_string(unsigned(view)) + "-" + std::to_string(model.page), false);
            }
            model.page = 0;
            for (unsigned kind = 0; kind < 3; ++kind) {
              model.selectedGame = pet::Game(kind);
              game.begin(model.selectedGame, 123);
              for (unsigned phase = 0; phase < 5; ++phase) {
                game.phase = pet::GamePhase(phase);
                model.view = pet::View::Play;
                frame(model, size.first, size.second, "game-" + std::to_string(kind) + "-" + std::to_string(phase),
                      false);
              }
              model.view = pet::View::Games;
              frame(model, size.first, size.second, "game-selection-" + std::to_string(kind), false);
            }
          }
        }
    std::printf("%s: %d frames, %d text issues\n", localeCodes[currentLanguage], frames, issues);
    std::fflush(stdout);
  }
  std::fclose(report);
  std::printf("TOTAL %d frames, %d text issues\n", frames, issues);
  return issues ? 1 : 0;
}

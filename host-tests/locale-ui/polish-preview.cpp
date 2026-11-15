#include <cassert>

#include "../../src/apps_local/pet/PetScreens.h"
#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "../../src/apps_local/quest/QuestScreens.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "Trace.h"
static EpdFont jerseyHeader{&toybox_20};
static EpdFontFamily jerseyHeaderFamily{&jerseyHeader};
static void shot(petui::Model* pet, questui::Model* quest, const char* name) {
  currentCase = name;
  std::vector<uint8_t> pixels(480 * 800 / 8, 255);
  Target target(pixels, 480, 800, true);
  fonts.insert_or_assign(2, quest ? huntLabelFamily : f12);
  auto device = target.base.deviceContext();
  device.hasTouch = true;
  device.safeArea = {10, 1, 0, 1};
  fui::InputSnapshot input;
  toybox::Interactions hits;
  toybox::HeaderStatus status;
  std::snprintf(status.clock, sizeof(status.clock), "16:02");
  if (pet) {
    petui::Work work;
    petui::render(target, device, input, hits, status, *pet, work);
  } else {
    questui::Work work;
    toybox::Frame frame(target, device, input, hits, nullptr, status);
    toybox::Screen screen(frame);
    questui::build(screen, *quest, work);
  }
  FILE* out = std::fopen((outputDir + "/" + name + ".pbm").c_str(), "wb");
  std::fprintf(out, "P4\n480 800\n");
  for (auto x : pixels) std::fputc(x ^ 255, out);
  std::fclose(out);
}
int main(int argc, char** argv) {
  assert(argc == 3 || argc == 4);
  outputDir = argv[2];
  report = std::fopen((outputDir + "/issues.jsonl").c_str(), "w");
  for (int g = 0; g < 3; ++g)
    for (int n = 0; n < 3; ++n)
      sd[g * 3 + n].load(std::string(argv[1]) + "/assets_local/language-fonts/Interface" +
                         (g == 1   ? "_SC"
                          : g == 2 ? "_TC"
                                   : "") +
                         "_" +
                         std::to_string(n == 0   ? 8
                                        : n == 1 ? 10
                                                 : 12) +
                         ".cpfont");
  currentLanguage = 0;
  auto bytes = read(std::string(argv[1]) + "/.cache/locale-ui/fixtures/0.bin");
  catalogue.assign(bytes.begin(), bytes.end());
  for (size_t i = 0; i < catalogue.size();) {
    labels.push_back(catalogue.data() + i);
    i += std::strlen(catalogue.data() + i) + 1;
  }
  I18N.setLanguage(Language(currentLanguage));
  bind(currentLanguage);
  if (argc == 4) {
    pixelui::uppercaseLabels = true;
    fonts.insert_or_assign(fui::HEADER_CLOCK_FONT, jerseyHeaderFamily);
  }
  pet::Profile p;
  p.created = true;
  p.readingSeconds = 20 * 3600 + 60;
  p.sharedDays = 5;
  p.streak = 5;
  p.memories = 0xffff;
  pet::History h;
  h.count = 3;
  petui::Model pm;
  pm.profile = &p;
  pm.history = &h;
  pm.view = pet::View::Album;
  shot(&pm, nullptr, "pet-album");
  quest::Profile hero;
  hero.campaign.initialized = hero.campaign.prologueSeen = true;
  hero.campaign.introducedBooks = 12;
  hero.campaign.narratedEncounters = 720;
  hero.campaign.encounter = 719;
  hero.campaign.observedSeconds = 10000;
  hero.xp = quest::threshold(50) + 72;
  hero.bonus = 4000;
  std::snprintf(hero.name, sizeof(hero.name), "MILO");
  readingstats::Summary summary;
  questui::Model qm;
  qm.profile = &hero;
  qm.summary = &summary;
  qm.today = 20500;
  sdart::Pack art, rewards, story, outfits, enemies;
  assert(art.open(questassets::PATH, questassets::COUNT));
  assert(rewards.open(questassets::REWARDS_PATH, questassets::REWARDS_COUNT));
  assert(story.open(questassets::STORY_PATH, questassets::STORY_COUNT));
  assert(outfits.open(questassets::BESTIARY_OUTFITS_PATH, questassets::BESTIARY_OUTFITS_COUNT));
  assert(enemies.open(questassets::GUARDIANS_PATH, questassets::GUARDIANS_COUNT));
  qm.assets = &art;
  qm.rewardAssets = &rewards;
  qm.storyAssets = &story;
  qm.outfitAssets = &outfits;
  qm.enemyAssets = &enemies;
  qm.view = questui::View::Hero;
  shot(nullptr, &qm, "quest-home");
  qm.view = questui::View::Request;
  qm.selectedRequest = quest::kCrystalHunt;
  shot(nullptr, &qm, "hunt-crystal");
  qm.view = questui::View::HuntRewards;
  shot(nullptr, &qm, "hunt-rewards");
  qm.view = questui::View::Request;
  qm.selectedRequest = 0;
  assert(quest::acceptRequest(hero, 0));
  hero.campaign.observedSeconds += 120;
  shot(nullptr, &qm, "hunt-active");
  assert(quest::cancelRequest(hero, 0));
  hero.requestClaims[0] |= 1;
  hero.huntWins[quest::huntOutfitIndex(quest::huntOutfitItem(0))] = 3;
  shot(nullptr, &qm, "hunt-keepsake");
  assert(quest::acceptRequest(hero, 0));
  shot(nullptr, &qm, "hunt-repeat");
  std::fclose(report);
  std::printf("Polish previews: %d issues\n", issues);
  return issues ? 1 : 0;
}

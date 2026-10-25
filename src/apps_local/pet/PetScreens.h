#pragma once

#include <I18n.h>

#include "../ui/ToyboxScreen.h"
#include "../ui/WordleKeyboard.h"
#include "PetHistory.h"
#include "PetNeeds.h"
#include "PetPuzzleInput.h"
#include "PetState.h"

namespace petui {
namespace fui = freeink::ui;
class Palette;
enum : fui::ActionId {
  ChangeTab = 1,
  SetupTab,
  SelectSpecies,
  Start,
  OpenSettings,
  SaveSettings,
  SelectMode,
  SelectGoal,
  Feed,
  Stroke,
  Rest,
  OpenGames,
  SelectGame,
  BeginGame,
  GamePick,
  GameNext,
  GameDone,
  Read,
  OpenProjects,
  SelectProject,
  Page,
  OpenMemory,
  OpenLives,
  Continue,
  Back,
  Retry,
  OpenRules,
  OpenHealth,
  Clean,
  Cure,
  OpenLife,
  OpenRetire,
  ConfirmRetire,
  NewCompanion,
  NameKey,
  ConfirmName,
  SelectCategory,
  AcknowledgeDeath,
  OpenCollection,
  OpenMemories,
  OpenReadingInfo,
  GameReset,
  SelectCatalogFilter,
  OpenProjectPreview,
  PreviewStep,
  OpenHealthHelp,
  ConfirmSettings,
  OpenStatsHelp,
  OpenAccumulatedHelp,
  RemoveProject
};
inline constexpr uint32_t ART_COUNT = 64, LEGACY_ART_COUNT = 63, PETTING_HAND_ART = 63;
inline constexpr uint32_t HOME_ART_COUNT = 39, HABITAT_ART_COUNT = 2 + 2 * (pet::OBJECT_COUNT + HOME_ART_COUNT);
inline constexpr uint32_t HOME_BACKGROUNDS_COUNT = HOME_ART_COUNT + 6;
inline constexpr char HABITAT_ART_PATH[] = "/apps/pet/habitat.bin";
inline constexpr char HOME_ART_PATH[] = "/apps/pet/home.bin";
inline constexpr char ART_PATH[] = "/apps/pet/graphics.bin";
inline constexpr uint32_t FAREWELL_ART_COUNT = 6;
inline constexpr char FAREWELL_ART_PATH[] = "/apps/pet/farewell.bin";
inline constexpr uint32_t COLLECTION_ART_COUNT = 1;
inline constexpr char COLLECTION_ART_PATH[] = "/apps/pet/collection.bin";
struct Art {
  void* context = nullptr;
  bool (*draw)(void*, fui::DrawTarget&, fui::Rect, uint32_t, bool) = nullptr;
};
enum class CatalogFilter : uint8_t { Owned, Unowned };
enum class NoticeKind : uint8_t { Information, Cleaned, Cured, Game, Fed };
enum class GameOutcome : uint8_t { Rewarded, Practice, CareNeeded };
struct GameResult {
  uint8_t joyBefore = 0, joyAfter = 0, energyBefore = 0, energyAfter = 0;
  GameOutcome outcome = GameOutcome::Practice;
};
struct FeedResult {
  uint8_t before = 0, after = 0;
  bool visible = false;
};
struct CareResult {
  uint8_t hygieneBefore = 0, hygieneAfter = 0, joyBefore = 0, joyAfter = 0, medicineSpent = 0;
  bool visible = false;
};
struct Model {
  const pet::Profile* profile = nullptr;
  const pet::GameState* game = nullptr;
  const pet::History* history = nullptr;
  pet::Species choice = pet::Species::Silva;
  pet::View view = pet::View::New;
  pet::Tab tab = pet::Tab::Pet;
  pet::View memoryReturn = pet::View::Memories, lifeReturn = pet::View::Lives;
  pet::Mode selectedMode = pet::Mode::Relaxed;
  uint8_t selectedGoal = 1;
  pet::Game selectedGame = pet::Game::Pairs;
  CatalogFilter catalogFilter = CatalogFilter::Unowned;
  uint8_t previewItem = pet::OBJECT_COUNT;
  int today = 0, todaySeconds = 0, weekDays = 0, page = 0, memory = 0, projectSlot = 0;
  int64_t now = 0;
  int lifeEndedDay = 0;
  bool resourcesReady = true;
  bool historyWritable = true;
  bool portraitBackgrounds = false;
  bool loading = false, writable = true, clockKnown = true, historyChanged = false;
  const char* notice = nullptr;
  NoticeKind noticeKind = NoticeKind::Information;
  const char* nameDraft = "";
  Art art, scenes, habitat, sleepArt, farewellArt, collectionArt;
  uint8_t sleepStep = 0;
  pet::Affection affection;
  FeedResult feedResult;
  CareResult careResult;
  GameResult gameResult;
};
struct Work {
  petinput::PuzzleLayout puzzleLayout;
  pet::Profile previewProfile;
  wordlekeys::Layout keys;
  fui::ButtonProps button;
  fui::TabBarProps tabBar;
  fui::TabItem tabs[4]{};
  char tabLabels[4][32]{};
  char label[160]{}, secondary[128]{};
  int pages = 1;
};
const char* eggName(pet::Species);
const char* displayName(const char*, pet::Species);
void naming(toybox::Screen&, const Model&, Work&, fui::Rect);
fui::ActionEvent routeNaming(toybox::Interactions&, const wordlekeys::Layout&, const fui::InputSnapshot&);
const char* stageName(pet::Stage);
const char* modeName(pet::Mode);
const char* playBlockText(pet::PlayBlock);
const char* objectName(unsigned);
void build(toybox::Screen&, const Model&, Work&, Palette* = nullptr);
void render(fui::DrawTarget&, const fui::DeviceContext&, const fui::InputSnapshot&, toybox::Interactions&,
            toybox::HeaderStatus, const Model&, Work&);
void dashboard(toybox::Screen&, const Model&, Work&, fui::Rect, Palette* = nullptr);
void care(toybox::Screen&, const Model&, Work&, fui::Rect);
void home(toybox::Screen&, const Model&, Work&, fui::Rect, Palette* = nullptr);
void album(toybox::Screen&, const Model&, Work&, fui::Rect);
void memoryDetails(toybox::Screen&, const Model&, Work&, fui::Rect);
void readingInfo(toybox::Screen&, const Model&, Work&, fui::Rect);
void setup(toybox::Screen&, const Model&, Work&, fui::Rect);
void settings(toybox::Screen&, const Model&, Work&, fui::Rect);
void careDetails(toybox::Screen&, const Model&, Work&, fui::Rect);
void lifeDetails(toybox::Screen&, const Model&, Work&, fui::Rect);
void details(toybox::Screen&, const Model&, Work&, fui::Rect);
void games(toybox::Screen&, const Model&, Work&, fui::Rect);
void play(toybox::Screen&, const Model&, Work&, fui::Rect);
void collectionWindow(toybox::Screen&, const Model&, Work&, fui::Rect);
void deathWindow(toybox::Screen&, const Model&, Work&, fui::Rect);
void affectionWindow(toybox::Screen&, const Model&, Work&, fui::Rect);
void noticeWindow(toybox::Screen&, const Model&, Work&, fui::Rect);
}  // namespace petui

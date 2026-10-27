#pragma once
#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/PixelMenu.h"
#include "FlashImport.h"
#include "FlashScreens.h"
#include "FlashStorage.h"
class FlashcardsActivity final : public Activity {
 public:
  FlashcardsActivity(GfxRenderer& r, MappedInputManager& i) : Activity("Flashcards", r, i) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void loop() override;
  void onExit() override;
  bool preventAutoSleep() override { return view == View::Prepare || view == View::Import; }

 private:
  enum class View : uint8_t { Browse, Help, Prepare, Import, Result };
  std::vector<flashcards::DeckInfo> entries;
  flashcards::Catalog catalog;
  toybox::Interactions interactions;
  bool interactionsReady = false;
  int page = 0, helpPage = 0;
  flashui::DeckRow deckRows[flashui::kMaxLibraryRows];
  std::string path = "/FLASHCARDS", selectedPath;
  std::unique_ptr<flashcards::Importer> importer;
  View view = View::Browse;
  bool error = false;
  uint32_t imported = 0, skipped = 0;
  flashui::ImportModel importUi;
  uint32_t importStartedMs = 0, lastProgressMs = 0;
  bool progressPublished = false;
  static void progressTrampoline(void*, const flashcards::ImportProgress&);
  void updateProgress(const flashcards::ImportProgress&);
  char cache[112]{}, message[640]{}, buffer[640]{};
  void render(RenderLock&&) override;
  void activateIndex(int);
  void onBackButton();
  bool handleCustomInput();
  void scan();
  void openDeck(const char*);
  bool prepare();
};

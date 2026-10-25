#pragma once
#include <HalMemory.h>

#include <vector>

#include "../../activities/Activity.h"
#include "FlashScreens.h"
#include "FlashStorage.h"
class FlashHistoryActivity final : public Activity {
 public:
  enum class Open { History, Cards, LatestSession };
  FlashHistoryActivity(GfxRenderer& r, MappedInputManager& i) : Activity("Flashcards history", r, i) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&, const char* directory = nullptr,
                                          Open = Open::History);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View { History, Filter, Session, Cards, Card };
  enum class Direction { Reset, Older, Newer };
  struct Item {
    flashcards::Session session;
    int deck = 0;
  };
  std::vector<flashcards::DeckInfo> decks;
  flashcards::Catalog catalog;
  flashcards::HistoryWork work;
  study::StudyDeck noteDeck;
  study::Note note;
  HalMemory::PsramBuffer noteWorkspace;
  Item items[3], selected, anchor;
  flashui::HistoryRow rows[3];
  int itemCount = 0, scanDeck = 0, filterIndex = -1, year = 2026, month = 1, page = 0, totalSessions = 0,
      totalCards = 0;
  int cardPage = 0, textPage = 0, textPages = 1, cardCount = 0, cardIndices[10]{};
  bool error = false, reviewedOnly = false, showAnswer = false, allDiscovered = false;
  Open initial = Open::History;
  View view = View::History;
  Direction direction = Direction::Reset;
  char onlyDir[128]{}, path[640]{}, text[640]{}, monthLabel[64]{}, dateLabels[3][48]{}, cardLabels[10][160]{};
  uint8_t cardReadCache[1024]{};
  toybox::Interactions interactions;
  bool ready = false;
  flashui::TextPageCache textCache;
  uint8_t textSize = 0;
  int textPreferences = -1;
  void discover(const char*);
  void discoverAll();
  void addDeck(const char*);
  void loadHistory(Direction);
  void accept(const flashcards::Session&);
  void loadCards();
  bool loadNote(int);
  void back();
  static void visit(void* c, const flashcards::Session& s) { static_cast<FlashHistoryActivity*>(c)->accept(s); }
  static bool newer(const Item&, const Item&);
};

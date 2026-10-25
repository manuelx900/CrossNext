#pragma once
#include <HalStorage.h>

#include "../../activities/Activity.h"
#include "../ui/SectionNavigation.h"
#include "../ui/ToyboxScreen.h"
#include "ClippingsLine.h"
#include "QuotesCore.h"
#include "QuotesScreens.h"
class QuotesActivity final : public Activity {
 public:
  QuotesActivity(GfxRenderer& r, MappedInputManager& i) : Activity("Quotes", r, i) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { List, Detail, Confirm, Notice };
  void recordQuote();
  bool readQuote(int);
  void rebuild();
  void refresh(bool rebuildIndex = true, bool repaint = true);
  void route(int, int);
  void back();
  void turn(int);
  void favorite(int);
  void fail();
  quotes::Entry entries[quotes::kMax]{};
  uint16_t visible[quotes::kMax]{};
  uint16_t groups[quotes::kMax]{};
  int count = 0, oldest = 0, visibleCount = 0, bookCount = 0, favoriteCount = 0, total = 0;
  int cachedCapacity = 0;
  int tab = 0, listPage = 0, selection = 0, bodyPage = 0, bodyPages = 1, rowCount = 0;
  uint64_t selectedBook = 0, scanHash = quotes::kHashStart;
  uint32_t nextStart = 0, scanOffset = 0;
  uint8_t scanBuffer[512]{};
  unsigned scanAt = 0, scanBytes = 0;
  bool scanning = false, failed = false, oldFirst = false, stateWritable = true;
  View view = View::List;
  quotes::ClippingsLine scanLine;
  HalFile source;
  char record[4096]{}, bookTitle[192]{}, bookAuthor[128]{};
  quotes::Quote quote;
  quotesui::Row rows[5]{};
  int bookTotal = 0, bookFavorites = 0;
  pagedtext::Work paragraph;
  toybox::Interactions interactions;
  bool interactionsReady = false;
  sectionnav::Input navigation;
  uint32_t surfaceMeaning() const override;
};

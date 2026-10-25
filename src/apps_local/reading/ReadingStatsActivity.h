#pragma once

#include <cstdio>

#include "../../activities/Activity.h"
#include "../ui/SectionNavigation.h"
#include "../ui/ToyboxScreen.h"
#include "ReadingStatsScreens.h"
#include "ReadingStatsStore.h"

class ReadingStatsActivity final : public Activity {
 public:
  ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("ReadingStats", renderer, input) {}
  void openBook(const char* document, const char* title, const char* author) {
    std::snprintf(initialBook, sizeof(initialBook), "%s", document);
    std::snprintf(detail.title, sizeof(detail.title), "%s", title);
    std::snprintf(detail.author, sizeof(detail.author), "%s", author);
  }
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Load { Anchor, Summary, Day, Period, Streak, Books, Detail, Ready, Failed };
  void scan();
  bool beginScan(bool reverse = false);
  void loadPage(int page);
  void loadPeriod(readingstats::Period next, int day);
  void action(freeink::ui::ActionId action, int value);
  void back();
  // Fixed workspaces; no container grows with the journal or library.
  readingstats::Summary summary;
  readingstats::PeriodStats dayStats, periodStats;
  readingstats::Repository repository;
  readingstats::Record record;
  readingstats::Book books[readingstats::kRecentBooks], detail;
  // The summary pass also projects both book tabs; no journal reread on a tab switch.
  readingstats::Book finishedBooks[readingstats::kBooksPerPage];
  uint8_t readAhead[readingstats::kRecordBytes * 8]{};
  int finishedPage = -1, finishedCount = 0, finishedCapacity = readingstats::kBooksPerPage;
  bool summaryReady = false, dayReady = false;
  readingui::Work uiWork;
  toybox::Interactions interactions;
  Load loading = Load::Summary;
  readingstats::Period period = readingstats::Period::Calendar;
  readingstats::BookFilter filter = readingstats::BookFilter::Finished;
  int tab = 0, month = 0, currentDay = 0, displayedDay = 0, page = 0, count = 0, selected = 0, skipped = 0,
      pageSize = readingstats::kBooksPerPage, recentCount = 0;
  int streakPassFirst = 0;
  int queuedTab = -1;
  sectionnav::Input navigation;
  char initialBook[33]{};
  int pickerYear = 2026;
  bool pickerVisible = false, periodFromPicker = false;
  bool clockKnown = false;
  bool details = false, damaged = false, ready = false;
};

#pragma once

#include "VocabularyExport.h"
#include "VocabularyNotice.h"
#include "VocabularyStorage.h"
#include "WordsIndex.h"
#include "WordsReview.h"
#include "WordsScreens.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"

class WordsActivity final : public Activity, protected UiAppHost {
 public:
  WordsActivity(GfxRenderer& r, MappedInputManager& i)
      : Activity("Words", r, i), UiAppHost(r), fsrs(nullptr), scheduler(fsrs, study::Steps::defaults()) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  void prepareForSleep() override;

 private:
  vocabulary::StorageRecords records;
  vocabulary::VocabularyNotice notices;
  vocabulary::WordsIndex index;
  vocabulary::Word word;
  vocabulary::Context context;
  vocabulary::ExportJob exportJob{records, word, context};
  wordsui::Model model;
  wordsui::Geometry geometry;
  pagedtext::Work wrap;
  study::Fsrs fsrs;
  study::Scheduler scheduler;
  HalFile directory, scanFile;
  vocabulary::ReviewSession session;
  char filename[96]{}, filterLabel[224]{};
  char content[4608]{}, message[384]{};
  char displayTerm[192]{};
  vocabulary::Purpose purpose = vocabulary::Purpose::Vocabulary;
  int scanned = 0, currentCard = -1, listFocus = 0;
  int collectionPage = 0, collectionFocus = 0;
  wordsui::View listReturn = wordsui::View::List;
  enum class Scan : uint8_t { Collection, Review };
  Scan scan = Scan::Collection;
  bool scanning = false, failed = false;
  bool indexedScan = false, buildingIndex = false;
  bool resumeCardAfterNotice = false;
  uint32_t indexAt = 0;
  struct Totals {
    int total = 0, due = 0, eligible = 0, difficult = 0;
    int day = -1, minute = -1;
    bool valid = false;
  } totals[2];
  bool directPage = false;
  bool exporting = false;
  bool textLayoutValid = false, prewarmText = true;
  bool layoutAnswer = false, layoutDetail = false;
  bool listReaderContent = false;
  int layoutWidth = 0, layoutHeight = 0, layoutPreferences = -1;
  uint8_t titleSize = 1, bodySize = 0;
  wordsui::View guideReturn = wordsui::View::Home;
  int today = 0, minute = 0;
  unsigned long lastPoll = 0;
  static void screen(UiScreen&, void*);
  void dispatch(int, int = 0);
  void beginScan(bool review = false);
  void scanStep();
  void pollNotice();
  void noteUnreadable(const char*);
  void indexedStep();
  void scanEntry();
  void finishScan();
  void prepareList();

  bool openContext(const char*);
  void buildText();
  void clock();
  void startCard();
  void nextCard();
  void preview();
  void back();
};

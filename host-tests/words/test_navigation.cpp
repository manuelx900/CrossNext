#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "VocabularyNotice.h"
#include "VocabularyStorage.h"
#include "WordsIndex.h"
#include "WordsLabel.h"
#include "WordsReview.h"
#include "WordsScreens.h"

#define tr(key) #key
using namespace vocabulary;
static unsigned checks = 0;
#define CHECK(x)                                           \
  do {                                                     \
    ++checks;                                              \
    if (!(x)) {                                            \
      std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                                        \
    }                                                      \
  } while (0)

// Execute the production scan/navigation methods against the storage fault
// fixture. Rendering and wall time are controlled at the activity boundary.
class WordsActivity {
 public:
  StorageRecords records;
  VocabularyNotice notices;
  WordsIndex index;
  Word word;
  Context context;
  ReviewSession session;
  wordsui::Model model;
  struct Renderer {
    int getScreenWidth() { return 480; }
    int getScreenHeight() { return 800; }
  } renderer;
  HalFile directory, scanFile;
  char filename[96]{}, message[384]{};
  Purpose purpose = Purpose::Vocabulary;
  int scanned = 0, listFocus = 0, today = 20000, minute = 100, currentCard = -1;
  wordsui::View listReturn = wordsui::View::List;
  enum class Scan : uint8_t { Collection, Review };
  Scan scan = Scan::Collection;
  bool scanning = false, failed = false, indexedScan = false, buildingIndex = false, directPage = false;
  bool resumeCardAfterNotice = false;
  uint32_t indexAt = 0;
  struct Totals {
    int total = 0, due = 0, eligible = 0, difficult = 0, day = -1, minute = -1;
    bool valid = false;
  } totals[2];
  void clock() { model.clockReady = true; }
  void resetUi() {}
  void prepareList() {}
  void requestUpdate() {}
  void startCard() { model.view = wordsui::View::Review; }
  bool openContext(const char*);
  void beginScan(bool review = false);
  void scanEntry();
  void finishScan();
  void indexedStep();
  void scanStep();
  void noteUnreadable(const char*);
  void pollNotice();
  void nextCard();
  void run() {
    for (int n = 0; scanning; ++n) {
      CHECK(n < 10000);
      scanStep();
    }
  }
};
#include "words_navigation.inc"

int main() {
  char shown[192];
  for (const auto* term : {"resilient", "élan", "über", "κόσμος", "слово", "日本語"}) {
    wordsui::wordLabel(shown, sizeof(shown), term);
    CHECK(std::strlen(shown) > 0);
  }
  wordsui::wordLabel(shown, sizeof(shown), "resilient");
  CHECK(!std::strcmp(shown, "Resilient"));
  wordsui::wordLabel(shown, sizeof(shown), "élan");
  CHECK(!std::strcmp(shown, "Élan"));
  wordsui::wordLabel(shown, sizeof(shown), "ßeta");
  CHECK(!std::strcmp(shown, "ẞeta"));
  wordsui::wordLabel(shown, sizeof(shown), "iş", "tr");
  CHECK(!std::strcmp(shown, "İş"));
  wordsui::wordLabel(shown, sizeof(shown), "iki", "az-Latn");
  CHECK(!std::strcmp(shown, "İki"));
  wordsui::wordLabel(shown, sizeof(shown), "iş", "TR");
  CHECK(!std::strcmp(shown, "İş"));
  wordsui::wordLabel(shown, sizeof(shown), "iki", "Az-Latn");
  CHECK(!std::strcmp(shown, "İki"));
  CHECK(StorageRecords::ready());
  WordsActivity app;
  // A damaged first word and 999 healthy words, interleaved collections.
  char damaged[kIdBytes]{};
  char damagedContext[kIdBytes]{};
  for (int n = 0; n < 1000; ++n) {
    std::snprintf(app.word.headword, sizeof(app.word.headword), "term-%04d", n);
    copy(app.word.surface, sizeof(app.word.surface), app.word.headword);
    copy(app.word.meaning, sizeof(app.word.meaning), "A complete saved definition.");
    copy(app.word.dictionary, sizeof(app.word.dictionary), "test-dictionary");
    identify(app.word);
    copy(app.context.word, sizeof(app.context.word), app.word.id);
    copy(app.context.path, sizeof(app.context.path), "/Books/Test.epub");
    copy(app.context.surface, sizeof(app.context.surface), app.word.headword);
    copy(app.context.sentence, sizeof(app.context.sentence), "An example sentence.");
    app.context.purpose = n % 2 ? Purpose::Language : Purpose::Vocabulary;
    identify(app.context);
    copy(app.word.latest, sizeof(app.word.latest), app.context.id);
    app.word.encounters = 1;
    CHECK(app.records.put(app.context));
    CHECK(app.records.put(app.word));
    if (!n) {
      copy(damaged, sizeof(damaged), app.word.id);
      copy(damagedContext, sizeof(damagedContext), app.context.id);
    }
  }
  const auto broken = std::string(kWords) + "/" + damaged + ".dat";
  testStorage.files[broken][24] ^= 1;
  const auto originals = testStorage.files;
  testStorage.bytesRead = 0;
  app.beginScan();
  app.run();
  CHECK(!app.failed && app.model.saved == 999 && app.model.skipped == 1);
  CHECK(app.index.available());
  const auto initialBytes = testStorage.bytesRead;
  app.model.view = wordsui::View::List;
  testStorage.bytesRead = 0;
  app.beginScan();
  CHECK(!app.scanning && !app.model.busy && app.model.total == 499 && app.model.count == 5);
  CHECK(app.model.due == 499);
  CHECK(testStorage.bytesRead < initialBytes / 100);
  app.model.view = wordsui::View::WordList;
  app.model.page = 2;
  testStorage.bytesRead = 0;
  app.beginScan();
  CHECK(!app.scanning && app.model.count == 10 && app.model.capacity == 10);
  CHECK(testStorage.bytesRead <= 12 * sizeof(WordsIndex::Entry));
  app.session.clear();
  app.model.view = wordsui::View::List;
  app.model.page = 0;
  testStorage.bytesRead = 0;
  app.beginScan(true);
  app.run();
  CHECK(!app.failed && app.session.count() == 48 && app.model.view == wordsui::View::Review);
  const auto reviewBytes = testStorage.bytesRead;
  CHECK(reviewBytes < initialBytes / 3);
  // Real records remain the authority when a cached card becomes unreadable.
  const auto lost = std::string(kContexts) + "/" + app.session.entry(app.currentCard).context + ".dat";
  const auto saved = testStorage.files[lost];
  testStorage.files[lost][24] ^= 1;
  app.nextCard();
  CHECK(!app.failed && app.model.view == wordsui::View::Review && app.model.skipped == 2);
  CHECK(!app.index.available());
  testStorage.files[lost] = saved;
  // Unreadable optional cache: fall back without losing its batch cursor.
  app.model.view = wordsui::View::Home;
  app.beginScan();
  app.run();
  testStorage.files["/.crosspoint/vocabulary/.list-words.tmp"][20] ^= 1;
  app.model.view = wordsui::View::List;
  app.beginScan();
  app.run();
  CHECK(!app.failed && app.model.total == 499 && app.model.count == 5);
  // No writable index: source navigation remains usable and doesn't delete data.
  app.index.cancel();
  testStorage.writeLimit = 0;
  app.beginScan();
  app.run();
  CHECK(!app.failed && app.model.total == 499 && !app.index.available());
  testStorage.writeLimit = -1;
  // Update a reviewed word in every cached collection, then verify persistence.
  app.model.view = wordsui::View::Home;
  app.beginScan();
  app.run();
  CHECK(app.index.read(Purpose::Language, 0, 1));
  app.index.unpack(0, app.word, app.context);
  app.word.card.dueDay = 20100;
  app.word.lastRating = 4;
  const auto beforeMaintenance = testStorage.bytesRead;
  CHECK(app.index.update(app.word));
  CHECK(testStorage.bytesRead == beforeMaintenance);
  while (app.index.updating()) {
    const auto before = testStorage.bytesRead;
    app.index.stepUpdate();
    CHECK(testStorage.bytesRead - before <= (WordsIndex::kBatch - 1) * sizeof(WordsIndex::Entry));
  }
  CHECK(app.index.read(Purpose::Language, 0, 1));
  CHECK(app.index.entry(0).card.dueDay == 20100 && app.index.entry(0).rating == 4);
  CHECK(app.index.update(app.word));
  testStorage.files["/.crosspoint/vocabulary/.list-words.tmp"][20] ^= 1;
  app.model.view = wordsui::View::List;
  app.beginScan();
  app.run();
  CHECK(!app.failed && app.model.total == 499 && app.model.count == 5);
  CHECK(app.index.update(app.word));
  CHECK(!app.index.update(app.word));
  CHECK(!app.index.available() && !app.index.updating());
  app.index.cancel();
  testStorage.enumerationCalls = 0;
  testStorage.failEnumerationAt = 3;
  app.model.view = wordsui::View::Home;
  app.beginScan();
  app.run();
  CHECK(app.failed && !app.index.available() && Storage.ready());
  testStorage.failEnumerationAt = -1;
  app.beginScan();
  app.run();
  CHECK(!app.failed && app.model.saved == 999);
  app.index.cancel();
  app.notices.close();
  CHECK(testStorage.files == originals);
  testStorage.available = false;
  app.beginScan();
  CHECK(app.failed && !app.scanning);
  CHECK(std::strstr(app.message, "WORDS_STORAGE_ERROR"));
  testStorage.available = true;
  {
    VocabularyNotice accepted;
    accepted.beginScan();
    CHECK(accepted.observe(damagedContext));
    CHECK(accepted.acknowledge());
  }
  testStorage.writeLimit = 0;
  {
    WordsActivity restarted;
    restarted.notices.resume();
    restarted.beginScan();
    restarted.run();
    CHECK(!restarted.failed && restarted.model.saved == 999 && restarted.model.skipped == 0);
    // A new bad record still alerts while the old accepted batch is read-only.
    const auto freshPath = std::string(kContexts) + "/" + restarted.context.id + ".dat";
    const auto freshBytes = testStorage.files[freshPath];
    testStorage.files[freshPath][24] ^= 1;
    restarted.index.cancel();
    restarted.beginScan();
    restarted.run();
    CHECK(!restarted.failed && restarted.model.saved == 998 && restarted.model.skipped == 1);
    testStorage.files[freshPath] = freshBytes;
    restarted.index.cancel();
    restarted.notices.close();
  }
  testStorage.writeLimit = -1;
  for (const auto& file : originals) CHECK(testStorage.files[file.first] == file.second);
  std::printf("%u navigation checks; source scan %zu bytes, cached review %zu bytes; no original record changed\n",
              checks, initialBytes, reviewBytes);
}

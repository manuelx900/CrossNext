#include <HalMemory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "VocabularyStorage.h"

int checks = 0;
#define CHECK(x)                                      \
  do {                                                \
    ++checks;                                         \
    if (!(x)) {                                       \
      std::fprintf(stderr, "%d: %s\n", __LINE__, #x); \
      std::exit(1);                                   \
    }                                                 \
  } while (0)
using namespace vocabulary;
int main() {
  StorageRecords store;
  Word w, read;
  Context context;
  copy(w.headword, sizeof(w.headword), "glance");
  copy(w.surface, sizeof(w.surface), "glanced");
  copy(w.meaning, sizeof(w.meaning), "Echar un vistazo");
  copy(w.dictionary, sizeof(w.dictionary), "EN-ES");
  identify(w);
  copy(context.word, sizeof(context.word), w.id);
  copy(context.path, sizeof(context.path), "/Books/Harbor.epub");
  copy(context.surface, sizeof(context.surface), "glanced");
  copy(context.sentence, sizeof(context.sentence), "She glanced at the door.");
  identify(context);
  copy(w.latest, sizeof(w.latest), context.id);
  w.encounters = 1;
  CHECK(StorageRecords::ready());
  CHECK(store.put(context));
  CHECK(store.put(w));
  CHECK(store.word(w.id, read) == Read::Ok && read.encounters == 1);
  const auto live = std::string(kWords) + "/" + w.id + ".dat";
  const auto backup = std::string(kWords) + "/" + w.id + ".bak";
  const auto original = testStorage.files[live];
  testStorage.writeLimit = 100;
  w.encounters = 2;
  CHECK(!store.put(w));
  CHECK(testStorage.files[live] == original);
  CHECK(store.word(w.id, read) == Read::Ok && read.encounters == 1);
  testStorage.writeLimit = -1;
  for (int failure = 1; failure <= 2; ++failure) {
    testStorage.files[live] = original;
    testStorage.files.erase(backup);
    testStorage.renameCalls = 0;
    testStorage.failRenameAt = failure;
    CHECK(!store.put(w));
    CHECK(store.word(w.id, read) == Read::Ok && read.encounters == 1);
    testStorage.failRenameAt = -1;
    CHECK(store.put(w));
    CHECK(store.word(w.id, read) == Read::Ok && read.encounters == 2);
  }
  testStorage.files[backup] = original;
  testStorage.files[live][13] ^= 1;
  CHECK(store.word(w.id, read) == Read::Ok && read.encounters == 1);
  testStorage.renameCalls = 0;
  testStorage.failRenameAt = 1;
  CHECK(!store.put(w));
  CHECK(store.word(w.id, read) == Read::Ok && read.encounters == 1);
  testStorage.failRenameAt = -1;
  CHECK(store.put(w));
  CHECK(store.word(w.id, read) == Read::Ok && read.encounters == 2);
  CHECK(testStorage.files.count(backup) == 0);
  testStorage.files[live][24] ^= 1;
  CHECK(store.word(w.id, read) == Read::Error);
  testStorage.available = false;
  CHECK(store.word(w.id, read) == Read::Error);
  CHECK(!store.put(w));
  testStorage.available = true;
  CHECK(store.removeWord(w.id));
  CHECK(store.word(w.id, read) == Read::Missing);
  CHECK(store.removeContext(context.id));
  wordTestHeap = 1;
  auto notice = saveLookup(context, "glanced", "glance", "<p>Echar un vistazo</p>", true, "EN-ES", "en");
  CHECK(notice.attempted && notice.status == Capture::Failed && !notice.token.valid);
  wordTestHeap = 1024 * 1024;
  notice = saveLookup(context, "glanced", "glance", "<p>Echar un vistazo &amp; mirar</p>", true, "EN-ES", "en");
  CHECK(notice.status == Capture::Saved && store.word(notice.token.word, read) == Read::Ok);
  CHECK(std::strstr(read.meaning, "vistazo & mirar") != nullptr);
  CHECK(undoLookup(notice.token));
  notice = saveLookup(context, "glanced", "glance", "<p> </p>", true, "EN-ES", "en");
  CHECK(notice.status == Capture::Rejected && !notice.token.valid);
  std::printf("%d storage checks, 0 failures (short writes, failed renames, recovery, corruption, absent SD)\n",
              checks);
}

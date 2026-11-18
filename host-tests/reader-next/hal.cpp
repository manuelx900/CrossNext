#include <FS.h>
#include <HalStorage.h>

#include <cstdio>
#include <string>

#include "../../src/activities/reader/ProgressFile.h"
int main() {
  unsigned checks = 0, failed = 0;
#define CHECK(x)                                                      \
  do {                                                                \
    ++checks;                                                         \
    if (!(x)) {                                                       \
      ++failed;                                                       \
      std::printf("HAL check %u line %d failed\n", checks, __LINE__); \
    }                                                                 \
  } while (0)
  const auto initial = Storage.resourceRevision();
  CHECK(Storage.mkdir("/.crosspoint"));
  CHECK(Storage.resourceRevision() != initial);
  auto resource = Storage.resourceRevision();
  for (const auto* path :
       {"/.crosspoint/vocabulary/words/0123456789abcdef0123456789abcdef.dat",
        "/.crosspoint/vocabulary/contexts/0123456789abcdef0123456789abcdef.tmp",
        "/.crosspoint/vocabulary/contexts/0123456789abcdef0123456789abcdef.bak",
        "/.crosspoint/vocabulary/.list-words.tmp", "/.crosspoint/vocabulary/.list-language.tmp",
        "/.crosspoint/vocabulary/.notice-current.tmp", "/.crosspoint/vocabulary/.notice-accepted.bin",
        "/.crosspoint/vocabulary/acknowledged/01/0123456789abcdef0123456789abcdef.ack"}) {
    HalFile record;
    CHECK(Storage.openFileForWrite("T", path, record));
    CHECK(record.write(uint8_t(3)) == 1);
    CHECK(Storage.resourceRevision() == resource);
  }
  for (const auto* path : {"/.crosspoint/vocabulary/words/0123456789abcdef0123456789abcdeg.dat",
                           "/.crosspoint/vocabulary/words/0123456789abcdef0123456789abcdef.dat.other",
                           "/.crosspoint/vocabulary/words/../font.cpfont",
                           "/.crosspoint/vocabulary/other/0123456789abcdef0123456789abcdef.dat",
                           "/.crosspoint/vocabulary/acknowledged/ab/0123456789abcdef0123456789abcdef.ack",
                           "/.crosspoint/vocabulary/acknowledged/01/0123456789abcdef0123456789abcdef.ack.other",
                           "/.crosspoint/vocabulary/.notice-accepted.bin.other"})
    CHECK(storageMutation(path) == StorageMutation::Resources);
  CHECK(!Storage.mkdir("/.crosspoint"));
  CHECK(Storage.ensureDirectoryExists("/.crosspoint"));
  CHECK(Storage.resourceRevision() == resource);
  uint8_t progressBytes[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  CHECK(ProgressFile::writeAtomic("/.crosspoint/epub_123", progressBytes, sizeof(progressBytes)));
  CHECK(Storage.resourceRevision() == resource);
  CHECK(fakeFiles["/.crosspoint/epub_123/progress.bin"].bytes ==
        std::vector<uint8_t>(progressBytes, progressBytes + 10));
  CHECK(Storage.remove("/.crosspoint/epub_123/progress.bin"));
  const char* progress = "/.crosspoint/epub_123/progress.bin.tmp";
  auto content = Storage.contentRevision();
  {
    HalFile f;
    CHECK(Storage.openFileForWrite("T", progress, f));
    CHECK(f.write(uint8_t(7)) == 1);
    f.flush();
  }
  CHECK(Storage.contentRevision() != content);
  CHECK(Storage.resourceRevision() == resource);
  CHECK(Storage.rename(progress, "/.crosspoint/epub_123/progress.bin"));
  CHECK(Storage.resourceRevision() == resource);
  CHECK(Storage.remove("/.crosspoint/epub_123/progress.bin"));
  CHECK(Storage.resourceRevision() == resource);
  CHECK(Storage.writeFile("/.crosspoint/state.json", String("state")));
  CHECK(Storage.resourceRevision() == resource);
  {
    HalFile state;
    CHECK(Storage.openFileForWrite("T", progress, state));
    HalFile book;
    CHECK(Storage.openFileForWrite("T", "/book.epub", book));
    resource = Storage.resourceRevision();
    CHECK(state.write(uint8_t(4)) == 1);
    CHECK(Storage.resourceRevision() == resource);
    CHECK(book.write(uint8_t(4)) == 1);
    CHECK(Storage.resourceRevision() != resource);
  }
  resource = Storage.resourceRevision();
  for (const char* bad : {"/progress.bin", "/.crosspoint/epub_x/progress.bin", "/.crosspoint/epub_1/x/progress.bin",
                          "/.crosspoint/epub_1/../progress.bin", "/.crosspoint/epub_1/progress.bin.other"}) {
    CHECK(storageMutation(bad) == StorageMutation::Resources);
    HalFile f;
    CHECK(Storage.openFileForWrite("T", bad, f));
    CHECK(Storage.resourceRevision() != resource);
    resource = Storage.resourceRevision();
  }
  resource = Storage.resourceRevision();
  {
    HalFile f;
    CHECK(Storage.openDerivedCacheForWrite("T", "/.crosspoint/library-grid-0.pack", f));
    CHECK(f.write(uint8_t(1)) == 1);
    CHECK(Storage.resourceRevision() == resource);
  }
  CHECK(Storage.resourceRevision() == resource);
  {
    HalFile f;
    CHECK(Storage.openFileForWrite("T", "/.crosspoint/library-grid-0.pack", f));
    CHECK(Storage.resourceRevision() != resource);
  }
  resource = Storage.resourceRevision();
  fakeFiles["/.crosspoint/epub_1/thumb_180.bmp"].bytes.resize(100);
  {
    HalFile f;
    CHECK(Storage.openDerivedCoverForUpdate("T", "/.crosspoint/epub_1/thumb_180.bmp", 100, f));
    CHECK(f.seek(100));
    CHECK(f.write(uint8_t(1)) == 1);
    CHECK(f.truncate(101));
    f.flush();
    CHECK(Storage.resourceRevision() == resource);
    CHECK(f.seek(0));
    CHECK(f.write(uint8_t(1)) == 1);
    CHECK(Storage.resourceRevision() != resource);
    resource = Storage.resourceRevision();
    CHECK(f.seek(100));
    CHECK(f.write(uint8_t(1)) == 1);
    CHECK(Storage.resourceRevision() != resource);
  }
  resource = Storage.resourceRevision();
  {
    HalFile f;
    CHECK(Storage.openDerivedCoverForUpdate("T", "/.crosspoint/epub_1/thumb_180.bmp", 100, f));
    CHECK(f.truncate(10));
    CHECK(Storage.resourceRevision() != resource);
  }
  resource = Storage.resourceRevision();
  const char* state = "/.crosspoint/epub_123/progress.bin";
  const auto v1 = Storage.stateFileRevision(state);
  CHECK(v1 && Storage.stateFileRevision(state) == v1);
  const auto v2 = Storage.stateFileRevision("/.crosspoint/epub_124/progress.bin");
  CHECK(v2 && v2 != v1);
  CHECK(Storage.stateFileRevision("/.crosspoint/epub_0123/progress.bin") == 0);
  CHECK(Storage.stateFileRevision("/.crosspoint/epub_18446744073709551616/progress.bin") == 0);
  CHECK(Storage.stateFileRevision("/book.epub") == 0);
  {
    HalFile f;
    CHECK(Storage.openFileForWrite("T", "/.crosspoint/epub_123/progress.bin.tmp", f));
    CHECK(Storage.stateFileRevision(state) != v1);
    CHECK(Storage.stateFileRevision("/.crosspoint/epub_124/progress.bin") == v2);
    const auto v3 = Storage.stateFileRevision(state);
    CHECK(f.write(uint8_t(1)) == 1);
    CHECK(Storage.stateFileRevision(state) != v3);
  }
  CHECK(Storage.resourceRevision() == resource);
  const auto journal = Storage.stateFileRevision("/.crosspoint/reading-history/sessions.bin");
  {
    HalFile f;
    CHECK(Storage.openFileForWrite("T", "/.crosspoint/reading-history/sessions.bin", f));
    CHECK(f.write(uint8_t(1)) == 1);
  }
  CHECK(Storage.stateFileRevision("/.crosspoint/reading-history/sessions.bin") != journal);
  CHECK(Storage.resourceRevision() == resource);
  {
    HalFile f;
    CHECK(
        Storage.openDerivedCacheForWrite("T", "/.crosspoint/reading-history/0123456789abcdef0123456789abcdef.idx", f));
    CHECK(f.write(uint8_t(1)) == 1);
    CHECK(Storage.resourceRevision() == resource);
  }
  for (unsigned n = 0; n < 100; ++n) {
    const auto path = "/.crosspoint/epub_" + std::to_string(1000 + n) + "/progress.bin";
    CHECK(Storage.stateFileRevision(path.c_str()) != 0);
  }
  CHECK(Storage.stateFileRevision(state) != v1);
  for (const char* path : {"/.crosspoint/epub_1/sections/0.bin", "/.crosspoint/epub_1/sections/1.classic.bin.3.part",
                           "/.crosspoint/epub_1/sections/5.p1234abcd.bin.prep.tmp",
                           "/.crosspoint/epub_1/html/.tmp_2.html", "/.crosspoint/epub_1/events-v1/0.evc.tmp"}) {
    CHECK(epubDerivedPath(path).valid());
    const auto before = Storage.resourceRevision();
    {
      HalFile f;
      CHECK(Storage.openDerivedCacheForWrite("T", path, f));
      CHECK(f.write(uint8_t(1)) == 1);
    }
    CHECK(Storage.resourceRevision() == before);
    CHECK(Storage.removeDerivedCache(path));
    CHECK(Storage.resourceRevision() == before);
    {
      HalFile f;
      CHECK(Storage.openFileForWrite("T", path, f));
    }
    CHECK(Storage.resourceRevision() != before);
  }
  for (const char* bad : {"/.crosspoint/epub_01/sections/0.bin", "/.crosspoint/epub_1/sections/01.bin",
                          "/.crosspoint/epub_1/sections/../0.bin", "/.crosspoint/epub_1/sections/0.bin/x",
                          "/.crosspoint/epub_1/html/0.xhtml", "/.crosspoint/epub_1/events-v1/0.evc.tmp.x"}) {
    CHECK(!epubDerivedPath(bad).valid());
    const auto before = Storage.resourceRevision();
    {
      HalFile f;
      CHECK(Storage.openDerivedCacheForWrite("T", bad, f));
    }
    CHECK(Storage.resourceRevision() != before);
  }
  CHECK(sameDerivedOwner("/.crosspoint/epub_1/sections/1.bin.part", "/.crosspoint/epub_1/sections/1.bin"));
  CHECK(!sameDerivedOwner("/.crosspoint/epub_1/sections/1.bin", "/.crosspoint/epub_2/sections/1.bin"));
  CHECK(!sameDerivedOwner("/.crosspoint/epub_1/sections/1.bin", "/.crosspoint/epub_1/html/1.html"));
  Storage.prepareForDeepSleep();
  CHECK(Storage.resourceRevision() != resource);
  CHECK(storageLockDepth == 0);
  CHECK(lockedOperations > 30);
  std::printf("Production HAL invalidation: %u checks, %u failures; source guards and interleaved writers\n", checks,
              failed);
  return failed ? 1 : 0;
}

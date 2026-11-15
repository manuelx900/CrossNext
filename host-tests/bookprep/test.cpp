#include <GfxRenderer.h>

#include <cstdio>
#include <cstdlib>

#include "src/apps_local/bookprep/BookPreparation.h"
#include "src/apps_local/bookprep/FontSelection.h"
unsigned checks = 0;
#define CHECK(x)                             \
  do {                                       \
    ++checks;                                \
    if (!(x)) {                              \
      printf("FAIL %d: %s\n", __LINE__, #x); \
      exit(1);                               \
    }                                        \
  } while (0)
void pump(bookprep::Job& j, GfxRenderer& r) {
  unsigned n = 0;
  while (bookprep::running(j.phase) && ++n < 100000) j.step(r);
  CHECK(n < 100000);
}
void add(const char* p) { testStorage.files[p] = {1, 2, 3}; }
int main() {
  {
    bookprep::FontSelection selection;
    CHECK(selection.add("") == 0);
    CHECK(selection.add("NotoSerif") == 1);
    CHECK(selection.add("NotoSerif") == 1);
    CHECK(selection.count == 2);
    CHECK(selection.add("12345678901234567890123456789012") == -1);
    CHECK(selection.omitted == 1);
    selection.selectAll(true);
    CHECK(selection.selectedCount() == 2);
    CHECK(selection.nextSelected(-1) == 0 && selection.nextSelected(0) == 1);
    selection.selectOnly(1);
    CHECK(selection.selectedCount() == 1 && selection.nextSelected(-1) == 1);
    selection.selectAll(false);
    CHECK(selection.nextSelected(-1) == -1);
  }
  {
    bookprep::FontSelection plan;
    plan.add("Vector");
    plan.add("Bitmap");
    plan.fonts[0].sizes = {};
    for (uint8_t pt = 8; pt <= 22; ++pt) plan.fonts[0].sizes.add(pt);
    plan.fonts[1].sizes = {};
    plan.fonts[1].sizes.add(12);
    plan.fonts[1].sizes.add(18);
    plan.fonts[1].sizes.add(255);
    plan.selectAll(true);
    CHECK(plan.passes(bookprep::SizeMode::All) == 18);
    CHECK(plan.passes(bookprep::SizeMode::Single) == 2);
    CHECK(plan.availableSizes().count() == 16);
    bookprep::FontSelection::Cursor cursor;
    unsigned passes = 0;
    while (plan.advance(cursor, bookprep::SizeMode::All, 14)) {
      CHECK(plan.fonts[cursor.font].sizes.contains(cursor.point));
      ++passes;
      CHECK(passes <= 18);
    }
    CHECK(passes == 18 && cursor.font == 1 && cursor.point == 255);
    cursor = {};
    CHECK(plan.advance(cursor, bookprep::SizeMode::Single, 15) && cursor.point == 15);
    CHECK(plan.advance(cursor, bookprep::SizeMode::Single, 15) && cursor.point == 12);
    CHECK(!plan.advance(cursor, bookprep::SizeMode::Single, 15));
    CHECK(!bookprep::acceptsFormat("/one.txt", bookprep::AllFormats));
    CHECK(bookprep::acceptsFormat("/one.EPUB", bookprep::FormatEpub));
    CHECK(!bookprep::acceptsFormat("/one.epub", bookprep::FormatXtc));
    CHECK(bookprep::acceptsFormat("/one.XTCH", bookprep::FormatXtch));
  }
  GfxRenderer renderer;
  add("/one.epub");
  add("/nested/two.EPUB");
  add("/nested/three.xtc");
  add("/nested/four.XTCH");
  add("/.crosspoint/no.epub");
  add("/plain.txt");
  add("/bad.epub");
  {
    bookprep::Job j;
    CHECK(j.begin());
    while (j.phase == bookprep::Phase::Counting) j.step(renderer);
    CHECK(j.total == 5);
    j.addThumbnailHeight(400);
    j.addThumbnailHeight(400);
    j.addThumbnailHeight(0);
    CHECK(j.thumbnailHeights[0] == 400 && j.thumbnailHeights[1] == 0);
    pump(j, renderer);
    CHECK(j.phase == bookprep::Phase::Complete);
    CHECK(j.processed == 5);
    CHECK(j.failed == 1);
    CHECK(epubsLoaded == 3);
    CHECK(xtcsLoaded == 2);
    CHECK(builds == 6);
    CHECK(thumbs == 4);
    CHECK(!testStorage.files.count("/.crosspoint/prepare-books.tmp"));
  }
  {
    bookprep::Job j;
    CHECK(j.begin());
    pump(j, renderer);
    CHECK(builds == 6);
    CHECK(j.processed == 5);
  }
  {
    bookprep::Job j;
    CHECK(j.begin());
    j.step(renderer);
    j.cancel();
    CHECK(j.phase == bookprep::Phase::Cancelled);
    CHECK(!testStorage.files.count("/.crosspoint/prepare-books.tmp"));
    CHECK(testStorage.files.count("/one.epub"));
  }
  {
    bookprep::Job j;
    CHECK(j.begin());
    testStorage.writeLimit = 0;
    pump(j, renderer);
    CHECK(j.phase == bookprep::Phase::Failed);
    testStorage.writeLimit = -1;
  }
  {
    bookprep::Job j;
    testStorage.available = false;
    CHECK(!j.begin());
    CHECK(j.phase == bookprep::Phase::Failed);
    testStorage.available = true;
  }
  testStorage.files.clear();
  add("/thumb-error.epub");
  add("/thumb-error.xtc");
  add("/no-cover.epub");
  {
    bookprep::Job j;
    CHECK(j.begin());
    j.addThumbnailHeight(200);
    j.addThumbnailHeight(400);
    pump(j, renderer);
    CHECK(j.total == 3);
    CHECK(j.processed == 3);
    CHECK(j.failed == 2);
  }
  testStorage.files.clear();
  add("/resume.epub");
  {
    bookprep::Job j;
    CHECK(j.begin());
    while (j.phase == bookprep::Phase::Counting) j.step(renderer);
    j.step(renderer);
    j.step(renderer);
    j.step(renderer);
    j.cancel();
    CHECK(j.phase == bookprep::Phase::Cancelled);
    CHECK(j.processed == 0);
    CHECK(testStorage.files.count("/resume.epub"));
  }
  {
    bookprep::Job j;
    CHECK(j.begin());
    pump(j, renderer);
    CHECK(j.processed == 1);
  }
  testStorage.files.clear();
  for (unsigned i = 0; i < 2408; ++i) {
    char p[60];
    std::snprintf(p, sizeof(p), "/library/book-%04u.xtc", i);
    add(p);
  }
  {
    bookprep::Job j;
    CHECK(j.begin());
    while (j.phase == bookprep::Phase::Counting) j.step(renderer);
    CHECK(j.total == 2408);
    while (j.processed < 500) j.step(renderer);
    CHECK(j.phase == bookprep::Phase::Preparing);
    CHECK(j.processed == 500);
    j.cancel();
    CHECK(j.processed == 500);
  }
  testStorage.files.clear();
  {
    bookprep::Job j;
    CHECK(j.begin());
    pump(j, renderer);
    CHECK(j.phase == bookprep::Phase::Complete);
    CHECK(j.total == 0);
  }
  testStorage.files.clear();
  prepared.clear();
  add("/multi.epub");
  add("/multi.xtc");
  const auto originalBuilds = builds, originalThumbs = thumbs, originalXtc = xtcsLoaded;
  for (int repeat = 0; repeat < 2; ++repeat) {
    bookprep::Job j;
    j.spec.fontId = 100;
    j.addThumbnailHeight(200);
    CHECK(j.begin(6));
    unsigned turns = 0;
    while (bookprep::running(j.phase) && ++turns < 100000) {
      if (j.phase == bookprep::Phase::AwaitingFont) {
        CHECK(j.processed == 2);
        ReaderRenderSpec next;
        next.fontId = 101 + j.fontPass;
        CHECK(j.nextFont(next));
        CHECK(j.processed == 0);
      } else
        j.step(renderer);
    }
    CHECK(turns < 100000 && j.phase == bookprep::Phase::Complete);
    CHECK(j.fontPass == 5 && j.fontPasses == 6 && j.total == 2);
    CHECK(builds - originalBuilds == 18);
    CHECK(thumbs - originalThumbs == unsigned((repeat + 1) * 2));
    CHECK(xtcsLoaded - originalXtc == unsigned(repeat + 1));
    CHECK(prepared.size() == 18);
  }
  {
    bookprep::Job j;
    CHECK(j.begin(2));
    while (j.phase != bookprep::Phase::AwaitingFont) j.step(renderer);
    j.cancel();
    CHECK(j.phase == bookprep::Phase::Cancelled);
    CHECK(!j.nextFont(ReaderRenderSpec{}));
    CHECK(prepared.size() == 21);
    CHECK(!testStorage.files.count("/.crosspoint/prepare-books.tmp"));
  }
  {
    bookprep::Job j;
    CHECK(!j.begin(0));
    CHECK(!j.begin(129 * 255 + 1));
  }
  testStorage.files.clear();
  prepared.clear();
  add("/opened.epub");
  const auto opened = std::make_shared<Epub>("/opened.epub", "/.crosspoint");
  unsigned builtBefore = builds, imagesBefore = preparedImages;
  {
    bookprep::Job j;
    j.spec.fontId = 314;
    j.addThumbnailHeight(400);
    CHECK(j.beginBook(opened));
    CHECK(j.phase == bookprep::Phase::Preparing);
    CHECK(j.progressTotal() == 4);
    pump(j, renderer);
    CHECK(j.phase == bookprep::Phase::Complete);
    CHECK(builds - builtBefore == 3);
    CHECK(preparedImages - imagesBefore == 6);
  }
  {
    bookprep::Job j;
    j.spec.fontId = 314;
    j.addThumbnailHeight(400);
    CHECK(j.beginBook(opened));
    CHECK(j.phase == bookprep::Phase::Complete);
    CHECK(builds - builtBefore == 3);
    CHECK(preparedImages - imagesBefore == 6);
  }
  {
    // A completion marker cannot hide a deleted thumbnail.
    testStorage.files.erase(opened->getThumbBmpPath(400));
    bookprep::Job j;
    j.spec.fontId = 314;
    j.addThumbnailHeight(400);
    CHECK(j.beginBook(opened));
    CHECK(j.phase == bookprep::Phase::Preparing);
    pump(j, renderer);
    CHECK(j.phase == bookprep::Phase::Complete);
    CHECK(testStorage.files.count(opened->getThumbBmpPath(400)));
  }
  {
    // A different layout has its own durable completion stamp.
    bookprep::Job j;
    j.spec.fontId = 315;
    j.addThumbnailHeight(400);
    CHECK(j.beginBook(opened));
    CHECK(j.phase == bookprep::Phase::Preparing);
    j.step(renderer);
    j.cancel();
    CHECK(j.phase == bookprep::Phase::Cancelled);
  }
  {
    bookprep::Job j;
    j.spec.fontId = 315;
    j.addThumbnailHeight(400);
    CHECK(j.beginBook(opened));
    CHECK(j.phase == bookprep::Phase::Preparing);
    failImages = true;
    pump(j, renderer);
    CHECK(j.phase == bookprep::Phase::Failed);
    failImages = false;
  }
  {
    bookprep::Job j;
    j.spec.fontId = 315;
    j.addThumbnailHeight(400);
    CHECK(j.beginBook(opened));
    CHECK(j.phase == bookprep::Phase::Preparing);
    pump(j, renderer);
    CHECK(j.phase == bookprep::Phase::Complete);
  }
  {
    ++testStorage.modified;
    bookprep::Job j;
    j.spec.fontId = 314;
    j.addThumbnailHeight(400);
    CHECK(j.beginBook(opened));
    CHECK(j.phase == bookprep::Phase::Preparing);
    pump(j, renderer);
    CHECK(j.phase == bookprep::Phase::Complete);
  }
  {
    bookprep::Job j;
    j.spec.fontId = 314;
    j.addThumbnailHeight(600);
    CHECK(j.beginBook(opened));
    CHECK(j.phase == bookprep::Phase::Preparing);
    j.cancel();
  }
  {
    bookprep::Job j;
    CHECK(!j.beginBook(std::shared_ptr<Epub>()));
  }
  testStorage.files.clear();
  prepared.clear();
  add("/engine.epub");
  auto engineBook = std::make_shared<Epub>("/engine.epub", "/.crosspoint");
  const auto startBuilds = builds;
  {
    bookprep::Job modern;
    CHECK(modern.beginBook(engineBook));
    pump(modern, renderer);
    CHECK(modern.phase == bookprep::Phase::Complete);
    CHECK(builds == startBuilds + 3);
  }
  {
    bookprep::Job classic;
    classic.spec.engine = EpubEngine::Classic;
    CHECK(classic.beginBook(engineBook));
    CHECK(classic.phase == bookprep::Phase::Preparing);
    pump(classic, renderer);
    CHECK(classic.phase == bookprep::Phase::Complete);
    CHECK(builds == startBuilds + 6);
  }
  for (auto engine : {EpubEngine::Modern, EpubEngine::Classic}) {
    bookprep::Job reused;
    reused.spec.engine = engine;
    CHECK(reused.beginBook(engineBook));
    CHECK(reused.phase == bookprep::Phase::Complete);
    CHECK(builds == startBuilds + 6);
  }
  testStorage.files.clear();
  add("/epub.epub");
  add("/comic.xtc");
  add("/comic.xtch");
  add("/other.txt");
  for (uint8_t mask : {uint8_t(bookprep::FormatEpub), uint8_t(bookprep::FormatXtc), uint8_t(bookprep::FormatXtch),
                       uint8_t(bookprep::FormatXtc | bookprep::FormatXtch)}) {
    bookprep::Job j;
    j.formats = mask;
    CHECK(j.begin(15));
    while (j.phase == bookprep::Phase::Counting) j.step(renderer);
    CHECK(j.total == unsigned(mask == (bookprep::FormatXtc | bookprep::FormatXtch) ? 2 : 1));
    CHECK(j.fontPasses == unsigned(mask & bookprep::FormatEpub ? 15 : 1));
    j.cancel();
  }
  {
    bookprep::Job j;
    j.formats = 0;
    CHECK(!j.begin());
  }

  printf("book preparation: %u checks, 0 failed\n", checks);
}

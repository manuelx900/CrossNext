#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

#include "../../src/activities/reader/ChapterPosition.h"
#define LOG_ERR(...) ((void)0)
struct Section {
  bool format = false;
  inline static int discarded = 0, suspended = 0;
  int currentPage = 0, pageCount = 100;
  int estimatedTotalPages() const { return pageCount; }
  bool isFormatBuild() { return format; }
  void discardBuild() { ++discarded; }
  void suspendBuild() { ++suspended; }
  std::optional<uint32_t> getVisibleTextOffsetForPage(uint16_t p) { return p * 10; }
  ~Section();
};
namespace EpubReaderUtils {
int calls = 0, spine = -1, page = -1, count = -1;
bool fail = false;
std::optional<uint32_t> offset;
bool saveProgress(int, int s, int p, int c, std::optional<uint32_t> o) {
  ++calls;
  spine = s;
  page = p;
  count = c;
  offset = o;
  return !fail;
}
}  // namespace EpubReaderUtils
namespace booktext {
int saves = 0;
bool save() {
  ++saves;
  return true;
}
}  // namespace booktext
struct ReaderActivity {
  inline static int sleeps = 0;
  void prepareForSleep() { ++sleeps; }
};
struct Book {
  operator int() const { return 1; }
  int getSpineItemsCount() const { return 3; }
  int getBookSize() const { return 1000; }
  std::string getTitle() const { return "book"; }
  float calculateProgress(int spine, float fraction) const { return (spine + fraction) / 3; }
};
struct ScreenshotInfo {
  enum class ReaderType { Epub } readerType;
  char title[80]{};
  int spineIndex = 0, currentPage = 0, totalPages = 0, progressPercent = 0;
};
struct EpubReaderActivity : ReaderActivity {
  struct FormatProgress {
    int spine, page, count;
    std::optional<uint32_t> offset;
  };
  std::optional<FormatProgress> formatProgress;
  std::unique_ptr<Book> epub = std::make_unique<Book>();
  std::unique_ptr<Section> section = std::make_unique<Section>();
  int lastSavedSpineIndex = -1, lastSavedPage = -1, lastSavedPageCount = -1;
  bool textPreferencesDirty = true, textFormatPending = false;
  bool formatReflowPending = false, progressSaveSuppressed = false;
  int cachedSpineIndex = 0, currentSpineIndex = 0, nextPageNumber = 50, cachedChapterTotalPageCount = 100;
  std::optional<uint32_t> cachedVisibleTextOffset = 9999, currentPageVisibleOffset = 200;
  bool isAtEndOfBook() const { return currentSpineIndex >= epub->getSpineItemsCount(); }
  ChapterPosition chapterPosition() const;
  int bookPercentFor(const ChapterPosition&) const;
  ScreenshotInfo getScreenshotInfo() const;
  int getProgressBasisPoints() const;
  int getProgressPercent() const { return getScreenshotInfo().progressPercent; }
  bool saveFinishedPosition();
  int footnoteDepth = 0, pendingManualTurn = 0;
  bool pendingPercentJump = false, automaticPageTurnActive = false;
  std::optional<int> pendingPageJump, pendingOffsetJump;
  std::string pendingAnchor;
  enum class Overlay { None } overlay = Overlay::None;
  void clearDeferredReposition() {
    cachedVisibleTextOffset.reset();
    cachedChapterTotalPageCount = 0;
  }
  void discardOverlayPage() {}
  void resetSection() {
    section.reset();
    formatReflowPending = false;
  }
  bool flushFormatProgress();
  bool saveProgress(int, int, int);
  void prepareForSleep();
};
#include "format-lifetime-under-test.inc"
int main() {
  {
    Section ordinary;
  }
  {
    Section preview;
    preview.format = true;
  }
  assert(Section::discarded == 1 && Section::suspended == 1);
  EpubReaderActivity r;
  r.formatReflowPending = true;
  r.formatProgress = EpubReaderActivity::FormatProgress{0, 50, 100, 9999};
  r.prepareForSleep();
  assert(booktext::saves == 1 && !r.textPreferencesDirty);
  assert(EpubReaderUtils::page == 50 && EpubReaderUtils::offset == 9999 && !r.formatProgress &&
         ReaderActivity::sleeps == 1);
  // A newer normal save supersedes old deferred progress.
  r.formatProgress = EpubReaderActivity::FormatProgress{0, 50, 100, 9999};
  r.formatReflowPending = false;
  r.section->currentPage = 20;
  assert(r.saveProgress(0, 20, 100));
  const auto calls = EpubReaderUtils::calls;
  assert(!r.formatProgress && r.flushFormatProgress() && EpubReaderUtils::calls == calls &&
         EpubReaderUtils::page == 20);
  // During reflow explicit saving uses the original target, not temporary page zero.
  r.formatReflowPending = true;
  assert(r.saveProgress(0, 0, 1));
  assert(EpubReaderUtils::page == 50 && EpubReaderUtils::offset == 9999);
  r.formatProgress = EpubReaderActivity::FormatProgress{0, 51, 100, 10010};
  EpubReaderUtils::fail = true;
  assert(!r.flushFormatProgress() && r.formatProgress);
  EpubReaderUtils::fail = false;
  assert(r.flushFormatProgress() && !r.formatProgress && EpubReaderUtils::page == 51);
  r.nextPageNumber = 75;
  r.cachedChapterTotalPageCount = 100;
  r.section->currentPage = 0;
  r.currentSpineIndex = 0;
  r.formatReflowPending = true;
  assert(r.getScreenshotInfo().currentPage == 76 && r.getScreenshotInfo().progressPercent == 25);
  assert(r.getProgressBasisPoints() == 2500);
  r.formatProgress = EpubReaderActivity::FormatProgress{0, 75, 100, 15000};
  r.progressSaveSuppressed = true;
  assert(r.saveFinishedPosition());
  const auto finishedCalls = EpubReaderUtils::calls;
  assert(!r.formatProgress && EpubReaderUtils::spine == 3 && r.getProgressBasisPoints() == 10000);
  assert(r.flushFormatProgress() && EpubReaderUtils::calls == finishedCalls);
  puts(
      "Production lifetime: Section RAII discard/suspend, sleep/exit progress flush, captured anchor, superseding save "
      "and retry");
}

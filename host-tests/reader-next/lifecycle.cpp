#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#define CHECK(x)                                            \
  do {                                                      \
    if (!(x)) {                                             \
      std::printf("lifecycle check failed %d\n", __LINE__); \
      std::exit(1);                                         \
    }                                                       \
  } while (0)
void* currentTask = reinterpret_cast<void*>(1);
void* holder = nullptr;
unsigned acquisitions = 0, clears = 0;
bool prepareIdle = false, idleQueued = false, spoolResident = true;
unsigned blockedIdle = 0, flushes = 0;
void attemptIdle() {
  if (!idleQueued) return;
  if (holder) {
    ++blockedIdle;
    return;
  }
  spoolResident = false;
  idleQueued = false;
  ++flushes;
}
void* xTaskGetCurrentTaskHandle() { return currentTask; }
void* xSemaphoreGetMutexHolder(void*) { return holder; }
struct Manager {
  void* renderingMutex = nullptr;
} activityManager;
struct RenderLock {
  RenderLock() {
    CHECK(holder != currentTask);
    holder = currentTask;
    ++acquisitions;
  }
  ~RenderLock() {
    holder = nullptr;
    attemptIdle();
  }
  static bool heldByCurrentTask();
};
struct Cache {
  void clear() {
    CHECK(holder == currentTask);
    ++clears;
  }
};
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
unsigned long millis() { return 1; }
struct Page {};
struct Section {
  int currentPage = 0, pageCount = 4;
  unsigned loads = 0;
  std::unique_ptr<Page> loadPage(int) {
    CHECK(holder == currentTask && spoolResident);
    attemptIdle();
    CHECK(spoolResident);
    ++loads;
    return std::make_unique<Page>();
  }
};
struct Book {
  int resolveHrefToSpineIndex(const std::string&) {
    CHECK(holder == currentTask);
    return 2;
  }
};
struct ClipSelectionActivity {
  enum class Mode { Clip };
};
enum class StrId { STR_CLIPPING_REMOVED, STR_CLIPPING_DELETE_FAILED };
struct Clippings {
  bool has = false, removed = false;
  bool hasClippings() const { return has; }
  bool removeClippingAt(size_t) {
    CHECK(holder == currentTask);
    return removed = true;
  }
} CLIPPINGS;
struct Settings {
  int screenMargin = 4;
} SETTINGS;
struct UITheme {
  static int getReaderTopBarHeight() { return 0; }
};
struct EpubReaderActivity {
  Cache residentPage;
  std::unique_ptr<Section> section = std::make_unique<Section>();
  std::unique_ptr<Book> epub = std::make_unique<Book>();
  static constexpr int CLIP_SELECTION_PAGE_WINDOW = 3, MAX_FOOTNOTE_DEPTH = 4;
  int currentSpineIndex = 0, nextPageNumber = 0, preparedPages = 0, footnoteDepth = 0;
  struct Saved {
    int spine, page;
  } savedPositions[MAX_FOOTNOTE_DEPTH]{};
  struct Note {
    std::string href;
  };
  std::vector<Note> currentPageFootnotes;
  std::string pendingAnchor;
  bool showClippingMessage = false;
  StrId clippingMessage = StrId::STR_CLIPPING_REMOVED;
  unsigned long clippingMessageTime = 0;
  struct Renderer {
    void getOrientedViewableTRBL(int* t, int* r, int* b, int* l) { *t = *r = *b = *l = 0; }
  } renderer;
  int clippingAtPoint(Page&, int, int) { return 0; }
  void requestUpdate() {}
  void clearDeferredReposition() { CHECK(holder == currentTask); }
  void resetSection() {
    CHECK(holder == currentTask);
    section.reset();
  }
  void clearPrefetchedPage() {
    CHECK(holder == currentTask);
    ++clears;
    if (prepareIdle) idleQueued = true;
  }
  void releasePageReuse();
  void startClipSelection(int, int, ClipSelectionActivity::Mode);
  void openFootnoteSelect(bool);
  void navigateToHref(const std::string&, bool);
};
#include "clip-prepare-under-test.inc"
#include "footnote-prepare-under-test.inc"
#include "href-under-test.inc"
#include "lock-owner-under-test.inc"
#include "page-release-under-test.inc"
void resetIdle() {
  CHECK(!holder);
  prepareIdle = true;
  idleQueued = false;
  spoolResident = true;
  blockedIdle = flushes = 0;
  CLIPPINGS = {};
}
int main() {
  EpubReaderActivity reader;
  CHECK(!RenderLock::heldByCurrentTask());
  reader.releasePageReuse();
  CHECK(clears == 2 && acquisitions == 1 && !holder);
  holder = currentTask;
  reader.releasePageReuse();
  CHECK(clears == 4 && acquisitions == 1 && holder == currentTask);
  holder = reinterpret_cast<void*>(2);
  CHECK(!RenderLock::heldByCurrentTask());
  holder = nullptr;
  resetIdle();
  EpubReaderActivity clipping;
  clipping.startClipSelection(-1, -1, ClipSelectionActivity::Mode::Clip);
  CHECK(clipping.preparedPages == 3 && blockedIdle == 3 && flushes == 1 && !spoolResident);
  resetIdle();
  {
    RenderLock outer;
    const auto before = acquisitions;
    clipping.startClipSelection(-1, -1, ClipSelectionActivity::Mode::Clip);
    CHECK(acquisitions == before && spoolResident && !flushes);
  }
  CHECK(flushes == 1);
  resetIdle();
  CLIPPINGS.has = true;
  clipping.startClipSelection(1, 1, ClipSelectionActivity::Mode::Clip);
  CHECK(CLIPPINGS.removed && clipping.showClippingMessage && flushes == 1);
  resetIdle();
  EpubReaderActivity notes;
  notes.currentPageFootnotes.reserve(2);
  notes.currentPageFootnotes.push_back({"#first"});
  notes.currentPageFootnotes.push_back({"#second"});
  notes.openFootnoteSelect(false);
  CHECK(notes.preparedPages == 1 && blockedIdle == 1 && flushes == 1);
  resetIdle();
  notes.currentPageFootnotes.resize(1);
  notes.openFootnoteSelect(false);
  CHECK(!notes.section && notes.pendingAnchor == "first" && notes.footnoteDepth == 1 && flushes == 1);
  resetIdle();
  EpubReaderActivity navigation;
  navigation.navigateToHref("other.xhtml#target", true);
  CHECK(!navigation.section && navigation.currentSpineIndex == 2 && navigation.pendingAnchor == "target");
  std::puts("Production page-cache release: locks before free; no recursive acquire from a locked result callback");
  std::puts(
      "Production child preparation: queued spool flush cannot overlap clipping/footnote page loads; nested navigation "
      "stays nonrecursive");
}

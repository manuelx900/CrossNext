// Host adapters execute the production activity methods below; no copy of their control flow.
#include <atomic>
#include <functional>
#include <memory>

#include "src/apps_local/library/LibraryEntryBackdrop.h"
namespace contract {
inline int coverOpens = 0, panelFrames = 0, indexOpens = 0, requestedPaints = 0;
inline bool cachedScene = false, sourceAvailable = true;
inline uint16_t nativePrefetched = 0;
inline Target* drawing = nullptr;
inline std::vector<uint8_t>* framebuffer = nullptr;
inline std::vector<uint8_t> resident;
struct File {
  void close() {}
};
struct StorageApi {
  bool openFileForRead(const char*, const char*, File&) {
    CHECK(panelFrames > 0);
    ++coverOpens;
    return sourceAvailable;
  }
} Storage;
enum class BmpReaderError { Ok };
struct Bitmap {
  void setReadAheadBuffer(uint8_t*, size_t) {}
  void setMemorySource(const uint8_t*, size_t) {}
  BmpReaderError parseHeaders() { return BmpReaderError::Ok; }
};
struct Rect {
  int x, y, width, height;
};
namespace FsHelpers {
inline bool hasEpubExtension(const std::string& p) { return p.ends_with(".epub"); }
inline bool hasXtcExtension(const std::string& p) { return p.ends_with(".xtc"); }
}  // namespace FsHelpers
struct UITheme {
  static int getFileIcon(const std::string&) { return 0; }
};
inline fui::BitmapRef listIconFor(int, int) { return {}; }
inline void paintCover(int x, int y, int w, int h) {
  drawing->fill(fui::makeRect(x, y, w, h), fui::Paint::solid(fui::Color::Black), 0, fui::CornersAll);
  drawing->fill(fui::makeRect(x + 5, y + 5, w - 10, h / 3), fui::Paint::solid(fui::Color::White), 0, fui::CornersAll);
}
struct Renderer {
  uint8_t getOrientation() const { return 0; }
  bool copyBufferToRegion(int, int, int, int, const uint8_t*, size_t) { return false; }
  bool copyRegionToBuffer(int, int, int, int, uint8_t*, size_t) { return false; }
  bool copyPackedBufferToRegion(int x, int y, int w, int h, const uint8_t*, size_t) {
    paintCover(x, y, w, h);
    return true;
  }
  void preserveImagePolarity(int, int, int, int) {}
} rendererInstance;
struct Gui {
  void drawCoverPlaceholder(Renderer&, Rect r) {
    drawing->fill(fui::makeRect(r.x, r.y + r.height / 3, r.width, r.height - r.height / 3),
                  fui::Paint::solid(fui::Color::Black), 0, fui::CornersAll);
    drawing->stroke(fui::makeRect(r.x, r.y, r.width, r.height), fui::Paint::solid(fui::Color::Black), 1, 0,
                    fui::CornersAll);
  }
  bool drawCoverThumbFill(Renderer&, Bitmap&, Rect r) {
    paintCover(r.x, r.y, r.width, r.height);
    return true;
  }
} GUI;
struct Settings {
  int screenInverted = 0, xtcCoverRotation = 0;
  uint8_t libraryDisplay = libraryui::kFinished, libraryFileTypes = libraryui::kAllFormats;
} SETTINGS;
namespace library {
inline uint32_t libraryContentRevision() { return 1; }
}  // namespace library
namespace paintclock {
inline uint32_t counter() { return 1; }
}  // namespace paintclock
namespace homecover {
enum class SceneKind { Library };
inline uint32_t sceneRevision() { return 1; }
inline bool restoreScene(Renderer&, fui::Rect, uint64_t, SceneKind, bool = false) {
  if (!cachedScene) return false;
  CHECK(framebuffer->size() == resident.size());
  *framebuffer = resident;
  return true;
}
inline void storeScene(Renderer&, bool) {}
inline bool scenePackPending() { return false; }
inline void stepScenePack() {}
inline void cancelScenePack() {}
}  // namespace homecover
struct Prepared {
  static uint64_t key(Renderer&, fui::Rect) { return 1; }
  static int sourceStamp() { return 1; }
  static bool canDraw(Renderer&, fui::Rect) { return true; }
  bool load(File&, Renderer&, fui::Rect, uint64_t, bool, const char*, const int*) { return false; }
  void cancel() {}
  bool writing() const { return false; }
  void step() {}
};
namespace libraryui {
using namespace ::libraryui;
using PreparedCoverCache = Prepared;
inline void loadingNotice(fui::DrawTarget& target, fui::Rect r, const char* label) {
  ::libraryui::loadingNotice(target, r, label);
}
}  // namespace libraryui
struct Prefetch {
  struct Bytes {
    const uint8_t* data = nullptr;
    size_t size = 0;
    bool native = false;
    explicit operator bool() const { return data != nullptr; }
  };
  Bytes find(uint64_t, uint32_t, uint32_t, int slot, int, uint64_t) {
    static constexpr uint8_t pixels[1] = {0};
    return nativePrefetched & (1u << slot) ? Bytes{pixels, 1, true} : Bytes{};
  }
  void cancelLoad() {}
};
struct Recent {
  struct Book {
    std::string path = "/books/book.epub";
  };
  std::array<Book, 9> books;
  const auto& getBooks() const { return books; }
} RECENT_BOOKS;
struct RecentBooksStore {
  static constexpr int MAX_RECENT_BOOKS = 30;
};
struct Index {
  bool ioFailed() const { return false; }
  int ordinalForRow(int, uint16_t n) { return n; }
  bool readRecord(int, int&) { return true; }
  bool readPath(int, std::string& out) {
    out = "/books/book.epub";
    return true;
  }
} indexInstance;
struct RenderLock {};
struct UiTabListActivity {
  void render(RenderLock&&);
};
struct LibraryListActivity;
struct Manager {
  bool idle = false;
  void requestIdleWork(LibraryListActivity&) {
    CHECK(panelFrames > 0);
    idle = true;
  }
  bool idleWorkIsCurrent(LibraryListActivity&) const { return idle; }
} activityManager;
struct LibraryListActivity : UiTabListActivity {
  static constexpr int ACTION_GRID_PAGE = 7, ACTION_ROW = 1;
  enum class CoverPresentation { Unannounced, Notice, Loading, Complete };
  enum class EntryPhase { Shell, Preparing, Ready };
  std::atomic<EntryPhase> entryPhase{EntryPhase::Ready};
  std::atomic<CoverPresentation> coverPresentation{CoverPresentation::Unannounced};
  Renderer& renderer = rendererInstance;
  Index& index = indexInstance;
  fui::ListNav nav;
  fui::Rect gridArea{};
  libraryui::GridLayout gridLayout;
  int windowTop = -1, gridFirst = -1, gridTotal = -1, gridCoverHeight = 0, gridPreparedCoverHeight = 0;
  int sortOrder = 0, gridRecord = 0, nativeHeights[9]{};
  bool gridCacheValid = false, gridInverted = false, windowReadFailed = false, buttonCursorVisible = false;
  uint8_t gridOrientation = 0;
  uint16_t nativeMissing = 0, gridMissingCovers = 0;
  uint32_t gridLibraryRevision = 0, gridSceneRevision = 0, coverAttempts = 0, coverWorkPaint = 1, prefetchWorkPaint = 1;
  std::atomic<bool> coverWorkRequested{false}, prefetchCancelled{true}, presentationPending{true};
  int pendingCoverHeight = 0;
  static constexpr size_t GRID_CACHE_BYTES = 40 * 1024;
  std::unique_ptr<uint8_t[]> gridCache;
  Bitmap gridCoverBitmap;
  File gridCoverFile;
  Prepared preparedCover;
  Prefetch coverPrefetch;
  std::vector<std::string> winTitles, winAuthors;
  std::string gridFileName, gridPath;
  char pixelTitle[192]{};
  struct Facet {
    bool enabled = false;
    bool active() const { return enabled; }
  } facetFilter;
  std::string query;
  struct Popup {
    bool processRender(Renderer&, int) { return false; }
    bool isActive() const { return false; }
  } optionPopup;
  int mappedInput = 0;
  toybox::Screen* screen = nullptr;
  fui::ListNav& activeNav() { return nav; }
  int selectedEntry() const { return 0; }
  int rowCount = 9;
  int listCount() const { return rowCount; }
  int bookRowCount() const { return rowCount; }
  void invalidateGrid() {
    gridFirst = -1;
    gridCacheValid = false;
    gridMissingCovers = 0;
    nativeMissing = 0;
    coverAttempts = 0;
    coverPresentation = CoverPresentation::Unannounced;
  }
  uint64_t gridSceneKey(int, int, int) { return 1; }
  bool rowTextFor(int, std::string& title, std::string& author, std::string* name) {
    title = "Book";
    author = "Author";
    *name = "book.epub";
    return true;
  }
  int storeIndexFor(int n) const { return n; }
  int rowFor(int n) const { return n; }
  void buildGrid(toybox::Screen&);
  void baselineBuildGrid(toybox::Screen&);
  void render(RenderLock&&);
  void runIdleWork();
  bool presentationComplete() const;
  int entryBookCount() const;
  bool libraryReady() const { return entryPhase == EntryPhase::Ready; }
  bool gridView() const { return true; }
  bool loadPendingCover() { return false; }
  void initializeLibrary() {
    CHECK(panelFrames > 0);
    ++indexOpens;
    entryPhase = EntryPhase::Ready;
    requestUpdate(true);
  }
  void requestUpdate(bool = true) { ++requestedPaints; }
  void prepareVisibleCover() {}
  void prefetchNextCover() {}
};
inline void UiTabListActivity::render(RenderLock&&) {
  auto& a = *static_cast<LibraryListActivity*>(this);
  if (a.libraryReady())
    a.buildGrid(*a.screen);
  else
    ::libraryui::loadingNotice(a.screen->target(), a.screen->body(), tr(STR_LIBRARY_LOADING));
  ++panelFrames;
}
using UiScreen = toybox::Screen;
#define LOG_DBG(...) ((void)0)
#include "activity-methods.inc"
#undef LOG_DBG
void gridContractChecks() {
  Font small(toybox_10), body(toybox_14), title(toybox_20), prose(ui_prose_14);
  for (bool wide : {false, true}) {
    const int w = wide ? 800 : 480, h = wide ? 480 : 800;
    std::vector<uint8_t> bits(w * h / 8, 255);
    Target target(bits.data(), w, h, w / 8);
    target.setFont(0, small.font);
    target.setFont(1, body.font);
    target.setFont(2, title.font);
    target.setFont(4, prose.font);
    drawing = &target;
    framebuffer = &bits;
    auto device = target.deviceContext();
    device.hasTouch = true;
    toybox::Interactions hits;
    fui::InputSnapshot input{};
    toybox::Frame frame(target, device, input, hits);
    toybox::Screen screen(frame);
    screen.setContentMargin({120, 10, 20, 10});
    LibraryListActivity activity;
    activity.screen = &screen;
    panelFrames = coverOpens = indexOpens = requestedPaints = 0;
    cachedScene = false;
    sourceAvailable = true;
    nativePrefetched = 0;
    LibraryListActivity empty;
    empty.rowCount = 0;
    CHECK(empty.presentationComplete());
    empty.entryPhase = LibraryListActivity::EntryPhase::Shell;
    CHECK(!empty.presentationComplete());
    activity.entryPhase = LibraryListActivity::EntryPhase::Shell;
    activity.render({});
    CHECK(panelFrames == 1 && coverOpens == 0 && indexOpens == 0);
    CHECK(!activity.presentationComplete());
    activity.runIdleWork();
    CHECK(indexOpens == 1 && activity.libraryReady());
    activity.render({});
    CHECK(panelFrames == 2 && coverOpens == 0);
    CHECK(requestedPaints == 2 && !activity.presentationComplete());
    std::fill(bits.begin(), bits.end(), 255);
    activity.render({});
    CHECK(coverOpens == libraryui::gridCapacity(w, h));
    CHECK(activity.presentationComplete());
    auto finished = bits;
    std::fill(bits.begin(), bits.end(), 255);
#ifdef HAS_LIBRARY_BASELINE
    LibraryListActivity before;
    before.baselineBuildGrid(screen);
    std::fill(bits.begin(), bits.end(), 255);
    before.coverPresentation = LibraryListActivity::CoverPresentation::Loading;
    before.baselineBuildGrid(screen);
    // Only the outer border changes; all image and pager pixels stay exact.
    const auto geometry = libraryui::gridLayout(device, screen.body());
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        bool border = false;
        for (int n = 0; n < geometry.capacity(); ++n) {
          const auto card = geometry.card(n);
          const auto art = card.inset({2, 2, 2, 2});
          border |= x >= card.x && x < card.right() && y >= card.y && y < card.bottom() &&
                    !(x >= art.x && x < art.right() && y >= art.y && y < art.bottom());
        }
        if (!border) {
          const auto offset = y * (w / 8) + x / 8;
          const auto mask = uint8_t(0x80 >> (x % 8));
          CHECK((bits[offset] & mask) == (finished[offset] & mask));
        }
      }
#endif
    resident = finished;
    cachedScene = true;
    coverOpens = 0;
    std::fill(bits.begin(), bits.end(), 255);
    LibraryListActivity warm;
    warm.screen = &screen;
    warm.render({});
    CHECK(coverOpens == 0 && warm.presentationComplete() && bits == finished);
    cachedScene = false;
    nativePrefetched = 0x1ff;
    coverOpens = requestedPaints = 0;
    std::fill(bits.begin(), bits.end(), 255);
    LibraryListActivity prefetched;
    prefetched.screen = &screen;
    prefetched.render({});
    CHECK(coverOpens == 0 && requestedPaints == 0 && prefetched.presentationComplete());
    CHECK(bits == finished);
    nativePrefetched = 0x1fe;
    coverOpens = requestedPaints = 0;
    LibraryListActivity partial;
    partial.screen = &screen;
    partial.render({});
    CHECK(coverOpens == 0 && requestedPaints == 1 && !partial.presentationComplete());
    std::fill(bits.begin(), bits.end(), 255);
    partial.render({});
    CHECK(coverOpens == 1 && partial.presentationComplete() && bits == finished);
    nativePrefetched = 0;
    cachedScene = false;
    sourceAvailable = false;
    coverOpens = 0;
    LibraryListActivity missing;
    missing.screen = &screen;
    target.runs.clear();
    missing.render({});
    CHECK(coverOpens == 0 && !missing.presentationComplete());
    CHECK(std::none_of(target.runs.begin(), target.runs.end(),
                       [](const auto& run) { return run.text == "BOOK.EPUB" || run.text == "book.epub"; }));
    missing.render({});
    CHECK(coverOpens == 2 * libraryui::gridCapacity(w, h) && !missing.presentationComplete());
    missing.coverAttempts = 0x1ff;
    missing.gridMissingCovers = 0;
    missing.render({});
    CHECK(missing.presentationComplete());
    CHECK(std::any_of(target.runs.begin(), target.runs.end(),
                      [](const auto& run) { return run.text == "BOOK.EPUB" || run.text == "book.epub"; }));
  }
}
}  // namespace contract

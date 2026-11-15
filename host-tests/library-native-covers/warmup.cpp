#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
struct Area {
  int x = 0, y = 0, width = 110, height = 177;
};
struct Renderer {
  bool wide = false;
  int getScreenWidth() const { return wide ? 800 : 480; }
  int getScreenHeight() const { return wide ? 480 : 800; }
  size_t getPackedRegionByteSize(int, int, int, int) const { return 2500; }
};
struct File {
  bool open = false;
  void close() { open = false; }
};
struct TestStorage {
  bool present = true;
  bool openFileForRead(const char* tag, const std::string& p, File& f) { return openFileForRead(tag, p.c_str(), f); }
  bool openFileForRead(const char*, const char*, File& f) {
    f.open = present;
    return present;
  }
} Storage;
namespace FsHelpers {
bool hasEpubExtension(const std::string& s) { return s.ends_with(".epub"); }
bool hasXtcExtension(const std::string&) { return false; }
}  // namespace FsHelpers
struct Book {
  std::string path, coverBmpPath;
};
struct Recents {
  std::vector<Book> books;
  const auto& getBooks() const { return books; }
} RECENT_BOOKS;
struct Settings {
  bool xtcCoverRotation = false;
} SETTINGS;
namespace libraryui {
int gridCapacity(int w, int h) { return w > h ? 4 : 9; }
struct PreparedCoverCache {
  struct Info {};
  unsigned remaining = 0, started = 0, finished = 0, releases = 0;
  bool available = true;
  inline static bool valid = false;
  bool writing() const { return remaining != 0; }
  bool enabled() const { return available; }
  void step() {
    if (remaining && !--remaining) ++finished;
  }
  void release() { ++releases; }
  static uint64_t key(Renderer&, Area) { return 1; }
  static bool inspect(File&, uint64_t, size_t, Info&) { return valid; }
  bool prepare(Renderer&, int&, Area, uint64_t, const char*, bool (*)(void*), void*) {
    ++started;
    remaining = 3;
    return true;
  }
  void cancel() { remaining = 0; }
};
}  // namespace libraryui
unsigned millis() { return 1000; }
class HomeActivity;
struct MappedInputManager {
  enum class SwipeDir { None };
  enum class Button { Back, Confirm, NavNext, NavPrevious, Left, Right };
  bool tap = false;
  SwipeDir wasSwipe() { return SwipeDir::None; }
  bool wasScreenTapped(int&, int&) { return tap; }
  bool wasScreenTouchPressed(int&, int&) { return false; }
  bool isScreenTouchHeld(int&, int&) { return false; }
  bool wasScreenTouchReleased() { return false; }
  bool isPressed(Button) { return false; }
  bool wasPressed(Button) { return false; }
  bool wasReleased(Button) { return false; }
};
bool workerBusy = false, waited = false;
bool* interruptFlag = nullptr;
struct RenderLock {
  enum class Mode { Blocking, Try };
  bool owned = false;
  explicit RenderLock(Mode mode) {
    if (workerBusy && mode == Mode::Blocking && interruptFlag && *interruptFlag) {
      waited = true;
      workerBusy = false;
    }
    owned = !workerBusy;
  }
  bool ownsLock() const { return owned; }
};
struct Manager {
  unsigned queued = 0;
  void requestIdleWork(HomeActivity&) { ++queued; }
} activityManager;
struct Grid {
  unsigned refreshed = 0;
  int thumbHeightFor() const { return 200; }
  Area coverArea(size_t) const { return {}; }
  void refreshCoverPaths() { ++refreshed; }
};
struct UITheme {
  static std::string getCoverThumbPath(const std::string& s, int) { return s; }
};
class HomeActivity {
 public:
  Grid* coverGridUi = nullptr;
  std::vector<Book> recentBooks;
  bool recentsLoading = false, recentsLoaded = false;
  size_t gridPreparationIndex = 0, shelfNativeIndex = 0;
  unsigned updates = 0, generated = 0;
  bool nativeShelfPending() const { return coverGridUi && !recentsLoading && shelfNativeIndex < recentBooks.size(); }
  bool prepareGridCover(Book&, int) {
    ++generated;
    return true;
  }
  void requestUpdate() { ++updates; }
  Renderer renderer;
  libraryui::PreparedCoverCache libraryWarmup;
  std::atomic<unsigned> libraryWarmIndex{0};
  bool libraryWarmInterrupted = false;
  unsigned libraryWarmInputAt = 0;
  MappedInputManager mappedInput;
  unsigned delivered = 0;
  File libraryWarmFile;
  int libraryWarmBitmap = 0, libraryWarmFallbackHeight = 200;
  Area libraryWarmArea;
  char libraryWarmPath[112]{};
  static bool continueLibraryWarmup(void*) { return true; }
  void runIdleWork();
  void schedule();
  void pollInput();
};
// Exact production idle worker and scheduling guard, supplied by the runner.
#include "warmup-production.h"
int main() {
  for (bool wide : {false, true})
    for (bool valid : {false, true}) {
      HomeActivity home;
      home.renderer.wide = wide;
      home.libraryWarmup.valid = valid;
      RECENT_BOOKS.books.clear();
      for (int n = 0; n < 12; ++n) RECENT_BOOKS.books.push_back({"/" + std::to_string(n) + ".epub"});
      unsigned ticks = 0;
      while (home.libraryWarmIndex < 9 && ++ticks < 100) {
        const auto before = activityManager.queued;
        home.schedule();
        if (activityManager.queued == before) return 1;
        home.runIdleWork();
        if (home.libraryWarmFile.open) return 2;
      }
      const auto expected = wide ? 4u : 9u;
      if (ticks >= 100 || home.libraryWarmup.writing() || !home.libraryWarmup.releases) return 3;
      if (home.libraryWarmup.finished != (valid ? 0u : expected)) return 4;
      const auto before = activityManager.queued;
      home.schedule();
      if (activityManager.queued != before) return 5;
    }
  HomeActivity interrupted;
  interrupted.libraryWarmup.valid = false;
  interrupted.schedule();
  interrupted.runIdleWork();
  if (interrupted.libraryWarmIndex != 0 || !interrupted.libraryWarmup.writing()) return 6;
  interrupted.libraryWarmup.cancel();
  interrupted.schedule();
  interrupted.runIdleWork();
  if (interrupted.libraryWarmIndex != 0 || interrupted.libraryWarmup.started != 2) return 7;
  Grid grid;
  HomeActivity shelf;
  shelf.coverGridUi = &grid;
  shelf.recentBooks = {{"a.epub", "a.bmp"}, {"b.epub", "b.bmp"}};
  shelf.recentsLoading = true;
  shelf.libraryWarmup.valid = false;
  for (int n = 0; n < 2; ++n) {
    shelf.schedule();
    shelf.runIdleWork();
  }
  if (shelf.generated != 2 || shelf.recentsLoading || !shelf.recentsLoaded || shelf.updates != 1 || grid.refreshed != 1)
    return 10;
  for (int n = 0; n < 8; ++n) {
    shelf.schedule();
    shelf.runIdleWork();
  }
  if (shelf.shelfNativeIndex != 2 || shelf.libraryWarmup.finished != 2 || shelf.libraryWarmIndex != 0) return 11;
  HomeActivity tapping;
  tapping.mappedInput.tap = true;
  workerBusy = true;
  waited = false;
  interruptFlag = &tapping.libraryWarmInterrupted;
  tapping.pollInput();
  if (tapping.delivered != 1 || !waited || !tapping.libraryWarmInterrupted) return 8;
  tapping.mappedInput.tap = false;
  workerBusy = true;
  waited = false;
  tapping.libraryWarmInterrupted = false;
  tapping.pollInput();
  if (tapping.delivered != 1 || waited || tapping.libraryWarmInterrupted) return 9;
  std::puts(
      "Production Home warmup: all nine portrait/four landscape writers drain; valid hits and interruption respect "
      "scheduling");
}

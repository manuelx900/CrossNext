#include <FreeInkUI.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdio>

#include "src/apps_local/library/CollectionActions.h"
#include "src/apps_local/library/CollectionFolderStates.h"
#define LOG_DBG(...) ((void)0)
namespace fui = freeink::ui;
enum { STR_CANCEL, STR_CONFIRM, STR_COLLECTIONS, STR_COLLECTION_DISCARD };
const char* tr(int) { return "test"; }
struct RenderLock {
  template <class T>
  explicit RenderLock(T&) {}
  void unlock() {}
};
struct Popup {
  template <class F>
  void showMessage(const char*, const char*, const char**, int, int, F) {}
};
bool knownEditorFolder(void* context, const char* path, collections::FolderSummary& summary) {
  return static_cast<collections::FolderStates*>(context)->get(path, summary, false);
}
unsigned long testNow = 1000;
unsigned long millis() { return testNow; }
uint64_t sourceRevision = 42;
uint64_t editorSourceRevision() { return sourceRevision; }
constexpr unsigned long EditorPaintBatchMs = 180;
class CollectionsActivity;
struct Manager {
  bool current = true;
  bool accept = true;
  CollectionsActivity* owner = nullptr;
  unsigned requests = 0;
  void requestIdleWork(CollectionsActivity& activity) {
    ++requests;
    if (accept) owner = &activity;
  }
  bool idleWorkIsCurrent(const CollectionsActivity&) const { return current; }
} activityManager;
class CollectionsActivity {
 public:
  enum class View { Catalog, Books, Editor, Destination };
  enum class BookPhase { Shell, Preparing, Ready };
  enum class CoverPresentation { Unannounced, Notice, Loading, Complete };
  std::atomic<BookPhase> bookPhase_{BookPhase::Ready};
  std::atomic<CoverPresentation> coverPresentation_{CoverPresentation::Complete};
  bool grid_ = false;
  std::atomic<bool> coverWork_{false}, pending_{false};
  unsigned long coverPaint_ = 0;
  void queueCoverWork() {
#include "cover-queue-production.h"
  }
  unsigned loads = 0, generations = 0, finishes = 0;
  unsigned bookCount_ = 0;
  collections::Store store_;
  std::string collectionName_;
  const char* nameFor(uint32_t) { return "Collection"; }
  int result = 0;
  struct MenuResult {
    int selection;
  };
  using ActivityResult = MenuResult;
  void setResult(ActivityResult r) { result = r.selection; }
  bool loadBooks() {
    ++loads;
    bookPhase_ = BookPhase::Ready;
    return true;
  }
  void generatePendingCover() { ++generations; }
  View view_ = View::Editor;
  collections::Folder folder_;
  collections::Selection selection_;
  collections::FolderScan editorScan_;
  collections::FolderStates editorFolders_;
  fui::ListNav nav;
  struct EditorLocation {
    int selected = 0, top = 0;
    uint16_t length = 0;
  };
  std::array<EditorLocation, 32> editorHistory_{};
  unsigned editorDepth_ = 0;
  uint64_t editorSource_ = 42;
  int editorFirst_ = -1, editorCount_ = 0, editorScanRow_ = -1;
  uint16_t editorFilesValid_ = 0, editorFilesChecked_ = 0;
  uint16_t editorUnavailable_ = 0;
  uint16_t editorKnownRows_ = 0;
  unsigned long editorPaintAt_ = 0;
  std::atomic<bool> editorInterrupted_{true}, editorScanPending_{false};
  bool editorNeedsPaint_ = false, dirty_ = false;
  std::string directory_ = "/", path_, entry_, editorScanPath_, incomingBook_;
  uint32_t collection_ = 1, origin_ = 0;
  unsigned failures = 0, paints = 0;
  struct App {
    void clearTapFlash() {}
  } app;
  Popup popup_;
  CollectionsActivity() {
    assert(selection_.open(collection_, true));
    assert(editorFolders_.begin());
  }
  int listCount() const { return int(folder_.count()); }
  void fail() { ++failures; }
  void changed() { ++paints; }
  void requestUpdate() { ++paints; }
  bool books(uint32_t);
  void catalog() { view_ = View::Catalog; }
  uint32_t catalogId(unsigned n) { return n; }
  void transfer(uint32_t) {}
  bool bookPath(unsigned, std::string&) { return false; }
  void onSelectBook(const std::string&) {}
  void finish() { ++finishes; }
  void changeDirectory(const std::string&);
  void cancelEditorScan();
  void settleEditorFolder();
  void toggleEntry(unsigned);
  void activateIndex(int);
  void onBackButton();
  static bool continueEditorScan(void*);
  void inspectEditorFolders();
  void runIdleWork();
};
// The runner includes the unmodified production method bodies, without hardware/UI dependencies.
#include "activity-production.h"
int main() {
  {
    CollectionsActivity a;
    assert(a.books(1));
    assert(a.bookPhase_ == CollectionsActivity::BookPhase::Ready && a.loads == 1);
  }
  {
    CollectionsActivity a;
    a.grid_ = true;
    assert(a.store_.open());
    const auto id = a.store_.create("Three books");
    assert(id && a.store_.updateCount(id, 3));
    const auto reads = testStorage.readBytes;
    const auto opens = testStorage.opens;
    assert(a.books(id) && a.bookCount_ == 3);
    assert(a.bookPhase_ == CollectionsActivity::BookPhase::Shell && !a.loads);
    assert(testStorage.readBytes == reads && testStorage.opens == opens);
    a.bookPhase_ = CollectionsActivity::BookPhase::Preparing;
    activityManager.current = false;
    a.runIdleWork();
    assert(!a.loads);
    activityManager.current = true;
    a.runIdleWork();
    assert(a.loads == 1 && a.bookPhase_ == CollectionsActivity::BookPhase::Ready);
    a.pending_ = true;
    activityManager.accept = false;
    a.queueCoverWork();
    assert(a.coverWork_ && !activityManager.owner && !a.generations);
    activityManager.accept = true;
    a.queueCoverWork();
    assert(activityManager.owner == &a);
    activityManager.owner = nullptr;
    a.queueCoverWork();
    assert(activityManager.owner == &a);
    a.runIdleWork();
    a.runIdleWork();
    assert(a.generations == 1);
    activityManager.owner = nullptr;
    a.onBackButton();
    assert(a.view_ == CollectionsActivity::View::Catalog && !a.finishes);
    a.onBackButton();
    assert(a.finishes == 1 && a.result == collectionui::ReturnHome);
  }
  for (int n = 0; n < 7; ++n) testStorage.files["/Ciencia/" + std::to_string(n) + ".epub"] = {1};
  for (int n = 0; n < 30; ++n) testStorage.files["/F" + std::to_string(n) + "/book.epub"] = {1};
  testStorage.files["/Deep/Sub/book.epub"] = {1};
  {
    CollectionsActivity a;
    a.changeDirectory("/");
    a.toggleEntry(0);
    collections::FolderSummary state;
    assert(a.editorFolders_.get("/Ciencia", state) && state.counted &&
           state.state == collections::FolderSelection::All);
    for (int pass = 0; pass < 3; ++pass) {
      a.activateIndex(0);
      assert(a.directory_ == "/Ciencia");
      if (pass) a.toggleEntry(0);
      const auto reads = testStorage.readBytes;
      const auto scans = testStorage.directoryReads;
      const auto opens = testStorage.opens;
      const auto writes = testStorage.writes;
      a.onBackButton();
      assert(a.directory_ == "/" && a.editorFolders_.get("/Ciencia", state));
      assert(state.state == (pass == 1 ? collections::FolderSelection::Partial : collections::FolderSelection::All));
      assert(testStorage.readBytes == reads && testStorage.directoryReads == scans && testStorage.opens == opens &&
             testStorage.writes == writes);
    }
    a.nav.selected = 15;
    a.nav.top = 9;
    std::string name;
    bool directory;
    assert(a.folder_.entry(15, name, directory) && directory);
    a.editorFolders_.put(("/" + name).c_str(), {collections::FolderSelection::None, 1, 0, true});
    a.activateIndex(15);
    a.onBackButton();
    assert(a.nav.selected == 15 && a.nav.top == 9);
    assert(collections::selectFolder(a.selection_, "/Deep", true, &state));
    a.editorFolders_.put("/Deep/Sub", {collections::FolderSelection::All, 1, 1, true});
    a.changeDirectory("/Deep");
    const auto before = testStorage.directoryReads;
    const auto reads = testStorage.readBytes;
    a.settleEditorFolder();
    assert(testStorage.directoryReads == before && testStorage.readBytes == reads);
    assert(!a.editorFolders_.get("/Deep", state));
    a.changeDirectory("/");
    a.editorFirst_ = 1;
    a.editorCount_ = 1;
    a.editorKnownRows_ = a.editorUnavailable_ = 0;
    a.editorScanPending_ = true;
    a.editorInterrupted_ = false;
    while (a.editorScanPending_) a.runIdleWork();
    assert(testStorage.directoryReads - before == 2 && testStorage.readBytes == reads);
    assert(a.editorFolders_.get("/Deep", state) && state.state == collections::FolderSelection::All);
    assert(!a.failures);
  }
  {
    CollectionsActivity a;
    a.changeDirectory("/");
    a.activateIndex(0);
    a.toggleEntry(3);
    const auto scans = testStorage.directoryReads;
    const auto reads = testStorage.readBytes;
    a.onBackButton();
    collections::FolderSummary state;
    assert(a.editorFolders_.get("/Ciencia", state) && state.state == collections::FolderSelection::Partial &&
           state.selected == 1);
    assert(testStorage.directoryReads == scans && testStorage.readBytes == reads && !a.failures);
  }
  {
    CollectionsActivity a;
    a.changeDirectory("/");
    assert(a.selection_.set("/Ciencia/0.epub", true));
    a.editorFirst_ = 0;
    a.editorCount_ = 10;
    a.editorScanPending_ = true;
    a.editorInterrupted_ = false;
    unsigned jobs = 0;
    const auto paints = a.paints;
    while (a.editorScanPending_ && ++jobs < 100) {
      const auto reads = testStorage.directoryReads;
      a.runIdleWork();
      testNow += 20;
      assert(testStorage.directoryReads - reads <= 4);
    }
    assert(jobs < 100 && a.paints == paints + 1);
    collections::FolderSummary state;
    assert(a.editorFolders_.get("/Ciencia", state) && state.state == collections::FolderSelection::Partial);
    auto reads = testStorage.directoryReads;
    auto bytes = testStorage.readBytes;
    a.editorScanPending_ = true;
    a.runIdleWork();
    assert(!a.editorScanPending_);
    assert(testStorage.directoryReads == reads && testStorage.readBytes == bytes);
    a.editorFolders_.clear();
    a.editorKnownRows_ = 0;
    a.editorScanPending_ = true;
    a.editorInterrupted_ = true;
    a.runIdleWork();
    assert(testStorage.directoryReads == reads);
    a.editorInterrupted_ = false;
    a.runIdleWork();
    assert(a.editorScanRow_ >= 0);
    a.editorInterrupted_ = true;
    a.runIdleWork();
    assert(a.editorScanRow_ == -1 && testStorage.directoryReads == reads);
    a.editorInterrupted_ = false;
    sourceRevision = 43;
    a.runIdleWork();
    assert(testStorage.directoryReads == reads);
    sourceRevision = 42;
    activityManager.current = false;
    a.runIdleWork();
    assert(testStorage.directoryReads == reads);
    activityManager.current = true;
  }
  {
    for (int n = 0; n < 200; ++n) testStorage.files["/Large/" + std::to_string(n) + ".epub"] = {1};
    CollectionsActivity a;
    for (int n = 0; n < 200; ++n) assert(a.selection_.set(("/Large/" + std::to_string(n) + ".epub").c_str(), true));
    assert(a.selection_.save());
    a.selection_.close();
    assert(a.selection_.open(a.collection_, true));
    a.changeDirectory("/");
    std::string name;
    bool directory;
    unsigned row = 0;
    for (; row < a.folder_.count(); ++row) {
      assert(a.folder_.entry(row, name, directory));
      if (directory && name == "Large") break;
    }
    assert(row < a.folder_.count());
    a.activateIndex(row);
    a.editorFirst_ = 0;
    a.editorCount_ = 10;
    a.editorFilesValid_ = a.editorFilesChecked_ = (1u << 10) - 1;
    const auto reads = testStorage.readBytes;
    const auto scans = testStorage.directoryReads;
    const auto opens = testStorage.opens;
    a.onBackButton();
    assert(a.directory_ == "/" && testStorage.readBytes == reads && testStorage.directoryReads == scans &&
           testStorage.opens == opens);
    collections::FolderSummary state;
    assert(!a.editorFolders_.get("/Large", state));
    a.editorFirst_ = row;
    a.editorCount_ = 1;
    a.editorKnownRows_ = a.editorUnavailable_ = 0;
    a.editorScanPending_ = true;
    a.editorInterrupted_ = false;
    unsigned jobs = 0;
    while (a.editorScanPending_ && ++jobs < 100) {
      const auto before = testStorage.directoryReads;
      a.runIdleWork();
      assert(testStorage.directoryReads - before <= 4);
    }
    assert(jobs < 100 && a.editorFolders_.get("/Large", state) && state.state == collections::FolderSelection::All &&
           !a.failures);
    for (int n = 2; n < 200; ++n) assert(a.selection_.set(("/Large/" + std::to_string(n) + ".epub").c_str(), false));
    a.editorFolders_.put("/Large", {collections::FolderSelection::Partial, 3, 2, false});
    a.activateIndex(row);
    a.editorFirst_ = 0;
    a.editorCount_ = 10;
    a.editorFilesValid_ = (1u << 10) - 1;
    a.editorFilesChecked_ = 3;
    a.toggleEntry(0);
    assert(!a.editorFolders_.get("/Large", state));
    const auto mixedReads = testStorage.readBytes;
    const auto mixedScans = testStorage.directoryReads;
    a.onBackButton();
    assert(a.directory_ == "/" && a.editorFolders_.get("/Large", state) &&
           state.state == collections::FolderSelection::Partial && !state.counted);
    assert(testStorage.readBytes == mixedReads && testStorage.directoryReads == mixedScans && !a.failures);
  }
  std::puts(
      "Production picker navigation: immediate correct All/Partial on back, manual selection, restored viewport, "
      "cached subtree reuse; zero return I/O passed");
}

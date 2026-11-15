#include <Epub/LayoutCacheOrder.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#define CHECK(c)                                                \
  do {                                                          \
    if (!(c)) {                                                 \
      std::printf("Profile failure at %d: %s\n", __LINE__, #c); \
      std::exit(1);                                             \
    }                                                           \
  } while (0)
#define LOG_ERR(...) ((void)0)
#define LOG_DBG(...) ((void)0)
struct ReaderRenderSpec {
  int id;
  int engine = 0;
};
struct StorageModel {
  std::map<std::string, int> files;
  unsigned renames = 0, removals = 0;
  bool failRename = false;
  bool exists(const char* path) { return files.count(path); }
  bool remove(const char* path) {
    ++removals;
    return files.erase(path);
  }
  bool removeDerivedCache(const char* p) { return remove(p); }
  bool renameDerivedCache(const char* a, const char* b) { return rename(a, b); }
  bool rename(const char* from, const char* to) {
    ++renames;
    if (failRename || !files.count(from) || files.count(to)) return false;
    files[to] = files[from];
    files.erase(from);
    return true;
  }
} Storage;
class Section {
 public:
  static constexpr uint8_t LAYOUT_CACHE_SLOTS = 4;
  std::string filePath;
  bool layoutCacheReady_ = true;
  bool epub = false, partial_ = false;
  int engine_ = 0, pageCount = 0;
  std::map<std::string, std::pair<int, bool>> states;
  ReaderRenderSpec effectiveSpec(const ReaderRenderSpec& spec) const { return spec; }
  std::string preparedLayoutPath(const ReaderRenderSpec&) const { return "prepared"; }
  bool loadSectionFile(const ReaderRenderSpec& spec);
  std::string layoutSlotPath(uint8_t slot) const { return "profile" + std::to_string(slot); }
  bool loadLayoutCandidate(const ReaderRenderSpec& spec) {
    if (!Storage.files.count(filePath) || Storage.files.at(filePath) != spec.id) return false;
    pageCount = states[filePath].first;
    partial_ = states[filePath].second;
    return true;
  }
  bool loadReaderLayout(const ReaderRenderSpec& spec);
};
#include "profile-under-test.inc"

namespace libraryui {
constexpr unsigned kFinished = 1;
}
namespace content {
constexpr unsigned VISIBILITY_VERSION = 3;
}
namespace library {
const char* libraryIndexPath() { return "/index"; }
uint32_t libraryContentRevision() { return 7; }
}  // namespace library
struct {
  unsigned libraryDisplay = 1, libraryUseMetadata = 1;
} SETTINGS;
class LibraryListActivity {
 public:
  enum class EntryPhase { Shell, Preparing, Ready };
  struct Header {
    bool metadataEnabled = true;
    unsigned reserved[1] = {3};
  } head;
  struct Index {
    Header head;
    bool hit = true;
    unsigned opens = 0, closes = 0;
    bool openRetained(const char*, uint32_t) {
      ++opens;
      return hit;
    }
    const Header& header() const { return head; }
    bool ranksDegraded() const { return false; }
    void close() { ++closes; }
  } index;
  bool degraded = false;
  EntryPhase entryPhase = EntryPhase::Shell;
  unsigned pins = 0, filters = 0;
  void resolvePinned() { ++pins; }
  void applyFilter() { ++filters; }
  bool restoreResidentLibrary();
};
#include "resident-under-test.inc"

void residentChecks() {
  LibraryListActivity warm;
  CHECK(warm.restoreResidentLibrary());
  CHECK(warm.entryPhase == LibraryListActivity::EntryPhase::Ready);
  CHECK(warm.pins == 1 && warm.filters == 1 && warm.index.closes == 0);
  LibraryListActivity cold;
  cold.index.hit = false;
  CHECK(!cold.restoreResidentLibrary());
  CHECK(cold.entryPhase == LibraryListActivity::EntryPhase::Shell && cold.filters == 0);
  LibraryListActivity changed;
  changed.index.head.metadataEnabled = false;
  CHECK(!changed.restoreResidentLibrary() && changed.index.closes == 1);
  changed.index.head.metadataEnabled = true;
  changed.index.head.reserved[0] = 0;
  CHECK(!changed.restoreResidentLibrary() && changed.filters == 0);
  SETTINGS.libraryDisplay = 0;
  LibraryListActivity finished;
  CHECK(!finished.restoreResidentLibrary() && finished.index.opens == 0);
  SETTINGS.libraryDisplay = 1;
}

int main() {
  residentChecks();
  Storage.files = {{"profile0", 1}, {"profile1", 2}, {"profile2", 3}, {"profile3", 4}};
  Section section;
  for (int id : {1, 2, 3, 4, 2}) CHECK(section.loadReaderLayout({id}));
  CHECK(section.filePath == "profile1");
  CHECK(Storage.renames == 0 && Storage.removals == 0);
  CHECK(!section.loadReaderLayout({5}));
  CHECK(Storage.files.at("profile1") == 2 && Storage.files.at("profile3") == 4);
  CHECK(Storage.files.at("profile2") == 1 && !Storage.files.count("profile0"));
  CHECK(Storage.renames == 1 && Storage.removals == 1);
  Storage.files = {{"profile0.swap", 9}};
  CHECK(section.loadReaderLayout({9}));
  CHECK(Storage.files.at("profile0") == 9 && section.filePath == "profile0");
  Storage.failRename = true;
  CHECK(!section.loadReaderLayout({10}));
  CHECK(!section.layoutCacheReady_ && Storage.files.at("profile0") == 9);
  Storage.failRename = false;
  Storage.files = {{"prepared", 3}, {"profile0", 3}, {"profile2", 3}};
  Section prepared;
  prepared.epub = true;
  prepared.states = {{"prepared", {7, false}}, {"profile0", {5, true}}, {"profile2", {12, true}}};
  const auto mutations = Storage.renames + Storage.removals;
  CHECK(prepared.loadSectionFile({3}) && prepared.filePath == "prepared" && prepared.pageCount == 7);
  prepared.states["prepared"] = {7, true};
  CHECK(prepared.loadSectionFile({3}) && prepared.filePath == "profile2" && prepared.pageCount == 12);
  prepared.states["profile2"] = {6, true};
  CHECK(prepared.loadSectionFile({3}) && prepared.filePath == "prepared" && prepared.pageCount == 7);
  CHECK(Storage.renames + Storage.removals == mutations);
  Storage.files.erase("prepared");
  CHECK(prepared.loadSectionFile({3}) && prepared.filePath == "profile0");
  layoutcache::clock = UINT32_MAX;
  layoutcache::touch("wrap", 1);
  CHECK(layoutcache::clock == 1 && layoutcache::victim("wrap") != 1);
  std::puts("Production layout profiles: zero mutations on hits; LRU eviction, recovery and failure checks passed");
}

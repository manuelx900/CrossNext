#include <cassert>
#include <cstdio>

#include "src/apps_local/library/CollectionFolderStates.h"
using namespace collections;
int main() {
  testStorage.files["/shelf/a.epub"] = {1};
  testStorage.files["/shelf/b.md"] = {2};
  testStorage.files["/shelf/zFolder/c.xtc"] = {3};
  testStorage.files["/shelf/zFolder/d.epub"] = {4};
  testStorage.files["/shelf/ignored.png"] = {5};
  Folder folder;
  std::string name;
  name.reserve(PathBytes);
  bool directory = false;
  const auto writes = testStorage.writes;
  const auto scans = testStorage.directoryReads;
  assert(folder.open("/shelf", 42) && folder.count() == 3 && folder.hasSubfolders());
  assert(testStorage.directoryReads - scans == 5 && testStorage.writes == writes);
  auto reads = testStorage.readBytes;
  assert(folder.entry(0, name, directory) && directory && name == "zFolder");
  assert(folder.entry(1, name, directory) && !directory && name == "a.epub");
  assert(folder.entry(2, name, directory) && name == "b.md");
  assert(testStorage.readBytes == reads);
  const auto allocations = HalMemory::allocations;
  assert(folder.open("/shelf/zFolder", 42) && !folder.hasSubfolders());
  const auto backScans = testStorage.directoryReads, backOpens = testStorage.opens;
  assert(folder.open("/shelf", 42) && folder.count() == 3);
  assert(testStorage.directoryReads == backScans && testStorage.opens == backOpens && testStorage.readBytes == reads);
  assert(HalMemory::allocations == allocations);
  testStorage.files["/shelf/new.epub"] = {6};
  assert(folder.open("/shelf", 43) && folder.count() == 4 && testStorage.directoryReads > backScans);
  for (int n = 0; n < 500; ++n)
    testStorage.files["/large/" + std::to_string(n) + std::string(120, 'x') + ".epub"] = {1};
  const auto largeWrites = testStorage.writes;
  assert(folder.open("/large", 43) && folder.count() == 500);
  assert(testStorage.writes - largeWrites < 24);
  assert(folder.entry(499, name, directory) && !directory && name.ends_with(".epub"));
  const auto largeScans = testStorage.directoryReads;
  assert(folder.open("/shelf", 43));
  assert(folder.open("/large", 43) && testStorage.directoryReads > largeScans);
  folder.close();
  HalMemory::failAfter = 1;
  assert(folder.open("/shelf", 44) && folder.count() == 4);
  assert(folder.entry(0, name, directory) && directory && name == "zFolder");
  folder.close();
  HalMemory::failAfter = 0;
  assert(!folder.open("/shelf", 45));
  HalMemory::failAfter = -1;
  FolderStates states;
  assert(states.begin());
  states.put("/a", {FolderSelection::All, 3, 3, true});
  states.put("/ab", {FolderSelection::All, 2, 2, true});
  states.put("/", {FolderSelection::All, 5, 5, true});
  states.changed("/a/one.epub", -1, false, false, false);
  FolderSummary result;
  assert(states.get("/a", result) && result.state == FolderSelection::Partial && result.selected == 2);
  assert(states.get("/ab", result) && result.state == FolderSelection::All && result.selected == 2);
  assert(states.get("/", result) && result.selected == 4);
  states.changed("/a/one.epub", 1, false, true, false);
  assert(states.get("/a", result) && result.state == FolderSelection::All);
  states.put("/a/sub", {FolderSelection::All, 1, 1, true});
  states.changed("/a", -3, true, false, false);
  assert(states.get("/a/sub", result) && result.state == FolderSelection::None);
  assert(states.get("/", result) && result.selected == 2);
  states.put("/unknown", {});
  states.changed("/unknown/book.epub", 1, false, true, false);
  assert(!states.get("/unknown", result));
  states.changed("/unknown", 0, false, false, true);
  assert(states.get("/unknown", result) && result.state == FolderSelection::None);
  for (int n = 0; n < 100; ++n) states.put(("/evict" + std::to_string(n)).c_str(), {});
  assert(!states.get("/a", result));
  states.clear();
  assert(!states.get("/evict99", result));
  states.close();
  HalMemory::failAfter = 0;
  assert(!states.begin());
  HalMemory::failAfter = -1;
  Store store;
  assert(store.open());
  const auto id = store.create("Summary");
  assert(id);
  Selection selection;
  assert(selection.open(id, true));
  assert(!selection.mayContainSelected("/"));
  assert(selection.set("/ámbito/nested/one.epub", true));
  for (const auto* prefix : {"/", "/ámbito", "/ámbito/", "/ámbito/nested"})
    assert(selection.mayContainSelected(prefix));
  assert(!selection.mayContainSelected("/unrelated"));
  assert(selection.save());
  selection.close();
  assert(selection.open(id, true));
  assert(selection.mayContainSelected("/ámbito/nested"));
  assert(selection.set("/ámbito/nested/one.epub", false));
  assert(!selection.mayContainSelected("/ámbito"));
  assert(selection.set("/shelf/a.epub", true));
  assert(selection.set("/shelf/b.md", true));
  assert(selection.set("/shelf/a.epub", false));
  assert(selection.mayContainSelected("/shelf"));
  assert(selection.set("/shelf/b.md", false));
  assert(!selection.mayContainSelected("/shelf"));
  selection.close();
  HalMemory::failAfter = 2;
  assert(selection.open(id, true));
  assert(selection.mayContainSelected("/unrelated"));
  HalMemory::failAfter = -1;
  selection.close();
  assert(selection.open(id, true));
  assert(selection.set("/ámbito/nested/one.epub", false));
  FolderSummary summary;
  assert(selectFolder(selection, "/shelf", true, &summary));
  assert(summary.counted && summary.state == FolderSelection::All && summary.total == 5 && summary.selected == 5);
  FolderScan scan;
  assert(scan.start("/shelf"));
  FolderSelection state;
  const auto cancelledReads = testStorage.directoryReads;
  assert(scan.step(selection, state, 4, [](void*) { return false; }, nullptr) == FolderScanResult::Pending);
  assert(testStorage.directoryReads == cancelledReads);
  while (scan.step(selection, state, 1) == FolderScanResult::Pending) {
  }
  assert(scan.summary().counted && scan.summary().selected == 5);
  assert(selection.set("/shelf/a.epub", false));
  assert(scan.start("/shelf"));
  while (scan.step(selection, state, 1) == FolderScanResult::Pending) {
  }
  assert(state == FolderSelection::Partial && !scan.summary().counted);
  std::puts(
      "Collection browsing: one directory pass; zero-I/O cached return/rows; bounded spill, revisions, summaries, "
      "cancellation and OOM passed");
}

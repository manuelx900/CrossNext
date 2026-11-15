#include <cassert>
#include <iostream>

#include "legacy_catalog.h"
#include "src/apps_local/library/CollectionFolder.h"
#include "src/apps_local/library/CollectionStore.h"
using namespace collections;
int main() {
  {
    const auto before = testStorage.files;
    const std::vector<uint8_t> legacy(LegacyCatalog, LegacyCatalog + sizeof(LegacyCatalog));
    testStorage.files["/.crosspoint/collections/catalog-a.bin"] = legacy;
    Store imported;
    assert(imported.open() && imported.count() == 2 && !imported.find(Finished));
    assert(imported.find(Favorites) && imported.find(7) && std::string(imported.find(7)->name) == "Science");
    assert(testStorage.files["/.crosspoint/collections/catalog-a.bin"] == legacy);
    assert(imported.create("Extra") >= 3);
    Store next;
    assert(next.open() && next.count() == 3 && !next.find(Finished));
    testStorage.files = before;
  }
  // A failed first write must not publish a corrupt generation.
  {
    Store fresh;
    assert(fresh.open());
    testStorage.writeLimit = 1;
    assert(!fresh.create("First"));
    testStorage.writeLimit = -1;
    Store reboot;
    assert(reboot.open() && reboot.count() == 1);
    Selection favorite;
    assert(favorite.open(Favorites, true));
    assert(favorite.set("/first.epub", true));
    testStorage.writeLimit = 1;
    assert(!favorite.save());
    testStorage.writeLimit = -1;
    favorite.close();
    bool selected = true;
    assert(isFavorite("/first.epub", selected) && !selected);
    testStorage.refuseRename = true;
    assert(!reboot.create("Unpublished"));
    testStorage.refuseRename = false;
    assert(fresh.open() && fresh.count() == 1);
  }
  Store store;
  assert(store.open());
  auto id = store.create("Reading list");
  assert(id >= 3);
  {
    Selection edit;
    assert(edit.open(id, true));
    assert(edit.set("/a/book.epub", true));
    assert(edit.set("/b/other.xtc", true));
    assert(edit.set("/a/book.epub", false));
    assert(edit.count() == 1);
    assert(edit.save());
  }
  {
    Selection read;
    bool selected;
    assert(read.open(id, false));
    assert(read.has("/a/book.epub", selected) && !selected);
    assert(read.has("/b/other.xtc", selected) && selected);
  }
  testStorage.files["/a/book.epub"] = {1};
  testStorage.files["/a/sub/new.epub"] = {2};
  testStorage.files["/b/other.xtc"] = {3};
  testStorage.files["/a/ignored.png"] = {4};
  auto originals = testStorage.files;
  {
    Selection edit;
    assert(edit.open(id, true));
    FolderSelection state;
    assert(folderSelection(edit, "/a", state) && state == FolderSelection::None);
    assert(selectFolder(edit, "/a", true));
    assert(edit.count() == 3);
    assert(folderSelection(edit, "/a", state) && state == FolderSelection::All);
    assert(edit.set("/a/book.epub", false));
    assert(edit.count() == 2);
    assert(folderSelection(edit, "/a", state) && state == FolderSelection::Partial);
    assert(folderSelection(edit, "/a/sub", state) && state == FolderSelection::All);
    assert(folderSelection(edit, "/b", state) && state == FolderSelection::All);
    assert(edit.save());
  }
  // Cancel an edit; only the last saved membership may survive.
  {
    Selection edit;
    assert(edit.open(id, true));
    assert(selectFolder(edit, "/a", false));
    assert(edit.count() == 1);
    FolderSelection state;
    assert(folderSelection(edit, "/a", state) && state == FolderSelection::None);
    assert(folderSelection(edit, "/b", state) && state == FolderSelection::All);
  }
  {
    Selection read;
    assert(read.open(id, false));
    assert(read.count() == 2);
  }
  // Interrupted writes retain the previous valid generation.
  {
    Selection edit;
    assert(edit.open(id, true));
    assert(edit.set("/b/other.xtc", false));
    testStorage.writeLimit = 1;
    assert(!edit.save());
    testStorage.writeLimit = -1;
  }
  {
    Selection read;
    assert(read.open(id, false));
    assert(read.count() == 2);
  }
  assert(store.favorite(id, true));
  Store reload;
  assert(reload.open());
  assert(reload.find(id)->favorite);
  uint16_t order[MaxCollections];
  assert(reload.order(order) == reload.count());
  assert(reload.at(order[0]).id == id);
  assert(!reload.find(Finished));
  assert(reload.order(order, false) == reload.count());
  for (unsigned n = 0; n < reload.count(); ++n) assert(reload.at(order[n]).id != Finished);
  assert(setFavorite("/b/other.xtc", true));
  bool fav = false;
  assert(isFavorite("/b/other.xtc", fav) && fav);
  assert(setFavorite("/b/other.xtc", false));
  assert(isFavorite("/b/other.xtc", fav) && !fav);
  assert(reload.erase(id));
  assert(reload.count() == 1);
  auto next = reload.create("Another");
  assert(next != 0);
  for (auto& [path, data] : originals)
    if (path.rfind("/.crosspoint/", 0) != 0) assert(testStorage.files[path] == data);
  assert(!validBookPath("/a/../b.epub"));
  assert(!validBookPath("/.crosspoint/book.epub"));
  {
    Selection edit;
    assert(edit.open(next, true));
    for (unsigned n = 0; n < 100; ++n) testStorage.files["/scan/" + std::to_string(n) + ".epub"] = {1};
    FolderScan scan;
    FolderSelection state = FolderSelection::All;
    assert(scan.start("/scan"));
    assert(scan.step(edit, state, 1) == FolderScanResult::Pending);
    unsigned passes = 1;
    while (scan.step(edit, state, 1) == FolderScanResult::Pending) assert(++passes < 110);
    assert(passes >= 100 && state == FolderSelection::None && edit.count() == 0);
    assert(scan.start("/scan"));
    scan.close();
    assert(scan.step(edit, state) == FolderScanResult::Error);
    std::string deep = "/deep";
    for (unsigned n = 0; n < 33; ++n) deep += "/d";
    testStorage.files[deep + "/book.epub"] = {1};
    assert(scan.start("/deep"));
    auto result = FolderScanResult::Pending;
    for (unsigned n = 0; n < 40 && result == FolderScanResult::Pending; ++n) result = scan.step(edit, state, 1);
    assert(result == FolderScanResult::Error);
    assert(scan.start("/scan"));
    do {
      result = scan.step(edit, state);
    } while (result == FolderScanResult::Pending);
    assert(result == FolderScanResult::Complete && state == FolderSelection::None);
  }
  {
    Selection bounded;
    assert(bounded.open(next, true));
    std::string path;
    for (unsigned n = 0; n < MaxBooks; ++n) {
      path = "/many/" + std::to_string(n) + ".epub";
      assert(bounded.set(path.c_str(), true));
    }
    assert(!bounded.set("/many/overflow.epub", true));
    assert(bounded.count() == MaxBooks);
    assert(bounded.save());
  }
  {
    Selection all;
    assert(all.open(next, false));
    assert(all.count() == MaxBooks);
    std::string path;
    assert(all.path(MaxBooks - 1, path));
    assert(path == "/many/32767.epub");
  }
  {
    Selection cache;
    assert(!cache.open(Finished, true));
    assert(cache.openFinishedCache(true) && !cache.hasSnapshot());
    assert(cache.set("/finished.epub", true) && cache.save());
    const auto generation = cache.generation();
    cache.close();
    assert(cache.openFinishedCache(false) && cache.hasSnapshot() && cache.generation() == generation);
    assert(cache.count() == 1);
    assert(!cache.open(Finished, false) && cache.count() == 0);
    testStorage.files["/.crosspoint/collections/books-2-a.bin"][0] = 0;
    assert(!cache.openFinishedCache(false));
    assert(cache.openFinishedCache(true) && !cache.hasSnapshot());
    assert(cache.set("/recovered.xtc", true) && cache.save());
    cache.close();
    assert(cache.openFinishedCache(false) && cache.count() == 1);
    std::string recovered;
    assert(cache.path(0, recovered) && recovered == "/recovered.xtc");
  }
  std::cout
      << "Collections: membership, folder mixing, cancel, interrupted save, favorites and source preservation passed\n";
}

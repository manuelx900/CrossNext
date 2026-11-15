#include <cstdio>

#include "../../src/apps_local/files/FolderVisibility.h"
int main() {
  int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
  using namespace foldervisibility;
  char path[128];
  testStorage = {};
  testStorage.directories.insert("/books");
  testStorage.directories.insert("/books/Study");
  testStorage.files["/books/Study/example.epub"] = {1, 2, 3};
  CHECK(!hidden("/books", "Study", false, path, sizeof(path)));
  CHECK(setMarked("/books/Study", true, path, sizeof(path)));
  CHECK(hidden("/books", "Study", false, path, sizeof(path)));
  CHECK(!hidden("/books", "Study", true, path, sizeof(path)));
  CHECK(Storage.exists("/books/Study/example.epub"));
  CHECK(testStorage.renameCalls == 0);
  CHECK(setMarked("/books/Study", true, path, sizeof(path)));
  CHECK(testStorage.files["/books/Study/.crossnext-hidden"].size() == 1);
  CHECK(setMarked("/books/Study", false, path, sizeof(path)));
  CHECK(!hidden("/books", "Study", false, path, sizeof(path)));
  CHECK(Storage.exists("/books/Study/example.epub"));
  for (const char* name : {"sleep", "fonts", "languages", "notes", "dictionaries", ".private"}) {
    CHECK(hidden("/", name, false, path, sizeof(path)));
    CHECK(!hidden("/", name, true, path, sizeof(path)));
  }
  CHECK(!defaultHidden("/books", "notes"));
  CHECK(markerPath(path, sizeof(path), "/", "books"));
  CHECK(std::strcmp(path, "/books/.crossnext-hidden") == 0);
  CHECK(markerPath(path, sizeof(path), "/books/Study/"));
  CHECK(std::strcmp(path, "/books/Study/.crossnext-hidden") == 0);
  CHECK(!markerPath(path, 8, "/books/Study"));
  CHECK(!setMarked("/books/Study", true, path, 8));
  CHECK(!setMarked("/missing", true, path, sizeof(path)));
  testStorage.files["/books/Study/.crossnext-hidden"] = {9, 9};
  CHECK(!setMarked("/books/Study", false, path, sizeof(path)));
  CHECK(testStorage.files["/books/Study/.crossnext-hidden"].size() == 2);
  testStorage.files.erase("/books/Study/.crossnext-hidden");
  testStorage.writeLimit = 0;
  CHECK(!setMarked("/books/Study", true, path, sizeof(path)));
  CHECK(!Storage.exists("/books/Study/.crossnext-hidden"));
  testStorage.available = false;
  CHECK(!setMarked("/books/Study", true, path, sizeof(path)));
  std::printf("folder visibility: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

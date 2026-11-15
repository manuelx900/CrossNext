#include <HalMemory.h>

#include <cstdio>
#include <string>

#include "../../lib/Epub/Epub/css/CacheValidationMemo.h"
#include "../../lib/LibraryIndex/FacetCache.h"
#include "../../src/apps_local/home/ShelfCatalogCache.h"
int main() {
  int checks = 0, failures = 0;
  const auto check = [&](bool ok, int line) {
    ++checks;
    if (!ok) {
      ++failures;
      std::printf("failed at %d\n", line);
    }
  };
#define CHECK(x) check(bool(x), __LINE__)
  library::FacetCache facets;
  using R = library::FacetCache::Result;
  CHECK(facets.activate(0, 60000) == R::Empty);
  facets.get()[0] = 77;
  facets.commit(3);
  facets.deactivate();
  CHECK(facets.activate(1, 60000) == R::Empty);
  facets.commit(4);
  CHECK(facets.activate(0, 60000) == R::Hit);
  CHECK(facets.get()[0] == 77);
  CHECK(facets.count() == 3);
  CHECK(facets.activate(2, 60000) == R::Empty);
  facets.commit(2);
  CHECK(facets.activate(0, 60000) == R::Hit);
  CHECK(facets.activate(1, 60000) == R::Empty);
  facets.clear();
  CHECK(!facets);
  CHECK(facets.activate(4, 0) == R::Empty);
  facets.commit(0);
  CHECK(facets.activate(4, 0) == R::Hit);
  CHECK(facets.count() == 0);
  CHECK(facets.activate(5, 10) == R::Unavailable);
  CHECK(facets.activate(0, 131073) == R::Unavailable);
  HalMemory::unavailable = true;
  CHECK(facets.activate(0, 200) == R::Unavailable);
  facets.clear();
  HalMemory::unavailable = false;
  homeui::ShelfCatalogCache shelf;
  CHECK(!shelf.matches(1, 2, true, 18));
  CHECK(shelf.begin(1, 2, true, 18));
  for (int n = 0; n < 60; ++n) CHECK(shelf.append(n, n));
  CHECK(!shelf.append(60, 60));
  shelf.finish();
  CHECK(shelf.matches(1, 2, true, 18));
  CHECK(shelf.matches(1, 2, true, 12));
  CHECK(!shelf.matches(2, 2, true, 18));
  CHECK(!shelf.matches(1, 3, true, 18));
  CHECK(!shelf.matches(1, 2, false, 18));
  CHECK(!shelf.matches(1, 2, true, 24));
  std::string path, title, author;
  CHECK(!shelf.read(59, path, title, author));
  CHECK(shelf.ordinal(59) == 59);
  CHECK(shelf.store(59, "/book59", "Title 59", "Author"));
  CHECK(shelf.contains("/book59"));
  CHECK(shelf.read(59, path, title, author));
  CHECK(path == "/book59" && title == "Title 59" && author == "Author");
  CHECK(!shelf.read(60, path, title, author));
  shelf.invalidate();
  CHECK(!shelf.matches(1, 2, true, 18));
  CHECK(!shelf.read(0, path, title, author));
  CHECK(shelf.begin(2, 3, false, 6));
  CHECK(shelf.append(1, 1));
  CHECK(!shelf.store(0, std::string(65536, 'a'), "x", "y"));
  CHECK(shelf.count() == 1);
  homeui::ShelfCatalogCache noShelf;
  HalMemory::unavailable = true;
  CHECK(!noShelf.begin(1, 1, true, 6));
  HalMemory::unavailable = false;
  CacheValidationMemo css;
  CHECK(!css.contains("/a", 12, 1));
  css.remember("/a", 12, 1);
  CHECK(css.contains("/a", 12, 1));
  CHECK(!css.contains("/a", 13, 1));
  CHECK(!css.contains("/a", 12, 2));
  CHECK(!css.contains("/a", 12, 0));
  CHECK(!css.contains("/b", 12, 1));
  css.remember("/b", 12, 1);
  css.remember("/c", 12, 1);
  css.remember("/d", 12, 1);
  CHECK(css.contains("/a", 12, 1));
  css.remember("/e", 12, 1);
  CHECK(!css.contains("/b", 12, 1));
  CHECK(css.contains("/a", 12, 1));
  css.forget("/a");
  CHECK(!css.contains("/a", 12, 1));
  css.clear();
  CHECK(!css.contains("/e", 12, 1));
  css.remember(std::string(512, 'x'), 12, 1);
  CHECK(!css.contains(std::string(512, 'x'), 12, 1));
  CacheValidationMemo noCss;
  HalMemory::unavailable = true;
  noCss.remember("/oom", 1, 1);
  CHECK(!noCss.contains("/oom", 1, 1));
  HalMemory::unavailable = false;
  std::printf("Performance cache contracts: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

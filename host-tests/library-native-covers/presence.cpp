#include <LibraryFormat.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
namespace library {
uint32_t revision = 1;
uint32_t libraryContentRevision() { return revision; }
}  // namespace library
namespace content {
bool hidden = false;
bool hiddenContent(const std::string&) { return hidden; }
}  // namespace content
struct TestStorage {
  unsigned checks = 0;
  bool present = true;
  bool exists(const char*) {
    ++checks;
    return present;
  }
} Storage;
// The runner inserts the production record and isMissing() body, excluding unrelated persistence dependencies.
#include "presence-production.h"
int main() {
  RecentBook book;
  book.path = "/A.epub";
  if (RecentBooksStore::isMissing(book) || Storage.checks != 1) return 1;
  for (int n = 0; n < 100; ++n)
    if (RecentBooksStore::isMissing(book)) return 2;
  if (Storage.checks != 1) return 3;
  ++library::revision;
  if (RecentBooksStore::isMissing(book) || Storage.checks != 2) return 4;
  Storage.present = false;
  ++library::revision;
  for (int n = 0; n < 5; ++n)
    if (!RecentBooksStore::isMissing(book)) return 5;
  if (Storage.checks != 7) return 6;
  Storage.present = true;
  if (RecentBooksStore::isMissing(book) || Storage.checks != 8) return 7;
  content::hidden = true;
  if (!RecentBooksStore::isMissing(book) || Storage.checks != 8) return 8;
  content::hidden = false;
  book.path = "/B.epub";
  Storage.present = false;
  if (!RecentBooksStore::isMissing(book) || Storage.checks != 9) return 9;
  Storage.present = true;
  if (RecentBooksStore::isMissing(book) || Storage.checks != 10) return 10;
  Storage.present = false;
  RecentBooksStore::invalidatePresence();
  if (!RecentBooksStore::isMissing(book) || Storage.checks != 11) return 11;
  std::puts(
      "Production recent presence: unchanged present paths avoid repeated SD checks; revision/path/hidden/missing "
      "changes respected");
}

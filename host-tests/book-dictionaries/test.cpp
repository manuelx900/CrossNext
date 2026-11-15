#include <HalStorage.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/util/BookDictionarySettings.h"

int main() {
  testStorage.directories.insert("/.crosspoint");
  bookdictionary::Settings first, other, restored;
  assert(!bookdictionary::load("/books/one.epub", first));
  assert(!std::strcmp(first.selected(false, "system"), "system"));
  first.overrides = 1;
  std::strcpy(first.definition, "French");
  assert(bookdictionary::save("/books/one.epub", first));
  assert(bookdictionary::load("/books/one.epub", restored));
  assert(restored.overrides == 1 && !std::strcmp(restored.definition, "French"));
  assert(!std::strcmp(restored.selected(true, "Spanish-English"), "Spanish-English"));
  assert(!bookdictionary::load("/books/two.epub", other));
  other.overrides = 3;
  std::strcpy(other.definition, "English");
  assert(bookdictionary::save("/books/two.epub", other));
  assert(bookdictionary::load("/books/two.epub", restored));
  assert(!*restored.selected(true, "default"));
  testStorage.writeLimit = 30;
  first.overrides = 0;
  assert(!bookdictionary::save("/books/one.epub", first));
  testStorage.writeLimit = -1;
  assert(bookdictionary::load("/books/one.epub", restored));
  assert(restored.overrides == 1);
  assert(bookdictionary::save("/books/one.epub", first));
  assert(bookdictionary::load("/books/one.epub", restored));
  assert(!restored.overrides && !std::strcmp(restored.selected(false, "global"), "global"));
  first.overrides = 3;
  std::strcpy(first.definition, "French");
  std::strcpy(first.translation, "French-English");
  assert(bookdictionary::save("/books/one.epub", first));
  testStorage.writeLimit = 10;
  assert(!bookdictionary::move("/books/one.epub", "/Read/failed.epub"));
  assert(bookdictionary::load("/books/one.epub", restored));
  assert(restored.overrides == 3 && !std::strcmp(restored.translation, "French-English"));
  assert(!bookdictionary::load("/Read/failed.epub", restored));
  testStorage.writeLimit = -1;
  assert(bookdictionary::move("/books/one.epub", "/Read/renamed.epub"));
  assert(!bookdictionary::load("/books/one.epub", restored));
  assert(bookdictionary::load("/Read/renamed.epub", restored));
  assert(restored.overrides == 3 && !std::strcmp(restored.translation, "French-English"));
  testStorage.writeLimit = 10;
  first.overrides = 0;
  assert(!bookdictionary::save("/Read/renamed.epub", first));
  testStorage.writeLimit = -1;
  assert(bookdictionary::load("/Read/renamed.epub", restored));
  assert(restored.overrides == 3 && !std::strcmp(restored.definition, "French"));
  for (auto& item : testStorage.files) {
    if (item.first.ends_with(".bin")) item.second[10] ^= 1;
  }
  assert(!bookdictionary::load("/books/one.epub", restored));
  assert(!bookdictionary::load("/Read/renamed.epub", restored));
  assert(!restored.overrides);
  std::strcpy(first.definition, "../invalid");
  assert(!bookdictionary::save("/books/one.epub", first));
  std::memset(first.definition, 'x', sizeof(first.definition));
  assert(!bookdictionary::save("/books/one.epub", first));
  std::puts("Per-book dictionaries: isolation, inherited defaults, explicit None, save failure and corruption passed");
}

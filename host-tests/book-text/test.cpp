#include <ArduinoJson.h>
#include <HalStorage.h>

#include <cassert>
#include <cstdio>

#include "../../src/apps_local/readertext/BookTextPreferences.h"
#include "CrossPointSettings.h"

int main() {
  testStorage.directories.insert("/.crosspoint");
  char existing[80];
  std::snprintf(existing, sizeof(existing), "/.crosspoint/epub_%llu",
                static_cast<unsigned long long>(std::hash<std::string>{}("/books/A.epub")));
  testStorage.directories.insert(existing);
  assert(!Storage.mkdir(existing));
  assert(booktext::begin("/books/A.epub"));
  assert(booktext::save() && testStorage.files.empty());
  SETTINGS.fontPointSize = 18;
  SETTINGS.paragraphIndentSpaces = 0;
  assert(booktext::save());
  JsonDocument global;
  booktext::writeGlobalDefaults(global);
  assert(global["fontSize"] == 14 && global["paragraphIndentSpaces"] == 2);
  SETTINGS.fontPointSize = 16;
  assert(booktext::save());
  SETTINGS.fontPointSize = 18;
  assert(booktext::save());
  const auto files = testStorage.files;
  assert(booktext::save() && files == testStorage.files);
  booktext::end();
  assert(SETTINGS.fontPointSize == 14 && SETTINGS.paragraphIndentSpaces == 2);
  assert(booktext::begin("/books/B.epub") && SETTINGS.fontPointSize == 14);
  booktext::end();
  assert(booktext::begin("/books/A.epub") && SETTINGS.fontPointSize == 18 && SETTINGS.paragraphIndentSpaces == 0);
  testStorage.writeLimit = 12;
  SETTINGS.fontPointSize = 16;
  assert(!booktext::save());
  booktext::end();
  testStorage.writeLimit = -1;
  assert(booktext::begin("/books/A.epub") && SETTINGS.fontPointSize == 18);
  booktext::end();
  SETTINGS.fontPointSize = 12;
  booktext::persist();
  assert(SETTINGS.saves == 1);
  assert(booktext::begin("/books/B.epub") && SETTINGS.fontPointSize == 12);
  booktext::end();
  assert(booktext::begin("/books/A.epub") && SETTINGS.fontPointSize == 18);
  booktext::end();
  assert(booktext::begin("/books/C.epub") && SETTINGS.fontPointSize == 12);
  SETTINGS.fontPointSize = 16;
  SETTINGS.saveOk = false;
  assert(!booktext::copyAsDefaults());
  global.clear();
  booktext::writeGlobalDefaults(global);
  assert(global["fontSize"] == 12);
  SETTINGS.saveOk = true;
  assert(booktext::copyAsDefaults());
  global.clear();
  booktext::writeGlobalDefaults(global);
  assert(global["fontSize"] == 16);
  booktext::end();
  assert(SETTINGS.fontPointSize == 16);
  assert(booktext::begin("/books/D.epub") && SETTINGS.fontPointSize == 16);
  booktext::end();
  assert(booktext::begin("/books/A.epub") && SETTINGS.fontPointSize == 18);
  booktext::end();
  SETTINGS.fontPointSize = 12;
  assert(booktext::copyAsDefaults());
  assert(SETTINGS.perBookTextSettings == 1);
  assert(booktext::begin("/books/A.epub") && SETTINGS.fontPointSize == 18);
  assert(booktext::setPerBook(false));
  assert(SETTINGS.perBookTextSettings == 0);
  SETTINGS.fontPointSize = 14;
  assert(booktext::save());
  global.clear();
  booktext::writeGlobalDefaults(global);
  assert(global["fontSize"].isNull());
  booktext::end();
  assert(SETTINGS.fontPointSize == 14);
  // Shared mode ignores, but retains, private records across book changes.
  assert(booktext::begin("/books/B.epub") && SETTINGS.fontPointSize == 14);
  SETTINGS.fontPointSize = 16;
  assert(booktext::save());
  booktext::end();
  assert(SETTINGS.fontPointSize == 16);
  SETTINGS.saveOk = false;
  assert(!booktext::setPerBook(true) && !SETTINGS.perBookTextSettings);
  SETTINGS.saveOk = true;
  assert(booktext::setPerBook(true));
  assert(booktext::begin("/books/A.epub") && SETTINGS.fontPointSize == 18);
  booktext::end();
  assert(SETTINGS.fontPointSize == 16);
  // Enabling private changes during a book preserves its current layout.
  assert(booktext::setPerBook(false));
  assert(booktext::begin("/books/C.epub") && SETTINGS.fontPointSize == 16);
  SETTINGS.fontPointSize = 12;
  assert(booktext::save());
  assert(booktext::setPerBook(true) && SETTINGS.fontPointSize == 12);
  SETTINGS.fontPointSize = 18;
  assert(booktext::save());
  booktext::end();
  assert(SETTINGS.fontPointSize == 12);
  assert(booktext::begin("/books/C.epub") && SETTINGS.fontPointSize == 18);
  booktext::end();
  // Re-enabling within a shared reader advances an existing dual-slot journal.
  assert(booktext::begin("/books/E.epub"));
  for (uint8_t point : {14, 16, 18, 14}) {
    SETTINGS.fontPointSize = point;
    assert(booktext::save());
  }
  booktext::end();
  assert(booktext::setPerBook(false));
  assert(booktext::begin("/books/E.epub") && SETTINGS.fontPointSize == 12);
  assert(booktext::setPerBook(true));
  booktext::end();
  assert(booktext::begin("/books/E.epub") && SETTINGS.fontPointSize == 12);
  booktext::end();
  // A failed shared save remains retryable and does not advance defaults.
  assert(booktext::setPerBook(false));
  assert(booktext::begin("/books/D.epub"));
  SETTINGS.fontPointSize = 14;
  SETTINGS.saveOk = false;
  assert(!booktext::save());
  SETTINGS.saveOk = true;
  assert(booktext::save());
  booktext::end();
  assert(SETTINGS.fontPointSize == 14);
  assert(booktext::setPerBook(true));
  SETTINGS.fontPointSize = 12;
  for (auto& entry : testStorage.files) entry.second[0] = 0;
  assert(booktext::begin("/books/A.epub") && SETTINGS.fontPointSize == 12);
  booktext::end();
  std::puts("Book text: isolation, global serialization, restart, failed writes and corruption passed");
}

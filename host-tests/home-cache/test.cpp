#include <cstdio>
#include <string>
#include <vector>

#include "../../src/apps_local/ui/PixelText.h"
#include "../../src/util/BookProgressCache.h"
#include "../../src/util/MenuFontStyle.h"

int main() {
  unsigned checks = 0, failed = 0;
  const auto check = [&](bool ok) {
    ++checks;
    if (!ok) {
      ++failed;
      std::printf("FAIL %u\n", checks);
    }
  };
  bookprogress::Cache cache;
  bookprogress::Stamp stamp;
  stamp.sourceBytes = 12000;
  stamp.sourceModified = 100;
  stamp.metadataBytes = 240;
  stamp.metadataModified = 101;
  stamp.progressBytes = 6;
  unsigned parsed = 0;
  const auto visit = [&](int book) {
    const auto path = "/books/book" + std::to_string(book) + ".epub";
    int percent = cache.find(path.c_str(), stamp);
    if (percent < 0) {
      ++parsed;
      percent = book;
      cache.store(path.c_str(), stamp, percent);
    }
    check(percent == book);
  };
  for (int round = 0; round < 100; ++round)
    for (int book = 0; book < 7; ++book) visit(book);
  check(parsed == 7);
  auto changed = stamp;
  changed.progress[2] = 1;
  check(cache.find("/books/book0.epub", changed) < 0);
  changed = stamp;
  ++changed.sourceBytes;
  check(cache.find("/books/book0.epub", changed) < 0);
  changed = stamp;
  ++changed.sourceModified;
  check(cache.find("/books/book0.epub", changed) < 0);
  changed = stamp;
  ++changed.metadataBytes;
  check(cache.find("/books/book0.epub", changed) < 0);
  changed = stamp;
  ++changed.metadataModified;
  check(cache.find("/books/book0.epub", changed) < 0);
  changed = stamp;
  ++changed.progressBytes;
  check(cache.find("/books/book0.epub", changed) < 0);
  for (int book = 7; book < 64; ++book) visit(book);
  visit(0);
  visit(64);
  check(cache.find("/books/book0.epub", stamp) == 0);
  check(cache.find("/books/book1.epub", stamp) < 0);
  check(cache.find("/books/book64.epub", stamp) == 64);
  const std::string longPath(512, 'x');
  cache.store(longPath.c_str(), stamp, 40);
  check(cache.find(longPath.c_str(), stamp) < 0);
  cache.store("/invalid.epub", stamp, -1);
  check(cache.find("/invalid.epub", stamp) < 0);
  check(menufont::normalized(0) == menufont::Style::Pixel);
  check(menufont::normalizedOverride(1) == menufont::Override::Pixel);
  check(menufont::resolve(2, 1) == menufont::Style::Pixel);
  check(menufont::resolve(0, 3) == menufont::Style::NotoSans);
  check(menufont::resolve(2, 0, 1) == menufont::Style::Pixel);
  check(menufont::normalized(255) == menufont::kDefault);
  char label[80];
  pixelui::uppercaseLabels = true;
  pixelui::copyNatural(label, sizeof(label), "Configuración y energía");
  check(std::string(label) == "CONFIGURACIÓN Y ENERGÍA");
  pixelui::uppercaseLabels = false;
  pixelui::copyNatural(label, sizeof(label), "Configuración y energía");
  check(std::string(label) == "Configuración y energía");
  pixelui::copyNatural(label, sizeof(label), "SETTINGS");
  check(std::string(label) == "Settings");
  std::printf("%u checks, %u failures; 700 shelf visits, %u metadata parses; cache %zu bytes\n", checks, failed, parsed,
              sizeof(cache));
  return failed ? 1 : 0;
}

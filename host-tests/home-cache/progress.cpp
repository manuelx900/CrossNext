#include <Epub.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "../../src/apps_local/home/CoverSceneCache.h"
#include "../../src/util/BookProgress.h"
int main(int argc, char** argv) {
  const bool baseline = argc > 1 && std::string(argv[1]) == "baseline";
  const bool oom = argc > 1 && std::string(argv[1]) == "oom";
  failPsram = oom;
  unsigned checks = 0, failures = 0;
  const auto check = [&](bool ok) {
    ++checks;
    if (!ok) ++failures;
  };
  testStateRevision = 1;
  for (int book = 0; book < 7; ++book) {
    const auto path = "/books/book" + std::to_string(book) + ".epub";
    const auto base = "/.crosspoint/epub_" + std::to_string(std::hash<std::string>{}(path));
    testStorage.files[path] = std::vector<uint8_t>(1000);
    testStorage.files[base + "/book.bin"] = std::vector<uint8_t>(64);
    testStorage.files[base + "/progress.bin"] = {0, 0, 10, 0, 100, 0};
  }
  for (int round = 0; round < 100; ++round)
    for (int book = 0; book < 7; ++book) check(loadBookProgress("/books/book" + std::to_string(book) + ".epub") == 5);
  const unsigned warmLoads = testEpubLoads;
  const auto warmBytes = testStorage.readBytes;
  check(loadBookProgress("/books/book0.epub") == 5);
  if (!oom && !baseline) check(testStorage.readBytes == warmBytes);
  check(warmLoads == (baseline || oom ? 700 : 7));
  const auto path = std::string("/books/book0.epub");
  const auto base = "/.crosspoint/epub_" + std::to_string(std::hash<std::string>{}(path));
  testStorage.files[base + "/progress.bin"][2] = 50;
  ++testStateRevision;
  check(loadBookProgress(path) == 25);
  testStorage.files[base + "/book.bin"].push_back(0);
  homecover::invalidateScenes();
  check(loadBookProgress(path) == 25);
  testStorage.files[path].push_back(0);
  homecover::invalidateScenes();
  check(loadBookProgress(path) == 25);
  ++testStateRevision;
  publishBookProgress(path, 40);
  if (!oom && !baseline) {
    const auto before = testStorage.readBytes;
    check(loadBookProgress(path) == 40);
    check(testStorage.readBytes == before);
  }
  testStorage.files.erase(base + "/progress.bin");
  ++testStateRevision;
  check(loadBookProgress(path) == -1);
  check(loadBookProgress("/missing.epub") == -1);
  std::printf("Production BookProgress: %u checks, %u failures; %u metadata loads for 700 warm shelf queries (%s)\n",
              checks, failures, warmLoads,
              baseline ? "baseline"
              : oom    ? "PSRAM unavailable"
                       : "cached");
  return failures ? 1 : 0;
}

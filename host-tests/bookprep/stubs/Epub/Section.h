#pragma once
#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/ReaderRenderSpec.h>

#include <memory>
class GfxRenderer;
class Section {
  std::string key;
  bool complete = false;
  int steps = 0;

 public:
  static uint8_t cacheVersion() { return 76; }
  uint16_t pageCount = 2;
  std::unique_ptr<Page> loadPage(int) { return std::make_unique<Page>(); }
  Section(const std::shared_ptr<Epub>& e, int n, GfxRenderer&) : key(e->path + std::to_string(n)) {}
  bool loadPreparedSectionFile(const ReaderRenderSpec& spec) {
    key += ":" + std::to_string(spec.fontCacheKey());
    return prepared.count(key);
  }
  bool isPartial() { return false; }
  bool startBuild(const ReaderRenderSpec&) {
    ++builds;
    return true;
  }
  bool buildSomeMore(int, bool (*)()) {
    if (++steps == 2) {
      complete = true;
      prepared.insert(key);
    }
    return true;
  }
  bool isBuildComplete() { return complete; }
};

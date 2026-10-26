#pragma once

#include <HalStorage.h>
#include <KOReaderSyncClient.h>
#include <ProgressComparison.h>

#include "ReadingStatsCloud.h"

class GfxRenderer;
namespace readingstats::cloud {
struct ProgressBook {
  std::string path, cache, alternate;
  KOReaderProgress local{};
  CrossPointPosition position{};
  uint32_t pages = 0;
  uint8_t saved[10]{};
  uint8_t savedSize = 0;
  bool fixedPages = false;
};

// Streams the card independently of Recents and the optional Library index.
class ProgressBooks {
 public:
  explicit ProgressBooks(GfxRenderer& renderer) : renderer(renderer) {}
  Result begin();
  Result next(ProgressBook& book);
  Result prepare(ProgressBook& book);
  Result resolve(ProgressBook& book, const KOReaderProgress* primary, const KOReaderProgress* alternate,
                 ProgressComparison& comparison, bool& useAlternate, CrossPointPosition& destination);
  Result apply(const ProgressBook& book, const CrossPointPosition& destination);
  Result unchanged(const ProgressBook& book);
  void close();

 private:
  static constexpr size_t maxDepth = 32, maxPath = 1024;
  GfxRenderer& renderer;
  struct Directory {
    HalFile file;
    size_t prefix = 0;
  } directories[maxDepth];
  std::string folder;
  char name[512]{};
  size_t depth = 0;
  uint32_t entries = 0;
};
}  // namespace readingstats::cloud

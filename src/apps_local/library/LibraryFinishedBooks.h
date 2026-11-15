#pragma once
#include <HalMemory.h>

#include <memory>

#include "../reading/ReadingCompletionIndex.h"
#include "../reading/ReadingStatsStore.h"
namespace libraryui {
class FinishedCacheStamp {
 public:
  bool capture(const char* libraryPath, uint64_t recentPaths = 0);
  bool matches(uint32_t generation) const;
  bool save(uint32_t generation) const;

 private:
  uint8_t bytes[40]{};
};
// Temporary work only while rebuilding the optional finished-book filter.
class FinishedBooks {
 public:
  bool begin();
  bool empty() const { return !hasCompletions; }
  bool contains(const std::string& path, bool& finished);

 private:
  readingstats::Repository repository;
  readingstats::Record record;
  readingstats::CompletionIndex index;
  HalMemory::PsramBuffer memory;
  HalMemory::PsramBuffer readAhead;
  std::unique_ptr<uint8_t[]> hashScratch;
  bool hasCompletions = false;
};
}  // namespace libraryui

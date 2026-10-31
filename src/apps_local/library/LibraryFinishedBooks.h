#pragma once
#include <HalMemory.h>

#include <memory>

#include "../reading/ReadingCompletionIndex.h"
#include "../reading/ReadingStatsStore.h"
namespace libraryui {
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
  std::unique_ptr<uint8_t[]> hashScratch;
  bool hasCompletions = false;
};
}  // namespace libraryui

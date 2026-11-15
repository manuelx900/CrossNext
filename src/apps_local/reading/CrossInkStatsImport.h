#pragma once

#include <HalMemory.h>
#include <LibraryIndexFile.h>

#include "CrossInkStatsCodec.h"
#include "ReadingStatsCloud.h"

namespace readingstats::crossink {
class Importer {
 public:
  ~Importer() { cancel(); }
  cloud::Result begin();
  cloud::Result step();
  void cancel();
  const Global& totals() const { return global; }
  uint32_t processed() const { return cursor; }
  uint32_t total() const { return index.bookCount(); }
  uint32_t matched() const { return count; }

 private:
  enum class Stage { Books, Commit, Done };
  Stage stage = Stage::Done;
  Global global;
  cloud::Header header;
  library::LibraryIndexFile index;
  library::ClixRecord entry{};
  HalMemory::PsramBuffer memory;
  Book* books = nullptr;
  Book pending;
  cloud::Reader previous;
  HalFile output;
  std::string root, path, metadata;
  uint8_t raw[cloud::kHeaderBytes]{};
  uint8_t hashBuffer[1024]{};
  uint32_t cursor = 0, count = 0;
  int slot = -1;
  bool writing = false;
  bool loadGlobal();
  bool loadBook(const std::string& cache, Book& book);
  cloud::Result commit();
};
bool installed();
}  // namespace readingstats::crossink

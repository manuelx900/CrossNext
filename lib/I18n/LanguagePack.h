#pragma once

#include <HalMemory.h>

#include <cstddef>
#include <cstdint>

class HalFile;

namespace languagepack {
static constexpr size_t MAX_TEXT_BYTES = 1024 * 1024;
static constexpr size_t MAX_ENTRIES = 8192;
static constexpr size_t MAX_STRING_BYTES = 8192;
static constexpr size_t MAX_INSTALLED = 1024;
static constexpr size_t FILENAME_BYTES = 64;

struct Info {
  char code[24]{};
  char name[64]{};
  uint32_t entries = 0;
  uint32_t textBytes = 0;
  uint32_t checksum = 0;
};

bool validFilename(const char* filename);
bool readInfo(HalFile& file, Info& info);
bool formatsMatch(const char* english, const char* translation);

// Loaded once at boot. Borrowed strings remain valid until the next reset.
class Pack {
 public:
  bool load(const char* filename);
  const char* get(size_t stringId) const;
  bool loaded() const { return static_cast<bool>(arena); }
  const Info& info() const { return metadata; }

 private:
  HalMemory::PsramBuffer arena;
  Info metadata;
};

struct CatalogEntry {
  char filename[FILENAME_BYTES]{};
  Info info;
};

// Temporary selector metadata; never retained by the rendering hot path.
class Catalog {
 public:
  bool scan();
  void clear() {
    entries.reset();
    count = 0;
  }
  size_t size() const { return count; }
  const CatalogEntry& operator[](size_t index) const;

 private:
  HalMemory::PsramBuffer entries;
  size_t count = 0;
};
}  // namespace languagepack

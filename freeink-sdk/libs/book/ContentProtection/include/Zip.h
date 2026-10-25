#pragma once

// FreeInk — minimal ZIP access for encrypted entries.
//
// The content module is parser-agnostic: it does its own central-directory
// scan so it works alongside any EPUB parser (CrossPoint's ZipFile,
// FreeInkBook's ZipCatalog) without sharing their state. Only what the read
// path needs: entry lookup, local-header data offsets, raw entry reads.
//
// Freestanding C++17.

#include <stdint.h>

#include <memory>
#include <string>

#include "ByteSource.h"

namespace freeink {
namespace content {

// Entry names are held as FNV-1a 64-bit hashes instead of a heap string apiece.
struct ZipEntryInfo {
  uint64_t nameHash = 0;
  uint32_t compressedSize = 0;
  uint32_t uncompressedSize = 0;
  uint32_t localHeaderOffset = 0;
  uint16_t method = 0;  // 0 = stored, 8 = deflate
};

class ZipScan {
 public:
  // Scans the central directory. Returns false if not a ZIP container.
  bool open(ByteSource& source);

  const ZipEntryInfo* find(const std::string& name) const;

  // Offset of an entry's raw stored data (past its local header).
  bool dataOffset(ByteSource& source, const ZipEntryInfo& entry, uint64_t* out) const;

  // Reads an entry's raw stored bytes (the "compressed" form).
  bool readRaw(ByteSource& source, const ZipEntryInfo& entry, uint8_t* out) const;

 private:
  std::unique_ptr<ZipEntryInfo[]> entries_;
  size_t entryCount_ = 0;
};

}  // namespace content
}  // namespace freeink

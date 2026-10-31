#include <Arduino.h>
#include <ContentVisibility.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <array>
#include <cstring>
#include <string_view>
#include <utility>

#include "LibraryBuilder.h"
#include "LibraryInventory.h"

namespace library {
namespace {
struct Scan {
  struct Folder {
    HalFile file;
    size_t length = 0;
    uint16_t dedupBase = 0;
  };
  std::array<Folder, LIBRARY_MAX_DEPTH + 1> folders;
  char path[512]{'/', '\0'}, name[512]{};
  CardSignature signature;
  uint64_t keys[LIBRARY_MAX_DEDUP_KEYS]{};
  uint16_t used = 0;
};
}  // namespace
CardCheck checkLibraryCard(const ClixHeader& header) {
  if (header.reserved[1] != 1) return CardCheck::Changed;
  // Handles and filename buffers exceed the task stack budget; one checked
  // allocation is released before any rebuild allocates its own workspace.
  auto scan = makeUniqueNoThrow<Scan>();
  if (!scan) {
    LOG_ERR("LIBIDX", "OOM: card directory check");
    return CardCheck::Unavailable;
  }
  scan->folders[0].file = Storage.open("/");
  scan->folders[0].length = 1;
  if (!scan->folders[0].file || !scan->folders[0].file.isDirectory()) return CardCheck::Unavailable;
  int depth = 0;
  unsigned visited = 0;
  while (depth >= 0 && scan->signature.count < CLIX_MAX_RECORDS) {
    auto& folder = scan->folders[depth];
    scan->path[folder.length] = '\0';
    bool failed = false;
    auto entry = folder.file.openNextFile(failed);
    if (failed) return CardCheck::Unavailable;
    if (!entry) {
      scan->used = folder.dedupBase;
      folder.file.close();
      --depth;
      continue;
    }
    const size_t length = entry.getName(scan->name, sizeof(scan->name));
    const bool directory = entry.isDirectory();
    const uint32_t size = directory ? 0 : static_cast<uint32_t>(entry.fileSize());
    entry.close();
    if ((++visited & 31) == 0) delay(1);
    if (!length || !inventory::visible(scan->name)) continue;
    if (directory && inventory::appRootFolder(scan->name, depth)) continue;
    const size_t prefix = folder.length + (folder.length == 1 ? 0 : 1);
    if (prefix + length >= sizeof(scan->path)) continue;
    if (folder.length > 1) scan->path[folder.length] = '/';
    std::memcpy(scan->path + prefix, scan->name, length + 1);
    if (directory) {
      if (depth >= LIBRARY_MAX_DEPTH) continue;
      auto child = Storage.open(scan->path);
      if (!child || !child.isDirectory()) return CardCheck::Unavailable;
      ++depth;
      scan->folders[depth].file = std::move(child);
      scan->folders[depth].length = prefix + length;
      scan->folders[depth].dedupBase = scan->used;
    } else if (size && inventory::representable(scan->name, folder.length) && inventory::bookName(scan->name)) {
      if (inventory::remember(scan->keys, LIBRARY_MAX_DEDUP_KEYS, folder.dedupBase, scan->used, scan->name, size) ==
          inventory::Remember::Duplicate)
        continue;
      scan->signature.add(clixPathHash(scan->path, prefix + length), size);
    }
  }
  return matchesCardSignature(header, scan->signature) ? CardCheck::Unchanged : CardCheck::Changed;
}
}  // namespace library

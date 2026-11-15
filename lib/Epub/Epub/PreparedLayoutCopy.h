#pragma once

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <string>

namespace preparedlayout {
// Cold path only: preserve the named layout and the primary layout used by
// progress probes. A checked 4 KiB buffer avoids a chapter-sized allocation or
// a large stack buffer; it is released before any parser starts.
inline bool copy(const std::string& source, const std::string& destination, void (*yield)() = nullptr) {
  const std::string temporary = destination + ".prep.tmp", backup = destination + ".swap";
  if (Storage.exists(backup.c_str()) && !Storage.exists(destination.c_str()) &&
      !Storage.renameDerivedCache(backup.c_str(), destination.c_str()))
    return false;
  auto buffer = makeUniqueNoThrow<uint8_t[]>(4096);
  if (!buffer) {
    LOG_ERR("SCT", "OOM: prepared layout copy");
    return false;
  }
  bool complete = false;
  {
    HalFile input, output;
    if (!Storage.openFileForRead("SCT", source, input) ||
        !Storage.openDerivedCacheForWrite("SCT", temporary.c_str(), output))
      return false;
    size_t remaining = input.size();
    while (remaining) {
      const size_t amount = std::min<size_t>(remaining, 4096);
      if (input.read(buffer.get(), amount) != int(amount) || output.write(buffer.get(), amount) != amount) break;
      remaining -= amount;
      if (yield) yield();
    }
    output.flush();
    complete = remaining == 0 && output.size() == input.size();
    // Check the final SD sync before publishing; the destructor cannot report it.
    if (!output.close()) complete = false;
  }
  if (!complete) {
    LOG_ERR("SCT", "Could not finish prepared layout copy");
    Storage.removeDerivedCache(temporary.c_str());
    return false;
  }
  const bool previous = Storage.exists(destination.c_str());
  if (previous && ((Storage.exists(backup.c_str()) && !Storage.removeDerivedCache(backup.c_str())) ||
                   !Storage.renameDerivedCache(destination.c_str(), backup.c_str()))) {
    Storage.removeDerivedCache(temporary.c_str());
    return false;
  }
  if (!Storage.renameDerivedCache(temporary.c_str(), destination.c_str())) {
    if (previous) Storage.renameDerivedCache(backup.c_str(), destination.c_str());
    Storage.removeDerivedCache(temporary.c_str());
    return false;
  }
  if (previous) Storage.removeDerivedCache(backup.c_str());
  return true;
}
}  // namespace preparedlayout

#pragma once

#include <OpdsEntry.h>

inline bool canBrowseOpdsEntry(const OpdsEntry& entry) {
  return !entry.href.empty() &&
         (entry.type == OpdsEntryType::NAVIGATION || (!entry.purchase && !entry.indirect && !entry.lcpLicense));
}

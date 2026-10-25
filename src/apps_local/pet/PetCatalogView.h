#pragma once

#include "PetScreens.h"

namespace petui {
inline bool catalogMatches(const pet::Profile& p, unsigned item, CatalogFilter filter) {
  if (item >= pet::OBJECT_COUNT) return false;
  switch (filter) {
    case CatalogFilter::Owned:
      return pet::owns(p, item);
    case CatalogFilter::Unowned:
      return !pet::owns(p, item);
  }
  return false;
}
inline unsigned catalogCount(const pet::Profile& p, unsigned slot, CatalogFilter filter) {
  if (slot >= pet::SLOT_COUNT) return 0;
  unsigned count = 0;
  for (unsigned i = 0; i < pet::OBJECTS_PER_SLOT; ++i) count += catalogMatches(p, pet::objectAt(slot, i), filter);
  return count;
}
inline unsigned catalogLeading(const pet::Profile& p, unsigned slot, CatalogFilter filter) {
  if (slot >= pet::SLOT_COUNT || filter != CatalogFilter::Owned) return pet::OBJECT_COUNT;
  const unsigned item = p.equipped[slot];
  return item < pet::OBJECT_COUNT && pet::objectSlot(item) == slot && pet::owns(p, item) ? item : pet::OBJECT_COUNT;
}
inline unsigned catalogItem(const pet::Profile& p, unsigned slot, CatalogFilter filter, unsigned at) {
  if (slot >= pet::SLOT_COUNT) return pet::OBJECT_COUNT;
  const unsigned first = catalogLeading(p, slot, filter);
  const bool promoted = first < pet::OBJECT_COUNT;
  if (promoted) {
    if (!at) return first;
    --at;
  }
  for (unsigned i = 0; i < pet::OBJECTS_PER_SLOT; ++i) {
    const unsigned item = pet::objectAt(slot, i);
    if (promoted && item == first) continue;
    if (catalogMatches(p, item, filter)) {
      if (!at) return item;
      --at;
    }
  }
  return pet::OBJECT_COUNT;
}
inline unsigned catalogOrdinal(const pet::Profile& p, unsigned slot, CatalogFilter filter, unsigned item) {
  if (slot >= pet::SLOT_COUNT || !catalogMatches(p, item, filter)) return pet::OBJECT_COUNT;
  const auto first = catalogLeading(p, slot, filter);
  if (first == item) return 0;
  unsigned ordinal = first < pet::OBJECT_COUNT ? 1 : 0;
  for (unsigned i = 0; i < pet::OBJECTS_PER_SLOT; ++i) {
    const auto candidate = pet::objectAt(slot, i);
    if (candidate == first || !catalogMatches(p, candidate, filter)) continue;
    if (candidate == item) return ordinal;
    ++ordinal;
  }
  return pet::OBJECT_COUNT;
}
inline unsigned catalogAdjacent(const pet::Profile& p, unsigned slot, CatalogFilter filter, unsigned item, int step) {
  if (step != -1 && step != 1) return pet::OBJECT_COUNT;
  const auto ordinal = catalogOrdinal(p, slot, filter, item);
  if (ordinal == pet::OBJECT_COUNT || (!ordinal && step < 0)) return pet::OBJECT_COUNT;
  return catalogItem(p, slot, filter, unsigned(int(ordinal) + step));
}
}  // namespace petui

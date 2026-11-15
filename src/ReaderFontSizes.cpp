#include "ReaderFontSizes.h"

#include <iterator>

bool readerFontUpgradesUbuntu(const SdCardFontFamilyInfo& family) { return family.vector && family.name == "Ubuntu"; }

int readerFontFamilyCount(const SdCardFontRegistry* registry) {
  int count = 1;
  if (registry) {
    for (const auto& family : registry->getFamilies())
      if (!readerFontUpgradesUbuntu(family)) ++count;
  }
  return count;
}

const SdCardFontFamilyInfo* readerFontFamilyAt(const SdCardFontRegistry* registry, int index) {
  if (!registry || index < 0) return nullptr;
  if (index == 0) {
    const auto* family = registry->findFamily("Ubuntu");
    return family && readerFontUpgradesUbuntu(*family) ? family : nullptr;
  }
  for (const auto& family : registry->getFamilies()) {
    if (readerFontUpgradesUbuntu(family)) continue;
    if (--index == 0) return &family;
  }
  return nullptr;
}

int readerFontFamilyIndex(const SdCardFontRegistry* registry, const char* familyName) {
  if (!registry || !familyName || !*familyName) return 0;
  int index = 1;
  for (const auto& family : registry->getFamilies()) {
    if (readerFontUpgradesUbuntu(family)) continue;
    if (family.name == familyName) return index;
    ++index;
  }
  return 0;
}

std::vector<uint8_t> readerFontPointSizes(const SdCardFontRegistry* registry, const char* sdFamilyName) {
  if (registry) {
    const char* name = sdFamilyName && *sdFamilyName ? sdFamilyName : "Ubuntu";
    if (const auto* family = registry->findFamily(name)) {
      if ((!sdFamilyName || !*sdFamilyName) && !readerFontUpgradesUbuntu(*family))
        return {std::begin(BUILTIN_READER_POINT_SIZES), std::end(BUILTIN_READER_POINT_SIZES)};
      if (family->vector) {
        return {std::begin(VECTOR_READER_POINT_SIZES), std::end(VECTOR_READER_POINT_SIZES)};
      }
      const auto sizes = family->availableSizes();
      if (!sizes.empty()) {
        std::vector<uint8_t> selected;
        selected.reserve(std::size(VECTOR_READER_POINT_SIZES));
        for (const auto target : VECTOR_READER_POINT_SIZES) {
          const auto nearest = snapToNearestPointSize(sizes, target);
          if (selected.empty() || selected.back() != nearest) selected.push_back(nearest);
        }
        return selected;
      }
    }
  }
  return {std::begin(BUILTIN_READER_POINT_SIZES), std::end(BUILTIN_READER_POINT_SIZES)};
}

uint8_t snapToNearestPointSize(const uint8_t* sizes, const size_t count, const uint8_t pt) {
  if (!sizes || count == 0) return pt;

  uint8_t best = sizes[0];
  uint8_t bestDelta = best > pt ? best - pt : pt - best;
  for (size_t i = 1; i < count; i++) {
    const uint8_t delta = sizes[i] > pt ? sizes[i] - pt : pt - sizes[i];
    // Strictly-less keeps the smaller size on a tie, since `sizes` is ascending.
    if (delta < bestDelta) {
      best = sizes[i];
      bestDelta = delta;
    }
  }
  return best;
}

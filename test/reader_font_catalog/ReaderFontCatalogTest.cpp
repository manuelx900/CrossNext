#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#define private public
#include "ReaderFontSizes.h"
#undef private

// Catalog fixtures isolate SD discovery while using the production family types.
const SdCardFontFamilyInfo* SdCardFontRegistry::findFamily(const std::string& name) const {
  for (const auto& family : families_)
    if (family.name == name) return &family;
  return nullptr;
}
std::vector<uint8_t> SdCardFontFamilyInfo::availableSizes() const {
  std::vector<uint8_t> result;
  result.reserve(files.size());
  for (const auto& file : files) result.push_back(file.pointSize);
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

namespace {
void addFamily(SdCardFontRegistry& registry, const char* name, bool vector) {
  auto& family = registry.families_.emplace_back();
  family.name = name;
  family.vector = vector;
}
}  // namespace

TEST(ReaderFontCatalog, InstalledUbuntuUpgradesTheBuiltInEntryWithoutDuplicatingIt) {
  SdCardFontRegistry registry;
  registry.families_.reserve(3);
  addFamily(registry, "Bitter", true);
  addFamily(registry, "Noto Serif", false);
  addFamily(registry, "Ubuntu", true);
  EXPECT_EQ(readerFontFamilyCount(&registry), 3);
  ASSERT_NE(readerFontFamilyAt(&registry, 0), nullptr);
  EXPECT_EQ(readerFontFamilyAt(&registry, 0)->name, "Ubuntu");
  EXPECT_EQ(readerFontFamilyAt(&registry, 1)->name, "Bitter");
  EXPECT_EQ(readerFontFamilyAt(&registry, 2)->name, "Noto Serif");
  EXPECT_EQ(readerFontFamilyAt(&registry, 3), nullptr);
  EXPECT_EQ(readerFontFamilyAt(&registry, -1), nullptr);
  EXPECT_EQ(readerFontFamilyIndex(&registry, "Ubuntu"), 0);
  EXPECT_EQ(readerFontFamilyIndex(&registry, ""), 0);
  EXPECT_EQ(readerFontFamilyIndex(&registry, "Bitter"), 1);
  EXPECT_EQ(readerFontFamilyIndex(&registry, "Noto Serif"), 2);
  EXPECT_EQ(readerFontFamilyIndex(&registry, "Deleted"), 0);
  const auto sizes = readerFontPointSizes(&registry, "");
  ASSERT_EQ(sizes.size(), std::size(VECTOR_READER_POINT_SIZES));
  EXPECT_EQ(sizes.front(), 12);
  EXPECT_EQ(sizes.back(), 18);
  EXPECT_EQ(readerFontPointSizes(&registry, "Ubuntu"), sizes);
}

TEST(ReaderFontCatalog, MissingResourcesKeepTheResidentFallbackAvailable) {
  SdCardFontRegistry registry;
  registry.families_.reserve(1);
  addFamily(registry, "Bitter", true);
  EXPECT_EQ(readerFontFamilyCount(&registry), 2);
  EXPECT_EQ(readerFontFamilyAt(&registry, 0), nullptr);
  EXPECT_EQ(readerFontFamilyAt(&registry, 1)->name, "Bitter");
  EXPECT_EQ(readerFontPointSizes(&registry, ""), std::vector<uint8_t>{12});
  EXPECT_EQ(readerFontFamilyCount(nullptr), 1);
  EXPECT_EQ(readerFontFamilyIndex(nullptr, "Bitter"), 0);
  EXPECT_EQ(readerFontFamilyAt(nullptr, 0), nullptr);
}

TEST(ReaderFontCatalog, BitmapUbuntuRemainsASeparateSdSelection) {
  SdCardFontRegistry registry;
  registry.families_.reserve(1);
  addFamily(registry, "Ubuntu", false);
  registry.families_[0].files.reserve(2);
  registry.families_[0].files.push_back({nullptr, 14, 0});
  registry.families_[0].files.push_back({nullptr, 18, 0});
  EXPECT_EQ(readerFontFamilyCount(&registry), 2);
  EXPECT_EQ(readerFontFamilyAt(&registry, 0), nullptr);
  ASSERT_NE(readerFontFamilyAt(&registry, 1), nullptr);
  EXPECT_FALSE(readerFontUpgradesUbuntu(*readerFontFamilyAt(&registry, 1)));
  EXPECT_EQ(readerFontFamilyIndex(&registry, "Ubuntu"), 1);
  EXPECT_EQ(readerFontFamilyIndex(&registry, ""), 0);
  EXPECT_EQ(readerFontPointSizes(&registry, ""), std::vector<uint8_t>{12});
  EXPECT_EQ(readerFontPointSizes(&registry, "Ubuntu"), (std::vector<uint8_t>{14, 18}));
}

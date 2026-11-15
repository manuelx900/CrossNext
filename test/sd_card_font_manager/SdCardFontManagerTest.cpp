#include <GfxRenderer.h>
#include <SdCardFontManager.h>
#include <SdCardFontRegistry.h>
#include <gtest/gtest.h>

TEST(SdCardFontManagerOwnership, UnloadingSelectionPreservesResidentContentFamily) {
  GfxRenderer renderer;
  SdCardFontManager content, selection;
  SdCardFontFamilyInfo noto{"Noto Serif"}, optional{"Optional"};
  ASSERT_TRUE(content.loadFamily(noto, renderer, 14, 101));
  ASSERT_EQ(content.loadFamilyExtraSize(noto, renderer, 12, 102), 102);
  ASSERT_TRUE(selection.loadFamily(optional, renderer, 16));
  const int selected = selection.getFontId("Optional");
  ASSERT_NE(selected, 0);
  ASSERT_EQ(renderer.sd.size(), 3);
  selection.unloadAll(renderer);
  EXPECT_EQ(renderer.sd.count(101), 1);
  EXPECT_EQ(renderer.sd.count(102), 1);
  EXPECT_EQ(renderer.sd.count(selected), 0);
  EXPECT_EQ(renderer.fonts.count(101), 1);
  content.unloadAll(renderer);
  EXPECT_TRUE(renderer.sd.empty());
  EXPECT_TRUE(renderer.fonts.empty());
}

TEST(SdCardFontManagerOwnership, ExplicitIdCollisionDoesNotRemoveAnotherOwner) {
  GfxRenderer renderer;
  SdCardFontManager first, second;
  SdCardFontFamilyInfo family{"Noto Serif"};
  ASSERT_TRUE(first.loadFamily(family, renderer, 14, 101));
  EXPECT_FALSE(second.loadFamily(family, renderer, 14, 101));
  second.unloadAll(renderer);
  EXPECT_EQ(renderer.sd.size(), 1);
  EXPECT_EQ(first.getFontId("Noto Serif", 14), 101);
}

TEST(SdCardFontManagerOwnership, ExtraSizeReusesItsExistingHandle) {
  GfxRenderer renderer;
  SdCardFontManager manager;
  SdCardFontFamilyInfo family{"Noto Serif"};
  ASSERT_TRUE(manager.loadFamily(family, renderer, 14, 101));
  EXPECT_EQ(manager.loadFamilyExtraSize(family, renderer, 14, 103), 101);
  EXPECT_EQ(renderer.sd.size(), 1);
}

#include "../../lib/Epub/Epub/ReaderRenderSpec.h"
TEST(ReaderResourceIdentity, MetricsReplacementInvalidatesLayoutsAndClipAnchors) {
  ReaderRenderSpec oldSpec, replacement;
  oldSpec.fontId = replacement.fontId = 103;
  oldSpec.fontContentHash = 123;
  replacement.fontContentHash = 456;
  EXPECT_NE(oldSpec.fontCacheKey(), replacement.fontCacheKey());
  EXPECT_NE(readerRenderSpecSignature(oldSpec), readerRenderSpecSignature(replacement));
  replacement.fontContentHash = oldSpec.fontContentHash;
  EXPECT_EQ(oldSpec.fontCacheKey(), replacement.fontCacheKey());
}

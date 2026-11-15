#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <map>
#include <new>

#include "FontCacheManager.h"
#include "FontDecompressor.h"
#include "SdCardFont.h"
#include "TtfEpdFont.h"

namespace {

bool countHeapAllocations = false;
size_t heapAllocationCount = 0;

// TTF fonts are PSRAM-gated and stubbed out on the host (see stubs/TtfEpdFont.h);
// every manager gets an empty map for that slot.
const std::map<int, TtfEpdFont*> kNoTtfFonts;

const SdCardFont::PrewarmCall* findCall(const SdCardFont& font, const uint8_t styleMask) {
  for (int i = 0; i < font.prewarmCallCount; i++) {
    if (font.prewarmCalls[i].styleMask == styleMask) return &font.prewarmCalls[i];
  }
  return nullptr;
}

}  // namespace

void* operator new(const size_t size) {
  if (countHeapAllocations) heapAllocationCount++;
  if (void* allocation = std::malloc(size)) return allocation;
  throw std::bad_alloc();
}

void operator delete(void* allocation) noexcept { std::free(allocation); }

void operator delete(void* allocation, size_t) noexcept { std::free(allocation); }

TEST(FontCacheManagerTest, PrewarmScopeBatchesEachFontAndResolvedStyleSeparately) {
  SdCardFont readerFont;
  SdCardFont fallbackFont;
  const std::map<int, EpdFontFamily> noBuiltinFonts;
  const std::map<int, SdCardFont*> sdFonts{{-17, &readerFont}, {23, &fallbackFont}};
  FontCacheManager manager(noBuiltinFonts, sdFonts, kNoTtfFonts);

  auto scope = manager.createPrewarmScope();
  manager.recordText("A", -17, EpdFontFamily::REGULAR);
  manager.recordText("B", -17, EpdFontFamily::BOLD);
  manager.recordText("C", 23, EpdFontFamily::REGULAR);
  scope.endScanAndPrewarm();

  ASSERT_EQ(2, readerFont.prewarmCallCount);
  const auto* regularCall = findCall(readerFont, 0x01);
  ASSERT_NE(nullptr, regularCall);
  EXPECT_STREQ("A", regularCall->text);
  EXPECT_FALSE(regularCall->accumulate);
  const auto* boldCall = findCall(readerFont, 0x02);
  ASSERT_NE(nullptr, boldCall);
  EXPECT_STREQ("B", boldCall->text);
  EXPECT_FALSE(boldCall->accumulate);

  ASSERT_EQ(1, fallbackFont.prewarmCallCount);
  EXPECT_STREQ("C", fallbackFont.prewarmCalls[0].text);
  EXPECT_EQ(0x01, fallbackFont.prewarmCalls[0].styleMask);
}

TEST(FontCacheManagerTest, PrewarmScopeMergesStylesThatResolveToTheSameSdFontData) {
  SdCardFont font;
  font.resolvedStyles[EpdFontFamily::BOLD] = EpdFontFamily::REGULAR;
  const std::map<int, EpdFontFamily> noBuiltinFonts;
  const std::map<int, SdCardFont*> sdFonts{{7, &font}};
  FontCacheManager manager(noBuiltinFonts, sdFonts, kNoTtfFonts);

  auto scope = manager.createPrewarmScope();
  manager.recordText("A", 7, EpdFontFamily::REGULAR);
  manager.recordText("B", 7, EpdFontFamily::BOLD);
  scope.endScanAndPrewarm();

  ASSERT_EQ(1, font.prewarmCallCount);
  EXPECT_STREQ("AB", font.prewarmCalls[0].text);
  EXPECT_EQ(0x01, font.prewarmCalls[0].styleMask);
}

TEST(FontCacheManagerTest, PrewarmScopeMergesBuiltInStylesThatShareFontData) {
  EpdFontData regularData{reinterpret_cast<const void*>(1)};
  EpdFontData boldData{reinterpret_cast<const void*>(1)};
  const EpdFontFamily family(&regularData, &boldData, &regularData, nullptr);
  const std::map<int, EpdFontFamily> builtinFonts{{5, family}};
  const std::map<int, SdCardFont*> noSdFonts;
  FontDecompressor decompressor;
  FontCacheManager manager(builtinFonts, noSdFonts, kNoTtfFonts);
  manager.setFontDecompressor(&decompressor);

  auto scope = manager.createPrewarmScope();
  manager.recordText("A", 5, EpdFontFamily::REGULAR);
  manager.recordText("B", 5, EpdFontFamily::BOLD);
  manager.recordText("C", 5, EpdFontFamily::ITALIC);
  scope.endScanAndPrewarm();

  ASSERT_EQ(2, decompressor.prewarmCallCount);
  const auto& first = decompressor.prewarmCalls[0];
  const auto& second = decompressor.prewarmCalls[1];
  const auto* regularCall = first.fontData == &regularData ? &first : &second;
  const auto* boldCall = first.fontData == &boldData ? &first : &second;
  EXPECT_STREQ("AC", regularCall->text);
  EXPECT_STREQ("B", boldCall->text);
}

TEST(FontCacheManagerTest, PrewarmScopePreservesUniqueMultibyteCodepoints) {
  SdCardFont font;
  const std::map<int, EpdFontFamily> noBuiltinFonts;
  const std::map<int, SdCardFont*> sdFonts{{7, &font}};
  FontCacheManager manager(noBuiltinFonts, sdFonts, kNoTtfFonts);

  auto scope = manager.createPrewarmScope();
  manager.recordText("\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80\xC3\xA9", 7, EpdFontFamily::REGULAR);
  scope.endScanAndPrewarm();

  ASSERT_EQ(1, font.prewarmCallCount);
  EXPECT_STREQ("\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80", font.prewarmCalls[0].text);
}

TEST(FontCacheManagerTest, PrewarmScanDoesNotAllocateHeapMemory) {
  SdCardFont font;
  const std::map<int, EpdFontFamily> noBuiltinFonts;
  const std::map<int, SdCardFont*> sdFonts{{7, &font}};
  FontCacheManager manager(noBuiltinFonts, sdFonts, kNoTtfFonts);

  heapAllocationCount = 0;
  countHeapAllocations = true;
  auto scope = manager.createPrewarmScope();
  manager.recordText("Repeated text: \xC3\xA9 \xE4\xB8\xAD \xF0\x9F\x98\x80", 7, EpdFontFamily::REGULAR);
  scope.endScanAndPrewarm();
  countHeapAllocations = false;

  EXPECT_EQ(0U, heapAllocationCount);
}

TEST(FontCacheManagerTest, IncrementalPrewarmRequestsAccumulation) {
  SdCardFont font;
  const std::map<int, EpdFontFamily> noBuiltinFonts;
  const std::map<int, SdCardFont*> sdFonts{{7, &font}};
  FontCacheManager manager(noBuiltinFonts, sdFonts, kNoTtfFonts);
  manager.prewarmCache(7, "title", 1);
  ASSERT_EQ(1, font.prewarmCallCount);
  EXPECT_TRUE(font.prewarmCalls[0].accumulate);
}

TEST(FontCacheManagerTest, ReaderScopeAccumulatesSdFontsButTransientScopeReplaces) {
  SdCardFont font;
  const std::map<int, EpdFontFamily> noBuiltinFonts;
  const std::map<int, SdCardFont*> sdFonts{{7, &font}};
  FontCacheManager manager(noBuiltinFonts, sdFonts, kNoTtfFonts);
  {
    auto scope = manager.createPrewarmScope(FontCacheManager::ScopeCache::Reader);
    manager.recordText("AB", 7, EpdFontFamily::BOLD);
    scope.endScanAndPrewarm();
  }
  ASSERT_EQ(1, font.prewarmCallCount);
  EXPECT_EQ(2, font.prewarmCalls[0].styleMask);
  EXPECT_TRUE(font.prewarmCalls[0].accumulate);
  {
    auto scope = manager.createPrewarmScope();
    manager.recordText("CD", 7, EpdFontFamily::REGULAR);
    scope.endScanAndPrewarm();
  }
  ASSERT_EQ(2, font.prewarmCallCount);
  EXPECT_FALSE(font.prewarmCalls[1].accumulate);
}
#ifdef FCM_TEST_VECTOR
TEST(FontCacheManagerTest, ReaderSizedFontsSurviveTransientScopesAndExplicitResetStillClears) {
  TtfEpdFont reader, transient;
  reader.readerCached = true;
  const std::map<int, EpdFontFamily> families;
  const std::map<int, SdCardFont*> sd;
  const std::map<int, TtfEpdFont*> vector{{10, &reader}, {20, &transient}};
  FontCacheManager manager(families, sd, vector);
  {
    auto scope = manager.createPrewarmScope();
    manager.recordText("Reader", 10, EpdFontFamily::REGULAR);
    scope.endScanAndPrewarm();
    EXPECT_TRUE(reader.retained);
    EXPECT_EQ(reader.clears, 0u);
    EXPECT_EQ(transient.clears, 1u);
  }
  EXPECT_EQ(reader.clears, 0u);
  manager.clearCache();
  EXPECT_EQ(reader.clears, 1u);
}

TEST(FontCacheManagerTest, SpeculativePrewarmStopsBetweenSmallGlyphBatches) {
  TtfEpdFont font;
  const std::map<int, EpdFontFamily> families;
  const std::map<int, SdCardFont*> sd;
  const std::map<int, TtfEpdFont*> vector{{10, &font}};
  FontCacheManager manager(families, sd, vector);
  unsigned calls = 0;
  {
    auto scope = manager.createPrewarmScope(FontCacheManager::ScopeCache::Reader);
    manager.recordText("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 10, EpdFontFamily::REGULAR);
    scope.endScanAndPrewarm([](void* ctx) { return ++*static_cast<unsigned*>(ctx) < 3; }, &calls);
    EXPECT_EQ(font.additions, 1u);
    EXPECT_EQ(std::strlen(font.prepared), 16u);
  }
  EXPECT_EQ(font.additions, 1u);
}

TEST(FontCacheManagerTest, ThaiClustersSurviveScanDeduplication) {
  TtfEpdFont font;
  EpdFontData data;
  const std::map<int, EpdFontFamily> families{{10, EpdFontFamily(&data, &data, &data, &data)}};
  const std::map<int, SdCardFont*> sd;
  const std::map<int, TtfEpdFont*> vector{{10, &font}};
  FontCacheManager manager(families, sd, vector);
  auto scope = manager.createPrewarmScope(FontCacheManager::ScopeCache::Reader);
  manager.recordText("กิ่กิ่", 10, EpdFontFamily::BOLD);
  scope.endScanAndPrewarm();
  EXPECT_STREQ(font.prepared, "\xee\x80\x81");
}

TEST(FontCacheManagerTest, ReaderPrewarmsOnlyRequestedVectorStylesAtEachSize) {
  TtfEpdFont body, heading;
  const std::map<int, EpdFontFamily> builtin;
  const std::map<int, SdCardFont*> sd;
  const std::map<int, TtfEpdFont*> vector{{10, &body}, {20, &heading}};
  FontCacheManager manager(builtin, sd, vector);
  auto scope = manager.createPrewarmScope(FontCacheManager::ScopeCache::Reader);
  manager.recordText("body", 10, EpdFontFamily::ITALIC);
  manager.recordText("Heading", 20, EpdFontFamily::BOLD);
  scope.endScanAndPrewarm();
  EXPECT_EQ(body.mask, 4);
  EXPECT_EQ(heading.mask, 2);
  EXPECT_TRUE(body.retained);
  EXPECT_TRUE(heading.retained);
}
#endif

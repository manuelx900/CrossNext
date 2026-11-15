#include <Epub/ParsedText.h>
#include <Epub/parsers/ParagraphInputCache.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <gtest/gtest.h>

#include <cstdio>

#include "../../src/apps_local/reader/PageReuse.h"
namespace {
class Bytes : public Print {
 public:
  std::vector<uint8_t> data;
  size_t write(uint8_t byte) override {
    data.push_back(byte);
    return 1;
  }
  size_t write(const uint8_t* bytes, size_t count) override {
    data.insert(data.end(), bytes, bytes + count);
    return count;
  }
};
std::vector<uint8_t> layout(GfxRenderer& renderer, WordWidthCache* cache, int width, bool focus = false,
                            bool ruby = false, bool hyphens = false) {
  ParsedText text(hyphens, focus);
  text.setWidthCache(cache);
  static const char* const words[] = {"lectura", "España", "a", "rápida", "漢字", "texto", "dato", "fin"};
  for (size_t n = 0; n < 160; ++n) {
    text.addWord(words[n % 8], static_cast<EpdFontFamily::Style>(n % 4), n % 11 == 0);
    if (ruby && n % 13 == 0) text.setRubyForWordAt(n, "nota");
  }
  Bytes bytes;
  size_t line = 0;
  text.layoutAndExtractLines(
      renderer, 0, uint16_t(width),
      [&](std::unique_ptr<TextBlock> block, uint32_t) {
        EXPECT_TRUE(block->serialize(bytes));
        ++line;
      },
      true, -1, 125);
  EXPECT_GT(line, 0u);
  return bytes.data;
}
std::unique_ptr<Page> page() {
  auto result = std::make_unique<Page>();
  result->elements.reserve(1);
  std::vector<std::string> words = {"misma", "página"};
  std::vector<int16_t> x = {0, 50};
  std::vector<EpdFontFamily::Style> styles(2, EpdFontFamily::REGULAR);
  auto text = std::make_unique<TextBlock>(words, x, styles, std::vector<uint8_t>{0, 0}, std::vector<uint16_t>{0, 0});
  result->elements.push_back(std::make_unique<PageLine>(std::move(text), 3, 8));
  result->addFootnote("1", "chapter.xhtml#note");
  EXPECT_TRUE(result->addLink("other.xhtml", 1, 2, 10, 10));
  return result;
}
}  // namespace
TEST(ParagraphGapReuse, KeepsExactSerializedLinesAcrossStylesWidthsAndAnnotations) {
  for (int width : {100, 180, 320, 480})
    for (bool focus : {false, true})
      for (bool ruby : {false, true}) {
        GfxRenderer direct, memo;
        direct.boldGlyphAdvance = memo.boldGlyphAdvance = 10;
        direct.boundaryVariation = memo.boundaryVariation = 1;
        WordWidthCache cache;
        const auto before = layout(direct, nullptr, width, focus, ruby);
        const auto after = layout(memo, &cache, width, focus, ruby);
        EXPECT_EQ(before, after);
        EXPECT_LT(memo.boundaryMeasurements, direct.boundaryMeasurements);
      }
}
TEST(ParagraphGapReuse, ReportsReducedBoundaryQueriesWithTheSameLines) {
  GfxRenderer direct, memo;
  WordWidthCache cache;
  const auto before = layout(direct, nullptr, 320);
  const auto after = layout(memo, &cache, 320);
  EXPECT_EQ(before, after);
  EXPECT_LT(memo.boundaryMeasurements * 2, direct.boundaryMeasurements);
  std::printf("Paragraph boundaries: %zu -> %zu queries, identical serialized lines\n", direct.boundaryMeasurements,
              memo.boundaryMeasurements);
}
TEST(ParagraphGapReuse, UnstableSdMetricsKeepTheDirectPath) {
  GfxRenderer direct, memo;
  direct.sdFont = memo.sdFont = true;
  WordWidthCache cache;
  EXPECT_EQ(layout(direct, nullptr, 320), layout(memo, &cache, 320));
  EXPECT_EQ(memo.boundaryMeasurements, direct.boundaryMeasurements);
}
TEST(ParagraphGapReuse, NoPsramAndAllocationFailureKeepTheDirectPath) {
  for (bool absent : {false, true}) {
    GfxRenderer direct, memo;
    WordWidthCache cache;
    const auto before = layout(direct, nullptr, 180, true, true);
    HalMemory::absent = absent;
    HalMemory::fail = !absent;
    const auto after = layout(memo, &cache, 180, true, true);
    HalMemory::fail = HalMemory::absent = false;
    EXPECT_EQ(before, after);
    EXPECT_EQ(memo.boundaryMeasurements, direct.boundaryMeasurements);
  }
}
TEST(ParagraphGapReuse, ScratchAndWordIdentitiesAreResetBetweenLayouts) {
  GfxRenderer direct, memo;
  WordWidthCache cache;
  for (uint32_t epoch : {1, 2, 3}) {
    direct.metricsEpoch = memo.metricsEpoch = epoch;
    direct.boundaryVariation = memo.boundaryVariation = int(epoch);
    EXPECT_EQ(layout(direct, nullptr, 260), layout(memo, &cache, 260));
  }
  EXPECT_EQ(cache.resetParagraphGaps(4097), nullptr);
}
TEST(CurrentPageReuse, PreservesBytesNotesLinksAndObjectIdentity) {
  readerpage::Reuse cache;
  readerpage::Key key;
  key.section = &cache;
  key.spine = 0;
  key.page = 1;
  key.resource = key.media = 1;
  auto original = page();
  auto* identity = original.get();
  Bytes before;
  ASSERT_TRUE(original->serialize(before));
  ASSERT_TRUE(cache.keep(original, key, 24 * 1024));
  EXPECT_FALSE(original);
  auto same = cache.take(key);
  ASSERT_EQ(same.get(), identity);
  Bytes after;
  ASSERT_TRUE(same->serialize(after));
  EXPECT_EQ(before.data, after.data);
  EXPECT_EQ(same->footnotes.size(), 1u);
  EXPECT_EQ(same->links.size(), 1u);
  EXPECT_EQ(cache.retainedBytes(), 0u);
}
TEST(CurrentPageReuse, RejectsEveryChangedKeyAndZeroRevision) {
  for (int field = 0; field < 7; ++field) {
    readerpage::Reuse cache;
    readerpage::Key key;
    key.section = &cache;
    key.spine = 0;
    key.page = 1;
    key.resource = key.media = 1;
    auto value = page();
    ASSERT_TRUE(cache.keep(value, key, 0));
    auto changed = key;
    switch (field) {
      case 0:
        changed.resource = 2;
        break;
      case 1:
        changed.media = 2;
        break;
      case 2:
        changed.spine = 1;
        break;
      case 3:
        changed.page = 2;
        break;
      case 4:
        changed.spec.paragraphIndentSpaces = 3;
        break;
      case 5:
        changed.spec.lineCompression = 1.2001f;
        break;
      case 6:
        changed.resource = 0;
        break;
    }
    EXPECT_FALSE(cache.take(changed));
    EXPECT_EQ(cache.retainedBytes(), 0u);
  }
}
TEST(CurrentPageReuse, BudgetFailureDoesNotConsumeTheOrdinaryPage) {
  readerpage::Reuse cache;
  readerpage::Key key;
  key.section = &cache;
  key.spine = 0;
  key.page = 1;
  key.resource = key.media = 1;
  auto value = page();
  EXPECT_FALSE(cache.keep(value, key, readerpage::Reuse::Budget));
  EXPECT_TRUE(value);
  EXPECT_FALSE(cache.keep(value, key, readerpage::Reuse::Budget + 1));
  EXPECT_TRUE(value);
}

TEST(ParagraphInput, ExactTokenRoundTripAndRejectsPartialOrAnnotatedInputs) {
  ParsedText source(false, true);
  source.addWord("España", EpdFontFamily::BOLD, false, false, 101);
  source.addWord("漢字", EpdFontFamily::ITALIC, false, false, 108);
  std::vector<uint8_t> bytes(4096);
  const auto count = source.writeInput(bytes.data(), bytes.size(), 100);
  ASSERT_GT(count, 0u);
  ParsedText restored(false, true);
  EXPECT_EQ(restored.restoreInput(bytes.data(), count, 500, 20), ParsedText::InputRestore::Restored);
  std::vector<uint8_t> after(4096);
  EXPECT_EQ(restored.writeInput(after.data(), after.size(), 500), count);
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.begin() + count, after.begin()));
  for (size_t n = 0; n < count; ++n) {
    ParsedText truncated;
    EXPECT_EQ(truncated.restoreInput(bytes.data(), n, 100, 20), ParsedText::InputRestore::Invalid);
    EXPECT_TRUE(truncated.isEmpty());
  }
  ParsedText consumed;
  consumed.suppressFirstLineIndent();
  EXPECT_EQ(consumed.restoreInput(bytes.data(), count, 100, 20), ParsedText::InputRestore::Invalid);
  source.setRubyForWordAt(0, "nota");
  EXPECT_EQ(source.writeInput(after.data(), after.size(), 100), 0u);
}
TEST(ParagraphInput, LeaseAndAllocationFailureKeepNormalParsingAvailable) {
  ParagraphInputCache::retainReader(false);
  HalMemory::fail = true;
  ParagraphInputCache::retainReader(true);
  ParagraphInputCache unavailable;
  EXPECT_FALSE(unavailable.acquire());
  HalMemory::fail = false;
  ParagraphInputCache::retainReader(false);
  ParagraphInputCache::retainReader(true);
  ParagraphInputCache owner, other;
  EXPECT_TRUE(owner.acquire());
  EXPECT_FALSE(other.acquire());
  ParagraphInputCache::retainReader(false);
  owner.release();
  EXPECT_FALSE(other.acquire());
}

#ifdef READER_REAL_HYPHENATOR
TEST(HyphenReuse, KeepsExactLinesForLanguageFocusAndAllStyleBits) {
  uint32_t prefixHits = 0, breakHits = 0;
  for (const auto* language : {"es", "en", "unknown"}) {
    Hyphenator::setPreferredLanguage(language);
    for (bool focus : {false, true})
      for (int width : {80, 130, 180}) {
        WordWidthCache cache;
        auto compose = [&](WordWidthCache* memo) {
          GfxRenderer renderer;
          renderer.boldGlyphAdvance = 11;
          ParsedText text(true, focus);
          text.setWidthCache(memo);
          static constexpr uint8_t styles[] = {0, 1, 2, 3, 4, 8, 16, 32, 64, 128, 255};
          for (size_t n = 0; n < 88; ++n)
            text.addWord("extraordinariamente", static_cast<EpdFontFamily::Style>(styles[n % std::size(styles)]));
          Bytes bytes;
          text.layoutAndExtractLines(
              renderer, 0, uint16_t(width),
              [&](std::unique_ptr<TextBlock> line, uint32_t) { EXPECT_TRUE(line->serialize(bytes)); }, true, -1, 125);
          return bytes.data;
        };
        EXPECT_EQ(compose(nullptr), compose(&cache));
        EXPECT_GT(cache.breakHitCount(), 0u);
        prefixHits += cache.prefixHitCount();
        breakHits += cache.breakHitCount();
      }
  }
  EXPECT_GT(prefixHits, 0u);
  std::printf("Hyphen reuse: %u break hits, %u exact prefix hits across 18 layouts\n", breakHits, prefixHits);
  WordWidthCache cache;
  std::vector<Hyphenator::BreakInfo> values;
  Hyphenator::setPreferredLanguage("es");
  EXPECT_FALSE(cache.getBreaks("extraordinariamente", false, values));
  cache.putBreaks("extraordinariamente", false, Hyphenator::breakOffsets("extraordinariamente", false));
  EXPECT_TRUE(cache.getBreaks("extraordinariamente", false, values));
  EXPECT_FALSE(cache.getBreaks("extraordinariamente", true, values));
  Hyphenator::setPreferredLanguage("en");
  EXPECT_FALSE(cache.getBreaks("extraordinariamente", false, values));
  Hyphenator::setPreferredLanguage("es");
  WordWidthCache::Key plain{1, 0, 0, 0, 0, 100, 0}, caps = plain;
  caps.style = 128;
  uint16_t width = 0;
  EXPECT_FALSE(cache.getPrefix("prefix", plain, false, width));
  cache.putPrefix("prefix", plain, false, 40);
  EXPECT_FALSE(cache.getPrefix("prefix", caps, false, width));
  EXPECT_FALSE(cache.getPrefix("prefix", plain, true, width));
  EXPECT_TRUE(cache.getPrefix("prefix", plain, false, width));
  EXPECT_EQ(width, 40);
}
#endif

TEST(ParagraphInput, ReplaysDeferredCallbackBoundariesOnMismatchAndCancellation) {
  ParagraphInputCache::retainReader(true);
  ParagraphInputCache memo;
  ASSERT_TRUE(memo.acquire());
  ParsedText source;
  source.addWord("alpha", EpdFontFamily::REGULAR, false, false, 0);
  source.addWord("beta", EpdFontFamily::REGULAR, false, false, 6);
  std::vector<std::string> replayed;
  const auto process = [](void* context, const char* bytes, int length) {
    static_cast<std::vector<std::string>*>(context)->emplace_back(bytes, size_t(length));
  };
  memo.start(source.getInputBlockStyle(), 0, 0);
  EXPECT_FALSE(memo.text("alpha ", 6, process, &replayed));
  EXPECT_FALSE(memo.text("beta", 4, process, &replayed));
  memo.capture(source, 10);
  memo.start(source.getInputBlockStyle(), 0, 100);
  EXPECT_TRUE(memo.text("alpha ", 6, process, &replayed));
  EXPECT_TRUE(replayed.empty());
  EXPECT_FALSE(memo.text("different", 9, process, &replayed));
  EXPECT_EQ(replayed, std::vector<std::string>{"alpha "});
  replayed.clear();
  memo.start(source.getInputBlockStyle(), 0, 100);
  EXPECT_TRUE(memo.text("alpha ", 6, process, &replayed));
  EXPECT_TRUE(memo.text("beta", 4, process, &replayed));
  memo.cancel(process, &replayed);
  EXPECT_EQ(replayed, (std::vector<std::string>{"alpha ", "beta"}));
  memo.release();
  ParagraphInputCache::retainReader(false);
  HalMemory::absent = true;
  ParagraphInputCache::retainReader(true);
  EXPECT_FALSE(memo.acquire());
  HalMemory::absent = false;
  ParagraphInputCache::retainReader(false);
}

TEST(ReaderMetricSession, ReusesExactMetricsAcrossParsersAndExcludesConcurrentBorrowers) {
  const int initial = HalMemory::active;
  WordWidthCacheLease::retainReader(true);
  GfxRenderer renderer;
  std::vector<uint8_t> first;
  uint32_t hits = 0;
  {
    WordWidthCacheLease a;
    a.acquire();
    first = layout(renderer, a.cache(), 320);
    hits = a.cache()->hitCount();
    WordWidthCacheLease nested;
    nested.acquire();
    EXPECT_NE(a.cache(), nested.cache());
  }
  {
    WordWidthCacheLease b;
    b.acquire();
    EXPECT_EQ(hits, b.cache()->hitCount());
    EXPECT_EQ(first, layout(renderer, b.cache(), 320));
    EXPECT_GT(b.cache()->hitCount(), hits);
    WordWidthCache::Key old{1, 0, 0, 0, 0, 100, 0}, changed = old;
    uint16_t value;
    b.cache()->get("test", old, value);
    b.cache()->put("test", old, 42);
    changed.font = 2;
    EXPECT_FALSE(b.cache()->get("test", changed, value));
    WordWidthCacheLease::retainReader(false);
  }
  EXPECT_EQ(HalMemory::active, initial);
}

TEST(ParagraphInput, ChapterChangeReleasesAdmissionCapacityWithoutGlobalReset) {
  ParagraphInputCache::retainReader(true);
  ParagraphInputCache cache;
  ASSERT_TRUE(cache.acquire());
  cache.chapter("chapter-one.xhtml");
  ParsedText text;
  text.addWord("original", EpdFontFamily::REGULAR, false, false, 0);
  cache.start(text.getInputBlockStyle(), 0, 0);
  EXPECT_FALSE(cache.text("original", 8, nullptr, nullptr));
  cache.capture(text, 8);
  cache.start(text.getInputBlockStyle(), 0, 0);
  EXPECT_TRUE(cache.text("original", 8, nullptr, nullptr));
  cache.cancel(nullptr, nullptr);
  cache.chapter("chapter-two.xhtml");
  cache.start(text.getInputBlockStyle(), 0, 0);
  EXPECT_FALSE(cache.text("original", 8, nullptr, nullptr));
  cache.cancel(nullptr, nullptr);
  cache.release();
  ParagraphInputCache::retainReader(false);
}
TEST(PageMemoryInput, PreservesPageLinesLinksFootnotesAndRejectsTruncation) {
  auto source = page();
  Bytes encoded;
  ASSERT_TRUE(source->serialize(encoded));
  serialization::ReadBuffer input(encoded.data.data(), encoded.data.size());
  auto restored = Page::deserialize(input);
  ASSERT_TRUE(restored);
  Bytes roundtrip;
  ASSERT_TRUE(restored->serialize(roundtrip));
  EXPECT_EQ(encoded.data, roundtrip.data);
  EXPECT_TRUE(input.finish());
  for (size_t count = 0; count < encoded.data.size(); ++count) {
    serialization::ReadBuffer truncated(encoded.data.data(), count);
    EXPECT_FALSE(Page::deserialize(truncated));
  }
}

#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/ReaderRenderSpec.h>
#include <Epub/parsers/ChapterHtmlSlimParser.h>
#include <Epub/parsers/ClassicChapterParser.h>
#include <GfxRenderer.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace {
template <typename Parser>
std::vector<std::unique_ptr<Page>> parse(GfxRenderer& renderer, const char* html, const char* eventPath = "") {
  const std::string path = "/tmp/engine-choice.xhtml";
  {
    std::ofstream file(path);
    file << html;
  }
  std::vector<std::unique_ptr<Page>> pages;
  pages.reserve(8);
  const std::string base;
  CssParser css("/tmp");
  Parser parser(
      nullptr, path, renderer, 0, 1.2f, false, 0, 400, 640, false, false,
      [&pages](std::unique_ptr<Page> page, auto, auto, auto) { pages.push_back(std::move(page)); }, true, base, base, 2,
      {}, nullptr, &css, eventPath);
  EXPECT_TRUE(parser.parseAndBuildPages());
  std::filesystem::remove(path);
  return pages;
}

bool hasScaledText(const std::vector<std::unique_ptr<Page>>& pages) {
  for (const auto& page : pages)
    for (const auto& element : page->elements)
      if (element->getTag() == TAG_PageLine) {
        const auto* block = static_cast<const PageLine*>(element.get())->getBlock();
        for (size_t n = 0; n < block->wordCount(); ++n)
          if (block->wordFontId(n, 0) != 0) return true;
      }
  return false;
}
}  // namespace

TEST(EpubEngine, SavedSettingsDefaultToNewAndRejectUnknownValues) {
  EXPECT_EQ(epubEngineFromSetting(0), EpubEngine::Modern);
  EXPECT_EQ(epubEngineFromSetting(1), EpubEngine::Classic);
  EXPECT_EQ(epubEngineFromSetting(2), EpubEngine::Modern);
  EXPECT_EQ(epubEngineFromSetting(255), EpubEngine::Modern);
}

TEST(EpubEngine, LayoutAndClippingIdentityCannotCrossEngines) {
  ReaderRenderSpec modern;
  modern.fontId = 42;
  modern.viewportWidth = 480;
  modern.viewportHeight = 720;
  auto classic = modern;
  classic.engine = EpubEngine::Classic;
  EXPECT_NE(modern.fontCacheKey(), classic.fontCacheKey());
  EXPECT_NE(readerRenderSpecSignature(modern), readerRenderSpecSignature(classic));
  modern.fontContentHash = classic.fontContentHash = 0x12345678;
  EXPECT_NE(modern.fontCacheKey(), classic.fontCacheKey());
  EXPECT_NE(readerRenderSpecSignature(modern), readerRenderSpecSignature(classic));
}

TEST(EpubEngine, ClassicAndModernBothProduceReadablePages) {
  GfxRenderer renderer;
  const char* html = "<html><body><h1>Heading</h1><p>A readable book with a simple paragraph.</p></body></html>";
  auto modern = parse<ChapterHtmlSlimParser>(renderer, html);
  auto classic = parse<classic_epub::ClassicChapterParser>(renderer, html);
  ASSERT_FALSE(modern.empty());
  ASSERT_FALSE(classic.empty());
  EXPECT_FALSE(modern.front()->elements.empty());
  EXPECT_FALSE(classic.front()->elements.empty());
}

TEST(EpubEngine, ClassicRetainsUniformTextWhileModernHonorsInlineSizes) {
  GfxRenderer renderer;
  renderer.setFontVariantProvider({&renderer, [](void* ctx, int fontId, float scale) {
                                     const int variant = fontId + static_cast<int>(scale * 100);
                                     static_cast<GfxRenderer*>(ctx)->fontMap.emplace(variant,
                                                                                     EpdFontFamily(12, nullptr));
                                     return variant;
                                   }});
  const char* html = "<html><body><p>Normal <span style=\"font-size: 2em\">Large</span> normal.</p></body></html>";
  EXPECT_TRUE(hasScaledText(parse<ChapterHtmlSlimParser>(renderer, html)));
  EXPECT_FALSE(hasScaledText(parse<classic_epub::ClassicChapterParser>(renderer, html)));
}

TEST(EpubEngine, ClassicCannotPublishTableEventsThatChangeModernColumnPlanning) {
  GfxRenderer renderer;
  const char* path = "/tmp/engine-table.evc";
  std::filesystem::remove(path);
  const char* html =
      "<html><body><table><tr><td>ID</td><td>Description with many words</td></tr>"
      "<tr><td>1</td><td>A long explanation that needs the wider column.</td></tr>"
      "</table></body></html>";
  auto classic = parse<classic_epub::ClassicChapterParser>(renderer, html, path);
  ASSERT_FALSE(classic.empty());
  EXPECT_FALSE(std::filesystem::exists(path));
  auto modern = parse<ChapterHtmlSlimParser>(renderer, html, path);
  auto fresh = parse<ChapterHtmlSlimParser>(renderer, html);
  ASSERT_EQ(modern.size(), fresh.size());
  EXPECT_FALSE(std::filesystem::exists(path));
  for (size_t p = 0; p < fresh.size(); ++p) {
    ASSERT_EQ(modern[p]->elements.size(), fresh[p]->elements.size());
    for (size_t e = 0; e < fresh[p]->elements.size(); ++e) {
      EXPECT_EQ(modern[p]->elements[e]->xPos, fresh[p]->elements[e]->xPos);
      EXPECT_EQ(modern[p]->elements[e]->yPos, fresh[p]->elements[e]->yPos);
    }
  }
}

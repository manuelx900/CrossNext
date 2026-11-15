#include <HalStorage.h>
#include <SdCardFont.h>
#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>

TEST(NotoSerifResources, EverySizeLoadsAllStylesAndAccentedText) {
  for (const unsigned size : {12, 14, 16, 18}) {
    const std::string path =
        "assets_local/reader-fonts/sd-card/fonts/Noto Serif/Noto Serif_" + std::to_string(size) + ".cpfont";
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(input.is_open()) << path;
    const auto bytes = input.tellg();
    ASSERT_GT(bytes, 0);
    sdFontTestFile.clear();
    sdFontTestFile.reserve(static_cast<size_t>(bytes));
    input.seekg(0);
    sdFontTestFile.assign(std::istreambuf_iterator<char>(input), {});
    SdCardFont font;
    ASSERT_TRUE(font.load(path.c_str()));
    ASSERT_EQ(font.styleCount(), 4);
    for (const char* text : {"AV", "To", "fi", "office"}) {
      ASSERT_EQ(font.prewarm(text, 1, false, true), 0);
      int cold = 0, warm = 0, height = 0;
      font.getEpdFont(0)->getTextDimensions(text, &cold, &height);
      const size_t reads = sdFontTestReads;
      ASSERT_EQ(font.prewarm(text, 1, false, true), 0);
      font.getEpdFont(0)->getTextDimensions(text, &warm, &height);
      EXPECT_EQ(cold, warm);
      EXPECT_EQ(reads, sdFontTestReads);
    }
    ASSERT_EQ(font.prewarm("Español: áéíóú ü ñ ¿¡ English AV fi Ω ɪ Привет café ⁰₁₂ 0123456789", 0x0f, false, true), 0);
    for (uint8_t style = 0; style < 4; ++style) {
      const auto* face = font.getEpdFont(style);
      ASSERT_NE(face, nullptr);
      for (const uint32_t cp : {0x41u, 0xe1u, 0xe9u, 0xf1u, 0x3a9u, 0x26au, 0x301u, 0x41fu, 0x44fu, 0x2070u, 0x2082u}) {
        const auto* glyph = face->getGlyph(cp);
        ASSERT_NE(glyph, nullptr) << size << " style=" << unsigned(style) << " cp=" << cp;
        EXPECT_GT(glyph->width, 0);
        EXPECT_GT(glyph->height, 0);
      }
    }
    // Preserve the proportional numeral metrics used by the flash reader faces.
    EXPECT_NE(font.getEpdFont(0)->getGlyph('0')->advanceX, font.getEpdFont(0)->getGlyph('1')->advanceX);
    // A second use of the same glyph set should reuse the existing page cache.
    const size_t reads = sdFontTestReads;
    ASSERT_EQ(font.prewarm("Español: áéíóú ü ñ ¿¡ English AV fi Ω ɪ Привет café ⁰₁₂ 0123456789", 0x0f, false, true), 0);
    EXPECT_EQ(sdFontTestReads, reads);
    const size_t toc = 32 + 3 * 32;
    const auto read32 = [](size_t off) {
      return uint32_t(sdFontTestFile[off]) | uint32_t(sdFontTestFile[off + 1]) << 8 |
             uint32_t(sdFontTestFile[off + 2]) << 16 | uint32_t(sdFontTestFile[off + 3]) << 24;
    };
    sdFontTestFile.resize(read32(toc + 24) + read32(toc + 4) * 12);
    SdCardFont truncated;
    EXPECT_FALSE(truncated.load(path.c_str()));
  }
}

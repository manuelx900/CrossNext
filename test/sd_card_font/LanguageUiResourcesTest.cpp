#include <HalStorage.h>
#include <SdCardFont.h>
#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>

TEST(LanguageUiResources, AllFacesReuseMultilingualLabelsWithoutStorageReads) {
  for (const char* variant : {"", "_SC", "_TC"}) {
    for (unsigned size : {8, 10, 12}) {
      const auto path =
          std::string("assets_local/language-fonts/Interface") + variant + "_" + std::to_string(size) + ".cpfont";
      std::ifstream input(path, std::ios::binary);
      ASSERT_TRUE(input.is_open()) << path;
      sdFontTestFile.assign(std::istreambuf_iterator<char>(input), {});
      SdCardFont font;
      ASSERT_TRUE(font.load(path.c_str()));
      for (const char* text : {"Bibliothèque", "Настройки", "目標を設定", "设置", "設定", "ตั้งค่า", "القراءة", "קריאה"}) {
        ASSERT_EQ(0, font.prewarm(text, 3, false, true, true)) << path << " " << text;
        int expected = 0, height = 0;
        font.getEpdFont()->getTextDimensions(text, &expected, &height);
        ASSERT_GT(expected, 0);
        sdFontTestReads = 0;
        for (unsigned draw = 0; draw < 20; ++draw) {
          ASSERT_EQ(0, font.prewarm(text, 3, false, true, true));
          int width = 0;
          font.getEpdFont()->getTextDimensions(text, &width, &height);
          EXPECT_EQ(expected, width);
        }
        EXPECT_EQ(0U, sdFontTestReads) << path << " " << text;
      }
    }
  }
}

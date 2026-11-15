#include <GfxRenderer.h>
#include <GifToFramebufferConverter.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

namespace {
class GifFramebufferTest : public ::testing::Test {
 protected:
  std::string path = (std::filesystem::temp_directory_path() / "crossnext-gif-frame.gif").string();
  GifToFramebufferConverter decoder;
  void SetUp() override {
    const std::string hex =
        "47494638396108000400810000000000555555aaaaaaffffff21f90401000003002c000000000800040000081500010410304020c181"
        "051102206830614286030302003b";
    std::ofstream file(path, std::ios::binary);
    for (size_t i = 0; i < hex.size(); i += 2) file.put(static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  }
  void TearDown() override { std::filesystem::remove(path); }
  static bool ink(const GfxRenderer& renderer, int x, int y) {
    const int pos = y * 64 + x;
    return (renderer.frame[pos / 8] & (0x80 >> (pos % 8))) == 0;
  }
};

TEST_F(GifFramebufferTest, RendersSourceAndClipsNegativeDestination) {
  GfxRenderer full, clipped;
  full.frame.fill(0xff);
  clipped.frame.fill(0xff);
  RenderConfig cfg{0, 0, 8, 4};
  cfg.useDithering = false;
  ASSERT_TRUE(decoder.decodeToFramebuffer(path, full, cfg));
  cfg.x = -2;
  cfg.y = -1;
  ASSERT_TRUE(decoder.decodeToFramebuffer(path, clipped, cfg));
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 6; ++x) EXPECT_EQ(ink(clipped, x, y), ink(full, x + 2, y + 1));
  EXPECT_TRUE(std::any_of(full.frame.begin(), full.frame.end(), [](uint8_t b) { return b != 0xff; }));
}

TEST_F(GifFramebufferTest, CropsTheRequestedSourceBeforeScaling) {
  GfxRenderer full, cropped;
  full.frame.fill(0xff);
  cropped.frame.fill(0xff);
  RenderConfig cfg{0, 0, 8, 4};
  cfg.useDithering = false;
  ASSERT_TRUE(decoder.decodeToFramebuffer(path, full, cfg));
  cfg.maxWidth = 4;
  cfg.sourceCropX = 0.25f;
  ASSERT_TRUE(decoder.decodeToFramebuffer(path, cropped, cfg));
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) EXPECT_EQ(ink(cropped, x, y), ink(full, x + 2, y));
}

TEST_F(GifFramebufferTest, BoundsAndAllFourOrientations) {
  for (int direction = 0; direction < 4; ++direction) {
    GfxRenderer renderer;
    renderer.orientation = static_cast<GfxRenderer::Orientation>(direction);
    renderer.frame.fill(0xff);
    RenderConfig cfg{renderer.getScreenWidth() - 2, renderer.getScreenHeight() - 2, 8, 4};
    cfg.useDithering = false;
    ASSERT_TRUE(decoder.decodeToFramebuffer(path, renderer, cfg));
  }
  GfxRenderer renderer;
  RenderConfig cfg{0, 0, 1000000, 1000000};
  EXPECT_FALSE(decoder.decodeToFramebuffer(path, renderer, cfg));
  cfg.maxWidth = 8;
  cfg.maxHeight = 4;
  cfg.sourceCropX = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(decoder.decodeToFramebuffer(path, renderer, cfg));
}

TEST_F(GifFramebufferTest, CacheOnlyKeepsProgressScreenAndMatchesRenderedPixels) {
  for (int direction = 0; direction < 4; ++direction) {
    GfxRenderer normal, preparation;
    normal.orientation = preparation.orientation = static_cast<GfxRenderer::Orientation>(direction);
    preparation.frame.fill(0x5a);
    const auto before = preparation.frame;
    const auto first = path + ".normal.pxc", second = path + ".prepared.pxc";
    RenderConfig cfg{0, 0, 8, 4};
    cfg.cachePath = first;
    ASSERT_TRUE(decoder.decodeToFramebuffer(path, normal, cfg));
    cfg.cacheOnly = true;
    cfg.cachePath = second;
    ASSERT_TRUE(decoder.decodeToFramebuffer(path, preparation, cfg));
    EXPECT_EQ(before, preparation.frame);
    const auto bytes = [](const std::string& p) {
      std::ifstream file(p, std::ios::binary);
      return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const auto rendered = bytes(first), prepared = bytes(second);
    EXPECT_EQ(rendered.size(), 12U);
    EXPECT_EQ(rendered, prepared);
    std::filesystem::remove(first);
    std::filesystem::remove(second);
  }
}
}  // namespace

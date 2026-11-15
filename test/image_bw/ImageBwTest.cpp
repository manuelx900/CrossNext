#include <Epub/converters/DirectPixelWriter.h>
#include <gtest/gtest.h>

#include <algorithm>

TEST(ImageBw, DithersAllFourTonesIntoDifferentInkCoverage) {
  for (const auto tone : {0, 1, 2, 3}) {
    GfxRenderer renderer;
    renderer.frame.fill(0xff);
    renderer.dither = true;
    DirectPixelWriter writer;
    writer.init(renderer);
    for (int y = 0; y < 2; ++y) {
      writer.beginRow(y);
      for (int x = 0; x < 2; ++x) writer.writePixel(x, tone);
    }
    int black = 0;
    for (int y = 0; y < 2; ++y)
      for (int x = 0; x < 2; ++x) black += !(renderer.frame[y * 8] & (0x80 >> x));
    static constexpr int coverage[] = {4, 3, 1, 0};
    EXPECT_EQ(black, coverage[tone]);
  }
}
TEST(ImageBw, TextGrayscalePassLeavesDitheredImagesUntouched) {
  for (auto mode : {GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
    GfxRenderer renderer;
    renderer.dither = true;
    renderer.mode = mode;
    renderer.frame.fill(0xa5);
    DirectPixelWriter writer;
    writer.init(renderer);
    writer.beginRow(0);
    for (int x = 0; x < 64; ++x) writer.writePixel(x, x % 4, true);
    EXPECT_TRUE(std::all_of(renderer.frame.begin(), renderer.frame.end(), [](auto b) { return b == 0xa5; }));
  }
}
TEST(ImageBw, OrdinaryImagesKeepExistingBasePixels) {
  GfxRenderer renderer;
  renderer.frame.fill(0xff);
  DirectPixelWriter writer;
  writer.init(renderer);
  writer.beginRow(0);
  for (int x = 0; x < 4; ++x) writer.writePixel(x, x);
  EXPECT_EQ(renderer.frame[0], 0x1f);
}
TEST(ImageBw, DitherSupportsAllOrientations) {
  for (auto orientation : {GfxRenderer::Portrait, GfxRenderer::LandscapeClockwise, GfxRenderer::PortraitInverted,
                           GfxRenderer::LandscapeCounterClockwise}) {
    GfxRenderer renderer;
    renderer.orientation = orientation;
    renderer.dither = true;
    renderer.frame.fill(0xff);
    DirectPixelWriter writer;
    writer.init(renderer);
    for (int y = 0; y < renderer.getScreenHeight(); ++y) {
      writer.beginRow(y);
      for (int x = 0; x < renderer.getScreenWidth(); ++x) writer.writePixel(x, 0);
    }
    EXPECT_TRUE(std::all_of(renderer.frame.begin(), renderer.frame.end(), [](auto b) { return b == 0; }));
  }
}

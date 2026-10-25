#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/apps_local/study/StudyImageView.h"

namespace {
int checks = 0, failures = 0;
void check(bool value) {
  ++checks;
  if (!value) ++failures;
}
struct Source final : study::ByteSource {
  std::vector<uint8_t> data;
  int reads = 0;
  bool fail = false;
  uint32_t size() const override { return data.size(); }
  bool read(uint32_t at, void* out, uint32_t count) override {
    ++reads;
    if (fail || uint64_t(at) + count > data.size()) return false;
    std::memcpy(out, data.data() + at, count);
    return true;
  }
};
struct Target {
  mutable std::array<uint8_t, 120 * 90> pixels{};
  void drawPixel(int x, int y, bool black) const {
    check(x >= 10 && x < 110 && y >= 10 && y < 80);
    if (x >= 0 && x < 120 && y >= 0 && y < 90) pixels[y * 120 + x] = black;
  }
};
}  // namespace

int main() {
  Source source;
  source.data.resize(26 + 28 * 301);
  std::memcpy(source.data.data(), "XSTUDYI", 7);
  source.data[8] = 1;
  source.data[12] = 1;
  const uint32_t offset = 26;
  const uint16_t width = 223, height = 301, stride = 28;
  std::memcpy(source.data.data() + 16, &offset, 4);
  std::memcpy(source.data.data() + 20, &width, 2);
  std::memcpy(source.data.data() + 22, &height, 2);
  std::memcpy(source.data.data() + 24, &stride, 2);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      if ((x * 3 + y * 7) % 13 < 4) source.data[offset + y * stride + x / 8] |= 0x80 >> (x % 8);
  study::StudyImages images;
  check(images.open(source));
  const auto image = images.at(source, 0);
  check(image.valid());
  uint8_t band[study::kImageBandBytes];
  for (int rotation = 0; rotation < 4; ++rotation) {
    for (int zoom = 0; zoom < 4; ++zoom) {
      for (int pan : {-100000, 0, 100000}) {
        study::ImageView view;
        view.turns = rotation;
        view.zoom = zoom;
        view.panX = pan;
        view.panY = -pan;
        auto placement = study::placeImage(image, 10, 10, 100, 70, view);
        if (placement.width > 100) check(placement.left <= 10 && placement.left + placement.width >= 110);
        if (placement.height > 70) check(placement.top <= 10 && placement.top + placement.height >= 80);
        Target target;
        source.reads = 0;
        check(study::drawImageView(target, images, source, image, 10, 10, 100, 70, view, band));
        check(source.reads <= (height + 15) / 16);
        for (int y = 10; y < 80; ++y)
          for (int x = 10; x < 110; ++x) {
            int sx = 0, sy = 0;
            const bool expected = placement.sourceAt(x, y, image, sx, sy) &&
                                  (source.data[offset + sy * stride + sx / 8] & (0x80 >> (sx % 8)));
            check(bool(target.pixels[y * 120 + x]) == expected);
          }
      }
    }
  }
  study::ImageView view;
  view.magnify(1);
  check(view.zoom == 1);
  view.rotate();
  check(view.turns == 3 && view.zoom == 1);
  view.fit();
  check(view.turns == 3 && view.zoom == 0 && !view.panX && !view.panY);
  view.rotate();
  check(view.turns == 0);
  view.reset();
  check(!view.turns && !view.zoom);
  view.rotate(true);
  check(view.turns == 1);
  view.rotate(true);
  check(view.turns == 0);

  // Physical arrow directions for upright, right, inverted and left images.
  constexpr study::ImageDirection expected[4][4] = {
      {{-1, 0}, {0, -1}, {0, 1}, {1, 0}},
      {{0, -1}, {1, 0}, {-1, 0}, {0, 1}},
      {{1, 0}, {0, 1}, {0, -1}, {-1, 0}},
      {{0, 1}, {-1, 0}, {1, 0}, {0, -1}},
  };
  for (int turns = 0; turns < 4; ++turns) {
    for (int step = 0; step < 4; ++step) {
      const auto direction = study::imageStepDirection(step, turns);
      check(direction.x == expected[turns][step].x && direction.y == expected[turns][step].y);
      study::ImageView moved;
      moved.turns = turns;
      moved.zoom = 3;
      check(study::panImageThird(image, 100, 70, moved, direction.x, direction.y));
      check((moved.panX < 0) == (expected[turns][step].x > 0));
      check((moved.panX > 0) == (expected[turns][step].x < 0));
      check((moved.panY < 0) == (expected[turns][step].y > 0));
      check((moved.panY > 0) == (expected[turns][step].y < 0));
      for (int repeat = 0; repeat < 4; ++repeat) study::panImageThird(image, 100, 70, moved, direction.x, direction.y);
      check(!study::panImageThird(image, 100, 70, moved, direction.x, direction.y));
    }
  }
  check(study::imageStepDirection(-1, 3).x == 0);
  check(study::imageStepDirection(4, 1).y == 0);

  Source hd;
  const uint32_t hdOffset = 36;
  const uint16_t hdWidth = width * 4, hdHeight = height * 4, hdStride = (hdWidth + 7) / 8;
  hd.data.resize(hdOffset + hdStride * hdHeight);
  std::memcpy(hd.data.data(), "XSTUDYI", 7);
  hd.data[8] = 3;
  hd.data[12] = 1;
  for (int face = 0; face < 2; ++face) {
    auto* p = hd.data.data() + 16 + face * 10;
    std::memcpy(p, &hdOffset, 4);
    std::memcpy(p + 4, &hdWidth, 2);
    std::memcpy(p + 6, &hdHeight, 2);
    std::memcpy(p + 8, &hdStride, 2);
  }
  for (int y = 0; y < hdHeight; ++y)
    for (int x = 0; x < hdWidth; ++x)
      if ((x * 3 + y * 7) % 13 < 4) hd.data[hdOffset + y * hdStride + x / 8] |= 0x80 >> (x % 8);
  study::StudyImages highImages;
  check(!highImages.open(hd));  // A v3 file cannot enter the preview's 896-byte workspace.
  check(highImages.open(hd, true));
  const auto high = highImages.at(hd, 0, false);
  check(high.valid() && high.width == hdWidth && high.height == hdHeight);
  std::array<uint8_t, study::kZoomBandBytes> highBand{};
  for (int turns = 0; turns < 4; ++turns) {
    for (int zoom = 1; zoom <= 3; ++zoom) {
      study::ImageView zoomView;
      zoomView.turns = turns;
      zoomView.zoom = zoom;
      auto p = study::placeImage(image, 10, 10, 100, 70, zoomView);
      p.rotatedWidth = turns % 2 ? hdHeight : hdWidth;
      p.rotatedHeight = turns % 2 ? hdWidth : hdHeight;
      Target pixels;
      hd.reads = 0;
      check(study::drawImageView(pixels, highImages, hd, high, 10, 10, 100, 70, zoomView, highBand.data(), &image));
      check(hd.reads <= (hdHeight + 15) / 16);
      for (int y = 10; y < 80; ++y)
        for (int x = 10; x < 110; ++x) {
          int sx = 0, sy = 0;
          const bool expected =
              p.sourceAt(x, y, high, sx, sy) && (hd.data[hdOffset + sy * hdStride + sx / 8] & (0x80 >> (sx % 8)));
          check(bool(pixels.pixels[y * 120 + x]) == expected);
        }
    }
  }
  Source gray;
  gray.data.resize(36 + width * height, 128);
  std::memcpy(gray.data.data(), hd.data.data(), 36);
  gray.data[8] = 4;
  for (int face = 0; face < 2; ++face) {
    auto* p = gray.data.data() + 16 + face * 10;
    std::memcpy(p + 4, &width, 2);
    std::memcpy(p + 6, &height, 2);
    std::memcpy(p + 8, &width, 2);
  }
  study::StudyImages grayImages;
  check(!grayImages.open(gray));
  check(grayImages.open(gray, true));
  const auto grayImage = grayImages.at(gray, 0);
  check(grayImage.valid() && grayImage.bits == 8 && grayImage.stride == width);
  for (int turns = 0; turns < 4; ++turns)
    for (int zoom = 1; zoom <= 3; ++zoom) {
      study::ImageView v;
      v.turns = turns;
      v.zoom = zoom;
      Target out;
      gray.reads = 0;
      check(study::drawImageView(out, grayImages, gray, grayImage, 10, 10, 100, 70, v, highBand.data(), &image));
      check(gray.reads <= (height + 14) / 15 + 2);
      // A uniform gray stays a screen-sized dither, even while rotated/zoomed.
      for (int y = 10; y < 80; ++y)
        for (int x = 10; x < 110; ++x) check(bool(out.pixels[y * 120 + x]) == ((x + y) % 2 != 0));
    }
  gray.data[25] |= 0x80;
  gray.data[35] |= 0x80;
  const auto textImage = grayImages.at(gray, 0);
  check(textImage.valid() && textImage.highContrast && textImage.stride == width);
  std::fill(gray.data.begin() + 36, gray.data.end(), 100);
  study::ImageView textView;
  textView.zoom = 2;
  Target textPixels;
  check(study::drawImageView(textPixels, grayImages, gray, textImage, 10, 10, 100, 70, textView, highBand.data(),
                             &image));
  for (int y = 10; y < 80; ++y)
    for (int x = 10; x < 110; ++x) check(textPixels.pixels[y * 120 + x]);
  gray.fail = true;
  Target badGray;
  check(!study::drawImageView(badGray, grayImages, gray, grayImage, 10, 10, 100, 70, view, highBand.data(), &image));
  source.fail = true;
  Target target;
  check(!study::drawImageView(target, images, source, image, 10, 10, 100, 70, view, band));
  std::printf("Image zoom/rotation: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

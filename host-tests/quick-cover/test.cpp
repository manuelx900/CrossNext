#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../../src/apps_local/carnivoro/CarnivoroScreens.h"
#include "../../src/apps_local/home/CoverScanline.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/QuickSettingsScreen.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#pragma GCC diagnostic pop
namespace fui = freeink::ui;
namespace {
int checks = 0;
int failures = 0;
#define CHECK(expr)                                  \
  do {                                               \
    ++checks;                                        \
    if (!(expr)) {                                   \
      ++failures;                                    \
      if (failures > 30) std::exit(1);               \
      std::printf("FAIL %d: %s\n", __LINE__, #expr); \
    }                                                \
  } while (0)

// The same glyph bitmaps and metrics as the firmware, adapted to the SDK's
// freestanding framebuffer target. This is a host preview, not a device shot.
struct FontBridge {
  std::array<fui::FontGlyph, 224> glyphs{};
  fui::BitmapFont font{};
  explicit FontBridge(const EpdFontData& source) {
    for (uint32_t cp = 32; cp <= 255; ++cp) {
      for (uint32_t i = 0; i < source.intervalCount; ++i) {
        const auto& interval = source.intervals[i];
        if (cp < interval.first || cp > interval.last) continue;
        const auto& g = source.glyph[interval.offset + cp - interval.first];
        glyphs[cp - 32] = {static_cast<uint16_t>(g.dataOffset),
                           g.width,
                           g.height,
                           static_cast<uint8_t>(fp4::toPixel(g.advanceX)),
                           static_cast<int8_t>(g.left),
                           static_cast<int8_t>(-g.top)};
        break;
      }
    }
    font = {source.bitmap, glyphs.data(), 32, 255, source.advanceY, static_cast<uint8_t>(source.ascender), 64, 64, 1};
  }
};

fui::DeviceContext makeDevice(fui::DisplayTarget& target, bool inverted) {
  auto d = target.deviceContext();
  d.hasTouch = true;
  if (d.height > d.width) {
    d.safeArea = inverted ? fui::Insets{0, 1, 10, 1} : fui::Insets{10, 1, 0, 1};
    d.orientation = inverted ? fui::Orientation::PortraitInverted : fui::Orientation::Portrait;
  } else {
    d.safeArea = inverted ? fui::Insets{1, 0, 1, 10} : fui::Insets{1, 10, 1, 0};
    d.orientation = inverted ? fui::Orientation::LandscapeCounterClockwise : fui::Orientation::LandscapeClockwise;
  }
  return d;
}
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30};
  fui::DisplayTarget target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;

  Canvas(int w, int h, bool inverted = false)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(makeDevice(target, inverted)),
        frame(target, device, input, interactions),
        screen(frame) {
    target.setFont(3, tiny.font);
    target.setFont(toybox::kSmallFont, small.font);
    target.setFont(toybox::kUiFont, body.font);
    target.setFont(toybox::kDisplayFont, title.font);
  }

  void checkHits() {
    CHECK(!interactions.overflowed());
    paintclock::notePainted();
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& slot = interactions.data()[n];
      const auto& r = slot.rect;
      CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
      CHECK(r.width >= 44 && r.height >= 32);
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = static_cast<int16_t>(r.x + r.width / 2);
      tap.touchY = static_cast<int16_t>(r.y + r.height / 2);
      const auto event = interactions.route(tap);
      CHECK(event.action == slot.action && event.value == slot.value);
    }
  }

  bool has(fui::ActionId action, int value = 0) const {
    for (size_t n = 0; n < interactions.count(); ++n)
      if (interactions.data()[n].action == action && interactions.data()[n].value == value) return true;
    return false;
  }

  void write(const std::string& path) const {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
      std::perror(path.c_str());
      std::exit(2);
    }
    std::fprintf(file, "P4\n%d %d\n", width, height);
    for (const auto byte : pixels) std::fputc(byte ^ 255, file);
    std::fclose(file);
  }
};

}  // namespace

bool ink(const Canvas& c, int x, int y) { return (c.pixels[y * ((c.width + 7) / 8) + x / 8] & (0x80 >> (x % 8))) == 0; }
// Replay the former firmware triangle raster to prove the skipped top scanline.
struct LegacyRaster {
  Canvas& c;
  int getScreenWidth() const { return c.width; }
  int getScreenHeight() const { return c.height; }
  void drawPixel(int x, int y, bool state) const {
    c.target.fill(fui::makeRect(x, y, 1, 1), fui::Paint::solid(state ? fui::Color::Black : fui::Color::White));
  }
  void polygon(const int*, const int*, int, bool) const;
};
void LegacyRaster::polygon(const int* xPoints, const int* yPoints, int numPoints, bool state) const {
  if (numPoints < 3) return;

  // Find bounding box
  int minY = yPoints[0], maxY = yPoints[0];
  for (int i = 1; i < numPoints; i++) {
    if (yPoints[i] < minY) minY = yPoints[i];
    if (yPoints[i] > maxY) maxY = yPoints[i];
  }

  // Clip to screen
  if (minY < 0) minY = 0;
  if (maxY >= getScreenHeight()) maxY = getScreenHeight() - 1;

  // Allocate node buffer for scanline algorithm
  auto nodeX = std::make_unique<int[]>(numPoints);
  if (!nodeX) {
    return;
  }

  // Scanline fill algorithm
  for (int scanY = minY; scanY <= maxY; scanY++) {
    int nodes = 0;

    // Find all intersection points with edges
    int j = numPoints - 1;
    for (int i = 0; i < numPoints; i++) {
      if ((yPoints[i] < scanY && yPoints[j] >= scanY) || (yPoints[j] < scanY && yPoints[i] >= scanY)) {
        // Calculate X intersection using fixed-point to avoid float
        int dy = yPoints[j] - yPoints[i];
        if (dy != 0) {
          nodeX[nodes++] = xPoints[i] + (scanY - yPoints[i]) * (xPoints[j] - xPoints[i]) / dy;
        }
      }
      j = i;
    }

    // Sort nodes by X
    std::sort(nodeX.get(), nodeX.get() + nodes);

    // Fill between pairs of nodes
    for (int i = 0; i < nodes - 1; i += 2) {
      int startX = nodeX[i];
      int endX = nodeX[i + 1];

      // Clip to screen
      if (startX < 0) startX = 0;
      if (endX >= getScreenWidth()) endX = getScreenWidth() - 1;

      // Draw horizontal line
      for (int x = startX; x <= endX; x++) {
        drawPixel(x, scanY, state);
      }
    }
  }
}

int main(int argc, char** argv) {
  int legacyStray = 0, newStray = 0;
  for (auto shape : {std::pair<int, int>{480, 800}, {800, 480}})
    for (bool inverted : {false, true}) {
      for (int percent : {0, 1, 10, 50, 99, 100}) {
        Canvas off(shape.first, shape.second, inverted), on(shape.first, shape.second, inverted);
        quickui::Model model;
        model.battery = percent;
        quickui::Work work;
        quickui::build(off.screen, model, work);
        model.charging = true;
        quickui::build(on.screen, model, work);
        const auto header = quickui::layout(on.device, true, true, true).header;
        const auto battery = fui::makeRect(header.right() - 28, header.y + 23, 26, 18);
        int changed = 0;
        for (int y = battery.y; y < battery.bottom(); ++y)
          for (int x = battery.x; x < battery.right(); ++x) changed += ink(off, x, y) != ink(on, x, y);
        CHECK(changed >= 10);  // Charging is visible even at zero or full charge.
        off.checkHits();
        on.checkHits();
        if (argc > 1 && percent == 1 && !inverted && shape.first == 480) {
          off.write(std::string(argv[1]) + "/quick-unplugged.pbm");
          on.write(std::string(argv[1]) + "/quick-charging.pbm");
        }
      }
      for (int taper : {-1, 1})
        for (int width : {1, 2, 15, 71, 102, 139})
          for (int height : {48, 176, 279, 368}) {
            Canvas before(shape.first, shape.second, inverted), after(shape.first, shape.second, inverted),
                decoded(shape.first, shape.second, inverted);
            const auto box = fui::makeRect(20, 90, width, height);
            before.target.fill(box, fui::Paint::solid(fui::Color::Black));
            after.target.fill(box, fui::Paint::solid(fui::Color::Black));
            const int cut = std::max(3, height / 16), outside = taper < 0 ? box.x : box.right() - 1,
                      inside = taper < 0 ? box.right() - 1 : box.x;
            LegacyRaster raster{before};
            int xs[] = {outside, inside, outside}, ys[] = {box.y, box.y, box.y + cut};
            raster.polygon(xs, ys, 3, false);
            int bottomY[] = {box.bottom() - 1, box.bottom() - 1, box.bottom() - 1 - cut};
            raster.polygon(xs, bottomY, 3, false);
            const auto black = fui::Paint::solid(fui::Color::Black);
            before.target.line({int16_t(outside), int16_t(box.y + cut)}, {int16_t(inside), box.y}, 1, black);
            before.target.line({int16_t(outside), int16_t(box.bottom() - 1 - cut)},
                               {int16_t(inside), int16_t(box.bottom() - 1)}, 1, black);
            before.target.line({int16_t(outside), int16_t(box.y + cut)},
                               {int16_t(outside), int16_t(box.bottom() - 1 - cut)}, 1, black);

            auto clear = [&](int x, int y, int w, int h) {
              after.target.fill(fui::makeRect(x, y, w, h), fui::Paint::solid(fui::Color::White));
            };
            auto draw = [&](int x, int y, int w, int h) {
              after.target.fill(fui::makeRect(x, y, w, h), fui::Paint::solid(fui::Color::Black));
            };
            homecover::finishTaper(box, taper, clear, draw);
            uint8_t row[8]{};
            for (int y = 0; y < 32; ++y)
              homecover::paintRow(row, 0, 32, 32, y, box, taper, [&](int x, int yy, int w, int h) {
                decoded.target.fill(fui::makeRect(x, yy, w, h), fui::Paint::solid(fui::Color::Black));
              });
            homecover::finishTaper(
                box, taper,
                [&](int x, int y, int w, int h) {
                  decoded.target.fill(fui::makeRect(x, y, w, h), fui::Paint::solid(fui::Color::White));
                },
                [&](int x, int y, int w, int h) {
                  decoded.target.fill(fui::makeRect(x, y, w, h), fui::Paint::solid(fui::Color::Black));
                });
            CHECK(after.pixels == decoded.pixels);
            for (int x = 0; x < width; ++x) {
              const int inset = homecover::edgeInset(x, box, taper);
              for (int y = 0; y < height; ++y) {
                const bool outsideShape = y < inset || y >= height - inset;
                if (outsideShape) {
                  if (ink(before, box.x + x, box.y + y)) ++legacyStray;
                  if (ink(after, box.x + x, box.y + y)) ++newStray;
                  CHECK(!ink(after, box.x + x, box.y + y));
                }
              }
            }
          }
    }
  for (int width : {1, 31, 137, 220})
    for (int height : {1, 48, 230})
      for (int sourceWidth : {width, width + 13, width * 2})
        for (int sourceHeight : {height, height + 17, height * 2}) {
          const auto slot = fui::makeRect(7, 9, width, height);
          std::vector<uint8_t> actual(width * height), expected(width * height);
          std::vector<uint8_t> row((sourceWidth + 7 + 3) / 4);
          for (int y = 0; y < sourceHeight; ++y) {
            for (size_t n = 0; n < row.size(); ++n) row[n] = uint8_t(n * 31 + y * 53);
            homecover::paintRow(row.data(), 7, sourceWidth, sourceHeight, y, slot, 0, [&](int x, int yy, int w, int h) {
              for (int dy = 0; dy < h; ++dy)
                for (int dx = 0; dx < w; ++dx) actual[(yy - slot.y + dy) * width + x - slot.x + dx] = 1;
            });
            const int top = y * height / sourceHeight, bottom = (y + 1) * height / sourceHeight;
            for (int x = 0; x < width; ++x) {
              const int sx = 7 + x * sourceWidth / width;
              if (((row[sx / 4] >> (6 - sx % 4 * 2)) & 3) == 3) continue;
              for (int yy = top; yy < bottom; ++yy) expected[yy * width + x] = 1;
            }
          }
          CHECK(actual == expected);
        }
  CHECK(legacyStray > 0 && newStray == 0);
  std::printf("%d checks, %d failures; legacy corner pixels=%d, corrected=%d\n", checks, failures, legacyStray,
              newStray);
  return failures ? 1 : 0;
}

#pragma once

#include <algorithm>
#include <cstdint>

#include "StudyImages.h"

namespace study {
struct ImageDirection {
  int x, y;
};
inline ImageDirection imageStepDirection(int step, uint8_t turns) {
  static constexpr ImageDirection directions[] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}};
  if (step < 0 || step >= 4) return {};
  const auto direction = directions[step];
  switch (turns % 4) {
    case 0:
      return direction;
    case 1:
      return {-direction.y, direction.x};
    case 2:
      return {-direction.x, -direction.y};
    case 3:
      return {direction.y, -direction.x};
  }
  return {};
}
struct ImageView {
  uint8_t zoom = 0, turns = 0;
  int panX = 0, panY = 0;
  void fit() {
    zoom = 0;
    panX = panY = 0;
  }
  void reset() { *this = {}; }
  void rotate(bool right = false) {
    turns = turns ? 0 : right ? 1 : 3;
    panX = panY = 0;
  }
  void magnify(int step) {
    const int next = std::clamp(int(zoom) + step, 0, 3);
    if (next > zoom) {
      panX *= 2;
      panY *= 2;
    } else if (next < zoom) {
      panX /= 2;
      panY /= 2;
    }
    zoom = next;
    if (!zoom) panX = panY = 0;
  }
};
struct ImagePlacement {
  int left = 0, top = 0, width = 0, height = 0, rotatedWidth = 0, rotatedHeight = 0;
  uint8_t turns = 0;
  bool sourceAt(int x, int y, const ImageRef& image, int& sx, int& sy) const {
    if (x < left || y < top || x >= left + width || y >= top + height || !width || !height) return false;
    const int rx = (x - left) * rotatedWidth / width;
    const int ry = (y - top) * rotatedHeight / height;
    switch (turns) {
      case 0:
        sx = rx;
        sy = ry;
        break;
      case 1:
        sx = ry;
        sy = image.height - 1 - rx;
        break;
      case 2:
        sx = image.width - 1 - rx;
        sy = image.height - 1 - ry;
        break;
      case 3:
        sx = image.width - 1 - ry;
        sy = rx;
        break;
    }
    return sx >= 0 && sy >= 0 && sx < image.width && sy < image.height;
  }
};
inline ImagePlacement placeImage(const ImageRef& image, int x, int y, int width, int height, ImageView& view) {
  ImagePlacement out;
  if (!image.valid() || width <= 0 || height <= 0) return out;
  out.turns = view.turns % 4;
  out.rotatedWidth = out.turns % 2 ? image.height : image.width;
  out.rotatedHeight = out.turns % 2 ? image.width : image.height;
  const int base = std::max(1, std::min({256, width * 256 / out.rotatedWidth, height * 256 / out.rotatedHeight}));
  const int scale = base * (1 << std::min<int>(view.zoom, 3));
  out.width = std::max(1, out.rotatedWidth * scale / 256);
  out.height = std::max(1, out.rotatedHeight * scale / 256);
  const int extraX = std::max(0, out.width - width), extraY = std::max(0, out.height - height);
  view.panX = std::clamp(view.panX, -(extraX + 1) / 2, extraX / 2);
  view.panY = std::clamp(view.panY, -(extraY + 1) / 2, extraY / 2);
  out.left = x + (width - out.width) / 2 + view.panX;
  out.top = y + (height - out.height) / 2 + view.panY;
  return out;
}

inline bool panImageThird(const ImageRef& image, int width, int height, ImageView& view, int dx, int dy) {
  if (!view.zoom) return false;
  const auto placement = placeImage(image, 0, 0, width, height, view);
  const int beforeX = view.panX, beforeY = view.panY;
  const int extraX = std::max(0, placement.width - width), extraY = std::max(0, placement.height - height);
  view.panX -= dx * ((extraX + 2) / 3);
  view.panY -= dy * ((extraY + 2) / 3);
  placeImage(image, 0, 0, width, height, view);
  return beforeX != view.panX || beforeY != view.panY;
}

// Walk columns for quarter turns so each SD band is read once, as for rows.
template <typename Target>
bool drawImageView(const Target& target, StudyImages& images, ByteSource& source, const ImageRef& image, int x, int y,
                   int width, int height, ImageView& view, uint8_t* band, const ImageRef* preview = nullptr) {
  auto placement = placeImage(preview ? *preview : image, x, y, width, height, view);
  // Retain the preview's exact zoom/pan geometry when switching pixel sources.
  placement.rotatedWidth = placement.turns % 2 ? image.height : image.width;
  placement.rotatedHeight = placement.turns % 2 ? image.width : image.height;
  if (!placement.width || !band) return false;
  const bool columns = placement.turns % 2;
  const int outerCount = columns ? width : height, innerCount = columns ? height : width;
  const int stepX = placement.rotatedWidth * 65536 / placement.width;
  const int stepY = placement.rotatedHeight * 65536 / placement.height;
  int cachedFirst = -1, cachedRows = 0;
  for (int outer = 0; outer < outerCount; ++outer) {
    for (int inner = 0; inner < innerCount; ++inner) {
      const int dx = x + (columns ? outer : inner), dy = y + (columns ? inner : outer);
      int sx = 0, sy = 0;
      if (image.bits == 8) {
        if (dx < placement.left || dy < placement.top || dx >= placement.left + placement.width ||
            dy >= placement.top + placement.height)
          continue;
      } else if (!placement.sourceAt(dx, dy, image, sx, sy))
        continue;
      if (image.bits != 8 && (sy < cachedFirst || sy >= cachedFirst + cachedRows)) {
        cachedFirst = sy / kImageBandRows * kImageBandRows;
        cachedRows = images.readBand(source, image, cachedFirst, band);
        if (!cachedRows) return false;
      }
      if (image.bits == 8) {
        // Interpolate native gray before quantizing at screen resolution: the
        // dither pattern must not grow with the photograph.
        const int rx = std::clamp((dx - placement.left) * stepX / 256, 0, (placement.rotatedWidth - 1) * 256);
        const int ry = std::clamp((dy - placement.top) * stepY / 256, 0, (placement.rotatedHeight - 1) * 256);
        int fx = rx, fy = ry;
        switch (placement.turns) {
          case 0:
            break;
          case 1:
            fx = ry;
            fy = (image.height - 1) * 256 - rx;
            break;
          case 2:
            fx = (image.width - 1) * 256 - rx;
            fy = (image.height - 1) * 256 - ry;
            break;
          case 3:
            fx = (image.width - 1) * 256 - ry;
            fy = rx;
            break;
        }
        const int x0 = fx >> 8, y0 = fy >> 8;
        const int x1 = std::min(x0 + 1, int(image.width) - 1), y1 = std::min(y0 + 1, int(image.height) - 1);
        if (y0 < cachedFirst || y1 >= cachedFirst + cachedRows) {
          cachedFirst = y0 / (kImageBandRows - 1) * (kImageBandRows - 1);
          cachedRows = images.readBand(source, image, cachedFirst, band);
          if (!cachedRows || y1 >= cachedFirst + cachedRows) return false;
        }
        const auto* top = band + (y0 - cachedFirst) * image.stride;
        const auto* bottom = band + (y1 - cachedFirst) * image.stride;
        const int ax = fx & 255, ay = fy & 255;
        const int t = top[x0] * (256 - ax) + top[x1] * ax;
        const int b = bottom[x0] * (256 - ax) + bottom[x1] * ax;
        const int gray = (t * (256 - ay) + b * ay) >> 16;
        static constexpr uint8_t thresholds[4][4] = {
            {8, 136, 40, 168}, {200, 72, 232, 104}, {56, 184, 24, 152}, {248, 120, 216, 88}};
        if (gray < (image.highContrast ? 128 : thresholds[dy & 3][dx & 3])) target.drawPixel(dx, dy, true);
      } else {
        const auto* row = band + (sy - cachedFirst) * image.stride;
        if (row[sx >> 3] & (0x80 >> (sx & 7))) target.drawPixel(dx, dy, true);
      }
    }
  }
  return true;
}
}  // namespace study

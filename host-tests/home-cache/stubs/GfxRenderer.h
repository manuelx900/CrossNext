#pragma once
#include <algorithm>
#include <array>
#include <vector>
class GfxRenderer {
 public:
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };
  Orientation orientation = Portrait;
  Orientation getOrientation() const { return orientation; }
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };
  RenderMode mode = BW;
  RenderMode getRenderMode() const { return mode; }
  bool inverted = false;
  mutable unsigned fillCalls = 0, packedWrites = 0;
  mutable std::array<int, 4> clip{0, 0, 32767, 32767};
  std::array<int, 4> getClipRect() const { return clip; }
  void setClipRect(int x, int y, int w, int h) const { clip = {x, y, w, h}; }
  int width = 480, height = 800;
  mutable std::vector<unsigned char> frame = std::vector<unsigned char>(480 * 800);
  int getScreenWidth() const { return width; }
  int getScreenHeight() const { return height; }
  void fillRect(int x, int y, int w, int h, bool black = true) const {
    ++fillCalls;
    for (int yy = std::max({0, y, clip[1]}); yy < std::min({height, y + h, clip[1] + clip[3]}); ++yy)
      for (int xx = std::max({0, x, clip[0]}); xx < std::min({width, x + w, clip[0] + clip[2]}); ++xx)
        frame[yy * width + xx] = black;
  }
  void drawRect(int x, int y, int w, int h) const {
    fillRect(x, y, w, 1);
    fillRect(x, y + h - 1, w, 1);
    fillRect(x, y, 1, h);
    fillRect(x + w - 1, y, 1, h);
  }
  template <class T>
  void fillRectDither(int x, int y, int w, int h, T) const {
    fillRect(x, y, w, h);
  }
  void preserveImagePolarity(int x, int y, int w, int h) const {
    if (!inverted) return;
    for (int yy = y; yy < y + h; ++yy)
      for (int xx = x; xx < x + w; ++xx) frame[yy * width + xx] ^= 1;
  }
  bool portrait() const { return orientation == Portrait || orientation == PortraitInverted; }
  size_t getPackedRegionByteSize(int, int, int w, int h) const {
    return size_t(((portrait() ? h : w) + 7) / 8) * (portrait() ? w : h);
  }
  void coordinates(int dx, int dy, int w, int h, int& px, int& py) const {
    switch (orientation) {
      case Portrait:
        px = dy;
        py = w - 1 - dx;
        break;
      case LandscapeClockwise:
        px = w - 1 - dx;
        py = h - 1 - dy;
        break;
      case PortraitInverted:
        px = h - 1 - dy;
        py = dx;
        break;
      case LandscapeCounterClockwise:
        px = dx;
        py = dy;
        break;
    }
  }
  bool copyPackedRegionToBuffer(int x, int y, int w, int h, uint8_t* out, size_t count) const {
    const auto bytes = getPackedRegionByteSize(x, y, w, h);
    if (count < bytes) return false;
    std::fill_n(out, bytes, 255);
    const int stride = ((portrait() ? h : w) + 7) / 8;
    for (int dy = 0; dy < h; ++dy)
      for (int dx = 0; dx < w; ++dx) {
        int px, py;
        coordinates(dx, dy, w, h, px, py);
        if (frame[(y + dy) * width + x + dx]) out[py * stride + px / 8] &= ~(0x80 >> (px & 7));
      }
    return true;
  }
  bool copyPackedBufferToRegion(int x, int y, int w, int h, const uint8_t* in, size_t count) const {
    if (count < getPackedRegionByteSize(x, y, w, h)) return false;
    const int stride = ((portrait() ? h : w) + 7) / 8;
    ++packedWrites;
    for (int dy = 0; dy < h; ++dy)
      for (int dx = 0; dx < w; ++dx) {
        int px, py;
        coordinates(dx, dy, w, h, px, py);
        frame[(y + dy) * width + x + dx] = !(in[py * stride + px / 8] & (0x80 >> (px & 7)));
      }
    return true;
  }
  size_t getRegionByteSize(int, int, int w, int h) const { return size_t(w) * h; }
  bool copyRegionToBuffer(int x, int y, int w, int h, uint8_t* out, size_t count) const {
    if (count != size_t(w) * h) return false;
    for (int row = 0; row < h; ++row) std::copy_n(frame.data() + (y + row) * width + x, w, out + row * w);
    return true;
  }
  bool copyBufferToRegion(int x, int y, int w, int h, const uint8_t* in, size_t count) const {
    if (count != size_t(w) * h) return false;
    for (int row = 0; row < h; ++row) std::copy_n(in + row * w, w, frame.data() + (y + row) * width + x);
    return true;
  }
};

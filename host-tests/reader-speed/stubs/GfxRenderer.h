#pragma once
#include <Arduino.h>
#include <FontCacheManager.h>

#include <array>
#include <cstdint>
class GfxRenderer {
 public:
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };
  enum RenderMode { BW, GRAYSCALE_MSB, GRAYSCALE_LSB };
  bool dither = false;
  RenderMode mode = BW;
  Orientation orientation = LandscapeCounterClockwise;
  std::array<uint8_t, 64 * 40 / 8> frame{};
  uint8_t* getWriteTarget() { return frame.data(); }
  int getWriteOriginY() const { return 0; }
  int getWriteRows() const { return 40; }
  RenderMode getRenderMode() const { return mode; }
  bool grayPlanesAreAbsolute() const { return mode != BW; }
  bool imagesDitheredInBw() const { return dither; }
  int getDisplayWidthBytes() const { return 8; }
  int getDisplayWidth() const { return 64; }
  int getDisplayHeight() const { return 40; }
  Orientation getOrientation() const { return orientation; }
  int getScreenWidth() const { return orientation == Portrait || orientation == PortraitInverted ? 40 : 64; }
  int getScreenHeight() const { return orientation == Portrait || orientation == PortraitInverted ? 64 : 40; }
  bool hasFrameBuffer() const { return true; }
  FontCacheManager* getFontCacheManager() const { return nullptr; }
  bool glyphIntersectsStrip(int, int, int, int) const { return true; }
  void preserveImagePolarity(int, int, int, int) {}
  void fillRect(int, int, int, int, bool = true) {}
};

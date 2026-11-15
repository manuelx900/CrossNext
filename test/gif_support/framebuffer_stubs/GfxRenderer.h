#pragma once
#include <array>
#include <cstdint>

class GfxRenderer {
 public:
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };
  enum RenderMode { BW, GRAYSCALE_MSB, GRAYSCALE_LSB };
  Orientation orientation = LandscapeCounterClockwise;
  std::array<uint8_t, 64 * 40 / 8> frame{};
  uint8_t* getWriteTarget() { return frame.data(); }
  int getWriteOriginY() const { return 0; }
  int getWriteRows() const { return 40; }
  RenderMode getRenderMode() const { return BW; }
  bool grayPlanesAreAbsolute() const { return false; }
  bool imagesDitheredInBw() const { return false; }
  int getDisplayWidthBytes() const { return 8; }
  int getDisplayWidth() const { return 64; }
  int getDisplayHeight() const { return 40; }
  Orientation getOrientation() const { return orientation; }
  int getScreenWidth() const { return orientation == Portrait || orientation == PortraitInverted ? 40 : 64; }
  int getScreenHeight() const { return orientation == Portrait || orientation == PortraitInverted ? 64 : 40; }
};

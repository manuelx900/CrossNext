#pragma once
#include <cstdint>
// The render task records a logical full-width header band for output-polarity compensation.
namespace chrome {
enum class HeaderColor : uint8_t { Appearance, Black, White, Count };
constexpr HeaderColor normalizedHeaderColor(uint8_t value) {
  return value < uint8_t(HeaderColor::Count) ? HeaderColor(value) : HeaderColor::Appearance;
}
inline int headerHeight = 0;
inline bool lightHeader = false;
inline bool framebufferLightHeader = false;
// Custom artwork screens paint the selected polarity before output compensation.
inline bool headerPaintedLight = false;
inline HeaderColor headerColor = HeaderColor::Appearance;
inline HeaderColor framebufferHeaderColor = HeaderColor::Appearance;
constexpr bool resolveLightHeader(HeaderColor color, bool appearanceLight) {
  return color == HeaderColor::Appearance ? appearanceLight : color == HeaderColor::White;
}
inline bool headerIsLight() { return resolveLightHeader(headerColor, lightHeader); }
inline void recordPaintedHeader(int height, bool light) {
  headerHeight = height;
  headerPaintedLight = light;
}
inline void beginFrame() {
  headerHeight = 0;
  headerPaintedLight = false;
  framebufferLightHeader = lightHeader;
  framebufferHeaderColor = headerColor;
}
inline bool invertHeaderForOutput(bool invertedDisplay) {
  const bool light = resolveLightHeader(framebufferHeaderColor, framebufferLightHeader);
  return invertedDisplay != (light != headerPaintedLight);
}
}  // namespace chrome

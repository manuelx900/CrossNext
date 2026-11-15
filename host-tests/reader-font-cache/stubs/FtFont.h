#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace freeink::font {
struct GlyphBitmap {
  uint16_t width = 8, height = 8;
  int advance = 9, xoff = 0, yoff = -8;
  const uint8_t* pixels = nullptr;
};
inline unsigned rasterCalls = 0, kernCalls = 0;
class FtFont {
 public:
  using ReadFn = unsigned long (*)(void*, unsigned long, unsigned char*, unsigned long);
  using GlyphId = uint32_t;
  struct GlyphMetrics {
    int advance26_6 = 9 * 64;
  };
  struct LineMetrics {
    int ascender26_6 = 10 * 64, height26_6 = 12 * 64;
  };
  enum class HintingMode { Auto };
  struct RenderOptions {
    HintingMode hinting = HintingMode::Auto;
    bool stemDarkening = false;
  };
  bool init(const uint8_t*, uint32_t, uint16_t, int = 400, bool = false) { return true; }
  bool initStream(ReadFn, void*, unsigned long, uint16_t, int = 400, bool = false) { return true; }
  void deinit() {}
  bool setRenderOptions(RenderOptions) { return true; }
  void setGsubByteBudget(size_t) {}
  void setGposByteBudget(size_t) {}
  GlyphId ligatureGlyphId(const uint32_t*, int) { return 0; }
  void releaseLigatureTable() {}
  bool hasGlyph(uint32_t cp) { return cp < 0xfb00; }
  bool lineMetrics26_6(uint32_t, LineMetrics&) { return true; }
  int ascent(uint16_t) { return 10; }
  int lineHeight(uint16_t) { return 12; }
  GlyphId glyphId(uint32_t cp) { return cp; }
  bool metricsGlyph26_6(GlyphId, uint32_t, GlyphMetrics&) { return true; }
  const GlyphBitmap* rasterizeGlyph26_6(GlyphId, uint32_t) {
    ++rasterCalls;
    static const std::array<uint8_t, 64> pixels = [] {
      std::array<uint8_t, 64> p{};
      p.fill(255);
      return p;
    }();
    static const GlyphBitmap bitmap{8, 8, 9, 0, 8, pixels.data()};
    return &bitmap;
  }
  int32_t kerning26_6(uint32_t left, uint32_t right, uint32_t) {
    ++kernCalls;
    return int((left + right) % 5) * 4;
  }
};
}  // namespace freeink::font

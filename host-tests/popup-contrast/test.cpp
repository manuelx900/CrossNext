#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <vector>

#include "../../src/components/PopupLayout.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../lib/EpdFont/builtinFonts/ubuntu_12_regular.h"
#pragma GCC diagnostic pop
namespace fui = freeink::ui;
namespace {
int checks = 0, failed = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failed;                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
struct Font {
  std::array<fui::FontGlyph, 224> glyphs{};
  fui::BitmapFont font{};
  Font() {
    for (unsigned cp = 32; cp <= 255; ++cp)
      for (unsigned i = 0; i < ubuntu_12_regular.intervalCount; ++i) {
        auto interval = ubuntu_12_regular.intervals[i];
        if (cp < interval.first || cp > interval.last) continue;
        auto g = ubuntu_12_regular.glyph[interval.offset + cp - interval.first];
        glyphs[cp - 32] = {static_cast<uint16_t>(g.dataOffset),
                           g.width,
                           g.height,
                           static_cast<uint8_t>(fp4::toPixel(g.advanceX)),
                           static_cast<int8_t>(g.left),
                           static_cast<int8_t>(-g.top)};
        break;
      }
    font = {ubuntu_12_regular.bitmap,
            glyphs.data(),
            32,
            255,
            ubuntu_12_regular.advanceY,
            static_cast<uint8_t>(ubuntu_12_regular.ascender),
            64,
            64,
            1};
  }
};
void scenario(int w, int h, bool inverted, int radius, const char* message) {
  const int stride = (w + 7) / 8;
  std::vector<uint8_t> pixels(stride * h, 0x55);
  fui::DisplayTarget target(pixels.data(), w, h, stride, fui::Orientation::LandscapeCounterClockwise);
  Font font;
  target.setFont(0, font.font);
  fui::TextStyle style;
  style.font = 0;
  style.color = inverted ? fui::Color::Black : fui::Color::White;
  const auto popup = popupui::layout(target, fui::makeRect(1, 10, w - 2, h - 11), message, style, 16, 12, 2, h / 6);
  popupui::paintChrome(target, popup, 2, radius);
  auto white = [&](int x, int y) { return (pixels[y * stride + x / 8] & (0x80 >> (x % 8))) != 0; };
  const bool background = inverted;
  CHECK(white(popup.body.x + popup.body.width / 2, popup.body.y + 3) == background);
  CHECK(white(popup.body.x + popup.body.width / 2, popup.body.y - 1) != background);
  target.text(popup.text, message, popup.style);
  int glyphPixels = 0, backgroundPixels = 0;
  for (int y = popup.text.y; y < popup.text.bottom(); ++y)
    for (int x = popup.text.x; x < popup.text.right(); ++x) {
      if (white(x, y) == background)
        ++backgroundPixels;
      else
        ++glyphPixels;
    }
  CHECK(glyphPixels > 10);
  CHECK(backgroundPixels > 10);
  CHECK(popup.body.x >= 0 && popup.body.right() <= w && popup.body.y >= 0 && popup.body.bottom() <= h);
}
}  // namespace
int main() {
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (int w : {480, 800})
      for (bool inverted : {false, true})
        for (int radius : {0, 6})
          for (auto id : {StrId::STR_LOADING_POPUP, StrId::STR_DICT_LOOKING_UP, StrId::STR_DICT_INDEXING,
                          StrId::STR_BOOKMARK_ADDED, StrId::STR_WORDS_SAVED, StrId::STR_ERROR_GENERAL_FAILURE})
            scenario(w, w == 480 ? 800 : 480, inverted, radius, I18N.get(id));
  }
  std::printf("popup contrast: %d checks, %d failures\n", checks, failed);
  return failed ? 1 : 0;
}

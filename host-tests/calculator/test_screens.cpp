#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../../src/apps_local/calculator/CalculatorScreens.h"
#include "../../src/apps_local/pomodoro/TimerSettingsUi.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#pragma GCC diagnostic pop
namespace fui = freeink::ui;
unsigned checks = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                               \
    }                                             \
  } while (0)
struct Font {
  std::array<fui::FontGlyph, 224> glyphs{};
  fui::BitmapFont font{};
  explicit Font(const EpdFontData& source) {
    for (uint32_t cp = 32; cp <= 255; ++cp) {
      for (uint32_t i = 0; i < source.intervalCount; ++i) {
        const auto& interval = source.intervals[i];
        if (cp < interval.first || cp > interval.last) continue;
        const auto& g = source.glyph[interval.offset + cp - interval.first];
        glyphs[cp - 32] = {uint16_t(g.dataOffset), g.width,       g.height, uint8_t(fp4::toPixel(g.advanceX)),
                           int8_t(g.left),         int8_t(-g.top)};
        break;
      }
    }
    font = {source.bitmap, glyphs.data(), 32, 255, source.advanceY, uint8_t(source.ascender), 64, 64, 1};
  }
};
void save(const char* directory, const char* name, int width, int height, const std::vector<uint8_t>& bits) {
  if (!directory) return;
  char path[512];
  std::snprintf(path, sizeof(path), "%s/%s.pbm", directory, name);
  FILE* file = std::fopen(path, "wb");
  CHECK(file);
  std::fprintf(file, "P4\n%d %d\n", width, height);
  for (uint8_t b : bits) std::fputc(b ^ 255, file);
  std::fclose(file);
}
int main(int argc, char** argv) {
  const char* directory = argc > 1 ? argv[1] : nullptr;
  Font prose(ui_prose_14), small(ui_prose_10), jersey(toybox_14), jerseyBody(toybox_20), tiny(toybox_10);
  for (auto language : {Language::EN, Language::ES})
    for (bool light : {false, true})
      for (bool pixel : {false, true})
        for (int width : {480, 800}) {
          I18N.setLanguage(language);
          chrome::lightHeader = light;
          pixelui::uppercaseLabels = pixel;
          const int height = width == 480 ? 800 : 480;
          std::vector<uint8_t> bits((width + 7) / 8 * height, 255);
          fui::DisplayTarget target(bits.data(), width, height, (width + 7) / 8,
                                    fui::Orientation::LandscapeCounterClockwise);
          target.setFont(0, pixel ? jersey.font : prose.font);
          target.setFont(1, pixel ? jerseyBody.font : prose.font);
          target.setFont(2, pixel ? jerseyBody.font : prose.font);
          target.setFont(3, pixel ? tiny.font : small.font);
          target.setFont(4, small.font);
          fui::InputSnapshot input{};
          toybox::Interactions interactions;
          toybox::Frame frame(target, target.deviceContext(), input, interactions);
          toybox::Screen screen(frame);
          calc::Engine engine;
          engine.press(calc::Key::D1);
          engine.press(calc::Key::Dot);
          engine.press(calc::Key::D2);
          engine.press(calc::Key::D3);
          engine.press(calc::Key::Mul);
          engine.press(calc::Key::D4);
          engine.press(calc::Key::Equals);
          CHECK(std::strcmp(engine.display(), "4.92") == 0);
          calculatorui::build(screen, engine);
          CHECK(!interactions.overflowed());
          CHECK(interactions.count() == 20);
          const auto pad = calculatorui::geometry(target.deviceContext());
          CHECK(pad.cellH >= 44 && pad.cellW >= 44);
          for (int i = 0; i < 20; ++i) {
            const auto key = calc::keyRect(calc::kPad, pad, i);
            CHECK(key.x >= 0 && key.y > pad.display.bottom());
            CHECK(key.right() <= width && key.bottom() <= height);
            CHECK(calc::keyAt(calc::kPad, pad, key.x + key.w / 2, key.y + key.h / 2) == i);
            CHECK(calc::keyAt(calc::kPad, pad, key.right(), key.y) != i);
          }
          // Repeated digits retain their ink height until the result really fills its box.
          int previousHeight = 0;
          for (int digits : {1, 2, 3, 4, 12}) {
            std::fill(bits.begin(), bits.end(), 255);
            calc::Engine number;
            for (int n = 0; n < digits; ++n) number.press(calc::Key::D8);
            calculatorui::build(screen, number);
            const int left = pad.display.x + 16, right = pad.display.right() - 16;
            const int top = pad.display.y + 38, bottom = pad.display.bottom() - 12;
            int first = bottom, last = top;
            for (int y = top; y < bottom; ++y)
              for (int x = left; x < right; ++x)
                if ((bits[y * ((width + 7) / 8) + x / 8] & (128 >> (x % 8))) == 0) {
                  first = std::min(first, y);
                  last = std::max(last, y);
                }
            const int inkHeight = last - first + 1;
            if (digits <= 4) {
              if (previousHeight) CHECK(inkHeight == previousHeight);
              previousHeight = inkHeight;
            } else if (width == 480)
              CHECK(inkHeight < previousHeight);
            else
              CHECK(inkHeight == previousHeight);
          }
          std::fill(bits.begin(), bits.end(), 255);
          calculatorui::build(screen, engine);
          char name[96];
          std::snprintf(name, sizeof(name), "calculator-%s-%s-%s-%d", language == Language::ES ? "es" : "en",
                        light ? "rounded" : "square", pixel ? "jersey" : "noto", width);
          save(directory, name, width, height, bits);
          // A new Frame on the same buffer clears background controls without another framebuffer.
          toybox::Frame modalFrame(target, target.deviceContext(), input, interactions);
          toybox::Screen modal(modalFrame);
          timerui::confirmation(modal, tr(STR_POMODORO_SKIP_CONFIRM), tr(STR_UI_SKIP_431DE8), 501, 502);
          CHECK(interactions.count() == 2 && !interactions.overflowed());
          for (size_t i = 0; i < interactions.count(); ++i) {
            const auto hit = interactions.data()[i];
            CHECK(hit.rect.x >= 0 && hit.rect.y >= 0 && hit.rect.right() <= width && hit.rect.bottom() <= height);
            fui::InputSnapshot tap{};
            tap.touchReleased = true;
            tap.touchX = hit.rect.x + hit.rect.width / 2;
            tap.touchY = hit.rect.y + hit.rect.height / 2;
            CHECK(interactions.route(tap).action == hit.action);
          }
          fui::InputSnapshot outside{};
          outside.touchReleased = true;
          outside.touchX = 12;
          outside.touchY = 100;
          CHECK(interactions.route(outside).action == fui::NO_ACTION);
        }
  std::printf("Calculator UI/modal: %u checks, 0 failed; engine=%zu bytes\n", checks, sizeof(calc::Engine));
}

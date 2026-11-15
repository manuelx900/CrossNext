#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/plugins/PluginCatalogScreen.h"
#include "../../src/components/icons/search32.h"
#include "../../src/util/PluginPickerLayout.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#pragma GCC diagnostic pop
namespace fui = freeink::ui;
namespace {
unsigned checks = 0;
#define CHECK(value)                                  \
  do {                                                \
    ++checks;                                         \
    if (!(value)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #value); \
      std::exit(1);                                   \
    }                                                 \
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
struct Target : fui::DrawTarget {
  fui::DisplayTarget backend;
  std::vector<fui::Rect> outlines;
  Target(uint8_t* bits, int width, int height, int stride,
         fui::Orientation orientation = fui::Orientation::LandscapeCounterClockwise)
      : backend(bits, width, height, stride, orientation) {}
  void setFont(int slot, const fui::BitmapFont& font) { backend.setFont(slot, font); }
  auto deviceContext() const { return backend.deviceContext(); }
  fui::Size measureText(fui::FontId font, const char* value, fui::TextStyle style) const override {
    return backend.measureText(font, value, style);
  }
  int16_t lineHeight(fui::FontId font) const override { return backend.lineHeight(font); }
  fui::Rect inkBounds(fui::FontId font, const char* value, fui::TextStyle style) const override {
    return backend.inkBounds(font, value, style);
  }
  void fill(fui::Rect rect, fui::Paint paint, uint8_t radius, uint8_t corners) override {
    backend.fill(rect, paint, radius, corners);
  }
  void stroke(fui::Rect rect, fui::Paint paint, uint8_t width, uint8_t radius, uint8_t corners) override {
    outlines.push_back(rect);
    backend.stroke(rect, paint, width, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t width, fui::Paint paint) override { backend.line(a, b, width, paint); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint paint) override {
    backend.triangle(a, b, c, paint);
  }
  void bitmap(fui::Rect rect, fui::BitmapRef bitmap, fui::BitmapMode mode, fui::Paint paint,
              fui::Rotation rotation) override {
    backend.bitmap(rect, bitmap, mode, paint, rotation);
  }
  struct Run {
    fui::Rect rect;
    std::string text;
    fui::TextStyle style;
  };
  std::vector<Run> runs;
  void text(fui::Rect rect, const char* value, fui::TextStyle style) override {
    runs.push_back({rect, value, style});
    backend.text(rect, value, style);
  }
};

void exercise(const char* output) {
  Font pixelSmall(toybox_10), pixelBody(toybox_14), regularSmall(ui_prose_10), regularBody(ui_prose_14);
  for (bool wide : {false, true})
    for (bool light : {false, true})
      for (bool pixel : {false, true})
        for (bool english : {false, true})
          for (int count : {0, 1, 18})
            for (bool touch : {false, true}) {
              I18n::getInstance().setLanguage(english ? Language::EN : Language::ES);
              chrome::lightHeader = light;
              const int w = wide ? 800 : 480, h = wide ? 480 : 800;
              std::vector<uint8_t> bits(w * h / 8, 255);
              Target target(bits.data(), w, h, w / 8);
              target.setFont(0, pixel ? pixelSmall.font : regularSmall.font);
              for (int slot : {1, 2}) target.setFont(slot, pixel ? pixelBody.font : regularBody.font);
              for (int slot : {3, 4, int(fui::HEADER_CLOCK_FONT)})
                target.setFont(slot, pixel ? pixelSmall.font : regularSmall.font);
              auto device = target.deviceContext();
              device.hasTouch = touch;
              toybox::Interactions hits;
              fui::InputSnapshot input;
              toybox::Frame frame(target, device, input, hits);
              toybox::Screen screen(frame);
              toybox::HeaderStatus status;
              status.percent = 100;
              std::snprintf(status.clock, sizeof(status.clock), "22:10");
              pluginui::header(screen, tr(STR_PLUGINS), status,
                               count > 1 ? fui::bitmapFromIcon(icon_search_32) : fui::BitmapRef{},
                               count > 1 ? 5 : fui::NO_ACTION);
              std::array<fui::ListItem, 18> rows{};
              for (int n = 0; n < count; ++n) {
                rows[n].label = n == 0 ? tr(STR_PLUGIN_HUB) : english ? "Reading companion" : "Compañero de lectura";
                rows[n].subtitle =
                    n == 0    ? tr(STR_PLUGIN_HUB_DESCRIPTION)
                    : english ? "Book information and reading activity, available on the device."
                              : "Información de tus libros y actividad de lectura, disponible en el dispositivo.";
                rows[n].value = ">";
                rows[n].actionValue = int16_t(n);
              }
              fui::ListNav nav;
              fui::ListProps props;
              pluginui::styleRows(props, true);
              props.items = rows.data();
              props.count = count;
              props.action = 1;
              props.inputMask = fui::InputTouch | fui::InputLongPress;
              screen.syncListViewport(nav, props, count);
              fitPluginSubtitleToViewport(target, screen.body().height, props);
              if (count)
                pluginui::list(screen, props);
              else
                screen.centeredText(tr(STR_NO_PLUGINS_INSTALLED), screen.theme().bodyText);
              if (count) {
                CHECK(!nav.consumeRebuildNeeded());
                CHECK(!nav.followPending);
                if (count > 1 && nav.drawnRows > 1) {
                  nav.requestSelection(1);
                  screen.syncListViewport(nav, props, count);
                  pluginui::list(screen, props);
                  CHECK(nav.top == 0);
                  CHECK(!nav.consumeRebuildNeeded());
                }
              }
              CHECK(!hits.overflowed());
              int rowHits = 0;
              for (size_t n = 0; n < hits.count(); ++n) {
                const auto& hit = hits.data()[n];
                CHECK(hit.rect.x >= 0 && hit.rect.y >= 0 && hit.rect.right() <= w && hit.rect.bottom() <= h);
                if (hit.action != 1) continue;
                ++rowHits;
                CHECK(hit.rect.y >= screen.body().y && hit.rect.bottom() <= screen.body().bottom());
                fui::InputSnapshot tap;
                tap.touchReleased = true;
                tap.touchX = hit.rect.x + hit.rect.width / 2;
                tap.touchY = hit.rect.y + hit.rect.height / 2;
                CHECK(hits.route(tap).value == hit.value);
              }
              CHECK(count ? rowHits > 0 : rowHits == 0);
              for (const auto& run : target.runs) {
                if (run.rect.y < screen.body().y) continue;
                CHECK(run.rect.bottom() <= screen.body().bottom());
                if (run.text == tr(STR_PLUGIN_HUB_DESCRIPTION)) CHECK(!run.style.bold);
              }
              // The shared target inverts the light header on hardware.
              if (light)
                for (int i = 0; i < chrome::headerHeight * w / 8; ++i) bits[i] = ~bits[i];
              if (!touch) continue;
              char file[512];
              std::snprintf(file, sizeof(file), "%s/plugins-%s-%s-%s-%s-%d.pbm", output, wide ? "wide" : "portrait",
                            light ? "light" : "boxed", pixel ? "pixel" : "regular", english ? "en" : "es", count);
              auto* out = std::fopen(file, "wb");
              CHECK(out);
              std::fprintf(out, "P4\n%d %d\n", w, h);
              for (auto& b : bits) b = ~b;
              std::fwrite(bits.data(), 1, bits.size(), out);
              std::fclose(out);
            }
}
}  // namespace
int main(int argc, char** argv) {
  exercise(argc > 1 ? argv[1] : ".");
  std::printf("%u plugin UI checks passed\n", checks);
}

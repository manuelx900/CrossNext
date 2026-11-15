#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/library/LibraryControls.h"
#include "../../src/apps_local/library/LibraryLoadingNotice.h"
#include "../../src/apps_local/ui/PageIndicator.h"
#include "../../src/apps_local/ui/QuickSettingsScreen.h"
#include "../../src/apps_local/ui/StatusBattery.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#pragma GCC diagnostic pop
namespace fui = freeink::ui;
namespace {
unsigned checks = 0;
#define CHECK(v)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(v)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #v); \
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
struct Target : fui::DrawTarget {
  fui::DisplayTarget backend;
  std::vector<fui::Rect> outlines;
  struct FillRecord {
    fui::Rect rect;
    fui::Color color;
  };
  std::array<FillRecord, 3> fills{};
  unsigned fillCount = 0;
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
    fills[fillCount++ % fills.size()] = {rect, paint.color};
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

void referenceBattery(fui::DrawTarget& target, fui::Rect b, int percent, bool charging, fui::Color color) {
  auto ink = fui::Paint::solid(color), paper = fui::Paint::solid(fui::invertedColor(color));
  target.stroke(b, ink, 2);
  target.fill(fui::makeRect(b.right(), b.y + 5, 4, 9), ink);
  const int fill = charging ? b.width - 8 : (b.width - 8) * std::clamp(percent, 0, 100) / 100;
  target.fill(fui::makeRect(b.x + 4, b.y + 4, fill, b.height - 8), ink);
  if (charging) {
    int x = b.x + 13, y = b.y - 1;
    target.line({int16_t(x + 5), int16_t(y)}, {int16_t(x), int16_t(y + 11)}, 2, paper);
    target.line({int16_t(x), int16_t(y + 11)}, {int16_t(x + 7), int16_t(y + 9)}, 2, paper);
    target.line({int16_t(x + 7), int16_t(y + 9)}, {int16_t(x + 2), int16_t(y + 20)}, 2, paper);
  }
}
void batteryParity() {
  for (auto color : {fui::Color::White, fui::Color::Black})
    for (bool charging : {false, true})
      for (int percent = 0; percent <= 100; ++percent) {
        std::vector<uint8_t> old(80 * 40 / 8, color == fui::Color::White ? 0 : 255), now = old;
        Target a(old.data(), 80, 40, 10), b(now.data(), 80, 40, 10);
        referenceBattery(a, fui::makeRect(20, 10, 30, 19), percent, charging, color);
        statusbattery::draw(b, fui::makeRect(20, 10, 30, 19), percent, charging, color);
        CHECK(old == now);
        std::fill(now.begin(), now.end(), color == fui::Color::White ? 0 : 255);
        fui::BatteryIndicatorProps props;
        statusbattery::configure(props);
        props.percent = percent;
        props.charging = charging;
        props.color = color;
        fui::paintBatteryIndicator(b, fui::makeRect(20, 10, 34, 19), props);
        CHECK(old == now);
      }
}
void render(const char* output) {
  Font small(toybox_10), body(toybox_14), title(toybox_20), prose(ui_prose_14);
  for (bool wide : {false, true})
    for (bool en : {false, true}) {
      const int w = wide ? 800 : 480, h = wide ? 480 : 800;
      std::vector<uint8_t> bits(w * h / 8, 255);
      Target target(bits.data(), w, h, w / 8);
      target.setFont(0, small.font);
      target.setFont(1, body.font);
      target.setFont(2, title.font);
      target.setFont(3, small.font);
      target.setFont(4, prose.font);
      auto device = target.deviceContext();
      device.hasTouch = true;
      device.safeArea = {10, 1, 0, 1};
      toybox::Interactions hits;
      fui::InputSnapshot input{};
      toybox::Frame frame(target, device, input, hits);
      toybox::Screen screen(frame);
      I18N.setLanguage(en ? Language::EN : Language::ES);
      chrome::lightHeader = false;
      pixelui::Header header;
      header.title = tr(STR_LIBRARY);
      header.back = true;
      header.battery = 76;
      pixelui::header(screen, header);
      pixelui::begin(screen, 0);
      libraryui::filterTabs(screen, 0, 2);
      const auto grid = libraryui::gridLayout(device, screen.body());
      for (int n = 0; n < grid.capacity(); ++n) {
        auto r = grid.card(n);
        target.stroke(r, fui::Paint::solid(fui::Color::Black), 2, 0, fui::CornersAll);
        fui::TextStyle text;
        text.font = 4;
        text.align = fui::TextAlign::Center;
        text.maxLines = 2;
        target.text(fui::makeRect(r.x + 8, r.y + 20, r.width - 16, 60), n % 2 ? "Don Quijote" : "La Odisea", text);
      }
      pageui::build(screen, grid.pager, 1, 8, 7, -1, 7, 6);
      const unsigned beforeNotice = target.fillCount;
      libraryui::loadingNotice(target, grid.area, tr(STR_LIBRARY_LOADING_COVERS));
      CHECK(target.fillCount == beforeNotice + 3);
      const auto& outer = target.fills[beforeNotice % target.fills.size()];
      const auto& borderFill = target.fills[(beforeNotice + 1) % target.fills.size()];
      const auto& body = target.fills[(beforeNotice + 2) % target.fills.size()];
      CHECK(outer.color == fui::Color::Black);
      CHECK(borderFill.color == fui::Color::White);
      CHECK(body.color == fui::Color::Black);
      CHECK(outer.rect.x < borderFill.rect.x && borderFill.rect.x < body.rect.x);
      CHECK(outer.rect.right() > borderFill.rect.right() && borderFill.rect.right() > body.rect.right());
      CHECK(!hits.overflowed());
      CHECK(!target.runs.empty());
      auto& notice = target.runs.back();
      CHECK(notice.style.color == fui::Color::White);
      CHECK(notice.text == tr(STR_LIBRARY_LOADING_COVERS));
      CHECK(notice.rect.x >= grid.area.x && notice.rect.right() <= grid.area.right());
      CHECK(notice.rect.y >= grid.area.y && notice.rect.bottom() <= grid.area.bottom());
      CHECK(target.measureText(4, notice.text.c_str(), notice.style).width <= notice.rect.width);
      char file[1024];
      std::snprintf(file, sizeof(file), "%s/notice-%s-%s.pbm", output, wide ? "wide" : "portrait", en ? "en" : "es");
      auto* fp = std::fopen(file, "wb");
      CHECK(fp);
      std::fprintf(fp, "P4\n%d %d\n", w, h);
      for (auto& b : bits) b = ~b;
      std::fwrite(bits.data(), 1, bits.size(), fp);
      std::fclose(fp);
    }
}
void quickBatteryBounds(const char* output) {
  Font small(toybox_10), body(toybox_14), title(toybox_20), prose(ui_prose_14);
  for (bool wide : {false, true})
    for (bool light : {false, true}) {
      const int w = wide ? 800 : 480, h = wide ? 480 : 800;
      std::vector<uint8_t> bits(w * h / 8, 255);
      Target target(bits.data(), w, h, w / 8);
      target.setFont(0, small.font);
      target.setFont(1, body.font);
      target.setFont(2, title.font);
      target.setFont(3, small.font);
      target.setFont(4, prose.font);
      auto device = target.deviceContext();
      device.hasTouch = true;
      toybox::Interactions hits;
      fui::InputSnapshot input{};
      toybox::Frame frame(target, device, input, hits);
      toybox::Screen screen(frame);
      chrome::lightHeader = light;
      quickui::Model model;
      model.battery = 76;
      model.charging = true;
      quickui::Work work;
      quickui::build(screen, model, work);
      auto layout = quickui::layout(device, model.light, model.temperature, model.touch);
      bool found = false;
      fui::Rect battery{};
      for (auto r : target.outlines)
        if (r.width == statusbattery::width && r.height == statusbattery::height) {
          found = true;
          battery = r;
          CHECK(r.right() + statusbattery::terminal == layout.header.right());
          CHECK(r.x >= layout.header.x && r.y >= layout.header.y && r.bottom() <= layout.header.bottom());
        }
      CHECK(found);
      bool percentFound = false;
      for (const auto& run : target.runs)
        if (run.text == "76%") {
          percentFound = true;
          CHECK(run.rect.right() + 6 <= battery.x);
        }
      CHECK(percentFound);
      CHECK(!hits.overflowed());
      char file[1024];
      std::snprintf(file, sizeof(file), "%s/quick-%s-%s.pbm", output, wide ? "wide" : "portrait",
                    light ? "light" : "boxed");
      auto* fp = std::fopen(file, "wb");
      CHECK(fp);
      std::fprintf(fp, "P4\n%d %d\n", w, h);
      for (auto& b : bits) b = ~b;
      std::fwrite(bits.data(), 1, bits.size(), fp);
      std::fclose(fp);
    }
}

}  // namespace
#include "grid_contract.h"
int main(int argc, char** argv) {
  contract::gridContractChecks();
  batteryParity();
  render(argc > 1 ? argv[1] : ".");
  quickBatteryBounds(argc > 1 ? argv[1] : ".");
  std::printf("%u pixel/layout checks passed\n", checks);
  return 0;
}

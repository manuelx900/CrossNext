#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/bookprep/BookPreparationScreens.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/TextAlignment.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
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
  bookprep::FontSelection fonts;
  for (const char* name :
       {"", "AtkinsonHyperlegible", "Bookerly", "Literata", "NotoSerif", "OpenDyslexic", "Roboto", "SourceSerif"})
    fonts.add(name);
  fonts.selectOnly(3);
  fonts.fonts[1].selected = true;
  Font normal(ui_prose_14), small(ui_prose_10), pixel(toybox_14), tile(toybox_10);
  for (auto language : {Language::EN, Language::ES})
    for (bool wide : {false, true})
      for (bool light : {false, true})
        for (bool jersey : {false, true})
          for (int view = 0; view < 5; ++view)
            for (int selected : {0, 1, 2}) {
              fonts.selectAll(false);
              if (selected) fonts.selectOnly(3);
              if (selected == 2) fonts.fonts[1].selected = true;
              const int width = wide ? 800 : 480, height = wide ? 480 : 800;
              std::vector<uint8_t> bits(width * height / 8, 255);
              Target target(bits.data(), width, height, width / 8);
              for (int n = 0; n < 8; ++n) target.setFont(n, jersey ? pixel.font : normal.font);
              target.setFont(3, jersey ? tile.font : small.font);
              auto device = target.deviceContext();
              device.safeArea = {9, 7, 3, 7};
              toybox::Interactions interactions;
              const fui::InputSnapshot input{};
              toybox::Frame frame(target, device, input, interactions);
              toybox::Screen screen(frame);
              I18N.setLanguage(language);
              chrome::lightHeader = light;
              pixelui::uppercaseLabels = jersey;
              bookprepui::Model model;
              model.engine = selected == 1 ? EpubEngine::Classic : EpubEngine::Modern;
              model.view = bookprepui::View(view);
              model.fonts = &fonts;
              model.currentFont = 3;
              model.page = 0;
              model.pointSize = 14;
              for (uint8_t pt = 8; pt <= 22; ++pt) model.sizes.add(pt);
              model.sizeMode = selected == 2 ? bookprep::SizeMode::All : bookprep::SizeMode::Single;
              model.status = tr(STR_PREPARE_WORKING);
              model.detail = "Historia de la literatura.epub";
              model.note = "Capítulo 4 / 12";
              model.counts = "24 / 80";
              model.font = "Fuente 2 / 3 · Literata · 14 pt";
              model.done = 104;
              model.total = 240;
              model.running = true;
              bookprepui::build(screen, model);
              CHECK(!interactions.overflowed());
              unsigned toggles = 0, starts = 0, confirm = 0;
              paintclock::notePainted();
              for (size_t n = 0; n < interactions.count(); ++n) {
                const auto& hit = interactions.data()[n];
                if (hit.rect.x < 0 || hit.rect.y < 0 || hit.rect.right() > width || hit.rect.bottom() > height)
                  std::printf("bounds v=%d wide=%d id=%u %d,%d %dx%d\n", view, wide, hit.action, hit.rect.x, hit.rect.y,
                              hit.rect.width, hit.rect.height);
                CHECK(hit.rect.x >= 0 && hit.rect.y >= 0 && hit.rect.right() <= width && hit.rect.bottom() <= height);
                fui::InputSnapshot tap;
                tap.touchReleased = true;
                tap.touchX = hit.rect.x + hit.rect.width / 2;
                tap.touchY = hit.rect.y + hit.rect.height / 2;
                const auto event = interactions.route(tap);
                CHECK(event.action == hit.action && event.value == hit.value);
                toggles += hit.action == bookprepui::ToggleFont;
                starts += hit.action == bookprepui::Start;
                confirm += hit.action == bookprepui::ConfirmStart;
              }
              // Every visible choice has a physical focus and Confirm path.
              for (size_t n = 0; n < interactions.count(); ++n) {
                fui::InputSnapshot move{};
                move.focusNext = true;
                interactions.route(move);
                fui::Rect focus;
                CHECK(interactions.buttonFocusRect(focus));
                fui::InputSnapshot confirmInput{};
                confirmInput.confirm = true;
                const auto selectedEvent = interactions.route(confirmInput);
                CHECK(selectedEvent.action != fui::NO_ACTION);
              }
              if (model.view == bookprepui::View::Options) CHECK(starts == unsigned(selected > 0));
              if (model.view == bookprepui::View::Fonts) {
                CHECK(toggles == unsigned(std::min(int(fonts.count), bookprepui::pageSize(device))));
                CHECK(starts == 0);
                CHECK(!confirm);
              }
              if (model.view == bookprepui::View::ConfirmMany) {
                CHECK(confirm == 1 && !starts && !toggles);
                bool warning = false;
                const char* notice = tr(STR_PREPARE_ENGINE_HINT);
                for (const auto& run : target.runs) warning |= run.text == notice;
                CHECK(warning);
              }
              if (output && language == Language::ES && light && jersey && selected == 2) {
                char path[1024];
                std::snprintf(path, sizeof(path), "%s/%s-%d.pbm", output,
                              view == 0   ? "options"
                              : view == 1 ? "fonts"
                              : view == 2 ? "sizes"
                              : view == 3 ? "warning"
                                          : "progress",
                              width);
                auto* file = std::fopen(path, "wb");
                CHECK(file);
                std::fprintf(file, "P4\n%d %d\n", width, height);
                for (auto byte : bits) std::fputc(byte ^ 255, file);
                std::fclose(file);
              }
            }
}
}  // namespace
void compactPopup(const char* output) {
  Font normal(ui_prose_14), small(ui_prose_10);
  for (bool wide : {false, true})
    for (bool light : {false, true}) {
      const int width = wide ? 800 : 480, height = wide ? 480 : 800;
      std::vector<uint8_t> bits(width * height / 8, 0xaa);
      Target target(bits.data(), width, height, width / 8);
      for (int n = 0; n < 8; ++n) target.setFont(n, normal.font);
      target.setFont(3, small.font);
      auto device = target.deviceContext();
      toybox::Interactions interactions;
      const fui::InputSnapshot input{};
      toybox::Frame frame(target, device, input, interactions);
      toybox::Screen screen(frame);
      chrome::lightHeader = light;
      bookprepui::Model model;
      model.running = true;
      model.status = tr(STR_INDEXING);
      model.done = 43;
      model.total = 100;
      bookprepui::openedProgress(screen, model);
      CHECK(!interactions.overflowed());
      CHECK(bits.front() == 0xaa && bits.back() == 0xaa);
      bool cancel = false;
      for (size_t n = 0; n < interactions.count(); ++n) {
        const auto& hit = interactions.data()[n];
        cancel |= hit.action == bookprepui::Cancel;
        CHECK(hit.rect.x >= 0 && hit.rect.y >= 0 && hit.rect.right() <= width && hit.rect.bottom() <= height);
      }
      CHECK(cancel);
      if (output) {
        char path[1024];
        std::snprintf(path, sizeof(path), "%s/popup-%d-%d.pbm", output, width, int(light));
        auto* file = std::fopen(path, "wb");
        CHECK(file);
        std::fprintf(file, "P4\n%d %d\n", width, height);
        for (auto byte : bits) std::fputc(byte ^ 255, file);
        std::fclose(file);
      }
    }
}
int main(int argc, char** argv) {
  exercise(argc > 1 ? argv[1] : nullptr);
  compactPopup(argc > 1 ? argv[1] : nullptr);
  std::printf("book preparation UI: %u checks, 0 failed\n", checks);
}

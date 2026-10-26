#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/reading/ReadingStatsScreens.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#pragma GCC diagnostic pop
namespace fui = freeink::ui;
namespace {
int checks = 0;
int failures = 0;
#define CHECK(expr)                                  \
  do {                                               \
    ++checks;                                        \
    if (!(expr)) {                                   \
      ++failures;                                    \
      std::printf("FAIL %d: %s\n", __LINE__, #expr); \
    }                                                \
  } while (0)

// The same glyph bitmaps and metrics as the firmware, adapted to the SDK's
// freestanding framebuffer target. This is a host preview, not a device shot.
struct FontBridge {
  std::array<fui::FontGlyph, 95> glyphs{};
  fui::BitmapFont font{};
  explicit FontBridge(const EpdFontData& source) {
    for (size_t n = 0; n < glyphs.size(); ++n) {
      const EpdGlyph& g = source.glyph[n];
      glyphs[n] = {static_cast<uint16_t>(g.dataOffset),
                   g.width,
                   g.height,
                   static_cast<uint8_t>(fp4::toPixel(g.advanceX)),
                   static_cast<int8_t>(g.left),
                   static_cast<int8_t>(-g.top)};
    }
    font = {source.bitmap, glyphs.data(), 32, 126, source.advanceY, static_cast<uint8_t>(source.ascender), 64, 64, 1};
  }
};

fui::DeviceContext makeDevice(fui::DisplayTarget& target) {
  auto d = target.deviceContext();
  d.hasTouch = true;
  d.safeArea = {10, 0, 0, 0};
  return d;
}
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30};
  fui::DisplayTarget target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;

  Canvas(int w, int h)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(makeDevice(target)),
        frame(target, device, input, interactions),
        screen(frame) {
    target.setFont(3, tiny.font);
    target.setFont(toybox::kSmallFont, small.font);
    target.setFont(toybox::kUiFont, body.font);
    target.setFont(toybox::kDisplayFont, title.font);
  }

  void checkHits() {
    CHECK(!interactions.overflowed());
    paintclock::notePainted();
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& slot = interactions.data()[n];
      const auto& r = slot.rect;
      CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
      const bool dateCell = slot.action == readingui::SelectDay;
      if (r.width < (dateCell ? 24 : 44) || r.height < (dateCell ? 24 : 40))
        std::printf("small action %d value %d at %d,%d %dx%d canvas %dx%d\n", slot.action, slot.value, r.x, r.y,
                    r.width, r.height, width, height);
      CHECK(r.width >= (dateCell ? 24 : 44) && r.height >= (dateCell ? 24 : 40));
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = static_cast<int16_t>(r.x + r.width / 2);
      tap.touchY = static_cast<int16_t>(r.y + r.height / 2);
      const auto event = interactions.route(tap);
      CHECK(event.action == slot.action && event.value == slot.value);
    }
  }

  bool has(fui::ActionId action, int value = 0) const {
    for (size_t n = 0; n < interactions.count(); ++n)
      if (interactions.data()[n].action == action && interactions.data()[n].value == value) return true;
    return false;
  }

  void write(const std::string& path) const {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
      std::perror(path.c_str());
      std::exit(2);
    }
    std::fprintf(file, "P4\n%d %d\n", width, height);
    for (const auto byte : pixels) std::fputc(byte ^ 255, file);
    std::fclose(file);
  }
};

}  // namespace
int main(int argc, char** argv) {
  readingstats::Summary summary;
  summary.anchorDay = readingstats::dayIndex({2026, 10, 26});
  summary.seconds = 250000;
  summary.pages = 800;
  summary.sessions = 30;
  summary.completed = 2;
  readingstats::Book books[5]{};
  for (auto& b : books) {
    readingstats::copyText(b.title, sizeof(b.title), "A very long book title for the reading history");
    b.seconds = 9000;
    b.sessions = 10;
    b.pages = 160;
    b.completed = true;
    b.progressBp = 10000;
  }
  readingstats::PeriodStats period;
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      for (int source = 0; source < 2; ++source)
        for (int tab = 0; tab < 4; ++tab) {
          Canvas c(size.first, size.second);
          readingui::Work work;
          readingui::Model m;
          m.syncAvailable = true;
          m.summary = &summary;
          m.periodStats = &period;
          m.today = summary.anchorDay;
          m.month = 2026 * 12 + 9;
          m.tab = tab;
          m.battery = 100;
          m.source = source ? readingstats::Source::Unified : readingstats::Source::Local;
          m.importedAvailable = true;
          m.books = books;
          m.count = readingui::capacity(c.device);
          m.pageSize = m.count;
          m.bookTotal = 20;
          readingui::build(c.screen, m, work);
          c.checkHits();
          CHECK(c.has(readingui::ImportHistory));
          m.syncAvailable = false;
          Canvas unconfigured(size.first, size.second);
          readingui::build(unconfigured.screen, m, work);
          unconfigured.checkHits();
          CHECK(!unconfigured.has(readingui::ImportHistory));
          m.syncAvailable = true;
          if (argc > 1 && source)
            c.write(std::string(argv[1]) + "/stats-" + std::to_string(int(language)) + "-" +
                    std::to_string(size.first) + "-" + std::to_string(tab) + ".pbm");
          if (tab == 3) {
            m.detail = &books[0];
            Canvas detail(size.first, size.second);
            readingui::build(detail.screen, m, work);
            detail.checkHits();
          }
          if (tab == 2) {
            readingstats::setPeriod(period, readingstats::Period::Month, summary.anchorDay, false);
            m.periodStats = &period;
            m.period = readingstats::Period::Month;
            Canvas p(size.first, size.second);
            readingui::build(p.screen, m, work);
            p.checkHits();
          }
          if (source) {
            m.importedAvailable = false;
            Canvas empty(size.first, size.second);
            readingui::build(empty.screen, m, work);
            empty.checkHits();
          }
        }
      for (int state = 0; state < 4; ++state)
        for (bool setup : {false, true})
          for (bool progress : {false, true}) {
            Canvas c(size.first, size.second);
            readingui::Work work;
            readingui::ImportModel m;
            m.state = static_cast<readingui::ImportState>(state);
            m.setup = setup;
            m.progress = progress;
            m.books = 139;
            m.battery = 100;
            readingui::buildImport(c.screen, m, work);
            c.checkHits();
            if (argc > 1 && !setup)
              c.write(std::string(argv[1]) + "/import-" + std::to_string(int(language)) + "-" +
                      std::to_string(size.first) + "-" + std::to_string(state) + ".pbm");
          }
    }
  }
  std::printf("reading cloud UI: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

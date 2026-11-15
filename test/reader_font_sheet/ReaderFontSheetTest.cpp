#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
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
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, sample{ui_prose_14};
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
    target.setFont(5, sample.font);
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

      if (!((slot.action == 1 && r.height < 40) || (r.width >= 44 && r.height >= 40)))
        std::printf("Small target %dx%d action=%d value=%d rect=%d,%d,%d,%d\n", width, height, int(slot.action),
                    slot.value, r.x, r.y, r.width, r.height);
      CHECK((slot.action == 1 && r.height < 40) || (r.width >= 44 && r.height >= 40));
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
  const std::string proof = argc > 1 ? argv[1] : "/proof";
  for (bool light : {false, true}) {
    chrome::lightHeader = light;
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      for (int page : {0, 1, 2}) {
        Canvas c(size.first, size.second);
        if (size.first < size.second) {
          const auto safe = c.device.safeRect();
          const auto sheet = readertext::sheetRect(c.device);
          CHECK(sheet.y >= safe.y);
          CHECK(sheet.bottom() == safe.bottom());
        }
        readertext::Model model;
        readertext::Work work;
        model.view = readertext::SheetView::Families;
        model.fontCount = 13;
        model.fontPage = page;
        model.fontLabel = [](const void*, int) -> const char* { return "Atkinson Hyperlegible"; };
        readertext::build(c.screen, model, work);
        c.checkHits();
        const int capacity = readertext::fontRowsPerPage(c.device);
        if (light) CHECK(capacity == 6);
        int rows = 0;
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto& hit = c.interactions.data()[n];
          if (hit.action != 6 || hit.value < readertext::FontPick) continue;
          ++rows;
          CHECK(hit.value >= readertext::FontPick + page * capacity);
          CHECK(hit.value < readertext::FontPick + std::min(13, (page + 1) * capacity));
          for (size_t k = n + 1; k < c.interactions.count(); ++k) {
            const auto& other = c.interactions.data()[k].rect;
            CHECK(hit.rect.right() <= other.x || other.right() <= hit.rect.x || hit.rect.bottom() <= other.y ||
                  other.bottom() <= hit.rect.y);
          }
        }
        CHECK(rows == std::max(0, std::min(capacity, 13 - page * capacity)));
        if (!page) c.write(proof + "/fonts-" + std::to_string(int(light)) + "-" + std::to_string(size.first) + ".pbm");
      }
    }
  }
  for (bool light : {false, true}) {
    chrome::lightHeader = light;
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (bool standalone : {false, true})
        for (int extra : {0, 60}) {
          if (extra && (!standalone || size.first > size.second)) continue;
          const int minTop = standalone ? (size.first > size.second ? 180 : 240 + extra) : 0;
          Canvas measure(size.first, size.second);
          const int pages = readertext::layoutPageCount(measure.device, minTop, standalone);
          if (!standalone) CHECK(pages == 2);
          bool indent = false, paragraphs = false;
          for (int page = 0; page < pages; ++page) {
            Canvas c(size.first, size.second);
            readertext::Model m;
            readertext::Work work;
            m.tab = 1;
            m.contentsAvailable = !standalone;
            m.layoutPage = page;
            m.globalSettings = standalone;
            m.standalone = standalone;
            m.minTop = minTop;
            m.spacingLabel = "120%";
            m.alignment = "Justified";
            m.wordSpacing = 100;
            m.characterSpacing = 2;
            m.indent = 2;
            m.paragraphs = true;
            if (standalone) {
              pixelui::Header header;
              header.title = "Text settings";
              header.back = true;
              header.battery = 82;
              pixelui::header(c.screen, header);
              fui::TextStyle sample;
              sample.font = 5;
              sample.maxLines = 2;
              const int top = size.first > size.second ? 98 : 106;
              c.target.text(
                  fui::makeRect(22, top, size.first - 44, minTop - top - 28),
                  "A comfortable reading sample shows the selected size and spacing before you open your next book.",
                  sample);
              pixelui::text(c.target, fui::makeRect(22, minTop - 28, size.first - 44, 24), "Noto Serif / 14 pt", 3);
            }
            readertext::build(c.screen, m, work);
            c.checkHits();
            if (!standalone && size.first < size.second && page == 0) {
              fui::Rect first{}, last{};
              for (size_t n = 0; n < c.interactions.count(); ++n) {
                const auto& hit = c.interactions.data()[n];
                if (hit.value == readertext::AlignmentJustify) first = hit.rect;
                if (hit.value == readertext::AlignmentBook) last = hit.rect;
              }
              CHECK(first.width == 72 && first.height == 72 && last.width == 72);
              CHECK(std::abs(first.x - (size.first - last.right())) <= 1);
              // The group is centered in the area below its 40px title band.
              CHECK(first.y >= readertext::sheetRect(c.device).y && first.bottom() <= size.second - 44);
            }
            if (!standalone && page == 1) {
              CHECK(c.has(6, readertext::Paragraphs));
              CHECK(c.has(6, readertext::IndentUp));
              CHECK(c.has(6, readertext::WordUp));
              CHECK(c.has(6, readertext::CharacterUp));
            }
            CHECK(readertext::sheetRect(c.device, false, minTop).y >= minTop);
            indent = indent || c.has(6, readertext::IndentUp);
            paragraphs = paragraphs || c.has(6, readertext::Paragraphs);
            fui::InputSnapshot next;
            next.focusNext = true;
            fui::InputSnapshot confirm;
            confirm.confirm = true;
            for (size_t n = 0; n < c.interactions.count(); ++n) {
              c.interactions.route(next);
              CHECK(c.interactions.route(confirm).action != fui::NO_ACTION);
            }
            for (size_t n = 0; n < c.interactions.count(); ++n) {
              const auto& a = c.interactions.data()[n];
              if (a.action != 6) continue;
              for (size_t k = n + 1; k < c.interactions.count(); ++k) {
                const auto& b = c.interactions.data()[k];
                if (b.action != 6) continue;
                CHECK(a.rect.right() <= b.rect.x || b.rect.right() <= a.rect.x || a.rect.bottom() <= b.rect.y ||
                      b.rect.bottom() <= a.rect.y);
              }
            }
            c.write(proof + "/layout-" + std::to_string(int(light)) + "-" + std::to_string(size.first) + "-" +
                    std::to_string(page) + (standalone ? "-settings" : "-book") + ".pbm");
          }
          CHECK(indent && paragraphs);
          for (int tab : {0, 3}) {
            Canvas c(size.first, size.second);
            readertext::Model m;
            readertext::Work w;
            m.tab = tab;
            m.contentsAvailable = !standalone;
            m.standalone = standalone;
            m.minTop = minTop;
            m.globalSettings = standalone;
            m.family = "Noto Serif";
            m.embedded = true;
            m.view = tab == 3 ? readertext::SheetView::Style : readertext::SheetView::Controls;
            readertext::build(c.screen, m, w);
            c.checkHits();
            CHECK(c.has(6, tab == 0 ? readertext::SizeUp : readertext::Focus));
            if (tab == 3 && standalone) {
              CHECK(c.has(6, readertext::CopyDefaults));
              Canvas off(size.first, size.second);
              readertext::Work offWork;
              m.useDefaults = false;
              readertext::build(off.screen, m, offWork);
              off.checkHits();
              CHECK(off.has(6, readertext::CopyDefaults));
              for (size_t n = 0; n < c.interactions.count(); ++n) {
                const auto& a = c.interactions.data()[n];
                const auto& b = off.interactions.data()[n];
                CHECK(a.action == b.action && a.value == b.value);
                CHECK(a.rect.x == b.rect.x && a.rect.y == b.rect.y && a.rect.width == b.rect.width &&
                      a.rect.height == b.rect.height);
              }
            }
            if (tab == 3 && !extra)
              c.write(proof + "/style-" + std::to_string(int(light)) + "-" + std::to_string(size.first) +
                      (standalone ? "-settings" : "-book") + ".pbm");
          }
        }
  }
  std::printf("font sheet UI: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

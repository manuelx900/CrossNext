#include <FreeInkApp.h>
#include <FreeInkUIDisplayTarget.h>
#include <FreeInkUIIcon.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/library/LibraryMenu.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/ToyboxTokens.h"
#include "../../src/components/icons/headerIcons.h"
#include "../../src/components/icons/listIcons.h"
#include "../../src/components/icons/search32.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/book_bold_10.h"
#include "../../src/apps_local/ui/fonts/book_text_10.h"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#pragma GCC diagnostic pop

namespace fui = freeink::ui;
namespace {
int checks = 0, failures = 0;
#define CHECK(value)                                  \
  do {                                                \
    ++checks;                                         \
    if (!(value)) {                                   \
      ++failures;                                     \
      std::printf("FAIL %d: %s\n", __LINE__, #value); \
    }                                                 \
  } while (0)

struct Font {
  std::array<fui::FontGlyph, 95> glyphs{};
  fui::BitmapFont font{};
  explicit Font(const EpdFontData& source) {
    for (size_t n = 0; n < glyphs.size(); ++n) {
      const auto& g = source.glyph[n];
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
class BookTarget : public fui::DrawTarget {
  fui::DisplayTarget target;

 public:
  BookTarget(uint8_t* pixels, int width, int height, int stride, fui::Orientation orientation)
      : target(pixels, width, height, stride, orientation) {}
  void setFont(fui::FontId id, const fui::BitmapFont& font) { target.setFont(id, font); }
  fui::DeviceContext deviceContext() const { return target.deviceContext(); }
  fui::Rect clipRect() const override { return target.clipRect(); }
  bool setClipRect(fui::Rect rect) override { return target.setClipRect(rect); }
  int16_t lineHeight(fui::FontId id) const override { return target.lineHeight(id); }
  fui::Size measureText(fui::FontId font, const char* text, fui::TextStyle style) const override {
    if (font == 4 && style.bold) font = 5;
    return target.measureText(font, text, style);
  }
  void text(fui::Rect box, const char* text, fui::TextStyle style) override {
    if (std::strcmp(text, "REFRESH FREQUENCY") == 0 || std::strcmp(text, "FRECUENCIA DE REFRESCO") == 0)
      CHECK(style.font == pixelui::kSmall && style.align == fui::TextAlign::Center);
    if (std::strcmp(text, "30 PAGES") == 0)
      CHECK(style.font == pixelui::kSmall && style.align == fui::TextAlign::Center);
    if (std::strncmp(text, "Chapter ", 8) == 0) {
      CHECK(style.font == 4 && !style.bold);
      int lines = 0;
      fui::layoutText(target, box, text, style, [&](const char*, fui::Rect) { ++lines; });
      CHECK(lines == (box.width < 500 ? 2 : 1));
    }
    if (style.font == 4 && style.bold) style.font = 5;
    target.text(box, text, style);
  }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius, uint8_t corners) override { target.fill(r, p, radius, corners); }
  void stroke(fui::Rect r, fui::Paint p, uint8_t width, uint8_t radius, uint8_t corners) override {
    target.stroke(r, p, width, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t width, fui::Paint p) override { target.line(a, b, width, p); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { target.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef b, fui::BitmapMode m, fui::Paint p, fui::Rotation rotation) override {
    target.bitmap(r, b, m, p, rotation);
  }
};
struct Canvas {
  std::vector<uint8_t> pixels;
  Font small{toybox_14}, body{toybox_20}, title{toybox_30}, tab{toybox_10};
  Font prose{book_text_10}, bold{book_bold_10};
  BookTarget target;
  fui::DeviceContext device;
  explicit Canvas(int width, int height)
      : pixels((width + 7) / 8 * height, 255),
        target(pixels.data(), width, height, (width + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(target.deviceContext()) {
    target.setFont(0, small.font);
    target.setFont(1, body.font);
    target.setFont(2, title.font);
    target.setFont(3, tab.font);
    target.setFont(4, prose.font);
    target.setFont(5, bold.font);
    device.hasTouch = true;
  }
};
fui::ActionEvent tap(fui::InteractionBuffer<24>& hits, int x, int y) {
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(x);
  input.touchY = static_cast<int16_t>(y);
  return hits.route(input);
}
}  // namespace

void bookListChecks(const char* outputPrefix) {
  const char* names[] = {"Novelas",
                         "Ciencia e historia",
                         "Alice's Adventures in Wonderland - Lewis Carroll",
                         "Demo - Ciudad de papel",
                         "Demo - El bosque de tinta",
                         "Demo - Mapas del cielo",
                         "Demo - The Last Garden",
                         "Demo - The Quiet Harbor"};
  for (const auto shape : {fui::Size{480, 800}, fui::Size{800, 480}}) {
    for (bool library : {false, true}) {
      Canvas canvas(shape.width, shape.height);
      fui::InteractionBuffer<24> hits;
      fui::InputSnapshot input;
      fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
      fui::Screen<24> screen(frame, toybox::themeTokens());
      pixelui::Header heading;
      heading.title = library ? "BIBLIOTECA" : "BOOKS";
      heading.back = true;
      heading.battery = 100;
      pixelui::header(canvas.target, canvas.device, heading);
      pixelui::begin(screen, libraryui::kFooterHeight);
      if (library) {
        fui::TabItem tabs[] = {fui::tabItem(0, true, true, "RECIENTES"), fui::tabItem(1, false, true, "TITULO"),
                               fui::tabItem(2, false, true, "AUTOR")};
        pixelui::tabs(screen, tabs, 3, 90, pixelui::kSmall);
      }
      constexpr int count = sizeof(names) / sizeof(names[0]);
      fui::ListItem items[count];
      for (int n = 0; n < count; ++n) {
        items[n].label = names[n];
        items[n].actionValue = n;
        items[n].labelBold = !library && n < 2;
        items[n].icon = fui::bitmapFromIcon(items[n].labelBold ? icon_folder_32 : icon_book_32);
        if (!library && !items[n].labelBold) {
          items[n].value = "epub";
        }
      }
      fui::ListNav nav;
      fui::ListProps props;
      libraryui::styleBookRows(props, canvas.target);
      props.items = items;
      props.count = count;
      props.selectedIndex = 0;
      props.action = 91;
      props.nav = &nav;
      const auto band = screen.body();
      pixelui::list(screen, props, false, false, 2);
      CHECK(props.labelText.font == 4);
      CHECK(ui_prose_10.advanceY == book_bold_10.advanceY);
      CHECK(book_text_10.advanceY == ui_prose_10.advanceY);
      CHECK(book_text_10.ascender == ui_prose_10.ascender);
      for (size_t n = 0; n < sizeof(book_text_10Glyphs) / sizeof(book_text_10Glyphs[0]); ++n) {
        CHECK(book_text_10Glyphs[n].advanceX == ui_prose_10Glyphs[n].advanceX);
      }
      for (const auto& item : items) {
        CHECK(fui::measureListRow(canvas.target, nullptr, band.width, props, item).height == props.rowHeight);
        CHECK(fui::listLabelTextStyle(props, item).bold == item.labelBold);
      }
      CHECK(canvas.target.lineHeight(props.labelText.font) > canvas.target.lineHeight(props.valueText.font));
      CHECK(tap(hits, band.x + 60, band.y + 25).value == 0);
      CHECK(!hits.overflowed());
      if (outputPrefix) {
        char fileName[512];
        std::snprintf(fileName, sizeof(fileName), "%s-%s-%dx%d.pgm", outputPrefix, library ? "library" : "selector",
                      shape.width, shape.height);
        auto* file = std::fopen(fileName, "wb");
        CHECK(file != nullptr);
        if (file) {
          std::fprintf(file, "P5\n%d %d\n255\n", shape.width, shape.height);
          for (int y = 0; y < shape.height; ++y)
            for (int x = 0; x < shape.width; ++x)
              std::fputc(canvas.pixels[y * ((shape.width + 7) / 8) + x / 8] & (0x80 >> (x % 8)) ? 255 : 0, file);
          std::fclose(file);
        }
      }
      // Wrapped rows must leave every index reachable by the measured page size.
      bool seen[count]{};
      for (int pass = 0; pass < count; ++pass) {
        hits.clear();
        props.topIndex = nav.top;
        props.selectedIndex = nav.top;
        pixelui::list(screen, props, false, false, 1);
        CHECK(nav.drawnRows > 0);
        for (int n = nav.top; n < nav.top + nav.drawnRows && n < count; ++n) seen[n] = true;
        if (nav.top + nav.drawnRows >= count) break;
        nav.requestScroll(nav.pageRows());
        screen.syncListViewport(nav, props, count);
      }
      for (bool reachable : seen) CHECK(reachable);
    }
  }
}

void chapterChecks(const char* outputPrefix) {
  for (const auto shape : {fui::Size{480, 800}, fui::Size{800, 480}}) {
    for (bool fixed : {false, true}) {
      bool seen[16]{};
      Canvas canvas(shape.width, shape.height);
      const int capacity = readertext::sheetRowsPerPage(canvas.device, fixed, 2);
      CHECK(capacity == (shape.width > shape.height ? 2 : 4));
      const int pages = (16 + capacity - 1) / capacity;
      for (int page = 0; page < pages; ++page) {
        fui::InteractionBuffer<24> hits;
        fui::InputSnapshot input;
        fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
        fui::Screen<24> screen(frame, toybox::themeTokens());
        readertext::Work work;
        readertext::Model model;
        model.tab = 2;
        model.fixed = fixed;
        model.contentsAvailable = true;
        model.listCount = 16;
        model.listPage = page;
        model.currentChapter = 0;
        model.listLabel = [](const void*, int n, char* text, size_t size) {
          std::snprintf(text, size, "Chapter %d. A long chapter title with several words", n + 1);
        };
        readertext::build(screen, model, work);
        int chapters = 0;
        for (size_t n = 0; n < hits.count(); ++n) {
          const auto& hit = hits.data()[n];
          if (hit.action != 6 || hit.value < readertext::ChapterPick || hit.value >= readertext::MorePick) continue;
          const int index = hit.value - readertext::ChapterPick;
          CHECK(index >= 0 && index < 16 && !seen[index]);
          seen[index] = true;
          ++chapters;
          CHECK(hit.rect.x >= 0 && hit.rect.right() <= shape.width);
          CHECK(hit.rect.y >= 0 && hit.rect.bottom() <= shape.height);
          const auto event = tap(hits, hit.rect.x + 12, hit.rect.y + 12);
          CHECK(event.action == 6 && event.value == hit.value);
          if (shape.height > shape.width) CHECK(hit.rect.height >= 2 * canvas.target.lineHeight(4));
        }
        CHECK(chapters == std::min(capacity, 16 - page * capacity));
        CHECK(!hits.overflowed());
        if (outputPrefix && page == 0 && !fixed) {
          char fileName[512];
          std::snprintf(fileName, sizeof(fileName), "%s-index-%dx%d.pgm", outputPrefix, shape.width, shape.height);
          auto* file = std::fopen(fileName, "wb");
          CHECK(file != nullptr);
          if (file) {
            std::fprintf(file, "P5\n%d %d\n255\n", shape.width, shape.height);
            for (int y = 0; y < shape.height; ++y)
              for (int x = 0; x < shape.width; ++x)
                std::fputc(canvas.pixels[y * ((shape.width + 7) / 8) + x / 8] & (0x80 >> (x % 8)) ? 255 : 0, file);
            std::fclose(file);
          }
        }
      }
      for (bool reachable : seen) CHECK(reachable);
    }
  }
}

void fixedRefreshChecks() {
  const auto language = I18N.getLanguage();
  for (auto current : {Language::EN, Language::ES}) {
    I18N.setLanguage(current);
    for (const auto shape : {fui::Size{480, 800}, fui::Size{800, 480}}) {
      int layoutHeight = 0;
      for (int tab : {1, 3}) {
        Canvas canvas(shape.width, shape.height);
        fui::InteractionBuffer<24> hits;
        fui::InputSnapshot input;
        fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
        fui::Screen<24> screen(frame, toybox::themeTokens());
        readertext::Work work;
        readertext::Model model;
        model.fixed = true;
        model.tab = tab;
        model.listCount = 1;
        model.listLabel = [](const void*, int, char* out, size_t size) {
          std::snprintf(out, size, "%s", tr(STR_REFRESH_FREQ));
        };
        model.listValue = [](const void*, int, char* out, size_t size) { std::snprintf(out, size, "30 pages"); };
        readertext::build(screen, model, work);
        const int control = tab == 1 ? readertext::Placement : readertext::MorePick;
        bool found = false;
        for (size_t n = 0; n < hits.count(); ++n) {
          const auto& hit = hits.data()[n];
          if (hit.action != 6 || hit.value != control) continue;
          found = true;
          if (tab == 1)
            layoutHeight = hit.rect.height;
          else
            CHECK(hit.rect.height == layoutHeight);
          CHECK(hit.rect.height >= 44);
          CHECK(hit.rect.bottom() <= shape.height);
          const auto event = tap(hits, hit.rect.x + 12, hit.rect.y + 12);
          CHECK(event.action == 6 && event.value == control);
        }
        CHECK(found);
        CHECK(!hits.overflowed());
      }
    }
  }
  I18N.setLanguage(language);
}

int main(int argc, char** argv) {
  bookListChecks(argc > 1 ? argv[1] : nullptr);
  chapterChecks(argc > 1 ? argv[1] : nullptr);
  fixedRefreshChecks();
  std::printf("Book lists: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

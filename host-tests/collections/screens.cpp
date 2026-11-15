#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/library/CollectionActions.h"
#include "../../src/apps_local/library/CollectionScreens.h"
#include "../../src/apps_local/library/LibraryControls.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/TextAlignment.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/book_text_10.h"
#include "../../src/apps_local/ui/fonts/reading_serif_11.h"
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

void dispatchChecks() {
  using App = fui::FreeInkApp<24, 6>;
  std::vector<uint8_t> bits(480 * 800 / 8, 255);
  Target target(bits.data(), 480, 800, 60);
  Font font(ui_prose_14);
  target.setFont(1, font.font);
  App app(target, target.deviceContext());
  unsigned dispatched = 0;
  const auto handler = [](const fui::ActionEvent&, void* context) { ++*static_cast<unsigned*>(context); };
  app.on(1, handler, &dispatched);
  collectionui::bindActions(app, handler, &dispatched);
  CHECK(!app.handlerOverflowed());
  app.setScreen([](App::ScreenType& screen, void*) {
    for (int n = 0; n < 6; ++n)
      screen.frame().hit(fui::makeRect(0, n * 50, 200, 48), collectionui::Command, n, fui::InputTouch);
    screen.frame().hit(fui::makeRect(0, 300, 200, 48), collectionui::Tab, 2, fui::InputTouch);
    screen.frame().hit(fui::makeRect(0, 350, 200, 48), collectionui::Options, 4, fui::InputTouch);
    screen.frame().hit(fui::makeRect(0, 400, 200, 48), collectionui::Page, 8, fui::InputTouch);
  });
  app.render();
  for (int n = 0; n < 9; ++n) {
    fui::InputSnapshot input;
    input.touchX = 100;
    input.touchY = n * 50 + 24;
    input.touchPressed = true;
    app.route(input);
    input.touchPressed = false;
    input.touchReleased = true;
    const auto event = app.route(input);
    CHECK(bool(event));
    CHECK(dispatched == unsigned(n + 1));
  }
}
void editorAndReaderChecks(const char* output) {
  Font normal(ui_prose_14), small(ui_prose_10), pixelFont(toybox_14), pixelSmall(toybox_10), serif(reading_serif_11),
      bookFont(book_text_10);
  for (bool wide : {false, true})
    for (bool light : {false, true})
      for (bool pixel : {false, true}) {
        const int w = wide ? 800 : 480, h = wide ? 480 : 800;
        std::vector<uint8_t> bits(w * h / 8, 255);
        Target target(bits.data(), w, h, w / 8);
        for (int n = 0; n < 8; ++n) target.setFont(n, pixel ? pixelFont.font : normal.font);
        target.setFont(3, pixel ? pixelSmall.font : small.font);
        auto device = target.deviceContext();
        device.hasTouch = true;
        toybox::Interactions hits;
        fui::InputSnapshot input{};
        toybox::Frame frame(target, device, input, hits);
        toybox::Screen screen(frame);
        I18N.setLanguage(Language::ES);
        chrome::lightHeader = light;
        pixelui::uppercaseLabels = pixel;
        pixelui::begin(screen);
        const auto before = screen.body();
        libraryui::filterTabs(screen, 0, 2, 3);
        const auto grid = libraryui::gridLayout(device, screen.body());
        CHECK(grid.card(0).height - 4 == libraryui::preparationCoverHeight(device, false));
        CHECK(libraryui::preparationCoverHeight(device, false) >
              libraryui::preparationCoverHeight(device, false, libraryui::kFooterHeight));
        CHECK(screen.body().y == before.y + libraryui::filterTabsHeight(device) + libraryui::kTabsGap);
        libraryui::HeaderWork heading;
        libraryui::header(screen, heading, tr(STR_COLLECTIONS), false, {}, {}, 0, 0, collectionui::Command, true,
                          collectionui::Options);
        const auto safe = device.safeRect();
        const auto headerGeometry = pixelui::geometry(device);
        fui::InputSnapshot menuTap;
        menuTap.touchReleased = true;
        menuTap.touchX = safe.right() - 30;
        menuTap.touchY = headerGeometry.back.y + headerGeometry.back.height / 2;
        CHECK(hits.route(menuTap).action == collectionui::Options);
        const int y = screen.body().y;
        target.setFont(4, pixel ? bookFont.font : small.font);
        const int rowHeight = collectionui::editorRowHeight(target);
        collectionui::editorRow(screen, fui::makeRect(before.x, y, before.width, rowHeight), "Libros de prueba", true,
                                true, false, false, 10, 11, 7, "", pixel);
        for (const auto& run : target.runs)
          if (run.text == "Libros de prueba") CHECK(run.style.bold == (pixel && !light));
        collectionui::editorRow(screen, fui::makeRect(before.x, y + rowHeight, before.width, rowHeight),
                                "Breves respuestas a las grandes preguntas - Stephen Hawking", false, false, false,
                                true, 10, 11, 8, ".epub");
        for (const auto& run : target.runs) {
          if (run.text.rfind("Breves respuestas", 0) == 0) {
            CHECK(run.rect.y >= y + rowHeight + collectionui::EditorPadding);
            CHECK(run.rect.bottom() <= y + 2 * rowHeight - collectionui::EditorPadding);
            CHECK(run.rect.height >=
                  fui::measureWrappedText(target, run.text.c_str(), run.style, run.rect.width).height);
            CHECK(run.rect.right() < before.right() - 12 - target.measureText(3, ".epub", {}).width);
          }
          if (run.text == ".epub") CHECK(run.rect.right() == before.right() - 12);
        }
        const auto tap = [&](int x, int yy) {
          fui::InputSnapshot event;
          event.touchReleased = true;
          event.touchX = int16_t(x);
          event.touchY = int16_t(yy);
          return hits.route(event);
        };
        auto event = tap(before.x + 24, y + rowHeight / 2);
        CHECK(event.action == 11 && event.value == 7);
        event = tap(before.x + 100, y + rowHeight / 2);
        CHECK(event.action == 10 && event.value == 7);
        event = tap(before.x + 24, y + rowHeight + rowHeight / 2);
        CHECK(event.action == 11 && event.value == 8);
        event = tap(before.x + 100, y + rowHeight + rowHeight / 2);
        CHECK(event.action == 10 && event.value == 8);
        CHECK(!hits.overflowed());

        char editorFile[512];
        std::fill(bits.begin(), bits.end(), 255);
        target.runs.clear();
        hits.clear();
        toybox::Frame editorFrame(target, device, input, hits);
        toybox::Screen editor(editorFrame);
        libraryui::HeaderWork editorHeading;
        libraryui::header(editor, editorHeading, "Hola", false, {}, {}, 0, 0, 0, false, 0);
        pixelui::begin(editor, 64);
        char selected[64];
        std::snprintf(selected, sizeof(selected), tr(STR_COLLECTION_SELECTED), 7u);
        pixelui::text(editor.target(), editor.takeTop(34, 6), selected, 4);
        pixelui::text(editor.target(), editor.takeTop(36, 6), "/Ciencia", 3);
        const auto editorBody = editor.body();
        const char* examples[] = {"Breves respuestas a las grandes preguntas - Stephen Hawking",
                                  "Eclipses - José María Maza Sancho",
                                  "El universo en tu mano - Christophe Galfard",
                                  "Física de lo imposible - Michio Kaku",
                                  "Guía del universo para escépticos - Steven Novella",
                                  "Por qué recordamos - Charan Ranganath",
                                  "Somos polvo de estrellas - José María Maza",
                                  "Astrofísica para gente con prisa",
                                  "Cosmos",
                                  "El mundo y sus demonios",
                                  "Una breve historia del tiempo",
                                  "El tejido del cosmos"};
        fui::ListNav editorNav;
        const auto editorViewport = collectionui::editorViewport(editor, editorNav, 12);
        const int visible = editorViewport.count;
        CHECK(visible > std::max(1, int(editorBody.height - 56) / std::max(68, 2 * int(target.lineHeight(4)) + 20)));
        CHECK(editorNav.inputPageRows() == visible);
        for (int n = 0; n < visible; ++n) {
          collectionui::editorRow(editor,
                                  fui::makeRect(editorViewport.rows.x, editorViewport.rows.y + n * rowHeight,
                                                editorViewport.rows.width, rowHeight),
                                  examples[n], false, n < 7, false, n + 1 == visible, 10, 11, n, ".epub");
        }
        fui::drawListScrollIndicator(editor.target(), editorBody, 12, visible, 0);
        dashboard::button(editor, fui::makeRect(10, h - 56, w - 20, 48), tr(STR_COLLECTION_SAVE), 13, 0, true);
        CHECK(!hits.overflowed());
        std::snprintf(editorFile, sizeof(editorFile), "%s/editor-%s-%s-%s.pbm", output, wide ? "wide" : "portrait",
                      light ? "light" : "boxed", pixel ? "pixel" : "regular");
        auto* editorOut = std::fopen(editorFile, "wb");
        CHECK(editorOut);
        std::fprintf(editorOut, "P4\n%d %d\n", w, h);
        for (auto byte : bits) {
          const uint8_t value = ~byte;
          std::fwrite(&value, 1, 1, editorOut);
        }
        std::fclose(editorOut);
        editorNav.requestScroll(1);
        const auto scrolled = collectionui::editorViewport(editor, editorNav, 31);
        CHECK(scrolled.first == 1 && editorNav.selected == 0);
        editorNav.requestScroll(1000);
        const auto last = collectionui::editorViewport(editor, editorNav, 31);
        CHECK(last.first + last.count == 31 && editorNav.selected == 0);
        editorNav.requestSelection(0);
        CHECK(collectionui::editorViewport(editor, editorNav, 31).first == 0);
        CHECK(collectionui::editorViewport(editor, editorNav, 0).count == 0);

        target.runs.clear();
        target.outlines.clear();
        hits.clear();
        target.setFont(4, pixel ? pixelSmall.font : serif.font);
        toybox::Frame readerFrame(target, device, input, hits);
        toybox::Screen readerScreen(readerFrame);
        readertext::Model model;
        readertext::Work work;
        model.tab = 2;
        model.contentsAvailable = true;
        model.listCount = 10;
        model.currentChapter = 0;
        model.listLabel = [](const void*, int, char* text, size_t size) {
          std::snprintf(text, size, "Primer capítulo con un título suficientemente largo para dos líneas");
        };
        readertext::build(readerScreen, model, work);
        int chapters = 0;
        for (const auto& run : target.runs) {
          CHECK(run.text != tr(STR_TEXT_SCOPE_BOOK));
          if (run.text.rfind("Primer capítulo", 0) != 0) continue;
          ++chapters;
          CHECK(run.rect.height >= target.lineHeight(run.style.font));
          int lines = 0;
          fui::layoutText(target, run.rect, run.text.c_str(), run.style, [&](const char*, fui::Rect) { ++lines; });
          CHECK(run.rect.height >= lines * target.lineHeight(run.style.font));
          CHECK(run.rect.y >= 0 && run.rect.bottom() <= h);
        }
        CHECK(chapters == (wide ? 2 : 4));
        CHECK(!hits.overflowed());
        char file[512];
        std::snprintf(file, sizeof(file), "%s/reader-%s-%s-%s.pbm", output, wide ? "wide" : "portrait",
                      light ? "light" : "boxed", pixel ? "pixel" : "regular");
        auto* out = std::fopen(file, "wb");
        CHECK(out);
        std::fprintf(out, "P4\n%d %d\n", w, h);
        for (auto& b : bits) b = ~b;
        std::fwrite(bits.data(), 1, bits.size(), out);
        std::fclose(out);
      }
}
void exercise(const char* output) {
  Font normal(ui_prose_14), small(ui_prose_10);
  for (bool wide : {false, true})
    for (bool light : {false, true})
      for (bool pixel : {false, true})
        for (bool english : {false, true})
          for (bool list : {false, true}) {
            const int w = wide ? 800 : 480, h = wide ? 480 : 800;
            std::vector<uint8_t> bits(w * h / 8, 255);
            Target target(bits.data(), w, h, w / 8);
            Font pixelFont(toybox_14), pixelSmall(toybox_10);
            for (int n = 0; n < 8; ++n) target.setFont(n, pixel ? pixelFont.font : normal.font);
            target.setFont(3, pixel ? pixelSmall.font : small.font);
            auto device = target.deviceContext();
            device.hasTouch = true;
            device.safeArea = {10, 1, 0, 1};
            toybox::Interactions hits;
            fui::InputSnapshot input{};
            toybox::Frame frame(target, device, input, hits);
            toybox::Screen screen(frame);
            I18N.setLanguage(english ? Language::EN : Language::ES);
            chrome::lightHeader = light;
            pixelui::uppercaseLabels = pixel;
            pixelui::Header header;
            header.title = tr(STR_LIBRARY);
            header.back = true;
            header.battery = 100;
            pixelui::header(screen, header);
            pixelui::begin(screen, 64);
            libraryui::filterTabs(screen, 1, 2);
            auto grid = collectionui::layout(device, screen.body());
            const char* spanish[] = {"Ciencia y naturaleza",  "Libros favoritos",      "Lecturas pendientes",
                                     "Historias para viajar", "Lecturas de noviembre", "Clásicos de la literatura"};
            const char* en[] = {"Science and nature",     "Favorite books",   "Reading queue",
                                "Stories for travelling", "November reading", "Literary classics"};
            auto names = english ? en : spanish;
            const char* count = list ? (english ? "12 books" : "12 libros") : "32768";
            const int cap = list ? std::max(1, (screen.body().height - 56) / collectionui::ListPitch) : grid.capacity();
            for (int n = 0; n < cap && n < 6; ++n) {
              if (list) {
                auto body = screen.body();
                collectionui::row(
                    screen,
                    fui::makeRect(body.x, body.y + n * collectionui::ListPitch, body.width, collectionui::ListHeight),
                    names[n], count, n == 0, 1, n, 6);
              } else
                collectionui::card(screen, grid.card(n), names[n], count, n == 0, 1, n, 6);
            }
            pageui::build(screen, grid.pager, 1, 3, 7, -1, 7, 6);
            dashboard::button(screen, fui::makeRect(11, h - 56, w - 22, 48), tr(STR_COLLECTION_NEW), 3, 0, true);

            for (const auto& r : target.outlines) CHECK(r.x >= 0 && r.y >= 0 && r.right() <= w && r.bottom() <= h);
            CHECK(!hits.overflowed());
            for (size_t n = 0; n < target.runs.size(); ++n)
              if (target.runs[n].text == count) {
                CHECK(n > 0);
                CHECK(target.runs[n - 1].rect.bottom() <= target.runs[n].rect.y);
                if (target.measureText(target.runs[n].style.font, count, target.runs[n].style).width >
                    target.runs[n].rect.width)
                  std::printf("Count %s: needed %d, available %d; wide=%d pixel=%d list=%d\n", count,
                              target.measureText(target.runs[n].style.font, count, target.runs[n].style).width,
                              target.runs[n].rect.width, wide, pixel, list);
                CHECK(target.measureText(target.runs[n].style.font, count, target.runs[n].style).width <=
                      target.runs[n].rect.width);
              }
            if (light)
              for (int i = 0; i < chrome::headerHeight * w / 8; ++i) bits[i] = ~bits[i];
            char file[512];
            std::snprintf(file, sizeof(file), "%s/collections-%s-%s-%s-%s-%s.pbm", output, wide ? "wide" : "portrait",
                          light ? "light" : "boxed", pixel ? "pixel" : "regular", english ? "en" : "es",
                          list ? "list" : "grid");
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
  dispatchChecks();
  editorAndReaderChecks(argc > 1 ? argv[1] : ".");
  exercise(argc > 1 ? argv[1] : ".");
  std::printf("%u collection layout checks passed\n", checks);
}

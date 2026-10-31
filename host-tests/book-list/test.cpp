#include <FreeInkApp.h>
#include <FreeInkUIDisplayTarget.h>
#include <FreeInkUIIcon.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/library/LibraryControls.h"
#include "../../src/apps_local/library/LibraryMenu.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/ThemePreviews.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/ToyboxTokens.h"
#include "../../src/components/icons/headerIcons.h"
#include "../../src/components/icons/listIcons.h"
#include "../../src/components/icons/search32.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../lib/EpdFont/builtinFonts/notosans_8_regular.h"
#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../src/apps_local/ui/fonts/book_bold_10.h"
#include "../../src/apps_local/ui/fonts/book_text_10.h"
#include "../../src/apps_local/ui/fonts/reading_serif_11.h"
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
  std::vector<fui::Rect> outlines;
  const char* watchedPath = nullptr;
  const char* watchedCount = nullptr;
  fui::Rect pathBox{}, countBox{};
  fui::TextStyle countStyle{};
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
    if (watchedPath && (std::strcmp(text, watchedPath) == 0 || std::strncmp(text, "...", 3) == 0)) pathBox = box;
    if (watchedCount && std::strcmp(text, watchedCount) == 0) {
      countBox = box;
      countStyle = style;
    }
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
    if (width) outlines.push_back(r);
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
      CHECK(capacity == 4);
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

void lightListChecks() {
  const auto saved = I18N.getLanguage();
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    char size[32];
    libraryui::fileSize(size, sizeof(size), UINT64_MAX);
    CHECK(std::strcmp(size, "—") == 0);
    libraryui::fileSize(size, sizeof(size), 0);
    CHECK(std::strcmp(size, "0 B") == 0);
    libraryui::fileSize(size, sizeof(size), 1023);
    CHECK(std::strcmp(size, "1023 B") == 0);
    libraryui::fileSize(size, sizeof(size), 1024);
    CHECK(std::strcmp(size, "1 KB") == 0);
    libraryui::fileSize(size, sizeof(size), 2ULL * 1024 * 1024 + 512 * 1024);
    CHECK(std::strcmp(size, lang == Language::EN ? "2.5 MB" : "2,5 MB") == 0);
    libraryui::fileSize(size, sizeof(size), 3ULL * 1024 * 1024 * 1024);
    CHECK(std::strcmp(size, lang == Language::EN ? "3.0 GB" : "3,0 GB") == 0);
    char guarded[4] = {'X', 'X', 'X', 'Z'};
    libraryui::fileSize(guarded, 3, UINT64_MAX);
    CHECK(guarded[2] == '\0' && guarded[3] == 'Z');
    for (const auto shape : {fui::Size{480, 800}, fui::Size{800, 480}}) {
      Canvas canvas(shape.width, shape.height);
      fui::ListItem items[24]{};
      for (auto& item : items) {
        item.label = "A very long book filename that must stay on one line.epub";
        item.value = "17.5 MB";
        item.icon = settingsart::Book;
      }
      bool seen[24]{};
      int start = 0;
      while (start < 24) {
        fui::InteractionBuffer<24> hits;
        fui::InputSnapshot input;
        fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
        fui::Screen<24> screen(frame, toybox::themeTokens());
        pixelui::begin(screen, libraryui::kFooterHeight);
        fui::ListProps props;
        libraryui::styleBookRows(props, canvas.target);
        libraryui::lightRows(props);
        for (int n = 0; n < 24; ++n) items[n].actionValue = n;
        props.items = items;
        props.count = 24;
        props.topIndex = start;
        props.action = 51;
        props.inputMask = fui::InputTouch;
        props.partialTrailingRow = false;
        props.selectedIndex = start;
        CHECK(props.labelText.maxLines == 2 && !props.labelText.bold && props.rowStyles.normal.borderWidth == 0);
        fui::ListNav nav;
        props.nav = &nav;
        fui::list(frame, screen.body(), props);
        libraryui::listFooter(screen, "24 books", 52, -1, 1, start > 0, start + nav.drawnRows < 24);
        int drawn = 0;
        for (size_t n = 0; n < hits.count(); ++n) {
          const auto& hit = hits.data()[n];
          CHECK(hit.rect.x >= 0 && hit.rect.right() <= shape.width && hit.rect.y >= 0 &&
                hit.rect.bottom() <= shape.height);
          const auto event = tap(hits, hit.rect.x + hit.rect.width / 2, hit.rect.y + hit.rect.height / 2);
          CHECK(event.action == hit.action && event.value == hit.value);
          if (hit.action == 51) {
            CHECK(hit.value >= start && hit.value < 24 && !seen[hit.value]);
            seen[hit.value] = true;
            ++drawn;
            CHECK(hit.rect.bottom() <= shape.height - libraryui::kFooterHeight);
          }
          if (hit.action == 52) CHECK(hit.rect.height >= 40 && hit.rect.width >= 44);
        }
        CHECK(drawn > 0 && !hits.overflowed());
        start += drawn;
      }
      for (bool value : seen) CHECK(value);
    }
  }
  I18N.setLanguage(saved);
}

void libraryPreferenceChecks(const char* outputPrefix) {
  using namespace libraryui;
  for (int mask = 0; mask < 16; ++mask) {
    CHECK(acceptsFormat("/books/Book.EPUB", mask) == bool(mask & 1));
    CHECK(acceptsFormat("/books/Book.XtCh", mask) == bool(mask & 2));
    CHECK(acceptsFormat("/books/Book.xtc", mask) == bool(mask & 2));
    CHECK(acceptsFormat("/books/Book.txt", mask) == bool(mask & 4));
    CHECK(acceptsFormat("/books/Book.md", mask) == bool(mask & 8));
    CHECK(acceptsFormat("/books/Book.Markdown", mask) == bool(mask & 8));
    CHECK(!acceptsFormat("/books/book.epub.tmp", mask));
    CHECK(!acceptsFormat("/books/book", mask));
  }
  const auto language = I18N.getLanguage();
  for (auto locale : {Language::EN, Language::ES}) {
    I18N.setLanguage(locale);
    for (bool light : {false, true})
      for (auto shape : {fui::Size{480, 800}, fui::Size{800, 480}}) {
        chrome::lightHeader = light;
        Canvas canvas(shape.width, shape.height);
        fui::InteractionBuffer<24> hits;
        fui::InputSnapshot input;
        fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
        fui::Screen<24> screen(frame, toybox::themeTokens());
        pixelui::begin(screen);
        libraryui::sortBar(screen, tr(STR_LIBRARY_AUTHOR_LAST), 70, true, false);
        bool pick = false, reverse = false;
        for (size_t n = 0; n < hits.count(); ++n) {
          const auto& h = hits.data()[n];
          CHECK(h.rect.height >= 44 && h.rect.x >= 0 && h.rect.right() <= shape.width);
          const auto result = tap(hits, h.rect.x + h.rect.width / 2, h.rect.y + h.rect.height / 2);
          CHECK(result.action == h.action);
          pick |= result.action == 70 && result.value == libraryui::kPickSort;
          reverse |= result.action == 70 && result.value == libraryui::kReverseSort;
        }
        CHECK(pick && reverse);
        for (bool expanded : {false, true}) {
          fui::ListProps books;
          libraryui::bookRows(books, canvas.target, expanded);
          CHECK(books.rowStyles.normal.borderWidth == (light ? 0 : 1));
          CHECK(books.labelText.maxLines == 1 && books.subtitleText.maxLines == 1);
          CHECK(!books.subtitleText.bold && books.subtitleText.font == 3);
          CHECK(books.rowHeight == (expanded ? 80 : 56));
        }
        hits.clear();
        canvas.target.fill(fui::makeRect(0, 0, shape.width, shape.height), fui::Paint::solid(fui::Color::White), 0,
                           fui::CornersAll);
        pixelui::Header heading;
        heading.title = tr(STR_LIBRARY_SETTINGS);
        heading.back = true;
        pixelui::header(canvas.target, canvas.device, heading);
        pixelui::begin(screen);
        fui::ListProps props;
        fui::ListItem rows[int(Preference::Count)];
        preferenceRows(props, rows, {});
        CHECK(props.count == 10 && rows[0].toggle && rows[0].toggleChecked);
        CHECK(!rows[1].toggle && !rows[2].toggle);
        CHECK(rows[3].sectionHeading && rows[6].sectionHeading);
        CHECK(!props.labelText.bold && props.headerText.bold);
        CHECK(props.rowStyles.normal.borderWidth == (light ? 0 : 1));
        for (int n = 3; n < 10; ++n) CHECK(rows[n].toggle && rows[n].toggleChecked);
        props.action = 72;
        fui::ListNav nav;
        screen.syncListViewport(nav, props, 10);
        fui::list(frame, screen.body(), props);
        CHECK(!hits.overflowed());
        if (outputPrefix && !light && locale == Language::EN && shape.width < shape.height) {
          char path[512];
          std::snprintf(path, sizeof(path), "%s-preferences.pgm", outputPrefix);
          auto* file = std::fopen(path, "wb");
          CHECK(file);
          if (file) {
            std::fprintf(file, "P5\n%d %d\n255\n", shape.width, shape.height);
            for (int y = 0; y < shape.height; ++y)
              for (int x = 0; x < shape.width; ++x)
                std::fputc(canvas.pixels[y * ((shape.width + 7) / 8) + x / 8] & (0x80 >> (x % 8)) ? 255 : 0, file);
            std::fclose(file);
          }
        }
      }
  }
  chrome::lightHeader = false;
  I18N.setLanguage(language);
}

void libraryLayoutChecks() {
  using namespace libraryui;
  for (int stored = 0; stored < 256; ++stored) {
    CHECK(layoutFromStored(stored) == (stored == 1 ? Layout::SortBar : Layout::Classic));
    const int count = stored == 1 ? 7 : 3;
    CHECK(sortCount(stored) == count);
    for (int sort = -1; sort < 9; ++sort) {
      const int normalized = sort >= 0 && sort < count ? sort : 0;
      CHECK(sortForLayout(stored, sort) == normalized);
      CHECK(nextSort(stored, sort, 1) == (normalized + 1) % count);
      CHECK(nextSort(stored, sort, -1) == (normalized + count - 1) % count);
    }
  }
  const auto language = I18N.getLanguage();
  for (auto locale : {Language::EN, Language::ES}) {
    I18N.setLanguage(locale);
    for (bool light : {false, true})
      for (auto shape : {fui::Size{480, 800}, fui::Size{800, 480}})
        for (int active = 0; active < 3; ++active) {
          chrome::lightHeader = light;
          Canvas canvas(shape.width, shape.height);
          fui::InteractionBuffer<24> hits;
          fui::InputSnapshot input;
          fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
          fui::Screen<24> screen(frame, toybox::themeTokens());
          pixelui::begin(screen);
          classicTabs(screen, active, 0x7f, 74);
          int count = 0;
          for (size_t n = 0; n < hits.count(); ++n) {
            const auto& hit = hits.data()[n];
            if (hit.action != 74) continue;
            CHECK(hit.value == count++);
            CHECK(hit.rect.height >= 44 && hit.rect.x >= 0 && hit.rect.right() <= shape.width);
            const auto event = tap(hits, hit.rect.x + hit.rect.width / 2, hit.rect.y + hit.rect.height / 2);
            CHECK(event.action == 74 && event.value == hit.value);
          }
          CHECK(count == 3 && !hits.overflowed());
        }
  }
  chrome::lightHeader = false;
  I18N.setLanguage(language);
}

void classicGroupPagingChecks() {
  const auto language = I18N.getLanguage();
  for (auto locale : {Language::EN, Language::ES}) {
    I18N.setLanguage(locale);
    for (bool light : {false, true})
      for (auto shape : {fui::Size{480, 800}, fui::Size{800, 480}})
        for (bool authors : {false, true}) {
          chrome::lightHeader = light;
          Canvas canvas(shape.width, shape.height);
          fui::ListItem items[24]{};
          bool seen[24]{};
          for (int n = 0; n < 24; ++n) {
            items[n].label = "A long book title that wraps without covering its group heading";
            items[n].actionValue = n;
            items[n].icon = settingsart::Book;
            if (n % 3 == 0) items[n].sectionHeading = authors ? "Austen, Jane" : "A";
          }
          int start = 0;
          while (start < 24) {
            fui::InteractionBuffer<24> hits;
            fui::InputSnapshot input;
            fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
            fui::Screen<24> screen(frame, toybox::themeTokens());
            pixelui::begin(screen, libraryui::kFooterHeight);
            libraryui::classicTabs(screen, authors ? 2 : 1, 1, 74);
            fui::ListProps props;
            libraryui::bookRows(props, canvas.target, false, true);
            CHECK(props.labelText.maxLines == 2 && props.headerRowHeight > 0);
            CHECK(!props.headerText.bold && props.sectionGap > 0);
            if (light)
              CHECK(canvas.target.lineHeight(props.headerText.font) < canvas.target.lineHeight(props.labelText.font));
            props.items = items;
            props.count = 24;
            props.topIndex = start;
            props.action = 51;
            props.selectedIndex = -1;
            fui::ListNav nav;
            props.nav = &nav;
            pixelui::list(screen, props, false, true);
            int drawn = 0;
            for (size_t n = 0; n < hits.count(); ++n) {
              const auto& hit = hits.data()[n];
              if (hit.action != 51) continue;
              CHECK(hit.value >= start && hit.value < 24 && !seen[hit.value]);
              CHECK(hit.rect.y >= screen.body().y && hit.rect.bottom() <= screen.body().bottom());
              const auto result = tap(hits, hit.rect.x + hit.rect.width / 2, hit.rect.y + hit.rect.height / 2);
              CHECK(result.action == 51 && result.value == hit.value);
              seen[hit.value] = true;
              ++drawn;
            }
            CHECK(drawn > 0 && drawn == nav.drawnRows && !hits.overflowed());
            if (drawn == 0) break;
            start += drawn;
          }
          for (bool value : seen) CHECK(value);
          const auto grid =
              libraryui::gridLayout(canvas.device, fui::makeRect(14, 150, shape.width - 28, shape.height - 200));
          CHECK(grid.capacity() == (shape.width > shape.height ? 4 : 9));
        }
  }
  chrome::lightHeader = false;
  I18N.setLanguage(language);
}

void familyPageChecks() {
  static constexpr const char* names[] = {
      "Noto Serif", "Atkinson Hyperlegible", "Bitter", "EB Garamond", "Literata", "Noto Sans",
      "Ubuntu",     "OpenDyslexic",          "Lexend", "Lora",        "Roboto",   "Vollkorn"};
  for (auto locale : {Language::EN, Language::ES}) {
    I18N.setLanguage(locale);
    for (bool light : {false, true})
      for (auto shape : {fui::Size{480, 800}, fui::Size{800, 480}}) {
        chrome::lightHeader = light;
        Canvas canvas(shape.width, shape.height);
        canvas.device.safeArea = {10, 1, 0, 1};
        const int capacity = readertext::fontRowsPerPage(canvas.device);
        if (light) CHECK(capacity == 5);
        bool seen[12]{};
        const int pages = (12 + capacity - 1) / capacity;
        for (int page = 0; page < pages; ++page) {
          fui::InteractionBuffer<24> hits;
          fui::InputSnapshot input;
          fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
          fui::Screen<24> screen(frame, toybox::themeTokens());
          readertext::Model model;
          model.view = readertext::SheetView::Families;
          model.fontPage = page;
          model.fontSelected = 4;
          model.fontCount = 12;
          model.fontLabel = [](const void*, int index) { return names[index]; };
          readertext::Work work;
          readertext::build(screen, model, work);
          int count = 0;
          const auto list = screen.body();
          for (size_t n = 0; n < hits.count(); ++n) {
            const auto& hit = hits.data()[n];
            if (hit.action != 6 || hit.value < readertext::FontPick) continue;
            const int index = hit.value - readertext::FontPick;
            CHECK(index >= page * capacity && index < std::min(12, (page + 1) * capacity));
            if (index < 0 || index >= 12) continue;
            CHECK(!seen[index]);
            seen[index] = true;
            CHECK(hit.rect.height >= 44);
            CHECK(hit.rect.x >= 0 && hit.rect.right() <= shape.width);
            CHECK(hit.rect.y >= list.y && hit.rect.bottom() <= list.bottom());
            const auto result = tap(hits, hit.rect.x + hit.rect.width / 2, hit.rect.y + hit.rect.height / 2);
            CHECK(result.action == 6 && result.value == hit.value);
            ++count;
          }
          CHECK(count == std::min(capacity, 12 - page * capacity));
          CHECK(!hits.overflowed());
        }
        for (bool value : seen) CHECK(value);
      }
  }
  chrome::lightHeader = false;
}

void portraitPageChecks() {
  Font classic{ubuntu_10_regular}, sans{ui_prose_10}, serif{reading_serif_11}, small{notosans_8_regular};
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (bool light : {false, true})
      for (bool library : {false, true})
        for (bool expanded : {false, true}) {
          for (int font = 0; font < 4; ++font)
            for (bool filtered : {false, true}) {
              chrome::lightHeader = light;
              Canvas canvas(480, 800);
              canvas.device.safeArea = {10, 1, 0, 1};
              if (font) {
                canvas.target.setFont(4, font == 1 ? classic.font : font == 2 ? sans.font : serif.font);
                canvas.target.setFont(3, small.font);
              }
              fui::ListItem items[21]{};
              for (int n = 0; n < 21; ++n) {
                items[n].label = "Alice's Adventures in Wonderland - a long book title";
                if (library && expanded) items[n].subtitle = "Lewis Carroll - Fantasy and adventure";
                items[n].actionValue = n;
                items[n].icon = settingsart::Book;
              }
              for (int count : {10, 11, 20, 21}) {
                int visited = 0;
                fui::ListNav nav;
                bool seen[21]{};
                for (int page = 0; visited < count && page < count; ++page) {
                  fui::InteractionBuffer<24> hits;
                  fui::InputSnapshot input;
                  fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
                  fui::Screen<24> screen(frame, toybox::themeTokens());
                  pixelui::begin(screen, libraryui::kFooterHeight);
                  if (library)
                    libraryui::sortBar(screen, tr(STR_LIBRARY_TAB_RECENT), 80, false, false);
                  else if (!light)
                    screen.takeTop(30, 8);
                  if (library && filtered) screen.takeTop(28, 4);
                  fui::ListProps props;
                  if (library)
                    libraryui::bookRows(props, canvas.target, expanded);
                  else {
                    libraryui::styleBookRows(props, canvas.target);
                    if (light) libraryui::lightRows(props);
                  }
                  const int oldHeight = props.rowHeight;
                  libraryui::fitPortraitRows(props, canvas.device, screen.body());
                  if (!light) CHECK(props.rowHeight == oldHeight);
                  props.items = items;
                  props.count = count;
                  props.action = 51;
                  props.inputMask = fui::InputTouch;
                  props.partialTrailingRow = false;
                  screen.syncListViewport(nav, props, count);
                  const int start = nav.top;
                  props.partialTrailingRow = false;
                  fui::list(frame, screen.body(), props);
                  int drawn = 0;
                  for (size_t n = 0; n < hits.count(); ++n) {
                    const auto& hit = hits.data()[n];
                    if (hit.action != 51) continue;
                    CHECK(hit.value >= start && hit.value < count);
                    if (!seen[hit.value]) ++visited;
                    seen[hit.value] = true;
                    CHECK(hit.rect.height >= 44);
                    CHECK(hit.rect.bottom() <= screen.body().bottom());
                    const auto event = tap(hits, hit.rect.x + 24, hit.rect.y + hit.rect.height / 2);
                    CHECK(event.action == hit.action && event.value == hit.value);
                    ++drawn;
                  }
                  CHECK(drawn > 0 && !hits.overflowed());
                  if (light) {
                    if (drawn != std::min(10, count - start))
                      std::fprintf(stderr,
                                   "rows font=%d library=%d expanded=%d filtered=%d viewport=%d row=%d drawn=%d\n",
                                   font, library, expanded, filtered, screen.body().height, props.rowHeight, drawn);
                    CHECK(drawn == std::min(10, count - start));
                  }
                  CHECK(nav.inputPageRows() == drawn);
                  if (!drawn) break;
                  nav.requestScroll(nav.inputPageRows());
                }
                for (int n = 0; n < count; ++n) CHECK(seen[n]);
              }
              fui::ListProps landscape;
              libraryui::bookRows(landscape, canvas.target, expanded);
              auto device = canvas.device;
              device.width = 800;
              device.height = 480;
              const int before = landscape.rowHeight;
              libraryui::fitPortraitRows(landscape, device, fui::makeRect(14, 120, 772, 300));
              CHECK(landscape.rowHeight == before);
            }
        }
  }
  chrome::lightHeader = false;
}

void footerAndViewButtonChecks() {
  const auto language = I18N.getLanguage();
  for (auto locale : {Language::EN, Language::ES}) {
    I18N.setLanguage(locale);
    for (auto shape : {fui::Size{480, 800}, fui::Size{800, 480}}) {
      chrome::lightHeader = true;
      for (bool books : {false, true})
        for (unsigned total : {0U, 13U, 67U, 99999U}) {
          Canvas canvas(shape.width, shape.height);
          canvas.device.safeArea = {10, 1, 0, 1};
          fui::InteractionBuffer<24> hits;
          fui::InputSnapshot input;
          fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
          fui::Screen<24> screen(frame, toybox::themeTokens());
          char count[48];
          std::snprintf(count, sizeof(count), books ? tr(STR_COVER_BOOK_COUNT) : tr(STR_BROWSER_ITEMS), total);
          const char* path = "microSD/books/A very long directory name that needs truncation/";
          canvas.target.watchedPath = path;
          canvas.target.watchedCount = count;
          libraryui::statusFooter(screen, path, count);
          CHECK(hits.count() == 0);
          CHECK(canvas.target.pathBox.right() + 16 <= canvas.target.countBox.x);
          CHECK(canvas.target.countBox.right() == canvas.device.safeRect().right() - 14);
          CHECK(canvas.target.countBox.bottom() <= canvas.device.safeRect().bottom());
          CHECK(canvas.target.pathBox.y == canvas.target.countBox.y);
          CHECK(canvas.target.countStyle.align == fui::TextAlign::Right && canvas.target.countStyle.maxLines == 1);
        }
      for (bool light : {false, true})
        for (bool grid : {false, true}) {
          chrome::lightHeader = light;
          Canvas canvas(shape.width, shape.height);
          fui::InteractionBuffer<24> hits;
          fui::InputSnapshot input;
          fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
          fui::Screen<24> screen(frame, toybox::themeTokens());
          libraryui::HeaderWork work;
          libraryui::header(screen, work, tr(STR_LIBRARY), true, fui::bitmapFromIcon(icon_search_32),
                            fui::bitmapFromIcon(icon_refresh_cw_32), 70, 71, 72, grid, 72);
          bool refresh = false, search = false, view = false, menu = false;
          for (size_t n = 0; n < hits.count(); ++n) {
            const auto& hit = hits.data()[n];
            CHECK(hit.rect.width >= 44 && hit.rect.height >= 44);
            CHECK(hit.rect.x >= 0 && hit.rect.right() <= shape.width);
            const auto event = tap(hits, hit.rect.x + hit.rect.width / 2, hit.rect.y + hit.rect.height / 2);
            refresh |= event.action == 71;
            search |= event.action == 70;
            view |= event.action == 72 && event.value == 0;
            menu |= event.action == 72 && event.value == -3;
            for (size_t k = n + 1; k < hits.count(); ++k) {
              const auto& other = hits.data()[k].rect;
              CHECK(hit.rect.right() <= other.x || other.right() <= hit.rect.x || hit.rect.bottom() <= other.y ||
                    other.bottom() <= hit.rect.y);
            }
          }
          CHECK(refresh && search && view && menu && !hits.overflowed());
        }
    }
  }
  chrome::lightHeader = false;
  I18N.setLanguage(language);
}

void libraryPolishChecks() {
  const auto language = I18N.getLanguage();
  for (auto locale : {Language::EN, Language::ES}) {
    I18N.setLanguage(locale);
    for (bool light : {false, true})
      for (auto size : {fui::Size{480, 800}, fui::Size{800, 480}})
        for (int active = 0; active < 2; ++active) {
          chrome::lightHeader = light;
          Canvas canvas(size.width, size.height);
          fui::InteractionBuffer<24> hits;
          fui::InputSnapshot input;
          fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
          fui::Screen<24> screen(frame, toybox::themeTokens());
          pixelui::begin(screen, libraryui::kFooterHeight);
          libraryui::filterTabs(screen, active, 81, 82, true);
          CHECK(hits.count() == 3);
          for (size_t n = 0; n < hits.count(); ++n) {
            const auto& h = hits.data()[n];
            CHECK(h.rect.height >= 44 && h.rect.width >= 44);
            const auto event = tap(hits, h.rect.x + h.rect.width / 2, h.rect.y + h.rect.height / 2);
            CHECK(event.action == h.action && event.value == h.value);
            if (h.action == 82) CHECK(h.value == libraryui::kReverseSort);
            for (size_t k = n + 1; k < hits.count(); ++k) {
              const auto other = hits.data()[k].rect;
              CHECK(h.rect.right() <= other.x || other.right() <= h.rect.x);
            }
          }
          hits.clear();
          canvas.target.outlines.clear();
          fui::ListItem rows[5];
          static constexpr StrId names[] = {StrId::STR_LIBRARY_GENRE, StrId::STR_LIBRARY_SERIES,
                                            StrId::STR_LIBRARY_AUTHOR_FIRST, StrId::STR_LIBRARY_AUTHOR_LAST,
                                            StrId::STR_LIBRARY_FIRST_LETTER};
          for (int n = 0; n < 5; ++n) {
            rows[n].label = I18N.get(names[n]);
            rows[n].actionValue = n;
          }
          fui::ListProps props;
          libraryui::filterCards(props, canvas.target);
          props.items = rows;
          props.count = 5;
          props.itemsWindowCount = 5;
          props.action = 83;
          props.selectedIndex = -1;
          libraryui::filterList(screen, props);
          if (!light) CHECK(canvas.target.outlines.size() == 1);
        }
  }
  for (auto bitmap : {themepreview::Framed, themepreview::Carousel, themepreview::Carousel2, themepreview::Bookshelf}) {
    CHECK(bitmap.format == fui::BitmapFormat::BW1);
    CHECK(bitmap.width == 128 && bitmap.height == 128);
    CHECK(bitmap.data[0] == 0);
  }
  chrome::lightHeader = false;
  I18N.setLanguage(language);
}

int main(int argc, char** argv) {
  bookListChecks(argc > 1 ? argv[1] : nullptr);
  chapterChecks(argc > 1 ? argv[1] : nullptr);
  fixedRefreshChecks();
  lightListChecks();
  libraryPreferenceChecks(argc > 1 ? argv[1] : nullptr);
  libraryLayoutChecks();
  classicGroupPagingChecks();
  footerAndViewButtonChecks();
  libraryPolishChecks();
  familyPageChecks();
  portraitPageChecks();
  std::printf("Book lists: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

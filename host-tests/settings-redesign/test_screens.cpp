#include <FreeInkUIDisplayTarget.h>
#include <FreeInkUIStroke.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/ShelfScreen.h"
#include "../../src/apps_local/ui/LightOptionMenu.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/reading_serif_14.h"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#pragma GCC diagnostic pop

#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../lib/EpdFont/builtinFonts/ubuntu_12_bold.h"
#include "../../src/apps_local/home/HomeScreen.h"
#include "../../src/apps_local/readertext/ContentsReturnScreen.h"
#include "../../src/apps_local/readertext/ReaderSavedScreens.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/readertext/ReferenceSlider.h"
#include "../../src/apps_local/ui/GamePanel.h"
#include "../../src/apps_local/ui/QuickSettingsScreen.h"
#include "../../src/apps_local/ui/SettingsHome.h"
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
      size_t index = n;
      for (uint32_t i = 0; i < source.intervalCount; ++i) {
        const auto& range = source.intervals[i];
        if (n + 32 >= range.first && n + 32 <= range.last) {
          index = range.offset + n + 32 - range.first;
          break;
        }
      }
      const EpdGlyph& g = source.glyph[index];
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
  d.safeArea = {9, 7, 3, 7};
  return d;
}
struct RecordingTarget : fui::DrawTarget {
  fui::DisplayTarget backend;
  RecordingTarget(uint8_t* data, int w, int h, int stride, fui::Orientation orientation)
      : backend(data, w, h, stride, orientation) {}
  void setFont(fui::FontId id, const fui::BitmapFont& font) { backend.setFont(id, font); }
  fui::Size measureText(fui::FontId id, const char* text, fui::TextStyle style) const override {
    return backend.measureText(id, text, style);
  }
  int16_t lineHeight(fui::FontId id) const override { return backend.lineHeight(id); }
  fui::Rect inkBounds(fui::FontId id, const char* text, fui::TextStyle style) const override {
    return backend.inkBounds(id, text, style);
  }
  fui::Rect clipRect() const override { return backend.clipRect(); }
  bool setClipRect(fui::Rect rect) override { return backend.setClipRect(rect); }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius = 0, uint8_t corners = fui::CornersAll) override {
    if (p.color == fui::Color::Black) blackFills.push_back(r);
    if (p.color == fui::Color::LightGray || p.color == fui::Color::DarkGray) grayFills.push_back(r);
    backend.fill(r, p, radius, corners);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t width, uint8_t radius = 0,
              uint8_t corners = fui::CornersAll) override {
    frames.push_back(r);
    if (radius) roundedFrames.push_back(r);
    backend.stroke(r, p, width, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override { backend.line(a, b, w, p); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { backend.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef ref, fui::BitmapMode mode,
              fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rotation = fui::Rotation::None) override {
    if (mode == fui::BitmapMode::Tile) ++tiledBitmaps;
    CHECK(r.width > 0 && r.height > 0);
    const auto clip = clipRect();
    const bool inside = r.x >= clip.x && r.y >= clip.y && r.right() <= clip.right() && r.bottom() <= clip.bottom();
    const bool partial =
        allowClippedBitmap && r.x < clip.right() && r.right() > clip.x && r.y < clip.bottom() && r.bottom() > clip.y;
    CHECK(inside || partial);
    backend.bitmap(r, ref, mode, p, rotation);
  }
  struct Label {
    std::string value;
    fui::FontId font;
    fui::Rect box;
  };
  bool allowEllipsis = false;
  bool allowClippedBitmap = false;
  int tiledBitmaps = 0;
  std::vector<Label> labels;
  std::vector<fui::Rect> blackFills, grayFills, frames, roundedFrames;
  void text(fui::Rect box, const char* value, fui::TextStyle style) override {
    fui::layoutText(backend, box, value, style, [&](const char* run, fui::Rect line) {
      if (line.bottom() > box.bottom() || std::strstr(run, "…") || std::strstr(run, "..."))
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
      if (!allowEllipsis) CHECK(!std::strstr(run, "…") && !std::strstr(run, "..."));
    });
    labels.push_back({value ? value : "", style.font, box});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, library{ui_prose_10},
      prose{ui_prose_14}, reading{reading_serif_14};
  RecordingTarget target;
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
        device(makeDevice(target.backend)),
        frame(target, device, input, interactions, nullptr, {100, false, true}),
        screen(frame) {
    target.setFont(3, tiny.font);
    target.setFont(4, library.font);
    target.setFont(0, small.font);
    target.setFont(1, body.font);
    target.setFont(2, title.font);
    target.setFont(toybox::kSmallFont, small.font);
    target.setFont(toybox::kUiFont, body.font);
    target.setFont(toybox::kDisplayFont, title.font);
  }

  void checkHits(fui::ActionId guard = 0) {
    CHECK(!interactions.overflowed());
    paintclock::notePainted();
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& slot = interactions.data()[n];
      const auto& r = slot.rect;
      CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
      CHECK(r.width >= 24 && r.height >= 24);
      if (guard && slot.action == guard) continue;
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = static_cast<int16_t>(r.x + r.width / 2);
      tap.touchY = static_cast<int16_t>(r.y + r.height / 2);
      const auto event = interactions.route(tap);
      if (event.action != slot.action || event.value != slot.value)
        std::printf("HIT expected %d:%d at %d,%d -> %d:%d\n", slot.action, slot.value, tap.touchX, tap.touchY,
                    event.action, event.value);
      CHECK(event.action == slot.action && event.value == slot.value);
    }
  }

  bool has(fui::ActionId action, int value = 0) const {
    for (size_t n = 0; n < interactions.count(); ++n)
      if (interactions.data()[n].action == action && interactions.data()[n].value == value) return true;
    return false;
  }

  int hits(fui::ActionId action) const {
    int count = 0;
    for (size_t n = 0; n < interactions.count(); ++n)
      if (interactions.data()[n].action == action) ++count;
    return count;
  }
  bool label(const char* value) const {
    char caps[192];
    pixelui::copyCaps(caps, sizeof(caps), value);
    for (const auto& l : target.labels)
      if (l.value == value || l.value == caps) return true;
    return false;
  }
  void write(const std::string& path) const {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
      std::perror(path.c_str());
      std::exit(2);
    }
    std::fprintf(file, "P4\n%d %d\n", width, height);
    for (size_t n = 0; n < pixels.size(); ++n)
      std::fputc(
          (pixels[n] ^ 255) ^ (chrome::lightHeader && n / ((width + 7) / 8) < size_t(chrome::headerHeight) ? 255 : 0),
          file);
    std::fclose(file);
  }
};

}  // namespace

int main(int argc, char** argv) {
  for (int radius : {0, 2, 5, 8, 255})
    for (int width : {1, 2, 3, 24})
      for (uint8_t corners :
           {uint8_t(fui::CornersAll), uint8_t(0), uint8_t(fui::CornerTopLeft | fui::CornerBottomRight)}) {
        std::array<bool, 64 * 48> pixels{};
        const auto box = fui::makeRect(11, 13, 64, 48);
        fui::forEachRoundedStrokeBand(box, width, radius, corners, [&](fui::Rect band) {
          CHECK(band.x >= box.x && band.right() <= box.right() && band.y >= box.y && band.bottom() <= box.bottom());
          CHECK(band.width > 0 && band.height == 1);
          for (int x = band.x; x < band.right(); ++x) {
            auto& pixel = pixels[(band.y - box.y) * 64 + x - box.x];
            CHECK(!pixel);
            pixel = true;
          }
        });
        if (width < 12) CHECK(!pixels[24 * 64 + 32]);
        CHECK(std::count(pixels.begin(), pixels.end(), true) > 0);
      }

  for (bool light : {false, true, false, true}) {
    chrome::lightHeader = light;
    const auto styles = toybox::buttonStyles();
    CHECK(styles.normal.foreground.color == fui::Color::Black);
    CHECK(styles.selected.foreground.color == fui::Color::White);
    CHECK(styles.selected.background.color == fui::Color::Black);
    CHECK(styles.normal.border.color == fui::Color::Black);
    CHECK(styles.normal.radius == (light ? lightui::kControlRadius : 0));
    CHECK(toybox::themeTokens().button.selected.background.color == fui::Color::Black);
  }
  for (bool light : {false, true}) {
    chrome::lightHeader = light;
    Canvas c(480, 800);
    const auto pixel = [&](int x, int y) {
      return bool(c.pixels[y * ((c.width + 7) / 8) + x / 8] & (0x80 >> (x % 8)));
    };
    gamepanel::row(c.screen, fui::makeRect(20, 120, 400, 60), "New game", 91, 0, gamepanel::Mark::New, true);
    dashboard::button(c.screen, fui::makeRect(20, 200, 400, 60), "Continue", 92, 0, true);
    CHECK(pixel(20, 120) == light);
    CHECK(pixel(20, 200) == light);
    CHECK(!pixel(24, 140) && !pixel(24, 220));
    CHECK(c.has(91) && c.has(92));
    if (light) {
      const auto tabs = lightui::tabStyles();
      for (const auto* state : {&tabs.selected, &tabs.focused, &tabs.active}) {
        CHECK(state->background.kind == fui::PaintKind::Solid);
        CHECK(state->background.color == fui::Color::Black);
        CHECK(state->foreground.color == fui::Color::White);
      }
    }
  }
  chrome::lightHeader = false;

  FontBridge labelFont(ubuntu_12_bold), captionFont(ubuntu_10_regular);
  for (bool wide : {false, true}) {
    Canvas c(wide ? 800 : 480, wide ? 480 : 800);
    pixelui::begin(c.screen);
    fui::ListItem choices[int(quickui::Shortcut::Count)]{};
    for (int n = 0; n < int(quickui::Shortcut::Count); ++n) {
      choices[n].label = quickui::label(quickui::Shortcut(n));
      choices[n].actionValue = n;
      choices[n].toggle = true;
      choices[n].toggleChecked = n == int(quickui::Shortcut::Rotate);
    }
    fui::ListProps props;
    settingsui::styleRows(props);
    quickui::styleConfiguration(props);
    props.items = choices;
    props.count = int(quickui::Shortcut::Count);
    props.selectedIndex = int(quickui::Shortcut::Dark);
    props.action = 1;
    props.inputMask = fui::InputTouch;
    CHECK(props.toggleCheckbox && props.checkboxCheckmark);
    CHECK(props.selectionMarker == fui::SelectionMarker::Underline);
    pixelui::list(c.screen, props);
    for (const auto& fill : c.target.blackFills)
      CHECK(fill.width < c.screen.body().width || fill.height < props.rowHeight);
    CHECK(!choices[0].toggleChecked && choices[int(quickui::Shortcut::Rotate)].toggleChecked);
    c.checkHits();
  }
  {
    Canvas c(480, 800);
    c.target.setFont(0, labelFont.font);
    c.target.setFont(4, captionFont.font);
    pixelui::begin(c.screen);
    fui::ListItem server;
    server.label = "Custom server";
    server.subtitle = "Default: sync.crosspointreader.com";
    fui::ListProps p;
    settingsui::styleRows(p);
    CHECK(!p.labelText.bold && !p.subtitleText.bold && !p.valueText.bold && p.headerText.bold);
    CHECK(p.subtitleText.font == 3);
    p.items = &server;
    p.count = 1;
    p.action = 1;
    const auto layout = fui::measureListRow(c.target, nullptr, c.screen.body().width, p, server);
    CHECK(layout.labelWidth > 300);
    CHECK(layout.subtitleHeight >= c.target.lineHeight(p.subtitleText.font));
    pixelui::list(c.screen, p);
    CHECK(c.label("Custom server"));
    CHECK(c.label("Default: sync.crosspointreader.com"));
  }
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}, {600, 800}}) {
    Canvas c(size.first, size.second);
    readertext::Model m;
    m.tab = 2;
    m.listCount = 16;
    m.contentsAvailable = true;
    m.currentChapter = 0;
    m.listLabel = [](const void*, int, char* out, size_t size) { std::snprintf(out, size, "CHAPTER"); };
    readertext::Work w;
    readertext::build(c.screen, m, w);
    const int rows = readertext::sheetRowsPerPage(c.device);
    CHECK(rows == 4);
    for (int n = 0; n < rows; ++n) CHECK(c.has(6, readertext::ChapterPick + n));
    CHECK(!c.has(6, readertext::ChapterPick + rows));
    c.checkHits();
    Canvas fonts(size.first, size.second);
    m.view = readertext::SheetView::Families;
    m.fontCount = 3;
    m.fontLabel = [](const void*, int n) { return n == 0 ? "Noto Serif" : n == 1 ? "Bitter" : "Lexend Deca"; };
    readertext::build(fonts.screen, m, w);
    for (int n = 0; n < 3; ++n) CHECK(fonts.has(6, readertext::FontPick + n));
    for (size_t n = 0; n < fonts.interactions.count(); ++n) {
      const auto& hit = fonts.interactions.data()[n];
      if (hit.action == 6 && hit.value >= readertext::FontPick && hit.value < readertext::FontPick + 3)
        CHECK(hit.rect.height == readertext::kFontRowHeight);
    }
    fonts.checkHits();
  }
  // Fixed saved-list pages must preserve identity in both directions and after deletion.
  chrome::lightHeader = true;
  for (auto lang : {Language::EN, Language::ES})
    for (bool wide : {false, true}) {
      I18N.setLanguage(lang);
      Canvas c(wide ? 800 : 480, wide ? 480 : 800);
      const auto box = readertext::returnBox(c.target, c.device, 24);
      readertext::returnControl(c.screen, box);
      CHECK(c.has(1));
      CHECK(c.label(tr(STR_READER_RETURN_POSITION)));
      CHECK(box.height == 44 && box.bottom() < c.device.safeRect().bottom() - 24);
      readertext::paintReturn(c.target, box, true);
      for (int y = box.y; y < box.bottom(); ++y)
        for (int x = box.x; x < box.right(); ++x)
          CHECK(!(c.pixels[y * ((c.width + 7) / 8) + x / 8] & (0x80 >> (x % 8))));
    }
  {
    Canvas group(480, 800);
    const auto band = fui::makeRect(12, 120, 456, 52);
    dashboard::segment(group.screen, band, 0, 2, "Computer", 75, 0, false);
    dashboard::segment(group.screen, band, 1, 2, "Person", 75, 1, true);
    CHECK(group.target.roundedFrames.size() == 1);
    CHECK(group.target.roundedFrames[0].width == band.width);
    CHECK(group.target.blackFills.size() == 1 && group.target.blackFills[0].width == band.width / 2 - 2);
    CHECK(group.has(75, 0) && group.has(75, 1));
  }
  for (bool light : {false, true}) {
    chrome::lightHeader = light;
    Canvas cards(480, 800);
    const auto box = fui::makeRect(30, 130, 120, 72);
    dashboard::frame(cards.screen, box);
    CHECK(cards.target.roundedFrames.empty() == !light);
    cards.target.fill(box, lightui::ink);
    lightui::trimCorners(cards.target, box);
    const auto blackAt = [&](int x, int y) {
      return !(cards.pixels[y * ((cards.width + 7) / 8) + x / 8] & (0x80 >> (x % 8)));
    };
    for (int cornerY : {int(box.y), int(box.bottom() - 1)})
      for (int offset = 0; offset < 5; ++offset) {
        CHECK(blackAt(box.x + offset, cornerY) == !light);
        CHECK(blackAt(box.right() - 1 - offset, cornerY) == !light);
      }
    CHECK(blackAt(box.x + box.width / 2, box.y));
    CHECK(blackAt(box.x + box.width / 2, box.y + box.height / 2));
    CHECK(!blackAt(box.x - 1, box.y));
  }
  chrome::lightHeader = true;
  for (auto lang : {Language::EN, Language::ES})
    for (bool wide : {false, true}) {
      I18N.setLanguage(lang);
      readertext::Model m;
      readertext::Work w;
      m.contentsAvailable = true;
      m.tab = 3;
      m.listCount = 8;
      m.listLabel = [](const void*, int n, char* out, size_t size) {
        std::snprintf(out, size, "%s", n == 5 ? tr(STR_READING_STATS_TITLE) : tr(STR_READER_FINISH_BOOK));
      };
      for (int page = 0; page < 2; ++page) {
        Canvas c(wide ? 800 : 480, wide ? 480 : 800);
        m.listPage = page;
        readertext::build(c.screen, m, w);
        c.checkHits();
        CHECK(readertext::sheetRowsPerPage(c.device, false, 3) == 5);
        CHECK(c.has(6, readertext::MorePick + 4) == (page == 0));
        CHECK(c.has(6, readertext::MorePick + 5) == (page == 1));
        CHECK(c.has(6, readertext::MorePick + 6) == (page == 1));
      }
      {
        Canvas layout(wide ? 800 : 480, wide ? 480 : 800);
        readertext::Model spacing;
        readertext::Work controls;
        spacing.contentsAvailable = true;
        spacing.tab = 1;
        spacing.spacingLabel = "Normal";
        readertext::build(layout.screen, spacing, controls);
        layout.checkHits();
        for (size_t n = 0; n < layout.interactions.count(); ++n) {
          const auto& hit = layout.interactions.data()[n];
          if (hit.action != 6) continue;
          if (hit.value != readertext::SpacingDown && hit.value != readertext::SpacingUp &&
              hit.value != readertext::MarginDown && hit.value != readertext::MarginUp)
            continue;
          bool padded = false;
          for (const auto box : layout.target.roundedFrames)
            if (box.width > hit.rect.width && box.x <= hit.rect.x && box.y <= hit.rect.y &&
                box.right() >= hit.rect.right() && box.bottom() >= hit.rect.bottom() + 12)
              padded = true;
          CHECK(padded);
        }
        bool alignmentFrame = false;
        for (const auto box : layout.target.roundedFrames)
          if (box.width > 300 && box.height == 78) alignmentFrame = true;
        CHECK(alignmentFrame);
      }
      Canvas families(wide ? 800 : 480, wide ? 480 : 800);
      m.view = readertext::SheetView::Families;
      m.fontCount = 1;
      m.fontLabel = [](const void*, int) { return "Noto Serif"; };
      readertext::build(families.screen, m, w);
      families.checkHits();
      CHECK(families.has(6, readertext::FontPick));
      CHECK(!families.target.blackFills.empty());
      CHECK(families.target.grayFills.empty());
    }
  chrome::lightHeader = false;
  for (auto lang : {Language::EN, Language::ES})
    for (bool wide : {false, true})
      for (int tab = 0; tab < 4; ++tab) {
        I18N.setLanguage(lang);
        chrome::lightHeader = true;
        Canvas c(wide ? 800 : 480, wide ? 480 : 800);
        c.target.setFont(0, c.library.font);
        c.target.setFont(1, c.prose.font);
        readertext::Model m;
        m.tab = tab;
        m.contentsAvailable = true;
        m.family = "Noto Serif";
        m.spacingLabel = "1.1x";
        m.listCount = 12;
        m.listLabel = [](const void*, int, char* out, size_t size) { std::snprintf(out, size, "Chapter title"); };
        if (tab == 3) m.view = readertext::SheetView::Style;
        readertext::Work w;
        readertext::build(c.screen, m, w);
        c.checkHits();
        const auto sheet = readertext::sheetRect(c.device);
        CHECK(sheet.height < c.height * (wide ? 90 : 65) / 100);
        for (const auto r : c.target.frames) CHECK(r.y >= sheet.y && r.bottom() <= c.height);
      }
  chrome::lightHeader = false;
  for (int count : {1, 2, 5, 10, 17}) {
    for (int capacity : {1, 3, 8}) {
      const int height = capacity * readersaved::RowHeight + (capacity - 1) * readersaved::RowGap;
      int selected = 0;
      const int pages = (count + capacity - 1) / capacity;
      for (int page = 0; page < pages; ++page) {
        const auto p = readersaved::project(selected, count, height);
        CHECK(p.first == page * capacity && p.end == std::min(count, (page + 1) * capacity));
        CHECK(p.number == page + 1 && p.total == pages);
        if (page + 1 < pages) selected = readersaved::destination(selected, count, capacity, 1);
      }
      for (int page = pages - 1; page >= 0; --page) {
        const auto p = readersaved::project(selected, count, height);
        CHECK(p.first == page * capacity && p.number == page + 1);
        selected = readersaved::destination(selected, count, capacity, -1);
      }
      for (int row = 0; row < count; ++row) {
        const auto p = readersaved::project(row, count, height);
        CHECK(row >= p.first && row < p.end);
      }
      const auto deleted = readersaved::project(count - 1, count - 1, height);
      CHECK(deleted.number <= deleted.total);
    }
  }
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}, {600, 800}}) {
    for (int selected = 0; selected < 10; ++selected) {
      Canvas c(size.first, size.second);
      c.target.setFont(4, labelFont.font);
      c.target.setFont(3, captionFont.font);
      c.target.allowEllipsis = true;
      readersaved::begin(c.screen, "ALICE'S ADVENTURES IN WONDERLAND AND A VERY LONG TITLE FOR WRAPPING",
                         "Lewis Carroll with a deliberately long author name for wrapping", 10, false);
      const auto foot = readersaved::reserveFooter(c.screen);
      const auto page = readersaved::project(selected, 10, c.screen.body().height);
      fui::ListItem items[10];
      for (int n = 0; n < 10; ++n) {
        items[n].label = "So she was considering in her own mind...";
        items[n].subtitle = "3% - CHAPTER I - 2/23";
        items[n].actionValue = n;
      }
      fui::ListProps p;
      readersaved::rows(p);
      p.items = items;
      p.action = 1;
      p.inputMask = fui::InputTouch;
      fui::ListNav nav;
      nav.selected = selected;
      readersaved::slice(p, nav, page);
      c.screen.list(p);
      for (int n = page.first; n < page.end; ++n) CHECK(c.has(1, n));
      fui::ButtonProps button;
      readersaved::footer(c.screen, button, foot, page.number, page.total, 2);
      c.checkHits();
      if (argc > 1 && selected == 0)
        c.write(std::string(argv[1]) + "/saved-" + std::to_string(size.first) + "x" + std::to_string(size.second) +
                ".pbm");
    }
  }
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}, {600, 800}}) {
      for (bool chapter : {false, true}) {
        Canvas c(size.first, size.second);
        c.target.setFont(0, labelFont.font);
        c.target.setFont(1, labelFont.font);
        c.target.setFont(3, captionFont.font);
        struct Spec {
          const char *title, *subtitle, *readout, *minLabel, *maxLabel;
          int value = 0, max = 100;
          fui::ActionId sliderAction = 11, stepAction = 12, okAction = 13, chromeAction = 14;
        } spec{chapter ? tr(STR_READER_CHAPTER_PAGE) : tr(STR_SLEEP_SCREEN),
               chapter ? tr(STR_SR_THIS_CHAPTER) : nullptr, chapter ? "1 / 8" : tr(STR_NEVER), chapter ? "1" : "1 min",
               chapter ? "8" : tr(STR_NEVER)};
        referenceslider::build(c.screen, spec, true);
        CHECK(c.has(11));
        CHECK(c.has(12, -1));
        CHECK(c.has(12, 1));
        CHECK(c.has(13));
        paintclock::notePainted();
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto& hit = c.interactions.data()[n];
          CHECK(hit.rect.x >= 0 && hit.rect.y >= 0 && hit.rect.right() <= c.width && hit.rect.bottom() <= c.height);
          if (hit.action == 14) continue;  // The popup guard deliberately encloses its controls.
          fui::InputSnapshot tap;
          tap.touchReleased = true;
          tap.touchX = hit.rect.x + hit.rect.width / 2;
          tap.touchY = hit.rect.y + hit.rect.height / 2;
          const auto event = c.interactions.route(tap);
          CHECK(event.action == hit.action && event.value == hit.value);
          if (hit.action == 11) {
            tap.touchX = hit.rect.x;
            CHECK(c.interactions.route(tap).dragPermille == 0);
            tap.touchX = hit.rect.right() - 1;
            CHECK(c.interactions.route(tap).dragPermille >= 995);
          }
        }
        if (argc > 1)
          c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" + std::to_string(size.first) +
                  "x" + std::to_string(size.second) + (chapter ? "-jump.pbm" : "-sleep.pbm"));
      }
    }

    I18N.setLanguage(lang);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}, {600, 800}, {800, 600}}) {
      Canvas c(size.first, size.second);
      c.target.setFont(0, labelFont.font);
      c.target.setFont(4, captionFont.font);
      pixelui::Header h;
      h.title = tr(STR_SETTINGS_TITLE);
      h.back = true;
      pixelui::header(c.screen, h);
      pixelui::begin(c.screen);
      fui::ListItem items[settingsui::homeCount];
      char labels[settingsui::homeCount][64];
      for (int n = 0; n < settingsui::homeCount; ++n) {
        settingsui::homeRow(n, items[n]);
        CHECK(items[n].subtitle == nullptr || items[n].subtitle[0] == 0);
        pixelui::copyCaps(labels[n], sizeof(labels[n]), items[n].label);
        items[n].label = labels[n];
      }
      fui::ListProps p;
      settingsui::styleHome(p);
      CHECK(!p.labelText.bold);
      CHECK(p.rowHeight == pixelui::kRowHeight);
      CHECK(pixelui::buttonStyles().normal.borderWidth == pixelui::kBorder);
      CHECK(pixelui::rowStyles().normal.borderWidth == 1);
      CHECK(pixelui::rowStyles().disabled.borderWidth == pixelui::rowStyles().normal.borderWidth);
      p.items = items;
      p.count = settingsui::homeCount;
      p.action = 1;
      p.inputMask = fui::InputTouch;
      fui::ListNav nav;
      c.screen.syncListViewport(nav, p, p.count, 1);
      c.target.allowClippedBitmap = true;
      pixelui::list(c.screen, p, false, true, pixelui::kBorder, true);
      if (size.first < size.second) {
        CHECK(!c.target.frames.empty());
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto& hit = c.interactions.data()[n];
          if (hit.action == 1 && hit.value == settingsui::homeCount - 1)
            CHECK(c.target.frames.back().bottom() == hit.rect.bottom());
        }
      }
      c.checkHits();
      if (size.first < size.second) {
        for (int n = 0; n < settingsui::homeCount; ++n) CHECK(c.has(1, n));
      }
      if (argc > 1)
        c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" + std::to_string(size.first) +
                "x" + std::to_string(size.second) + "-settings.pbm");
      Canvas theme(size.first, size.second);
      theme.target.setFont(0, labelFont.font);
      theme.target.setFont(4, captionFont.font);
      h.title = tr(STR_SETTINGS_APPEARANCE);
      pixelui::header(theme.screen, h);
      pixelui::begin(theme.screen);
      fui::ListItem choices[2];
      for (int n = 0; n < 2; ++n) {
        choices[n].label = n ? tr(STR_HOME_FOCUS_CAROUSEL) : tr(STR_HOME_FRAMED);
        choices[n].subtitle = n ? tr(STR_HOME_CAROUSEL_HELP) : tr(STR_HOME_FRAMED_HELP);
        choices[n].icon = n ? settingsart::CarouselPreview : settingsart::FramedPreview;
        choices[n].toggle = true;
        choices[n].toggleChecked = n == 1;
        choices[n].actionValue = n;
      }
      settingsui::styleAppearance(p);
      p.items = choices;
      p.count = 2;
      p.action = 1;
      p.toggleCheckbox = true;
      p.toggleWidth = p.toggleHeight = 34;
      fui::ListNav themeNav;
      themeNav.reset(-1);
      theme.screen.syncListViewport(themeNav, p, p.count);
      settingsui::appearanceList(theme.screen, p);
      int dividers = 0;
      for (const auto& fill : theme.target.blackFills)
        if (fill.width > theme.screen.body().width / 2 && fill.height == pixelui::kBorder) ++dividers;
      CHECK(dividers == 1);
      CHECK(p.rowGap == 0 && p.rowStyles.normal.borderWidth == 0);
      theme.checkHits();
      CHECK(theme.has(1, 0));
      CHECK(theme.has(1, 1));
      if (argc > 1)
        theme.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                    std::to_string(size.first) + "x" + std::to_string(size.second) + "-appearance.pbm");
    }
  }
  for (auto lang : {Language::EN, Language::ES})
    for (bool light : {false, true})
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        I18N.setLanguage(lang);
        chrome::lightHeader = light;
        Canvas c(size.first, size.second);
        c.target.setFont(0, labelFont.font);
        c.target.setFont(1, labelFont.font);
        c.target.setFont(4, captionFont.font);
        pixelui::begin(c.screen);
        const auto initial = c.screen.body();
        const auto body = fui::makeRect(initial.x, initial.y + 52, initial.width, initial.height - 52);
        fui::ListItem choices[4];
        const StrId names[] = {StrId::STR_HOME_FRAMED, StrId::STR_HOME_FOCUS_CAROUSEL, StrId::STR_HOME_BOOX,
                               StrId::STR_HOME_LICORICE};
        for (int n = 0; n < 4; ++n) {
          choices[n].label = I18N.get(names[n]);
          choices[n].icon = settingsart::CarouselPreview;
          choices[n].subtitle = size.first < size.second ? tr(STR_HOME_CAROUSEL_HELP) : nullptr;
          choices[n].actionValue = n;
        }
        fui::ListProps p;
        settingsui::styleAppearance(p);
        settingsui::styleThemeChoices(p, size.first > size.second);
        p.items = choices;
        p.count = 4;
        p.action = 19;
        p.selectedIndex = -1;
        p.toggleCheckbox = true;
        p.toggleWidth = p.toggleHeight = 34;
        int total = 0;
        for (const auto& item : choices)
          total += fui::measureListRow(c.target, c.screen.frame().assets(), body.width, p, item).height;
        CHECK(total <= 4 * p.rowHeight);
        const auto actions = settingsui::appearanceActions(body, body.y + 4 * p.rowHeight, size.first > size.second);
        CHECK(size.first > size.second ? actions.themes.x > body.x + body.width / 2
                                       : actions.themes.y >= body.y + total);
        CHECK(actions.recent.y == actions.themes.bottom() + 12);
        CHECK(actions.font.bottom() <= body.bottom());
        c.screen.setContentMarginAbsolute(fui::Insets{body.y, static_cast<int16_t>(c.device.width - body.right()),
                                                      static_cast<int16_t>(c.device.height - body.y - 4 * p.rowHeight),
                                                      body.x});
        settingsui::appearanceList(c.screen, p);
        int dividers = 0;
        for (const auto& fill : c.target.blackFills)
          if (fill.width == body.width && fill.height == pixelui::kBorder) ++dividers;
        CHECK(dividers == (light ? 0 : 3));
        for (int n = 0; n < 4; ++n) CHECK(c.has(19, n));
      }
  chrome::lightHeader = false;
  for (auto lang : {Language::EN, Language::ES})
    for (bool light : {false, true})
      for (bool wide : {false, true}) {
        I18N.setLanguage(lang);
        chrome::lightHeader = light;
        Canvas c(wide ? 800 : 480, wide ? 480 : 800);
        c.target.allowEllipsis = true;
        const auto paging = shelfui::pagingFor(c.device, 16);
        fui::ListItem items[9];
        const char* english[] = {"Chess", "Checkers",      "Go",        "Minesweeper", "Picross", "Wordle",
                                 "2048",  "The Intercept", "Battleship"};
        const char* spanish[] = {"Ajedrez", "Damas",         "Go",           "Buscaminas", "Picross", "Wordle",
                                 "2048",    "The Intercept", "Batalla naval"};
        const freeink::Icon* icons[] = {&icon_chess_32,       &icon_checkers_32,  &icon_go_32,
                                        &icon_minesweeper_32, &icon_picross_32,   &icon_wordle_32,
                                        &icon_connections_32, &icon_underhand_32, &icon_battleship_32};
        if (light) {
          c.target.setFont(0, c.prose.font);
          c.target.setFont(1, c.prose.font);
        }
        for (int n = 0; n < paging.rowsPerPage; ++n) {
          items[n].label = (lang == Language::ES ? spanish : english)[n];
          items[n].icon = fui::bitmapFromIcon(*icons[n]);
          items[n].actionValue = n;
        }
        shelfui::MenuModel m;
        m.items = items;
        m.title = tr(STR_SHORT_GAMES);
        m.count = paging.rowsPerPage;
        m.pageCount = paging.pageCount;
        m.selected = -1;
        shelfui::buildMenu(c.screen, m);
        c.checkHits();
        for (int n = 0; n < paging.rowsPerPage; ++n) CHECK(c.has(shelfui::ActionOpen, n));
        CHECK(c.has(shelfui::ActionGoToPage, 1));
        CHECK(!c.has(shelfui::ActionGoToPage, -1));
        if (argc > 1 && light)
          c.write(std::string(argv[1]) + "/shelf-" + (lang == Language::ES ? "es" : "en") +
                  (wide ? "-wide.pbm" : "-tall.pbm"));
      }
  chrome::lightHeader = false;
  for (auto lang : {Language::EN, Language::ES})
    for (bool wide : {false, true}) {
      I18N.setLanguage(lang);
      chrome::lightHeader = true;
      chrome::headerHeight = 72;
      for (int count : {1, 2, 9, 16})
        for (int focused = 0; focused < count; ++focused) {
          Canvas c(wide ? 800 : 480, wide ? 480 : 800);
          fui::DialogOption options[16];
          for (int n = 0; n < count; ++n) {
            options[n].label = tr(STR_HOME_FRAMED);
            options[n].action = 71;
            options[n].value = n;
          }
          lightui::ChoiceMenu menu;
          menu.title = tr(STR_SLEEP_SCREEN);
          menu.options = options;
          menu.count = count;
          menu.current = count - 1;
          menu.focus = focused;
          const auto g = lightui::choiceLayout(c.target, c.device.screen(), menu);
          CHECK(g.panel.x >= 0 && g.panel.y >= chrome::headerHeight && g.panel.right() <= c.width &&
                g.panel.bottom() <= c.height);
          lightui::choiceMenu(c.frame, c.device.screen(), menu);
          CHECK(c.has(71, focused));
          CHECK(!c.interactions.overflowed());
          for (size_t n = 0; n < c.interactions.count(); ++n) {
            const auto hit = c.interactions.data()[n];
            if (hit.action == menu.guard) continue;
            CHECK(hit.rect.y >= g.panel.y && hit.rect.bottom() <= g.panel.bottom());
            fui::InputSnapshot tap;
            tap.touchReleased = true;
            tap.touchX = hit.rect.x + hit.rect.width / 2;
            tap.touchY = hit.rect.y + hit.rect.height / 2;
            paintclock::notePainted();
            const auto event = c.interactions.route(tap);
            CHECK(event.action == hit.action && event.value == hit.value);
          }
          for (const auto r : c.target.blackFills) CHECK(r.height <= 2 || r.width < 24);
        }
      for (int count : {2, 16}) {
        Canvas c(wide ? 800 : 480, wide ? 480 : 800);
        fui::DialogOption options[16];
        for (int n = 0; n < count; ++n) {
          options[n].label = tr(STR_FORGET_NETWORK);
          options[n].action = 71;
          options[n].value = n;
        }
        lightui::ChoiceMenu menu;
        menu.title = tr(STR_FORGET_NETWORK);
        menu.headline = "Network: abcdefghijklmnopqrstuvwxyz123456";
        menu.message = tr(STR_FORGET_AND_REMOVE);
        menu.options = options;
        menu.count = count;
        menu.current = -1;
        menu.focus = count - 1;
        const auto g = lightui::choiceLayout(c.target, c.device.screen(), menu);
        CHECK(g.panel.y >= chrome::headerHeight && g.panel.bottom() <= c.height);
        lightui::choiceMenu(c.frame, c.device.screen(), menu);
        CHECK(c.has(71, count - 1));
        CHECK(!c.interactions.overflowed());
      }
      Canvas panel(wide ? 800 : 480, wide ? 480 : 800);
      quickui::Model model;
      quickui::Work work;
      const auto g = quickui::layout(panel.device, true, true, true);
      quickui::build(panel.screen, model, work);
      CHECK(panel.target.tiledBitmaps > 0);
      Canvas repainted(wide ? 800 : 480, wide ? 480 : 800);
      model.shadeBackdrop = false;
      quickui::build(repainted.screen, model, work);
      CHECK(repainted.target.tiledBitmaps == 0);
      CHECK(g.bottom < panel.height);
      CHECK(g.brightness.bottom() < g.tiles.y);
      CHECK(g.warmth.bottom() < g.tiles.y);
      panel.checkHits();
      for (const auto r : panel.target.frames) CHECK(r.right() <= panel.width && r.bottom() <= panel.height);
      Canvas slider(wide ? 800 : 480, wide ? 480 : 800);
      slider.target.setFont(0, slider.prose.font);
      slider.target.setFont(1, slider.prose.font);
      struct SliderSpec {
        const char *title, *subtitle = nullptr, *readout = "3", *minLabel = "1", *maxLabel = "10";
        int value = 2, max = 9;
        fui::ActionId sliderAction = 11, stepAction = 12, okAction = 13, chromeAction = 14;
      } spec{tr(STR_HOME_RECENT_BOOK_COUNT)};
      referenceslider::build(slider.screen, spec, true);
      slider.checkHits(14);
      CHECK(slider.has(11) && slider.has(12, -1) && slider.has(12, 1) && slider.has(13));
      for (const auto r : slider.target.blackFills) CHECK(r.height <= 4 || r.width < 24);
    }
  chrome::lightHeader = false;
  std::printf("settings redesign: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

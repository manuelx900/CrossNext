#include <FreeInkApp.h>
#include <FreeInkUIDisplayTarget.h>
#include <FreeInkUIIcon.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/library/LibraryMenu.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/ToyboxTokens.h"
#include "../../src/components/icons/headerIcons.h"
#include "../../src/components/icons/listIcons.h"
#include "../../src/components/icons/search32.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
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
struct Canvas {
  std::vector<uint8_t> pixels;
  Font small{toybox_14}, body{toybox_20}, title{toybox_30}, tab{toybox_10};
  fui::DisplayTarget target;
  fui::DeviceContext device;
  explicit Canvas(int width, int height)
      : pixels((width + 7) / 8 * height, 255),
        target(pixels.data(), width, height, (width + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(target.deviceContext()) {
    target.setFont(0, small.font);
    target.setFont(1, body.font);
    target.setFont(2, title.font);
    target.setFont(3, tab.font);
    device.hasTouch = true;
  }
};
bool validUtf8(const char* value) {
  while (*value) {
    const unsigned char c = static_cast<unsigned char>(*value++);
    const int extra = c < 128 ? 0 : c < 224 ? 1 : c < 240 ? 2 : 3;
    for (int n = 0; n < extra; ++n)
      if (!*value || (static_cast<unsigned char>(*value++) & 0xC0) != 0x80) return false;
  }
  return true;
}
fui::ActionEvent tap(fui::InteractionBuffer<24>& hits, int x, int y) {
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(x);
  input.touchY = static_cast<int16_t>(y);
  return hits.route(input);
}
struct Source {
  int calls = 0;
  static void row(void* context, uint16_t index, fui::ListItem& item) {
    ++static_cast<Source*>(context)->calls;
    item.label = "A very long book title with many words to check wrapping and touch navigation";
    item.value = ".epub";
    item.actionValue = static_cast<int16_t>(index);
  }
};
struct CountedText : fui::DrawTarget {
  mutable int layouts = 0;
  std::vector<fui::Rect> frames;
  fui::Size measureText(fui::FontId, const char* text, fui::TextStyle) const override {
    int characters = 0;
    for (; *text; ++text)
      if ((static_cast<unsigned char>(*text) & 0xC0) != 0x80) ++characters;
    return {static_cast<int16_t>(characters * 10), 20};
  }
  int16_t lineHeight(fui::FontId) const override {
    ++layouts;
    return 20;
  }
  void fill(fui::Rect, fui::Paint, uint8_t, uint8_t) override {}
  void stroke(fui::Rect r, fui::Paint, uint8_t width, uint8_t, uint8_t) override {
    if (width == 4) frames.push_back(r);
  }
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void text(fui::Rect, const char*, fui::TextStyle) override {}
  void bitmap(fui::Rect, fui::BitmapRef, fui::BitmapMode, fui::Paint, fui::Rotation) override {}
};
}  // namespace

void libraryChecks(const char* output) {
  Canvas canvas(480, 800);
  canvas.device.safeArea = {10, 3, 9, 3};
  fui::InteractionBuffer<24> hits;
  const fui::InputSnapshot input;
  fui::Frame<24> frame(canvas.target, canvas.device, input, hits);
  auto theme = toybox::themeTokens();
  theme.listRowRadius = 20;
  fui::Screen<24> screen(frame, theme);
  libraryui::HeaderWork work;
  fui::BatteryIndicatorProps battery;
  battery.percent = 100;
  battery.label = "100%";
  libraryui::header(screen, work, "LIBRARY", battery, true, true, fui::bitmapFromIcon(icon_search_32),
                    fui::bitmapFromIcon(icon_refresh_cw_32), 80, 81);
  const auto safe = canvas.device.safeRect();
  const auto back = pixelui::geometry(canvas.device).back;
  CHECK(HeaderBackTapTarget::contains(back.x + 20, back.y + 20));
  CHECK(!tap(hits, back.x + 20, back.y + 20));
  CHECK(tap(hits, safe.right() - 34, safe.y + 50).action == 80);
  CHECK(tap(hits, safe.right() - 84, safe.y + 50).action == 81);
  pixelui::begin(screen, libraryui::kFooterHeight);
  fui::TabItem tabs[3];
  const char* labels[] = {"RECENT", "TITLE", "AUTHOR"};
  for (int n = 0; n < 3; ++n) tabs[n] = fui::tabItem(n, n == 0, true, labels[n]);
  tabs[0].indicator = fui::TabIndicator::Down;
  const auto tabBand = screen.body();
  pixelui::tabs(screen, tabs, 3, 82, pixelui::kSmall);
  for (int n = 0; n < 3; ++n) {
    auto event = tap(hits, tabBand.x + tabBand.width * (2 * n + 1) / 6, tabBand.y + 26);
    CHECK(event.action == 82 && event.value == n);
  }
  fui::ListItem books[5];
  const char* names[] = {"ALICE'S ADVENTURES IN WONDERLAND", "GLYPHS-SENTENCE.TXT", "GLYPHS-LATIN.TXT",
                         "GLYPHS-HEADWORD.TXT", "HN-ARTICLE.TXT"};
  for (int n = 0; n < 5; ++n) {
    books[n].label = names[n];
    books[n].actionValue = static_cast<int16_t>(37 + n);
    books[n].icon = fui::bitmapFromIcon(n == 0 ? icon_book_32 : icon_file_text_32);
  }
  books[0].subtitle = "LEWIS CARROLL";
  fui::ListProps props;
  libraryui::styleRows(props);
  props.items = books;
  props.itemsWindowFirst = 37;
  props.itemsWindowCount = 5;
  props.topIndex = 37;
  props.count = 1000;
  props.selectedIndex = 37;
  props.action = 83;
  const auto body = screen.body();
  pixelui::list(screen, props);
  CHECK(props.rowRadius == 0);
  CHECK(tap(hits, body.x + 100, body.y + 20).value == 37);
  CHECK(!hits.overflowed());
  libraryui::footer(canvas.target, canvas.device, "1/5 BOOKS", 0);
  CHECK(body.bottom() <= safe.bottom() - libraryui::kFooterHeight);
  if (output) {
    auto* file = std::fopen(output, "wb");
    CHECK(file != nullptr);
    if (file) {
      std::fprintf(file, "P5\n480 800\n255\n");
      for (int y = 0; y < 800; ++y)
        for (int x = 0; x < 480; ++x) std::fputc(canvas.pixels[y * 60 + x / 8] & (0x80 >> (x % 8)) ? 255 : 0, file);
      std::fclose(file);
    }
  }
  for (bool touch : {false, true}) {
    canvas.device.hasTouch = touch;
    fui::Frame<24> again(canvas.target, canvas.device, input, hits);
    toybox::Screen app(again);
    fui::HeaderProps header;
    header.title = "GAME TITLE";
    toybox::headerBand(app, header);
    CHECK((HeaderBackTapTarget::w > 0) == touch);
    if (touch) {
      header.leadingIcon = fui::bitmapFromIcon(icon_header_back_32);
      header.leadingAction = 90;
      fui::Frame<24> next(canvas.target, canvas.device, input, hits);
      toybox::Screen custom(next);
      toybox::headerBand(custom, header);
      CHECK(HeaderBackTapTarget::w == 0);
    }
  }
}

int main(int argc, char** argv) {
  libraryChecks(argc > 1 ? argv[1] : nullptr);
  static_assert(sizeof(pixelui::RowSource) <= 1024, "The reused row workspace must stay bounded");
  Canvas canvas(480, 800);
  fui::TextStyle style;
  style.font = 1;
  style.maxLines = 2;
  char text[128];
  pixelui::copyCaps(text, sizeof(text), "a long sentence with several words that cannot all fit in two lines here");
  pixelui::fitLabel(canvas.target, text, 170, style);
  CHECK(std::strstr(text, "...") != nullptr);
  bool ellipsis = false;
  fui::layoutText(canvas.target, fui::makeRect(0, 0, 170, 200), text, style,
                  [&](const char* run, fui::Rect) { ellipsis |= std::strstr(run, "\xe2\x80\xa6") != nullptr; });
  CHECK(!ellipsis);
  pixelui::copyCaps(text, sizeof(text), "A book… end");
  pixelui::fitLabel(canvas.target, text, 400, style);
  CHECK(std::string(text) == "A BOOK… END");
  pixelui::copyCaps(text, sizeof(text), "mañana mañana mañana mañana mañana");
  pixelui::fitLabel(canvas.target, text, 100, style);
  CHECK(validUtf8(text));
  CHECK(pixelui::copyCaps(text, 3, "ñandú"));
  CHECK(std::string(text) == "Ñ");

  for (const char* glyph : {"A", "漢"}) {
    std::string longName;
    while (longName.size() + std::strlen(glyph) < 512) longName += glyph;
    CountedText counted;
    pixelui::fitLabel(counted, longName.data(), 170, style);
    CHECK(counted.layouts <= 11);
    CHECK(validUtf8(longName.c_str()));
    CHECK(std::strstr(longName.c_str(), "...") != nullptr);
    bool clipped = false;
    fui::layoutText(counted, fui::makeRect(0, 0, 170, 200), longName.c_str(), style,
                    [&](const char* run, fui::Rect) { clipped |= std::strstr(run, "\xe2\x80\xa6") != nullptr; });
    CHECK(!clipped);
  }

  for (const auto shape : {fui::Size{480, 800}, fui::Size{800, 480}}) {
    Canvas out(shape.width, shape.height);
    out.device.safeArea = {10, 3, 9, 3};
    const auto geometry = pixelui::geometry(out.device);
    const auto safe = out.device.safeRect();
    CHECK(geometry.header.y == 0 && geometry.header.width == shape.width);
    CHECK(geometry.back.y >= safe.y);
    CHECK(geometry.body.x >= safe.x && geometry.body.right() <= safe.right());
    CHECK(geometry.body.y > geometry.header.bottom());
    CHECK(geometry.body.bottom() <= safe.bottom());
    {
      const fui::InputSnapshot input;
      fui::InteractionBuffer<24> actions;
      fui::Frame<24> frame(out.target, out.device, input, actions);
      toybox::Screen app(frame);
      fui::HeaderProps heading;
      heading.title = "APP";
      toybox::headerBand(app, heading);
      CHECK(HeaderBackTapTarget::x >= safe.x && HeaderBackTapTarget::y >= safe.y);
      CHECK(HeaderBackTapTarget::x + HeaderBackTapTarget::w <= safe.right());
      CHECK(HeaderBackTapTarget::y + HeaderBackTapTarget::h < app.body().y);
      CHECK(!tap(actions, HeaderBackTapTarget::x + 20, HeaderBackTapTarget::y + 20));
    }
    const fui::InputSnapshot noInput;
    fui::InteractionBuffer<24> hits;
    fui::Frame<24> frame(out.target, out.device, noInput, hits);
    fui::Screen<24> screen(frame, toybox::themeTokens());
    pixelui::begin(screen);
    fui::TabItem tabs[4];
    const char* labels[] = {"DISPLAY", "READER", "CONTROLS", "SYSTEM"};
    fui::TextStyle tabStyle;
    tabStyle.font = 3;
    for (int n = 0; n < 4; ++n) {
      tabs[n] = fui::tabItem(n, n == 0, true, labels[n]);
      CHECK(out.target.measureText(3, labels[n], tabStyle).width < screen.body().width / 4 - 4);
    }
    const auto band = screen.body();
    pixelui::tabs(screen, tabs, 4, 7);
    for (int n = 0; n < 4; ++n) {
      const auto event = tap(hits, band.x + band.width * (2 * n + 1) / 8, band.y + 26);
      CHECK(event.action == 7 && event.value == n);
    }
  }

  fui::InteractionBuffer<24> hits;
  const fui::InputSnapshot noInput;
  fui::Frame<24> frame(canvas.target, canvas.device, noInput, hits);
  fui::Screen<24> screen(frame, toybox::themeTokens());
  pixelui::begin(screen);
  Source original;
  pixelui::RowSource rows;
  fui::ListProps props;
  pixelui::styleList(props);
  props.count = 1000;
  props.topIndex = 37;
  props.selectedIndex = 37;
  props.action = 9;
  props.rowProvider = &Source::row;
  props.rowProviderCtx = &original;
  rows.bind(props, canvas.target, screen.body());
  pixelui::list(screen, props);
  CHECK(original.calls > 0 && original.calls < 50);
  CHECK(!hits.overflowed());
  const auto band = screen.body();
  const auto first = tap(hits, band.x + 2, band.y + 30);
  const auto right = tap(hits, pixelui::rowArea(band, props).right() - 3, band.y + 30);
  CHECK(first.action == 9 && first.value == 37);
  CHECK(right.action == 9 && right.value == 37);
  fui::ListItem item;
  props.rowProvider(props.rowProviderCtx, 38, item);
  CHECK(item.actionValue == 38);
  CHECK(std::strstr(item.label, "A VERY LONG") == item.label);
  CHECK(std::string(item.value) == ".EPUB");
  CHECK(std::strstr(item.label, "\xe2\x80\xa6") == nullptr);
  CHECK(std::string("A very long book title with many words to check wrapping and touch navigation") != item.label);
  {
    CountedText target;
    fui::DeviceContext device;
    device.width = 480;
    device.height = 800;
    fui::InteractionBuffer<24> groups;
    fui::Frame<24> groupFrame(target, device, noInput, groups);
    fui::Screen<24> groupScreen(groupFrame, toybox::themeTokens());
    pixelui::begin(groupScreen);
    fui::ListItem books[4];
    for (auto& book : books) book.label = "BOOK";
    books[0].sectionHeading = "A";
    books[2].sectionHeading = "B";
    fui::ListProps list;
    libraryui::styleRows(list);
    list.items = books;
    list.count = 4;
    list.action = 10;
    CHECK(list.rowStyles.normal.borderWidth == 2);
    CHECK(list.subtitleText.maxLines == 1);
    pixelui::list(groupScreen, list, false, true);
    CHECK(target.frames.size() == 2);
    if (target.frames.size() == 2) {
      CHECK(target.frames[0].y > groupScreen.body().y);
      CHECK(target.frames[0].bottom() < target.frames[1].y);
    }
  }
  std::printf("Pixel menus: %d checks, %d failed; row workspace %zu bytes\n", checks, failures,
              sizeof(pixelui::RowSource));
  return failures ? 1 : 0;
}

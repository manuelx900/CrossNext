#include <FreeInkApp.h>
#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/home/HomeMenuInput.h"
#include "../../src/apps_local/home/HomeScreen.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_18.h"
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

struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, carouselTitle{toybox_18};
  fui::DisplayTarget target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};

  Canvas(int w, int h)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(target.deviceContext()) {
    target.setFont(0, small.font);
    target.setFont(1, body.font);
    target.setFont(2, title.font);
    target.setFont(3, tiny.font);
    target.setFont(4, carouselTitle.font);
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
  homeui::Model model;
  homeui::formatDate(model.date, sizeof(model.date), 2026, 10, 15);
  CHECK(std::string(model.date) == "15 OCT");
  model.battery = 100;
  model.selected = 1;
  homeui::copyLabel(model.title, sizeof(model.title), "Alice's Adventures in Wonderland");
  homeui::copyLabel(model.author, sizeof(model.author), "Lewis Carroll");
  homeui::copyLabel(model.continueLabel, sizeof(model.continueLabel), "Continue reading");
  homeui::copyLabel(model.emptyTitle, sizeof(model.emptyTitle), "No open book");
  homeui::copyLabel(model.emptyHelp, sizeof(model.emptyHelp), "Start reading a book to see it here.");
  static constexpr const char* labels[] = {"Browse Files", "Recent Books", "File Transfer", "Settings",
                                           "Games",        "Apps",         "Plugins",       "OPDS"};
  static constexpr homeui::Icon icons[] = {homeui::Icon::Files,    homeui::Icon::Library, homeui::Icon::Transfer,
                                           homeui::Icon::Settings, homeui::Icon::Games,   homeui::Icon::Apps,
                                           homeui::Icon::Plugins,  homeui::Icon::Plugins};
  for (int n = 0; n < 8; ++n) {
    homeui::copyLabel(model.rows[n].label, sizeof(model.rows[n].label), labels[n]);
    model.rows[n].icon = icons[n];
  }
  for (auto dimensions : {std::pair<int, int>{480, 800}, {800, 480}}) {
    for (int count : {6, 7, 8})
      for (bool book : {true, false}) {
        Canvas canvas(dimensions.first, dimensions.second);
        model.count = count;
        model.hasBook = book;
        const auto geometry = homeui::layout(canvas.device, count);
        struct Context {
          const homeui::Model* model;
          const homeui::Layout* geometry;
          fui::ButtonProps hit;
        } context{&model, &geometry, {}};
        fui::FreeInkApp<24, 6> app(canvas.target, canvas.device);
        app.setScreen(
            [](fui::FreeInkApp<24, 6>::ScreenType& screen, void* user) {
              auto& ctx = *static_cast<Context*>(user);
              homeui::build(screen, *ctx.model, *ctx.geometry, ctx.hit);
            },
            &context);
        app.render();
        auto tap = [&](int x, int y) {
          fui::InputSnapshot input;
          input.touchReleased = true;
          input.touchX = static_cast<int16_t>(x);
          input.touchY = static_cast<int16_t>(y);
          return app.route(input);
        };
        for (int n = 0; n < count; ++n) {
          const auto r = geometry.rows[n];
          CHECK(r.x >= 0 && r.y >= 0 && r.right() <= canvas.width && r.bottom() <= canvas.height);
          CHECK(r.height >= 44);
          const auto event = tap(r.x + r.width / 2, r.y + r.height / 2);
          CHECK(event.action == homeui::kSelect && event.value == n + (book ? 1 : 0));
        }
        CHECK(!tap(0, 0));
        const auto resume = tap(geometry.card.x + 10, geometry.card.y + 10);
        CHECK(book ? (resume.action == homeui::kSelect && resume.value == 0) : !resume);
        for (auto r : {geometry.card, geometry.cover, geometry.title, geometry.author, geometry.badge, geometry.menu}) {
          CHECK(r.width > 0 && r.height > 0 && r.x >= 0 && r.y >= 0 && r.right() <= canvas.width &&
                r.bottom() <= canvas.height);
        }
        const auto previous = tap(geometry.previous.x + 22, geometry.previous.y + 22);
        const auto next = tap(geometry.next.x + 22, geometry.next.y + 22);
        CHECK(!previous && !next);
        if (book) {
          model.canPrevious = model.canNext = true;
          app.render();
          const auto prevEvent = tap(geometry.previous.x + 22, geometry.previous.y + 22);
          const auto nextEvent = tap(geometry.next.x + 22, geometry.next.y + 22);
          CHECK(prevEvent.value == homeui::kPreviousBook && nextEvent.value == homeui::kNextBook);
          model.canPrevious = model.canNext = false;
          app.render();
        }
        if (book && argc > 2) {
          FILE* art = std::fopen(argv[2], "rb");
          CHECK(art != nullptr);
          if (art) {
            std::vector<uint8_t> pixels(21 * 224);
            CHECK(std::fread(pixels.data(), 1, pixels.size(), art) == pixels.size());
            std::fclose(art);
            fui::BitmapRef bitmap;
            bitmap.data = pixels.data();
            bitmap.width = 168;
            bitmap.height = 224;
            bitmap.format = fui::BitmapFormat::Mask1;
            canvas.target.bitmap(geometry.cover, bitmap, fui::BitmapMode::Contain);
          }
        }
        if (argc > 1)
          canvas.write(std::string(argv[1]) + "/" + std::to_string(canvas.width) + "-" + std::to_string(count) +
                       (book ? "-book.pbm" : "-empty.pbm"));
      }
  }
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    Canvas inset(size.first, size.second);
    inset.device.safeArea = {12, 16, 20, 24};
    const auto safe = inset.device.safeRect();
    const auto geometry = homeui::layout(inset.device, 7);
    for (auto rect : {geometry.card, geometry.menu}) {
      CHECK(rect.x >= safe.x && rect.y >= safe.y && rect.right() <= safe.right() && rect.bottom() <= safe.bottom());
    }
    homeui::copyLabel(model.title, sizeof(model.title),
                      "A very long title with many words that must wrap within the reading card and never overwrite "
                      "the menu below it");
    homeui::copyLabel(model.author, sizeof(model.author), "An author with a particularly long name");
    model.count = 7;
    model.hasBook = true;
    homeui::draw(inset.target, model, geometry);
    if (argc > 1) inset.write(std::string(argv[1]) + "/" + std::to_string(size.first) + "-long-insets.pbm");
  }
  // X4 Pro portrait: rotated byte alignment extends a cover snapshot into
  // the card outline. Cache both selection states and verify both transitions.
  {
    Canvas canvas(480, 800);
    canvas.device.safeArea = {10, 1, 0, 1};
    model.count = 6;
    model.hasBook = true;
    const auto geometry = homeui::layout(canvas.device, model.count);
    const int stride = (canvas.width + 7) / 8;
    const int y0 = geometry.cover.y / 8 * 8;
    const int y1 = (geometry.cover.bottom() + 7) / 8 * 8;
    const int sampleX = geometry.cover.x + geometry.cover.width / 2;
    const int sampleY = geometry.card.bottom() - 8;
    CHECK(y1 > sampleY && geometry.cover.bottom() <= sampleY);
    auto inkAt = [&](int x, int y) { return (canvas.pixels[y * stride + x / 8] & (0x80 >> (x % 8))) == 0; };
    for (int cachedSelection : {1, 0}) {
      std::fill(canvas.pixels.begin(), canvas.pixels.end(), 255);
      model.selected = cachedSelection;
      homeui::draw(canvas.target, model, geometry);
      const auto cached = canvas.pixels;
      homeui::drawCardSelection(canvas.target, model, geometry);
      CHECK(inkAt(sampleX, sampleY) == (cachedSelection == 0));
      model.selected = cachedSelection == 0 ? 1 : 0;
      std::fill(canvas.pixels.begin(), canvas.pixels.end(), 255);
      homeui::draw(canvas.target, model, geometry);
      for (int y = y0; y < y1; ++y)
        for (int x = geometry.cover.x; x < geometry.cover.right(); ++x) {
          const auto byte = y * stride + x / 8;
          const auto mask = static_cast<uint8_t>(0x80 >> (x % 8));
          canvas.pixels[byte] = static_cast<uint8_t>((canvas.pixels[byte] & ~mask) | (cached[byte] & mask));
        }
      homeui::drawCardSelection(canvas.target, model, geometry);
      CHECK(inkAt(sampleX, sampleY) == (model.selected == 0));
    }
  }
  for (auto size : {fui::Size{480, 800}, fui::Size{800, 480}}) {
    Canvas canvas(size.width, size.height);
    model.style = homeui::Style::Framed;
    model.hasBook = true;
    model.selected = 1;
    model.count = 6;
    homeui::copyLabel(model.title, sizeof(model.title), "Alice in Wonderland");
    const auto geometry = homeui::layout(canvas.device, model.count, model.style);
    CHECK(geometry.bookProgress.x >= geometry.cover.x);
    CHECK(geometry.bookProgress.right() <= geometry.cover.right());
    CHECK(geometry.bookProgress.y >= geometry.cover.y);
    CHECK(geometry.bookProgress.bottom() <= geometry.cover.bottom());
    for (int progress : {-1, 0, 62, 100}) {
      model.progress = progress;
      char percentage[8];
      std::snprintf(percentage, sizeof(percentage), "%d%%", std::clamp(progress, 0, 100));
      fui::TextStyle percentageStyle;
      percentageStyle.font = 0;
      CHECK(canvas.target.measureText(0, percentage, percentageStyle).width <= geometry.bookProgress.width - 4);
      std::fill(canvas.pixels.begin(), canvas.pixels.end(), 255);
      homeui::draw(canvas.target, model, geometry);
      canvas.target.fill(geometry.cover, fui::Paint::solid(fui::Color::Black));
      homeui::drawCardSelection(canvas.target, model, geometry);
      if (progress >= 0) {
        const auto ribbon = geometry.bookProgress;
        auto ink = [&](int x, int y) {
          return (canvas.pixels[y * ((canvas.width + 7) / 8) + x / 8] & (0x80 >> (x % 8))) == 0;
        };
        CHECK(ink(ribbon.x, ribbon.y));
        CHECK(!ink(ribbon.x + 2, ribbon.y + 2));
        CHECK(!ink(ribbon.x + 5, ribbon.y + 2));
      }
      if (argc > 1)
        canvas.write(std::string(argv[1]) + "/home-bookmark-" + std::to_string(size.width) + "-" +
                     std::to_string(progress) + ".pbm");
    }
  }
  for (auto style : {homeui::Style::Framed, homeui::Style::Carousel}) {
    for (auto size : {fui::Size{480, 800}, fui::Size{800, 480}}) {
      Canvas empty(size.width, size.height), spaces(size.width, size.height);
      homeui::Model untitledAuthor;
      untitledAuthor.style = style;
      untitledAuthor.hasBook = true;
      untitledAuthor.count = 6;
      homeui::copyLabel(untitledAuthor.title, sizeof(untitledAuthor.title),
                        "The extraordinary adventures of a reader who discovers a hidden world between the pages "
                        "of a very old book");
      const auto geometry = homeui::layout(empty.device, untitledAuthor.count, style);
      homeui::draw(empty.target, untitledAuthor, geometry);
      int extraInk = 0;
      for (int y = geometry.title.bottom(); y < geometry.author.bottom(); ++y)
        for (int x = geometry.title.x; x < geometry.title.right(); ++x)
          extraInk += (empty.pixels[y * ((empty.width + 7) / 8) + x / 8] & (0x80 >> (x % 8))) == 0;
      CHECK(extraInk > 0);
      homeui::copyLabel(untitledAuthor.author, sizeof(untitledAuthor.author), " \t\r\n");
      homeui::draw(spaces.target, untitledAuthor, geometry);
      CHECK(empty.pixels == spaces.pixels);
      if (argc > 1)
        empty.write(std::string(argv[1]) + "/home-no-author-" + std::to_string(size.width) + "-" +
                    std::to_string(int(style)) + ".pbm");
    }
  }
  for (auto style : {homeui::Style::Carousel})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (int count : {1, 6}) {
        Canvas canvas(size.first, size.second);
        model.style = style;
        model.count = count;
        model.hasBook = true;
        model.progress = 48;
        model.recentCount = 5;
        model.recentIndex = 2;
        const auto geometry = homeui::layout(canvas.device, count, style);
        CHECK(geometry.cover.bottom() < geometry.title.y);
        CHECK(geometry.title.bottom() <= geometry.author.y);
        CHECK(geometry.author.bottom() + 8 <= geometry.progress.y);
        CHECK(geometry.progress.bottom() <= geometry.menu.bottom());
        for (auto box : {geometry.cover, geometry.coverRegion, geometry.title, geometry.author, geometry.menu,
                         geometry.sides[0], geometry.sides[1], geometry.sides[2], geometry.sides[3]})
          CHECK(box.width > 0 && box.height > 0 && box.x >= 0 && box.y >= 0 && box.right() <= canvas.width &&
                box.bottom() <= canvas.height);
        for (int n = 0; n < count; ++n) {
          const auto row = geometry.rows[n];
          CHECK(row.width >= 44 && row.height >= 44 && row.right() <= canvas.width && row.bottom() <= canvas.height);
        }
        model.canPrevious = model.canNext = true;
        struct HitContext {
          const homeui::Model* model;
          const homeui::Layout* geometry;
          fui::ButtonProps hit;
        } ctx{&model, &geometry, {}};
        fui::FreeInkApp<24, 6> app(canvas.target, canvas.device);
        app.setScreen(
            [](fui::FreeInkApp<24, 6>::ScreenType& screen, void* user) {
              auto& c = *static_cast<HitContext*>(user);
              homeui::build(screen, *c.model, *c.geometry, c.hit);
            },
            &ctx);
        app.render();
        auto tap = [&](int x, int y) {
          fui::InputSnapshot input;
          input.touchReleased = true;
          input.touchX = x;
          input.touchY = y;
          return app.route(input);
        };
        CHECK(tap(geometry.cover.x + geometry.cover.width / 2, geometry.cover.y + 20).value == 0);
        CHECK(tap(geometry.cover.x - 4, geometry.cover.y + 20).value == homeui::kPreviousBook);
        CHECK(tap(geometry.cover.right() + 4, geometry.cover.y + 20).value == homeui::kNextBook);
        for (int n = 0; n < count; ++n) {
          const auto row = geometry.rows[n];
          CHECK(tap(row.x + row.width / 2, row.y + 20).value == n + 1);
        }
        homeui::draw(canvas.target, model, geometry);
        for (int progress : {-1, 0, 48, 100}) {
          model.progress = progress;
          std::fill(canvas.pixels.begin(), canvas.pixels.end(), 255);
          homeui::draw(canvas.target, model, geometry);
          const int margin = std::min(24, int(geometry.progress.width) / 12);
          const int x = geometry.progress.x + margin, y = geometry.progress.y + 5;
          auto ink = [&](int sx, int sy) {
            return (canvas.pixels[sy * ((canvas.width + 7) / 8) + sx / 8] & (0x80 >> (sx % 8))) == 0;
          };
          CHECK(ink(x, y));
          CHECK(ink(x, y + 13));
          CHECK(!ink(x - 1, y + 6));
          CHECK(!ink(x, y - 1));
          CHECK(!ink(x, y + 14));
          CHECK(ink(x + 3, y + 6) == (progress > 0));
        }
        if (argc > 1)
          canvas.write(std::string(argv[1]) + "/home-layout-" + std::to_string(int(style)) + "-" +
                       std::to_string(size.first) + "-" + std::to_string(count) + ".pbm");
      }
  for (auto style : {homeui::Style::Framed, homeui::Style::Carousel})
    for (auto size : {fui::Size{480, 800}, fui::Size{800, 480}})
      for (int count : {6, 7, 8})
        for (bool book : {false, true}) {
          model.style = style;
          model.hasBook = book;
          model.count = count;
          Canvas incremental(size.width, size.height), complete(size.width, size.height);
          const auto geometry = homeui::layout(incremental.device, count, style);
          model.selected = book ? 1 : 0;
          homeui::draw(incremental.target, model, geometry);
          for (int n = 0; n < count * 3; ++n) {
            const int next = (n * 5 + 1) % count + (book ? 1 : 0);
            const auto prior = incremental.pixels;
            homeui::drawMenuSelection(incremental.target, model, geometry, next);
            std::fill(complete.pixels.begin(), complete.pixels.end(), 255);
            homeui::draw(complete.target, model, geometry);
            CHECK(incremental.pixels == complete.pixels);
            for (int y = 0; y < size.height; ++y) {
              if (y >= geometry.menu.y && y < geometry.menu.bottom()) continue;
              const int stride = (size.width + 7) / 8;
              CHECK(std::equal(prior.begin() + y * stride, prior.begin() + (y + 1) * stride,
                               incremental.pixels.begin() + y * stride));
            }
          }
        }
  for (int books : {0, 1, 7})
    for (int rows : {1, 6, 7, 8}) {
      homeui::MenuInput input;
      int selected = books, expected = 0;
      for (int n = 0; n < 10001; ++n) {
        const int direction = n % 5 ? 1 : -1;
        CHECK(input.move(direction, rows));
        expected = (expected + direction + rows) % rows;
      }
      CHECK(input.push({homeui::MenuInput::Kind::Confirm}));
      homeui::MenuInput::Event event;
      CHECK(input.pop(event) && event.kind == homeui::MenuInput::Kind::Move);
      selected = homeui::MenuInput::selection(selected, books, rows, event.steps);
      CHECK(selected == books + expected);
      CHECK(input.pop(event) && event.kind == homeui::MenuInput::Kind::Confirm);
      CHECK(!input.pop(event));
      for (int n = 0; n < 16; ++n) CHECK(input.push({homeui::MenuInput::Kind::Confirm}));
      CHECK(!input.push({homeui::MenuInput::Kind::Resume}));
      input.clear();
      CHECK(!input.pop(event));
    }
  model.style = homeui::Style::Framed;
  for (int month = 1; month <= 12; ++month) {
    char date[20];
    homeui::formatDate(date, sizeof(date), 2027, month, 1);
    CHECK(std::string(date).substr(0, 3) == "01 ");
    CHECK(std::string(date).size() == 6);
  }
  char date[20];
  homeui::formatDate(date, sizeof(date), 2026, 12, 31);
  CHECK(std::string(date) == "31 DEC");
  homeui::formatDate(date, sizeof(date), 2027, 1, 1);
  CHECK(std::string(date) == "01 JAN");
  homeui::formatDate(date, sizeof(date), 0, 0, 0);
  CHECK(std::string(date) == "-- ---");
  char shortLabel[6];
  CHECK(homeui::copyLabel(shortLabel, sizeof(shortLabel), "a\xC3\xA9\xE7\x8C\xAB"));
  CHECK(std::string(shortLabel) == "A\xC3\x89");
  CHECK(!homeui::copyLabel(shortLabel, sizeof(shortLabel), "abcdef"));
  CHECK(std::string(shortLabel) == "ABCDE");
  homeui::formatDate(shortLabel, sizeof(shortLabel), 1970, 1, 1);
  CHECK(std::string(shortLabel) == "-- --");
  std::printf("Home UI: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

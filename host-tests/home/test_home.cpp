#include <ChromeRegion.h>
#include <FreeInkApp.h>
#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/home/CoverFit.h"
#include "../../src/apps_local/home/HomeMenuInput.h"
#include "../../src/apps_local/home/HomeScreen.h"
#include "../../src/apps_local/home/HomeShelfOrder.h"
#include "../../src/apps_local/home/LicoriceLayout.h"
#include "../../src/apps_local/ui/PixelText.h"

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
  for (bool wide : {false, true}) {
    fui::DeviceContext device;
    device.width = wide ? 800 : 480;
    device.height = wide ? 480 : 800;
    device.safeArea = {9, 7, 3, 7};
    const auto layout = homeui::shelfLayout(device);
    CHECK(layout.artHeight > 0 && layout.cellWidth > 0);
    CHECK(layout.grid.y + layout.rows * (layout.artHeight + 34) + (layout.rows - 1) * layout.gap < layout.footer.y);
    CHECK(layout.footer.bottom() == device.safeRect().bottom());
    CHECK(layout.artHeight == homeui::shelfLayout(device).artHeight);
  }
  for (const auto slot :
       {fui::makeRect(14, 640, 98, 148), fui::makeRect(14, 105, 140, 210), fui::makeRect(12, 350, 72, 108)})
    for (const auto source : {fui::Size{145, 218}, fui::Size{400, 600}, fui::Size{600, 400}, fui::Size{80, 120}}) {
      const auto fit = homecover::contain(slot, source.width, source.height);
      CHECK(fit.x >= slot.x && fit.y >= slot.y && fit.right() <= slot.right() && fit.bottom() <= slot.bottom());
      CHECK(fit.width <= source.width && fit.height <= source.height);
      CHECK(std::abs(int(fit.width) * source.height - int(fit.height) * source.width) <=
            std::max(source.width, source.height));
      CHECK(std::abs((fit.x - slot.x) - (slot.right() - fit.right())) <= 1);
      CHECK(std::abs((fit.y - slot.y) - (slot.bottom() - fit.bottom())) <= 1);
    }
  CHECK(homecover::contain(fui::makeRect(0, 0, 30, 40), 0, 100).empty());
  homeui::Model model;
  homeui::formatDate(model.date, sizeof(model.date), 2026, 10, 15);
  CHECK(std::string(model.date) == "15 OCT");
  model.battery = 100;
  model.progress = 67;
  model.recentCount = 3;
  model.selected = 1;
  homeui::copyLabel(model.title, sizeof(model.title), "Alice's Adventures in Wonderland");
  homeui::copyLabel(model.author, sizeof(model.author), "Lewis Carroll");
  homeui::copyLabel(model.continueLabel, sizeof(model.continueLabel), "Continue reading");
  homeui::copyLabel(model.emptyTitle, sizeof(model.emptyTitle), "No open book");
  homeui::copyLabel(model.emptyHelp, sizeof(model.emptyHelp), "Start reading a book to see it here.");
  static constexpr const char* labels[] = {"Browse Files", "Recent Books", "File Transfer", "Reading Stats",
                                           "Games",        "Apps",         "Settings",      "PET"};
  static constexpr homeui::Icon icons[] = {homeui::Icon::Files,    homeui::Icon::Library, homeui::Icon::Transfer,
                                           homeui::Icon::Stats,    homeui::Icon::Games,   homeui::Icon::Apps,
                                           homeui::Icon::Settings, homeui::Icon::Apps};
  for (int n = 0; n < 8; ++n) {
    homeui::copyLabel(model.rows[n].label, sizeof(model.rows[n].label), labels[n]);
    model.rows[n].icon = icons[n];
  }
  for (auto dimensions : {std::pair<int, int>{480, 800}, {800, 480}}) {
    for (int count : {1, 2, 3, 4, 5, 6, 7, 8})
      for (bool book : {true, false}) {
        for (int n = 0; n < homeui::kMaxRows; ++n) {
          homeui::copyLabel(model.rows[n].label, sizeof(model.rows[n].label), labels[n]);
          model.rows[n].icon = icons[n];
        }
        homeui::copyLabel(model.rows[6].label, sizeof(model.rows[6].label), count == 8 ? "Wordle" : "Settings");
        model.rows[6].icon = count == 8 ? homeui::Icon::Games : homeui::Icon::Settings;
        homeui::copyLabel(model.rows[7].label, sizeof(model.rows[7].label), "Settings");
        model.rows[7].icon = homeui::Icon::Settings;
        homeui::copyLabel(model.rows[count - 1].label, sizeof(model.rows[count - 1].label), "Settings");
        model.rows[count - 1].icon = homeui::Icon::Settings;
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
        const auto last = geometry.rows[count - 1];
        for (int x = last.x; x < last.right(); ++x)
          CHECK((canvas.pixels[(last.bottom() - 1) * ((canvas.width + 7) / 8) + x / 8] & (0x80 >> (x % 8))) == 0);
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
      for (int count : {1, 2, 3, 4, 5, 6, 7, 8}) {
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
        if (size.first < size.second) {
          CHECK(std::abs((geometry.rows[0].x - geometry.menu.x) -
                         (geometry.menu.right() - geometry.rows[count - 1].right())) <= 1);
        } else {
          CHECK(std::abs((geometry.rows[0].y - geometry.menu.y) -
                         (geometry.menu.bottom() - geometry.rows[count - 1].bottom())) <= 1);
          for (int n = 0; n < count; n += 2) {
            const auto right = geometry.rows[std::min(n + 1, count - 1)].right();
            CHECK(std::abs((geometry.rows[n].x - geometry.menu.x) - (geometry.menu.right() - right)) <= 1);
          }
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
      for (int count : {1, 2, 3, 4, 5, 6, 7, 8})
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
  for (int n = 0; n < 8; ++n) model.rows[n].icon = icons[n];
  for (const auto size : {fui::Size{480, 800}, fui::Size{800, 480}}) {
    Canvas c(size.width, size.height);
    model.style = homeui::Style::Boox;
    model.count = 7;
    model.hasBook = true;
    auto g = homeui::layout(c.device, model.count, model.style, &model);
    CHECK(g.cover.height > 200);
    CHECK(g.cover.width > 100);
    CHECK(g.coverRegion.bottom() <= c.height);
    CHECK(g.title.bottom() <= g.author.y);
    CHECK(g.author.bottom() <= g.progress.y);
    model.selected = 1;
    homeui::draw(c.target, model, g);
    const auto focusedFiles = c.pixels;
    model.selected = 2;
    homeui::draw(c.target, model, g);
    CHECK(c.pixels != focusedFiles);
    CHECK(!g.settingsButton.empty());
    CHECK(g.progress.bottom() < g.menu.y);
    for (int n = 0; n < 6; ++n) {
      CHECK(!g.primary[n].empty());
      CHECK(g.primary[n].bottom() <= c.height);
      CHECK(g.primary[n].x >= g.menu.x);
      CHECK(g.primary[n].right() <= g.menu.right());
    }
  }
  for (int n = 0; n < homeui::kPiccoActions; ++n) CHECK(homeui::piccoIndex(homeui::piccoAction(n)) == n);
  CHECK(homeui::piccoIndex(static_cast<homeui::QuickAction>(-1)) == -1);
  chrome::lightHeader = false;
  CHECK(!chrome::invertHeaderForOutput(false));
  CHECK(chrome::invertHeaderForOutput(true));
  chrome::lightHeader = true;
  CHECK(chrome::invertHeaderForOutput(false));
  CHECK(!chrome::invertHeaderForOutput(true));
  chrome::lightHeader = false;
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
  char natural[48];
  pixelui::copyNatural(natural, sizeof(natural), "APPS");
  CHECK(std::string(natural) == "Apps");
  pixelui::copyNatural(natural, sizeof(natural), "CONFIGURACIÓN");
  CHECK(std::string(natural) == "Configuración");
  pixelui::copyNatural(natural, sizeof(natural), "Wi-Fi, Sync and OPDS");
  CHECK(std::string(natural) == "Wi-Fi, Sync and OPDS");
  pixelui::copyNatural(natural, sizeof(natural), "EPUB books", pixelui::LabelCase::Preserve);
  CHECK(std::string(natural) == "EPUB books");
  pixelui::copyNatural(natural, sizeof(natural), "XTC / XTCH", pixelui::LabelCase::Preserve);
  CHECK(std::string(natural) == "XTC / XTCH");
  char shortLabel[6];
  CHECK(homeui::copyLabel(shortLabel, sizeof(shortLabel), "a\xC3\xA9\xE7\x8C\xAB"));
  CHECK(std::string(shortLabel) == "a\xC3\xA9");
  CHECK(!homeui::copyLabel(shortLabel, sizeof(shortLabel), "abcdef"));
  CHECK(std::string(shortLabel) == "abcde");
  homeui::copyLabel(natural, sizeof(natural), "AJUSTES");
  CHECK(std::strcmp(natural, "Ajustes") == 0);
  homeui::copyLabel(natural, sizeof(natural), "Frankenstein");
  CHECK(std::strcmp(natural, "Frankenstein") == 0);
  homeui::copyLabel(natural, sizeof(natural), "Mary Shelley");
  CHECK(std::strcmp(natural, "Mary Shelley") == 0);
  homeui::formatDate(shortLabel, sizeof(shortLabel), 1970, 1, 1);
  CHECK(std::string(shortLabel) == "-- --");
  {
    homeui::ShelfOrder order;
    CHECK(order.add(homeui::ShelfSource::Recent, 4, 90));
    CHECK(order.add(homeui::ShelfSource::Recent, 2, 70));
    CHECK(order.add(homeui::ShelfSource::Index, 33, 90));
    CHECK(order.count == 2);
    CHECK(order.entries[0].index == 4 && order.entries[0].source == homeui::ShelfSource::Recent);
    CHECK(order.entries[1].hash == 70);
    CHECK(order.add(homeui::ShelfSource::Index, 22, 50));
    CHECK(order.entries[2].source == homeui::ShelfSource::Index && order.entries[2].index == 22);
    for (int n = 0; n < 15; ++n) CHECK(order.add(homeui::ShelfSource::Index, n, 100 + n));
    CHECK(order.count == 18);
    while (order.count < int(std::size(order.entries)))
      CHECK(order.add(homeui::ShelfSource::Index, order.count, 1000 + order.count));
    CHECK(!order.add(homeui::ShelfSource::Recent, 0, 500));
    CHECK(order.count == int(std::size(order.entries)));
  }
  std::printf("Home UI: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

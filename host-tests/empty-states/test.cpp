#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/notes/NotesScreens.h"
#include "../../src/apps_local/quotes/QuotesScreens.h"
#include "../../src/apps_local/words/WordsScreens.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#pragma GCC diagnostic pop

namespace fui = freeink::ui;
namespace {
int checks = 0, failures = 0;
const char* scenario = "";
#define CHECK(expr)                                                    \
  do {                                                                 \
    ++checks;                                                          \
    if (!(expr)) {                                                     \
      ++failures;                                                      \
      std::printf("FAIL %s line %d: %s\n", scenario, __LINE__, #expr); \
    }                                                                  \
  } while (0)

struct FontBridge {
  std::array<fui::FontGlyph, 224> glyphs{};
  fui::BitmapFont font{};
  explicit FontBridge(const EpdFontData& source) {
    for (uint32_t cp = 32; cp <= 255; ++cp) {
      for (uint32_t i = 0; i < source.intervalCount; ++i) {
        const auto& interval = source.intervals[i];
        if (cp < interval.first || cp > interval.last) continue;
        const auto& g = source.glyph[interval.offset + cp - interval.first];
        glyphs[cp - 32] = {static_cast<uint16_t>(g.dataOffset),
                           g.width,
                           g.height,
                           static_cast<uint8_t>(fp4::toPixel(g.advanceX)),
                           static_cast<int8_t>(g.left),
                           static_cast<int8_t>(-g.top)};
        break;
      }
    }
    font = {source.bitmap, glyphs.data(), 32, 255, source.advanceY, static_cast<uint8_t>(source.ascender), 64, 64, 1};
  }
};

struct Target final : fui::DrawTarget {
  fui::DisplayTarget backend;
  const char* expected;
  bool found = false;
  fui::Rect notice{};
  Target(uint8_t* data, fui::Orientation orientation, const char* expected)
      : backend(data, 800, 480, 100, orientation), expected(expected) {}
  fui::Size measureText(fui::FontId f, const char* s, fui::TextStyle st) const override {
    return backend.measureText(f, s, st);
  }
  int16_t lineHeight(fui::FontId f) const override { return backend.lineHeight(f); }
  fui::Rect clipRect() const override { return backend.clipRect(); }
  bool setClipRect(fui::Rect r) override { return backend.setClipRect(r); }
  fui::Rect inkBounds(fui::FontId f, const char* s, fui::TextStyle st) const override {
    return backend.inkBounds(f, s, st);
  }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius, uint8_t corners) override {
    backend.fill(r, p, radius, corners);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t w, uint8_t radius, uint8_t corners) override {
    backend.stroke(r, p, w, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override { backend.line(a, b, w, p); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { backend.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef b, fui::BitmapMode m, fui::Paint p, fui::Rotation rotation) override {
    backend.bitmap(r, b, m, p, rotation);
  }
  void text(fui::Rect r, const char* value, fui::TextStyle style) override {
    if (value && !std::strcmp(value, expected)) {
      found = true;
      notice = r;
      const char* at = expected;
      fui::layoutText(backend, r, value, style, [&](const char* run, fui::Rect row) {
        while (*at == ' ' || *at == '\n') ++at;
        const size_t n = std::strlen(run);
        const bool matches = !std::strncmp(at, run, n);
        CHECK(matches);
        if (matches) at += n;
        CHECK(row.bottom() <= r.bottom());
        CHECK(backend.measureText(style.font, run, style).width <= r.width);
      });
      while (*at == ' ' || *at == '\n') ++at;
      CHECK(!*at);
    }
    backend.text(r, value, style);
  }
};

struct Canvas {
  std::vector<uint8_t> pixels = std::vector<uint8_t>(48000, 255);
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, prose{ui_prose_10};
  Target target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;
  static fui::DeviceContext context(const Target& t) {
    auto d = t.backend.deviceContext();
    d.hasTouch = true;
    d.safeArea = {9, 7, 3, 7};
    return d;
  }
  Canvas(fui::Orientation o, const char* expected)
      : target(pixels.data(), o, expected),
        device(context(target)),
        frame(target, device, input, interactions),
        screen(frame) {
    target.backend.setFont(0, small.font);
    target.backend.setFont(1, body.font);
    target.backend.setFont(2, title.font);
    target.backend.setFont(3, tiny.font);
    target.backend.setFont(4, prose.font);
  }
  void verify(const char* directory, const std::string& name) {
    CHECK(target.found);
    const auto r = target.notice;
    CHECK(r.x >= 0 && r.y >= 0 && r.right() <= device.width && r.bottom() <= device.height);
    for (size_t i = 0; i < interactions.count(); ++i) {
      const auto h = interactions.data()[i].rect;
      const bool overlaps = r.x < h.right() && h.x < r.right() && r.y < h.bottom() && h.y < r.bottom();
      CHECK(!overlaps);
    }
    if (directory) {
      const auto path = std::string(directory) + "/" + name + ".pbm";
      FILE* file = std::fopen(path.c_str(), "wb");
      if (!file) std::abort();
      std::fprintf(file, "P4\n800 480\n");
      for (uint8_t byte : pixels) std::fputc(byte ^ 255, file);
      std::fclose(file);
    }
  }
};
}  // namespace

int main(int argc, char** argv) {
  const fui::Orientation orientations[] = {fui::Orientation::Portrait, fui::Orientation::PortraitInverted,
                                           fui::Orientation::LandscapeClockwise,
                                           fui::Orientation::LandscapeCounterClockwise};
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (int i = 0; i < 4; ++i) {
      const std::string suffix = std::string(lang == Language::EN ? "en-" : "es-") + std::to_string(i);
      for (bool todo : {false, true}) {
        scenario = todo ? "Tasks" : "Notes";
        Canvas c(orientations[i], todo ? tr(STR_TODO_EMPTY) : tr(STR_NOTES_EMPTY));
        notesui::DeckModel model;
        model.todo = todo;
        notesui::buildDeck(c.screen, model);
        c.verify(argc > 1 ? argv[1] : nullptr, std::string(scenario) + "-" + suffix);
      }
      for (int tab = 0; tab < 3; ++tab) {
        scenario = "Quotes";
        Canvas c(orientations[i], tab == 2 ? tr(STR_RT_NO_FAVORITES) : tr(STR_QUOTES_HELP));
        quotesui::ListModel model;
        model.tab = tab;
        quotesui::list(c.screen, model);
        c.verify(argc > 1 ? argv[1] : nullptr, "Quotes-" + std::to_string(tab) + "-" + suffix);
      }
      {
        scenario = "Book quotes";
        Canvas c(orientations[i], tr(STR_QUOTES_HELP));
        quotesui::ListModel model;
        model.inBook = true;
        quotesui::list(c.screen, model);
        c.verify(argc > 1 ? argv[1] : nullptr, "Book-quotes-" + suffix);
      }
      scenario = "Words";
      Canvas c(orientations[i], tr(STR_WORDS_EMPTY_GUIDE));
      c.target.backend.setFont(3, c.prose.font);
      wordsui::Model model;
      model.view = wordsui::View::List;
      wordsui::Geometry geometry;
      pagedtext::Work work;
      wordsui::build(c.screen, model, geometry, work);
      c.verify(argc > 1 ? argv[1] : nullptr, "Words-" + suffix);
    }
  }
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

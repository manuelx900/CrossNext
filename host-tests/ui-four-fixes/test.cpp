#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/focus/FocusScreens.h"
#include "../../src/apps_local/home/HomeScreen.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/TextAlignment.h"
#include "../../src/apps_local/words/DictionaryScreen.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
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

void headings() {
  for (bool caps : {false, true}) {
    pixelui::uppercaseLabels = caps;
    for (const auto& sample : std::vector<std::pair<const char*, const char*>>{{"pan", "Pan"},
                                                                               {"árbol", "Árbol"},
                                                                               {"ñandú", "Ñandú"},
                                                                               {"über", "Über"},
                                                                               {"iPhone", "IPhone"},
                                                                               {"NASA", "NASA"},
                                                                               {"κόσμος", "Κόσμος"},
                                                                               {"мир", "Мир"},
                                                                               {"中文", "中文"},
                                                                               {"😀 pan", "😀 pan"},
                                                                               {"", ""},
                                                                               {" pan", " Pan"}}) {
      char title[192];
      dictionaryui::copyWordHeading(title, sizeof(title), sample.first);
      CHECK(std::string(title) == sample.second);
    }
    char tiny[4] = {'x', 'x', 'x', '!'};
    dictionaryui::copyWordHeading(tiny, 3, "árbol");
    CHECK(std::string(tiny) == "Á" && tiny[3] == '!');
    dictionaryui::copyWordHeading(tiny, 1, "pan");
    CHECK(tiny[0] == 0);
    dictionaryui::copyWordHeading(nullptr, 0, "pan");
  }
}
void save(const char* out, const char* name, int width, int height, const std::vector<uint8_t>& bits) {
  if (!out) return;
  char path[1024];
  std::snprintf(path, sizeof(path), "%s/%s-%d.pbm", out, name, width);
  auto* f = std::fopen(path, "wb");
  CHECK(f);
  std::fprintf(f, "P4\n%d %d\n", width, height);
  for (auto b : bits) std::fputc(b ^ 255, f);
  std::fclose(f);
}
void screens(const char* out) {
  Font normal(ui_prose_14), small(ui_prose_10), pixel(toybox_14), tile(toybox_10);
  for (bool wide : {false, true})
    for (bool light : {false, true})
      for (bool caps : {false, true}) {
        const int w = wide ? 800 : 480, h = wide ? 480 : 800;
        std::vector<uint8_t> bits(w * h / 8, 255);
        Target t(bits.data(), w, h, w / 8);
        for (int n = 0; n < 8; ++n) t.setFont(n, caps ? pixel.font : normal.font);
        t.setFont(3, caps ? tile.font : small.font);
        chrome::lightHeader = light;
        pixelui::uppercaseLabels = caps;
        I18N.setLanguage(Language::ES);
        toybox::Interactions hits;
        const fui::InputSnapshot input{};
        auto device = t.deviceContext();
        toybox::Frame frame(t, device, input, hits);
        toybox::Screen screen(frame);
        for (bool lookup : {false, true}) {
          std::fill(bits.begin(), bits.end(), 255);
          t.runs.clear();
          dictionaryui::Work work;
          dictionaryui::Model m;
          m.word = "pan";
          m.lookup = lookup;
          dictionaryui::build(screen, m, work);
          bool found = false;
          for (const auto& r : t.runs) found |= r.text == "Pan";
          CHECK(found);
          CHECK(std::string(m.word) == "pan");
          if (light && !caps && !lookup) save(out, "dictionary", w, h, bits);
        }
        for (auto style : {homeui::Style::Framed, homeui::Style::Carousel}) {
          std::fill(bits.begin(), bits.end(), 255);
          t.runs.clear();
          homeui::Model m;
          m.style = style;
          m.hasBook = true;
          m.count = 4;
          m.battery = 80;
          m.progress = 35;
          std::strcpy(m.continueLabel, "Seguir leyendo");
          std::strcpy(m.title, "La biblioteca de la medianoche");
          std::strcpy(m.author, "Matt Haig");
          std::strcpy(m.date, "Jue 5 nov");
          std::strcpy(m.time, "15:00");
          const char* rows[] = {"Archivos", "Biblioteca", "Estadísticas", "Ajustes"};
          for (int n = 0; n < 4; ++n) std::strcpy(m.rows[n].label, rows[n]);
          const auto g = homeui::layout(device, m.count, style, &m);
          homeui::draw(t, m, g);
          if (!g.badge.empty()) {
            std::string shown;
            unsigned lines = 0;
            for (const auto& r : t.runs)
              if (r.rect.x >= g.badge.x && r.rect.right() <= g.badge.right() &&
                  r.rect.y + t.lineHeight(r.style.font) > g.badge.y && r.rect.y < g.badge.bottom()) {
                if (!shown.empty()) shown += " ";
                shown += r.text;
                ++lines;
                CHECK(r.style.maxLines == 1);
              }
            CHECK(lines == 1);
            if (t.measureText(3, "SEGUIR LEYENDO", {}).width <= g.badge.width - 14) {
              CHECK(shown == "SEGUIR LEYENDO");
            } else {
              CHECK(shown == (caps ? "SEGUIR LEYENDO" : "Seguir leyendo"));
            }
          }
          if (light && !caps) save(out, style == homeui::Style::Framed ? "framed" : "carousel", w, h, bits);
        }
        std::fill(bits.begin(), bits.end(), 255);
        t.runs.clear();
        focus::State state;
        focus::addProject(state, "Estudio");
        focus::addTask(state, -1, "Leer", focus::TaskList::Today);
        focusui::Model model{&state};
        focusui::Work work;
        model.view = focusui::View::Menu;
        focusui::build(screen, model, work);
        if (light && !caps) save(out, "focus", w, h, bits);
      }
}
}  // namespace
int main(int argc, char** argv) {
  headings();
  screens(argc > 1 ? argv[1] : nullptr);
  std::printf("UI fixes: %u checks, 0 failed\n", checks);
}

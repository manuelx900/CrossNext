#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/carnivoro/CarnivoroScreens.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
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
      if (failures > 30) std::exit(1);               \
      std::printf("FAIL %d: %s\n", __LINE__, #expr); \
    }                                                \
  } while (0)

// The same glyph bitmaps and metrics as the firmware, adapted to the SDK's
// freestanding framebuffer target. This is a host preview, not a device shot.
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

fui::DeviceContext makeDevice(fui::DisplayTarget& target, bool inverted) {
  auto d = target.deviceContext();
  d.hasTouch = true;
  if (d.height > d.width) {
    d.safeArea = inverted ? fui::Insets{0, 1, 10, 1} : fui::Insets{10, 1, 0, 1};
    d.orientation = inverted ? fui::Orientation::PortraitInverted : fui::Orientation::Portrait;
  } else {
    d.safeArea = inverted ? fui::Insets{1, 0, 1, 10} : fui::Insets{1, 10, 1, 0};
    d.orientation = inverted ? fui::Orientation::LandscapeCounterClockwise : fui::Orientation::LandscapeClockwise;
  }
  return d;
}
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30};
  fui::DisplayTarget target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;

  Canvas(int w, int h, bool inverted = false)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(makeDevice(target, inverted)),
        frame(target, device, input, interactions),
        screen(frame) {
    target.setFont(3, tiny.font);
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
      CHECK(r.width >= 44 && r.height >= 32);
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
struct TextAudit : fui::DrawTarget {
  fui::DrawTarget& target;
  int width, height;
  bool paint = false;
  int caseId = 0, view = 0, page = 0;
  TextAudit(fui::DrawTarget& t, int w, int h) : target(t), width(w), height(h) {}
  fui::Size measureText(fui::FontId f, const char* s, fui::TextStyle st) const override {
    return target.measureText(f, s, st);
  }
  int16_t lineHeight(fui::FontId f) const override { return target.lineHeight(f); }
  fui::Rect inkBounds(fui::FontId f, const char* s, fui::TextStyle st) const override {
    return target.inkBounds(f, s, st);
  }
  void fill(fui::Rect r, fui::Paint p, uint8_t a = 0, uint8_t b = fui::CornersAll) override {
    if (paint) target.fill(r, p, a, b);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t w, uint8_t a = 0, uint8_t b = fui::CornersAll) override {
    if (paint) target.stroke(r, p, w, a, b);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override {
    if (paint) target.line(a, b, w, p);
  }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override {
    if (paint) target.triangle(a, b, c, p);
  }
  void bitmap(fui::Rect r, fui::BitmapRef b, fui::BitmapMode m, fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rot = fui::Rotation::None) override {
    if (paint) target.bitmap(r, b, m, p, rot);
  }
  void text(fui::Rect r, const char* s, fui::TextStyle st) override {
    bool fit = true;
    fui::layoutText(target, r, s, st, [&](const char* run, fui::Rect box) {
      if (box.bottom() > r.bottom() || box.right() > r.right() || std::strstr(run, "…")) fit = false;
    });
    if (!fit) {
      std::printf("TEXT CUT case=%d view=%d page=%d [%d,%d %dx%d] %s\n", caseId + 1, view, page, r.x, r.y, r.width,
                  r.height, s);
      CHECK(fit);
    }
    if (paint) target.text(r, s, st);
  }
};

}  // namespace
int main(int argc, char** argv) {
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (bool inverted : {false, true}) {
        I18N.setLanguage(lang);
        {
          Canvas metrics(size.first, size.second, inverted);
          FontBridge prose(ui_prose_10);
          metrics.target.setFont(3, prose.font);
        }
        Canvas c(size.first, size.second, inverted);
        FontBridge prose(ui_prose_10);
        c.target.setFont(3, c.tiny.font);
        c.target.setFont(4, prose.font);
        TextAudit audit(c.target, size.first, size.second);
        for (int caseId = 0; caseId < carnivoro::kCases; ++caseId) {
          if (argc > 2 && caseId != 999) continue;
          carnivoro::State state;
          state.caseId = caseId;
          state.difficulty = carnivoro::difficulty(caseId);
          state.picks = 4;
          state.innocents = 2;
          for (int i = 0; i < carnivoro::kAnimals; ++i) state.notes[i] = static_cast<carnivoro::Note>(i % 3);
          for (int i = 0; i < carnivoro::kCases; ++i)
            state.status[i] = i < caseId ? carnivoro::Status::Solved : carnivoro::Status::Active;
          carnivoroui::Model m;
          m.state = &state;
          audit.caseId = caseId;
          m.solution = carnivoro::solve(*carnivoro::getCase(caseId));
          static constexpr carnivoroui::View views[] = {
              carnivoroui::View::Menu,         carnivoroui::View::Cases,    carnivoroui::View::Intro,
              carnivoroui::View::Statements,   carnivoroui::View::Accuse,   carnivoroui::View::Verdict,
              carnivoroui::View::ResetConfirm, carnivoroui::View::Help,     carnivoroui::View::Verdict,
              carnivoroui::View::Verdict,      carnivoroui::View::Solution, carnivoroui::View::Solution,
              carnivoroui::View::Overview,     carnivoroui::View::Overview, carnivoroui::View::Overview};
          for (int view = 0; view < 15; ++view) {
            if ((view == 0 || view == 6 || view == 7) && caseId != 0) continue;
            if (argc > 2 && view != 3 && view != 10 && view != 12 && view != 13) continue;
            m.view = views[view];
            audit.view = view;
            if (view == 1)
              for (auto& status : state.status) status = carnivoro::Status::New;
            state.status[caseId] = (view == 5 || view == 10 || view == 13)   ? carnivoro::Status::Solved
                                   : (view == 9 || view == 11 || view == 14) ? carnivoro::Status::Exhausted
                                                                             : carnivoro::Status::Active;
            state.misses[caseId] = view == 8 ? 1
                                   : (view == 9 || view == 11 || view == 14)
                                       ? carnivoro::lives(carnivoro::difficulty(caseId))
                                       : 0;
            if (view == 8 && carnivoro::lives(carnivoro::difficulty(caseId)) < 2) continue;
            const int count = carnivoro::getCase(caseId)->count;
            const int pageCount = view == 7                               ? (size.first == 480 ? 5 : 10)
                                  : view == 3 || view == 10 || view == 11 ? count
                                  : view == 4 && size.first == 800        ? (count + 2) / 3
                                                                          : 1;
            for (int page = 0; page < pageCount; ++page) {
              if ((view == 0 || view == 6) && page) break;
              audit.paint = argc > 1 && (caseId == 0 || caseId == 300 || caseId == 999) && (page < 2 || view == 7);
              std::fill(c.pixels.begin(), c.pixels.end(), 255);
              c.interactions.beginBuild();
              c.interactions.clear();
              m.page = page;
              // A name in the overview must open its containing compact card page.
              m.openSuspect = (view == 3 || view == 10 || view == 11) ? page : -1;
              audit.page = page;
              m.casePage = carnivoro::caseIndex(caseId) / (size.first == 480 ? 16 : 8);
              pixelui::Header header;
              header.title = tr(STR_CAR_TITLE);
              header.back = true;
              header.battery = 100;
              if (audit.paint) pixelui::header(c.target, c.device, header);
              carnivoroui::Geometry g;
              pagedtext::Work work;
              carnivoroui::draw(audit, c.device, m, g, work);
              if (view == 7 && page >= m.textPages) break;
              if (view == 3 || view == 10 || view == 11) {
                CHECK(m.openSuspect == -1);
                CHECK(m.page == page / g.suspectCapacity);
                if (size.first == 480) CHECK(g.suspectCapacity >= 2);
                if (size.first == 480 && caseId == 999) CHECK(g.suspectCapacity == 3);
              }
              for (int i = 0; i < g.count; ++i)
                c.frame.hit(g.hits[i].box, g.hits[i].action, g.hits[i].value, fui::InputTouch);
              c.checkHits();
              if (view == 3 || view == 10 || view == 11) CHECK(c.has(carnivoroui::ShowOverview));
              if (m.view == carnivoroui::View::Cases) {
                for (int n = 0; n < 4; ++n) CHECK(c.has(carnivoroui::SelectDifficulty, n));
                for (int n = 0; n < g.caseCapacity; ++n) {
                  const int id = carnivoro::caseAt(state.difficulty, m.casePage * g.caseCapacity + n);
                  if (id >= 0) CHECK(c.has(carnivoroui::ChooseCase, id));
                }
              }
              for (int i = 0; i < g.count; ++i)
                for (int j = 0; j < i; ++j)
                  CHECK(g.hits[i].box.right() <= g.hits[j].box.x || g.hits[j].box.right() <= g.hits[i].box.x ||
                        g.hits[i].box.bottom() <= g.hits[j].box.y || g.hits[j].box.bottom() <= g.hits[i].box.y);
              if (m.view == carnivoroui::View::Statements) {
                int notes = 0, marks = 0;
                for (int i = 0; i < g.count; ++i) {
                  if (g.hits[i].action == carnivoroui::Note) {
                    CHECK(g.hits[i].value / 3 >= m.page * g.suspectCapacity);
                    ++notes;
                  }
                  if (g.hits[i].action == carnivoroui::CycleGuilt) ++marks;
                }
                CHECK(notes == 3 * std::min(g.suspectCapacity,
                                            int(carnivoro::getCase(caseId)->count) - m.page * g.suspectCapacity));
                CHECK(marks * 3 == notes);
              }
              if (m.view == carnivoroui::View::Overview) {
                const bool closed = carnivoro::closed(state, caseId);
                CHECK(g.count == (closed ? count : 3 * count) + 1);
                for (int i = 0; i < count; ++i) {
                  CHECK(c.has(carnivoroui::ReadStatement, i));
                  CHECK(c.has(carnivoroui::CycleNote, i) == !closed);
                  CHECK(c.has(carnivoroui::CycleGuilt, i) == !closed);
                }
              }
              if (m.view == carnivoroui::View::Verdict || m.view == carnivoroui::View::Solution) {
                CHECK(!c.has(carnivoroui::Note));
                CHECK(!c.has(carnivoroui::Mark));
                CHECK(!c.has(carnivoroui::CycleGuilt));
                CHECK(c.has(carnivoroui::ViewSolution) ==
                      (m.view == carnivoroui::View::Verdict && carnivoro::closed(state, caseId)));
              }
              if (argc > 1 && (caseId == 0 || caseId == 300 || caseId == 999) && (page < 2 || view == 7))
                c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                        std::to_string(size.first) + "-" + std::to_string(caseId) + "-" + std::to_string(view) + "-" +
                        std::to_string(page) + (inverted ? "-inverted" : "") + ".pbm");
            }
          }
          for (int speaker = 0; speaker < carnivoro::getCase(caseId)->count; ++speaker) {
            if (carnivoro::getCase(caseId)->statements[speaker].kind != carnivoro::Claim::Neighbour) continue;
            char clue[224];
            carnivoroui::statement(*carnivoro::getCase(caseId), speaker, clue, sizeof(clue));
            const int count = carnivoro::getCase(caseId)->count;
            CHECK(std::strstr(clue, carnivoroui::animalName((speaker + count - 1) % count)));
            CHECK(std::strstr(clue, carnivoroui::animalName((speaker + 1) % count)));
            CHECK(!std::strstr(clue, "circle") && !std::strstr(clue, "círculo"));
          }
        }
      }
  std::printf("%d screen checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

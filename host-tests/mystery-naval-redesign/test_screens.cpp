#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/battleship/BattleshipScreens.h"
#include "../../src/apps_local/carnivoro/CarnivoroScreens.h"
#include "../../src/apps_local/intercept/InterceptArt.h"
#include "../../src/apps_local/intercept/InterceptScreens.h"
#include "../../src/apps_local/ui/PageIndicator.h"

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

fui::DeviceContext makeDevice(fui::DisplayTarget& target) {
  auto d = target.deviceContext();
  d.hasTouch = true;
  d.safeArea = {10, 0, 0, 0};
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
    backend.fill(r, p, radius, corners);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t width, uint8_t radius = 0,
              uint8_t corners = fui::CornersAll) override {
    backend.stroke(r, p, width, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override { backend.line(a, b, w, p); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { backend.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef ref, fui::BitmapMode mode,
              fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rotation = fui::Rotation::None) override {
    CHECK(r.width > 0 && r.height > 0);
    backend.bitmap(r, ref, mode, p, rotation);
  }
  struct Label {
    std::string value;
    fui::FontId font;
    fui::Rect box;
  };
  std::vector<Label> labels;
  std::vector<fui::Rect> blackFills;
  void text(fui::Rect box, const char* value, fui::TextStyle style) override {
    CHECK(!value || !std::strchr(value, '\n'));
    fui::layoutText(backend, box, value, style, [&](const char* run, fui::Rect line) {
      if (line.bottom() > box.bottom() || std::strstr(run, "…"))
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
      CHECK(!std::strstr(run, "…"));
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
        frame(target, device, input, interactions),
        screen(frame) {
    target.setFont(3, tiny.font);
    target.setFont(4, library.font);
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
      CHECK(r.width >= 24 && r.height >= 24);
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

}  // namespace

int main(int argc, char** argv) {
  sdart::Pack interceptAssets;
  CHECK(interceptAssets.open(interceptart::PATH, interceptart::COUNT));

  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    auto shot = [&](Canvas& c, const std::string& name) {
      c.checkHits();
      if (argc > 1) c.write(std::string(argv[1]) + "/" + name + "-" + std::to_string(int(lang)) + ".pbm");
    };
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      int w = size.first, h = size.second;
      const auto tag = std::to_string(w);
      intercept::State in;
      for (auto view :
           {interceptui::View::Menu, interceptui::View::Restart, interceptui::View::About, interceptui::View::Story})
        for (int page = 0; page < (view == interceptui::View::About ? 2 : 1); ++page) {
          Canvas c(w, h);
          c.target.setFont(3, c.tiny.font);
          c.target.setFont(4, view == interceptui::View::Story ? c.prose.font : c.library.font);
          interceptui::Work work;
          interceptui::build(c.screen, {&in, view, false, 0, page, &interceptAssets}, work);
          shot(c, "intercept-" + tag + "-" + std::to_string(int(view)) + "-" + std::to_string(page));
          if (view == interceptui::View::About) CHECK(work.pages == 2);
        }
      {
        Canvas a(w, h), b(w, h);
        bshipui::BoardModel m;
        bshipui::Layout before, after;
        bshipui::buildBoardChrome(a.screen, m, &before);
        m.canFire = true;
        m.aim = 44;
        bshipui::buildBoardChrome(b.screen, m, &after);
        CHECK(before.target.x == after.target.x && before.target.y == after.target.y);
        CHECK(before.target.width == after.target.width && before.target.height == after.target.height);
        CHECK(before.target.bottom() <= (w > h ? a.device.safeRect().bottom() : h - 80));
        CHECK(after.target.bottom() <= (w > h ? b.device.safeRect().bottom() : h - 80));
        for (int cell = 0; cell < 100; ++cell) {
          const int y = before.target.y + cell / 10 * (before.target.height / 10);
          CHECK(y + before.target.height / 10 <= (w > h ? a.device.safeRect().bottom() : h - 80));
        }
      }
      for (int node : {31, 32}) {
        Canvas c(w, h);
        c.target.setFont(3, c.tiny.font);
        c.target.setFont(4, c.library.font);
        in.node = node;
        interceptui::Work work;
        interceptui::build(c.screen, {&in, interceptui::View::Story, false, 0, 0, &interceptAssets}, work);
        CHECK(c.has(interceptui::New));
        CHECK(c.has(interceptui::Cancel));
        shot(c, "intercept-end-" + tag + "-" + std::to_string(node));
      }
      if (argc > 2 && !std::strcmp(argv[2], "--intercept-only")) continue;
      bship::Game game;
      uint32_t seed = 42;
      bship::randomFleet(game.side[0].fleet, seed);
      bship::randomFleet(game.side[1].fleet, seed);
      game.side[0].placed = game.side[1].placed = 1;
      {
        Canvas c(w, h);
        auto sunkGame = game;
        for (int n = 0; n < 5; ++n)
          bship::markShot(sunkGame.side[0], bship::shipCell(sunkGame.side[0].fleet.ships[0], n));
        sunkGame.turn = 0;
        sunkGame.lastShot = bship::shipCell(sunkGame.side[0].fleet.ships[0], 4) + 1;
        bshipui::BoardModel m;
        m.game = &sunkGame;
        m.myTurn = true;
        bshipui::buildBoardChrome(c.screen, m);
        bool complete = false;
        for (const auto& label : c.target.labels) {
          char caption[192];
          pixelui::copyCaps(caption, sizeof(caption), label.value.c_str());
          complete |= std::strstr(caption, lang == Language::ES ? "HUNDIDO" : "SUNK") != nullptr;
        }
        CHECK(complete);
      }
      for (int turn : {0, 1}) {
        Canvas c(w, h);
        game.turn = turn;
        game.lastShot = 1;
        bshipui::BoardModel m;
        m.game = &game;
        m.gameOver = true;
        m.myTurn = turn == 0;
        m.report = "FINISHED RESULT";
        m.status = "PLAY AGAIN";
        bshipui::buildBoardChrome(c.screen, m);
        bool result = false;
        for (const auto& label : c.target.labels) {
          result |= label.value == "FINISHED RESULT";
          CHECK(label.value != "YOUR TURN" && label.value != "TU TURNO");
        }
        CHECK(result);
        CHECK(c.has(bshipui::ActionPlayAgain));
      }
      game.lastShot = 0;
      game.turn = 0;
      for (bool saved : {false, true}) {
        Canvas c(w, h);
        bshipui::StartModel m;
        m.fleet = &game.side[0].fleet;
        m.hasSavedGame = saved;
        m.played = 3;
        m.won = 1;
        bshipui::buildStartMenu(c.screen, m);
        shot(c, "naval-menu-" + tag + "-" + std::to_string(saved));
      }
      {
        Canvas c(w, h);
        bshipui::PlaceModel m;
        m.fleet = &game.side[0].fleet;
        m.selectedShip = 0;
        bshipui::Layout l;
        bshipui::buildPlaceChrome(c.screen, m, &l);
        CHECK(c.has(bshipui::ActionRotate));
        for (int n = 0; n < 5; ++n) CHECK(c.has(bshipui::ActionSelectShip, n));
        CHECK(l.place.width == l.place.height);
        shot(c, "naval-place-" + tag);
      }
      for (int aim : {-1, 83}) {
        Canvas c(w, h);
        bshipui::BoardModel m;
        m.game = &game;
        m.aim = aim;
        m.canFire = aim >= 0;
        m.status = aim >= 0 ? tr(STR_UI_FIRE_AT_S_3F134A) : tr(STR_UI_TAP_A_TARGET_0B935C);
        bshipui::Layout l;
        bshipui::buildBoardChrome(c.screen, m, &l);
        CHECK(c.has(bshipui::ActionFire) == m.canFire);
        CHECK(l.target.width == l.target.height);
        shot(c, "naval-board-" + tag + "-" + std::to_string(aim));
      }
      {
        Canvas c(w, h);
        bshipui::buildHowTo(c.screen, 0);
        CHECK(c.has(bshipui::ActionHelpNext));
        shot(c, "naval-help-" + tag);
      }
      // Hidden enemy arrangements must never change a live board or roster.
      {
        Canvas a(w, h), b(w, h);
        bshipui::BoardModel m;
        m.game = &game;
        bshipui::buildBoardChrome(a.screen, m);
        bship::randomFleet(game.side[1].fleet, seed);
        bshipui::buildBoardChrome(b.screen, m);
        CHECK(a.pixels == b.pixels);
      }
      carnivoro::State state;
      state.caseId = 0;
      state.status[0] = carnivoro::Status::Active;
      const auto* one = carnivoro::getCase(0);
      for (auto view : {carnivoroui::View::Menu, carnivoroui::View::Cases, carnivoroui::View::Intro,
                        carnivoroui::View::Statements, carnivoroui::View::Accuse, carnivoroui::View::Help})
        for (int page = 0; page < (view == carnivoroui::View::Help ? (w > h ? 10 : 5) : 1); ++page) {
          Canvas c(w, h);
          c.target.setFont(3, c.tiny.font);
          c.target.setFont(4, c.library.font);
          carnivoroui::Model m;
          m.state = &state;
          m.view = view;
          m.page = page;
          m.solution = carnivoro::solve(*one);
          carnivoroui::Geometry g;
          pagedtext::Work wrap;
          carnivoroui::build(c.screen, m, g, wrap);
          for (auto& label : c.target.labels) CHECK(label.value != "TU ANÁLISIS" && label.value != "Your analysis");
          if (view == carnivoroui::View::Help) {
            CHECK(m.textPages == (w > h ? 10 : 5));
            const auto body = pixelui::geometry(c.device).body;
            for (const auto& label : c.target.labels)
              if (label.font == 4) {
                if (label.box.bottom() > body.bottom() - 56)
                  std::printf("HELP FOOTER %s page%d [%d..%d] %s\n", tag.c_str(), page, label.box.y, label.box.bottom(),
                              label.value.c_str());
                CHECK(label.box.bottom() <= body.bottom() - 56);
              }
          }
          shot(c, "carn-" + tag + "-" + std::to_string(int(view)) + "-" + std::to_string(page));
        }
      for (int outcome = 0; outcome < 3; ++outcome) {
        Canvas c(w, h);
        state.status[0] = outcome == 0   ? carnivoro::Status::Active
                          : outcome == 1 ? carnivoro::Status::Solved
                                         : carnivoro::Status::Exhausted;
        state.misses[0] = outcome == 0 ? 1 : outcome == 1 ? 0 : 2;
        c.target.setFont(3, c.tiny.font);
        c.target.setFont(4, c.library.font);
        carnivoroui::Model m;
        m.state = &state;
        m.view = carnivoroui::View::Verdict;
        m.solution = carnivoro::solve(*one);
        carnivoroui::Geometry g;
        pagedtext::Work wrap;
        carnivoroui::build(c.screen, m, g, wrap);
        CHECK(c.has(carnivoroui::ViewSolution) == (outcome != 0));
        CHECK(!c.has(carnivoroui::Note));
        shot(c, "carn-result-" + tag + "-" + std::to_string(outcome));
      }
      {
        Canvas c(w, h);
        c.target.setFont(3, c.tiny.font);
        c.target.setFont(4, c.library.font);
        carnivoroui::Model m;
        m.state = &state;
        m.view = carnivoroui::View::Solution;
        m.solution = carnivoro::solve(*one);
        carnivoroui::Geometry g;
        pagedtext::Work wrap;
        carnivoroui::build(c.screen, m, g, wrap);
        CHECK(!c.has(carnivoroui::Note));
        CHECK(c.has(carnivoroui::Replay));
        shot(c, "carn-solution-" + tag);
      }
    }
  }
  for (const auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      Canvas c(size.first, size.second);
      sdart::Pack missing;
      interceptui::Work work;
      intercept::State state;
      interceptui::build(c.screen, {&state, interceptui::View::Story, false, 0, 0, &missing}, work);
      c.checkHits();
      CHECK(c.interactions.count() == 1 && c.has(interceptui::Back));
      CHECK(!c.has(interceptui::Choice));
      CHECK(!c.has(interceptui::Play));
    }
  }
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

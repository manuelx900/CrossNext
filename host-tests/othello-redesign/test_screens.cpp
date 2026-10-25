#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/pocketgames/PocketGamesScreens.h"
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
  namespace pg = pocketgames;
  namespace pu = pocketui;
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      const int w = size.first, h = size.second;
      pg::State state;
      pg::start(state, pg::Kind::Othello, 1);
      const pu::View views[] = {pu::View::Menu,       pu::View::Board,  pu::View::Help,
                                pu::View::ConfirmNew, pu::View::Result, pu::View::Review};
      for (auto view : views)
        for (int variant = 0; variant < (view == pu::View::Help || view == pu::View::Result ? 3 : 1); ++variant) {
          pg::start(state, pg::Kind::Othello, 1);
          if (view == pu::View::Result || view == pu::View::Review) {
            state.othello.over = true;
            for (int n = 0; n < 64; ++n)
              state.othello.board[n] = n < (variant == 0 ? 46 : variant == 1 ? 18 : 32) ? 1 : 2;
          }
          Canvas c(w, h);

          pu::Model model;
          model.state = &state;
          model.view = view;
          model.guidePage = variant;
          pu::build(c.screen, model);
          c.checkHits();
          if (view == pu::View::Menu) {
            CHECK(c.has(pu::Play));
            CHECK(c.has(pu::New));
            CHECK(c.has(pu::Help));
          }
          if (view == pu::View::Board) {
            CHECK(c.has(pu::Menu));
            CHECK(c.has(pu::Restart));
            const auto b = pu::boardRect(c.device);
            CHECK(b.x >= c.device.safeRect().x && b.y >= c.device.safeRect().y);
            CHECK(b.right() <= c.device.safeRect().right() && b.bottom() <= c.device.safeRect().bottom());
            for (int n = 0; n < 64; ++n)
              CHECK(pu::squareAt(c.device, 8, b.x + (n % 8) * b.width / 8 + b.width / 16,
                                 b.y + (n / 8) * b.height / 8 + b.height / 16) == n);
            CHECK(pu::squareAt(c.device, 8, b.right(), b.y) == -1);
            CHECK(pu::squareAt(c.device, 8, b.x, b.bottom()) == -1);
          }
          if (view == pu::View::Help) {
            CHECK(c.has(pu::GuidePage, variant - 1) == (variant > 0));
            CHECK(c.has(pu::GuidePage, variant + 1) == (variant < 2));
            CHECK(c.has(pu::Menu) == (variant == 2));
          }
          if (view == pu::View::ConfirmNew) {
            CHECK(c.has(pu::ConfirmNew));
            CHECK(c.has(pu::Cancel));
          }
          if (view == pu::View::Result) {
            CHECK(c.has(pu::New));
            CHECK(c.has(pu::ViewBoard));
            CHECK(c.has(pu::Menu));
          }
          if (view == pu::View::Review) {
            CHECK(c.has(pu::ViewResult));
            CHECK(c.has(pu::New));
            CHECK(!c.has(pu::Restart));
          }
          if (argc > 1) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/%s-%dx%d-%d-%d.pbm", argv[1], lang == Language::ES ? "es" : "en", w,
                          h, int(view), variant);
            c.write(path);
          }
        }
      for (int human : {1, 2}) {
        pg::State preview;
        pg::start(preview, pg::Kind::Othello, 42);
        if (human == 1) CHECK(pg::play(preview.othello, 19));
        const pg::Othello committed = preview.othello;
        pu::Model model;
        model.state = &preview;
        model.view = pu::View::Board;
        model.humanColour = human;
        model.botSquare = pg::chooseBotMove(preview.othello);
        CHECK(model.botSquare >= 0 && !preview.othello.board[model.botSquare]);
        Canvas c(w, h);
        pu::build(c.screen, model);
        c.checkHits();
        CHECK(c.has(pu::Menu) && c.has(pu::Restart));
        CHECK(!std::memcmp(committed.board, preview.othello.board, sizeof(committed.board)));
        CHECK(preview.othello.turn == committed.turn && preview.othello.over == committed.over);
        Canvas normal(w, h);
        model.botSquare = -1;
        pu::build(normal.screen, model);
        CHECK(c.interactions.count() == normal.interactions.count());
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto& placed = c.interactions.data()[n];
          const auto& unplaced = normal.interactions.data()[n];
          CHECK(placed.action == unplaced.action && placed.inputMask == unplaced.inputMask);
          CHECK(placed.navigationColumns == unplaced.navigationColumns &&
                placed.navigationRows == unplaced.navigationRows);
        }
        if (argc > 1) {
          char path[512];
          std::snprintf(path, sizeof(path), "%s/placement-%d-%d-%d.pbm", argv[1], int(lang), w, human);
          c.write(path);
        }
      }
      for (int outcome = 0; outcome < 3; ++outcome) {
        Canvas c(w, h);

        pu::Model m;
        m.state = &state;
        m.view = pu::View::Result;
        m.saveFailed = true;
        state.othello.over = true;
        for (int n = 0; n < 64; ++n) state.othello.board[n] = n < (outcome == 0 ? 46 : outcome == 1 ? 18 : 32) ? 1 : 2;
        pu::build(c.screen, m);
        c.checkHits();
        bool warning = false;
        for (const auto& label : c.target.labels) warning |= label.value == tr(STR_UI_COULD_NOT_SAVE_GAME_615E7A);
        CHECK(warning);
      }
      for (int phase = 0; phase < 3; ++phase) {
        pg::start(state, pg::Kind::Othello, 1);
        Canvas c(w, h);

        pu::Model m;
        m.state = &state;
        m.view = pu::View::Board;
        m.humanColour = phase == 1 ? 2 : 1;
        if (phase == 2) {
          state.othello.over = true;
          for (auto& cell : state.othello.board) cell = 1;
        }
        pu::build(c.screen, m);
        c.checkHits();
        CHECK(c.has(pu::Menu) && c.has(pu::Restart));
        std::string copy;
        copy.reserve(512);
        for (const auto& label : c.target.labels) {
          copy += label.value;
          copy += ' ';
        }
        CHECK((copy.find(tr(STR_OT_THINKING)) != std::string::npos) == (phase == 1));
        CHECK((copy.find(tr(STR_OT_TAP)) != std::string::npos) == (phase == 0));
        CHECK((copy.find(tr(STR_OT_FINAL_BOARD)) != std::string::npos) == (phase == 2));
        if (phase != 2) {
          // D3 is empty and legal for Black in the initial position.
          const auto b = pu::boardRect(c.device);
          const int x = b.x + 3 * b.width / 8 + b.width / 16;
          const int y = b.y + 2 * b.height / 8 + b.height / 16;
          bool ink = false;
          for (const auto& r : c.target.blackFills) ink |= x >= r.x && x < r.right() && y >= r.y && y < r.bottom();
          CHECK(ink == (phase == 0));
        }
      }
      for (auto view : {pu::View::Menu, pu::View::Board}) {
        Canvas c(w, h);

        pu::Model m;
        m.state = &state;
        m.view = view;
        m.saveFailed = true;
        m.invalid = true;
        pu::build(c.screen, m);
        c.checkHits();
      }
    }
  }
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

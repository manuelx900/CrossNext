#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/carnivoro/CarnivoroScreens.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleScreens.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/SettingsHome.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#include "../../src/apps_local/wallpapers/WallpapersScreens.h"
#include "../../src/apps_local/words/WordsScreens.h"
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

  Canvas(int w, int h)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(makeDevice(target)),
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
  fui::Rect textRects[64]{};
  int textCount = 0, errorText = -1;
  TextAudit(fui::DrawTarget& t, int w, int h) : target(t), width(w), height(h) {}
  fui::Size measureText(fui::FontId f, const char* s, fui::TextStyle st) const override {
    return target.measureText(f, s, st);
  }
  int16_t lineHeight(fui::FontId f) const override { return target.lineHeight(f); }
  fui::Rect inkBounds(fui::FontId f, const char* s, fui::TextStyle st) const override {
    return target.inkBounds(f, s, st);
  }
  void fill(fui::Rect r, fui::Paint p, uint8_t a = 0, uint8_t b = fui::CornersAll) override { target.fill(r, p, a, b); }
  void stroke(fui::Rect r, fui::Paint p, uint8_t w, uint8_t a = 0, uint8_t b = fui::CornersAll) override {
    target.stroke(r, p, w, a, b);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override { target.line(a, b, w, p); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { target.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef b, fui::BitmapMode m, fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rot = fui::Rotation::None) override {
    target.bitmap(r, b, m, p, rot);
  }
  void text(fui::Rect r, const char* s, fui::TextStyle st) override {
    if (s && s[0] && textCount < 64) {
      if (std::strcmp(s, tr(STR_CHESS_PUZZLE_SAVE_ERROR)) == 0 || std::strcmp(s, tr(STR_CHESS_PUZZLE_BAD_NUMBER)) == 0)
        errorText = textCount;
      textRects[textCount++] = r;
    }
    bool fit = true;
    fui::layoutText(target, r, s, st, [&](const char* run, fui::Rect box) {
      if (box.bottom() > r.bottom() || box.right() > r.right() || std::strstr(run, "…")) fit = false;
    });
    if (!fit) {
      std::printf("TEXT CUT [%d,%d %dx%d] %s\n", r.x, r.y, r.width, r.height, s);
      CHECK(fit);
    }
    target.text(r, s, st);
  }
};

}  // namespace

int main(int argc, char** argv) {
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      I18N.setLanguage(lang);
      const auto stem = std::string(argc > 1 ? argv[1] : "") + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                        std::to_string(size.first);
      for (int view = 0; view < 4; ++view) {
        Canvas c(size.first, size.second);
        FontBridge prose(ui_prose_14);
        c.target.setFont(1, prose.font);
        c.target.setFont(4, c.tiny.font);
        TextAudit target(c.target, c.width, c.height);
        toybox::Frame frame(target, c.device, c.input, c.interactions);
        toybox::Screen screen(frame, toybox::themeTokens());
        if (view == 0) {
          wallpapersui::SheetModel m;
          m.name = "IMG_5037_cover_480";
          m.detail = "PNG | 37 KB";
          wallpapersui::buildSheet(screen, m);
          CHECK(c.has(wallpapersui::ActionSet));
          CHECK(c.has(wallpapersui::ActionSlideshow));
          CHECK(c.has(wallpapersui::ActionExitPreview));
          CHECK(!c.has(wallpapersui::ActionConfirmDelete));
        } else if (view == 1) {
          wallpapersui::ConfirmModel m;
          m.name = "IMG_5037_cover_480";
          m.consequence = tr(STR_WALLPAPER_DELETE_ASK);
          wallpapersui::buildConfirm(screen, m);
          CHECK(c.has(wallpapersui::ActionKeep));
          CHECK(c.has(wallpapersui::ActionConfirmDelete));
        } else if (view == 2) {
          wallpapersui::EmptyModel m;
          m.title = tr(STR_WALLPAPERS_TITLE);
          wallpapersui::buildEmpty(screen, m);
        } else {
          wallpapersui::NoticeModel m;
          m.headline = tr(STR_UI_CANNOT_PREVIEW_215D84);
          m.body = tr(STR_UI_THIS_FILE_COULD_NOT_BE_OPENED_495DB9);
          wallpapersui::buildNotice(screen, m);
        }
        c.checkHits();
        for (size_t i = 0; i < c.interactions.count(); ++i)
          for (size_t j = 0; j < i; ++j) {
            auto a = c.interactions.data()[i].rect, b = c.interactions.data()[j].rect;
            CHECK(a.right() <= b.x || b.right() <= a.x || a.bottom() <= b.y || b.bottom() <= a.y);
          }
        const auto kill = wallpapersui::confirmDeleteRect(c.device);
        for (int row = 0; row < 4; ++row) {
          auto a = wallpapersui::contextRowRect(c.device, row);
          CHECK(a.height >= 44);
          CHECK(kill.bottom() <= a.y || a.bottom() <= kill.y || kill.right() <= a.x || a.right() <= kill.x);
        }
        if (argc > 1) c.write(stem + "-wallpaper-" + std::to_string(view) + ".pbm");
      }
      for (int resume = 0; resume < 2; ++resume)
        for (int group = 0; group < 3; ++group)
          for (int error = 0; error < 3; ++error) {
            Canvas c(size.first, size.second);
            c.target.setFont(4, c.tiny.font);
            TextAudit target(c.target, c.width, c.height);
            toybox::Frame frame(target, c.device, c.input, c.interactions);
            toybox::Screen screen(frame, toybox::themeTokens());
            chesspuzzle::Progress p;
            chesspuzzle::Session session;
            for (int i = 0; i < 80; ++i) chesspuzzle::markSolved(p, i);
            if (resume) p.lastIndex = 789;
            chesspuzzleui::Work work;
            chesspuzzleui::build(
                screen, {&session, &p, chesspuzzleui::View::Difficulty, group, 0, error == 1, error == 2}, work);
            c.checkHits();
            for (int n = 0; n < 3; ++n) CHECK(c.has(chesspuzzleui::Difficulty, n));
            CHECK(c.has(chesspuzzleui::Choose));
            CHECK(c.has(chesspuzzleui::Reset));
            CHECK(c.has(chesspuzzleui::Resume) == bool(resume));
            CHECK((target.errorText >= 0) == bool(error));
            if (target.errorText >= 0) {
              const auto a = target.textRects[target.errorText];
              for (int i = 0; i < target.textCount; ++i) {
                if (i == target.errorText) continue;
                const auto b = target.textRects[i];
                CHECK(a.right() <= b.x || b.right() <= a.x || a.bottom() <= b.y || b.bottom() <= a.y);
              }
              for (size_t i = 0; i < c.interactions.count(); ++i) {
                const auto b = c.interactions.data()[i].rect;
                CHECK(a.right() <= b.x || b.right() <= a.x || a.bottom() <= b.y || b.bottom() <= a.y);
              }
            }

            for (size_t i = 0; i < c.interactions.count(); ++i)
              for (size_t j = 0; j < i; ++j) {
                auto a = c.interactions.data()[i].rect, b = c.interactions.data()[j].rect;
                CHECK(a.right() <= b.x || b.right() <= a.x || a.bottom() <= b.y || b.bottom() <= a.y);
              }
            if (argc > 1 && !group)
              c.write(stem + "-difficulty-" + std::to_string(resume) + "-error-" + std::to_string(error) + ".pbm");
          }
      for (int index : {0, 399, 400, 749, 750, 999}) {
        Canvas c(size.first, size.second);
        c.target.setFont(4, c.tiny.font);
        TextAudit target(c.target, c.width, c.height);
        toybox::Frame frame(target, c.device, c.input, c.interactions);
        toybox::Screen screen(frame, toybox::themeTokens());
        chesspuzzle::Progress p;
        chesspuzzle::Session session;
        CHECK(chesspuzzle::load(session, index));
        const int group = index < 400 ? 0 : index < 750 ? 1 : 2;
        chesspuzzleui::Work work;
        chesspuzzleui::build(screen, {&session, &p, chesspuzzleui::View::Board, group, index, false, false}, work);
        c.checkHits();
        CHECK(c.has(chesspuzzleui::Restart));
        CHECK(c.has(chesspuzzleui::Choose));
        if (argc > 1 && !index) c.write(stem + "-puzzle.pbm");
      }
    }
  std::printf("Polish UI: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

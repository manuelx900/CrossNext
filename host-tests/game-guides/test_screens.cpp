#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../src/apps_local/ui/fonts/reading_serif_14.h"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#pragma GCC diagnostic pop

#include "../../src/apps_local/carnivoro/CarnivoroScreens.h"
#include "../../src/apps_local/go/GoScreens.h"
#include "../../src/apps_local/hearts/HeartsScreens.h"
#include "../../src/apps_local/solitaire/SolitaireScreens.h"
#include "../../src/apps_local/sudoku/SudokuScreens.h"
#include "../../src/apps_local/ui/PageIndicator.h"
#include "../../src/apps_local/wordle/WordleScreens.h"

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
  d.safeArea = {9, 7, 3, 7};
  return d;
}
struct RecordingTarget : fui::DrawTarget {
  fui::DisplayTarget backend;
  RecordingTarget(uint8_t* data, int w, int h, int stride, fui::Orientation orientation)
      : backend(data, w, h, stride, orientation) {
    bitmaps.reserve(64);
  }
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
  struct Segment {
    fui::Point a, b;
  };
  std::vector<Segment> segments;
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override {
    segments.push_back({a, b});
    backend.line(a, b, w, p);
  }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { backend.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef ref, fui::BitmapMode mode,
              fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rotation = fui::Rotation::None) override {
    CHECK(r.width > 0 && r.height > 0);
    CHECK(r.x >= 0 && r.y >= 0 && r.right() <= clipRect().right() && r.bottom() <= clipRect().bottom());
    bitmaps.push_back(ref.data);
    backend.bitmap(r, ref, mode, p, rotation);
  }
  struct Label {
    std::string value;
    fui::FontId font;
    fui::Rect box;
  };
  std::vector<const uint8_t*> bitmaps;
  std::vector<Label> labels;
  std::vector<fui::Rect> blackFills;
  void text(fui::Rect box, const char* value, fui::TextStyle style) override {
    fui::layoutText(backend, box, value, style, [&](const char* run, fui::Rect line) {
      if (line.bottom() > box.bottom() || std::strstr(run, "\xE2\x80\xA6") || std::strstr(run, "..."))
        if (std::strcmp(value, "...") != 0)
          std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
      // The original column diagram abbreviates its middle cells with literal dots.
      if (std::strcmp(value, "...") != 0) CHECK(!std::strstr(run, "\xE2\x80\xA6") && !std::strstr(run, "..."));
    });
    labels.push_back({value ? value : "", style.font, box});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge regular{ui_prose_10}, tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30},
      library{ubuntu_10_regular}, prose{ui_prose_14}, reading{reading_serif_14};
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
    target.setFont(4, regular.font);
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
      for (size_t j = 0; j < n; ++j) {
        const auto& a = interactions.data()[j].rect;
        CHECK(r.x >= a.right() || r.right() <= a.x || r.y >= a.bottom() || r.bottom() <= a.y);
      }
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
    for (const auto byte : pixels) std::fputc(byte ^ 255, file);
    std::fclose(file);
  }
};

}  // namespace

int main(int argc, char** argv) {
  const std::string folder = argc > 1 ? argv[1] : ".";
  for (int lang = 0; lang < 2; ++lang) {
    I18N.setLanguage(lang ? Language::ES : Language::EN);
    for (const auto size : {std::pair<int, int>{480, 800}, std::pair<int, int>{800, 480}}) {
      const int w = size.first, h = size.second;
      const std::string prefix = folder + "/" + (lang ? "es" : "en") + "-" + std::to_string(w) + "-";
      const auto shot = [&](Canvas& c, const std::string& name) {
        c.checkHits();
        c.write(prefix + name + ".pbm");
      };
      for (int p = 0; p < 3; ++p) {
        Canvas c(w, h);
        goui::buildHelp(c.screen, p);
        if (p == 0) {
          char caption[96], placement[192];
          pixelui::copyCaps(caption, sizeof(caption), tr(STR_GO_FOUR_LIBERTIES));
          pixelui::copyCaps(placement, sizeof(placement), tr(STR_GO_PLACE_HELP));
          const RecordingTarget::Label* intro = nullptr;
          const RecordingTarget::Label* label = nullptr;
          for (const auto& item : c.target.labels) {
            if (item.value.size() > 3 &&
                (std::string(tr(STR_GO_PLACE_HELP)).find(item.value) != std::string::npos ||
                 std::string(placement).find(item.value) != std::string::npos) &&
                (!intro || item.box.bottom() > intro->box.bottom()))
              intro = &item;
            if (item.value == caption) label = &item;
          }
          CHECK(intro && label);
          if (intro && label) CHECK(intro->box.bottom() <= label->box.y);
        }
        shot(c, "go" + std::to_string(p));
      }
      for (int p = 0; p < solitaireui::kHelpPages; ++p) {
        Canvas c(w, h);
        solitaireui::buildHelp(c.screen, p);
        shot(c, "sol" + std::to_string(p));
      }
      for (int p = 0; p < heartsui::howToPages(); ++p) {
        Canvas c(w, h);
        heartsui::buildHowTo(c.screen, {p});
        shot(c, "hearts" + std::to_string(p));
      }
      {
        Canvas c(w, h);
        wordleui::buildHowTo(c.screen);
        shot(c, "wordle");
      }
      const int pages = w > h ? 8 : 4;
      for (int p = 0; p < pages; ++p) {
        Canvas c(w, h);
        static carnivoro::State state;
        state.caseId = 0;
        carnivoroui::Model m;
        m.state = &state;
        m.view = carnivoroui::View::Help;
        m.page = p;
        carnivoroui::Geometry g;
        static pagedtext::Work work;
        carnivoroui::build(c.screen, m, g, work);
        CHECK(m.textPages == pages);
        shot(c, "car" + std::to_string(p));
      }
      for (int p = 0; p < sudokuui::howToPages(); ++p) {
        Canvas c(w, h);
        sudokuui::buildHowTo(c.screen, {p});
        shot(c, "sdk-help" + std::to_string(p));
      }
      for (int level = 0; level < sudoku::kLevelCount; ++level)
        for (const bool showMatches : {false, true})
          for (const bool failed : {false, true}) {
            Canvas c(w, h);
            sudokuui::SettingsModel m;
            m.level = static_cast<sudoku::Level>(level);
            m.showMatches = showMatches;
            m.saveFailed = failed;
            sudokuui::buildSettings(c.screen, m);
            CHECK(c.has(sudokuui::ActionToggleMatches));
            CHECK(c.has(sudokuui::ActionApplySettings));
            CHECK(c.has(sudokuui::ActionMenu));
            for (int n = 0; n < sudoku::kLevelCount; ++n) CHECK(c.has(sudokuui::ActionPickLevel, n));
            if (level == 0 && !failed)
              shot(c, "sdk-settings" + std::to_string(showMatches));
            else
              c.checkHits();
          }
      CHECK(sudoku::back(sudoku::Screen::Settings) == sudoku::Screen::Menu);
      for (const bool showMatches : {false, true})
        for (auto tool : {sudoku::EntryTool::Digit, sudoku::EntryTool::Erase, sudoku::EntryTool::Check}) {
          Canvas c(w, h);
          sudokuui::BoardModel m;
          m.entryTool = tool;
          m.showMatches = showMatches;
          m.game.puzzle.given[0] = 1;
          for (int cell = 0; cell < 81; ++cell)
            m.game.puzzle.solution[cell] = (cell / 9 * 3 + cell / 27 + cell % 9) % 9 + 1;
          m.game.entry[1] = 2;
          m.game.entry[2] = 9;
          m.game.entry[3] = 1;
          m.selectedCell = 3;
          m.game.armed = 1;
          CHECK(sudoku::checkEntry(m.game, 0) == sudoku::CellCheck::None);
          CHECK(sudoku::checkEntry(m.game, 1) == sudoku::CellCheck::Correct);
          CHECK(sudoku::checkEntry(m.game, 2) == sudoku::CellCheck::Wrong);
          CHECK(!sudoku::isClashing(m.game, 2));
          CHECK(sudoku::isClashing(m.game, 3));
          const auto before = m.game;
          sudokuui::buildBoard(c.screen, m);
          CHECK(std::memcmp(&before, &m.game, sizeof(before)) == 0);
          for (int n = 0; n < sudoku::kCells; ++n) {
            const auto box = sudokuui::cellRect(c.device, n);
            bool filled = false, marked = false, bracketed = false;
            for (const auto& r : c.target.blackFills) {
              filled |= r.x == box.x && r.y == box.y && r.width == box.width && r.height == box.height;
              bracketed |= r.x > box.x && r.y > box.y && r.right() < box.right() && r.bottom() < box.bottom() &&
                           ((r.width == 3 && r.height > 3) || (r.height == 3 && r.width > 3));
            }
            for (const auto& segment : c.target.segments) {
              const auto inside = [&](fui::Point p) {
                return p.x > box.x && p.x < box.right() && p.y > box.y && p.y < box.bottom();
              };
              marked |=
                  inside(segment.a) && inside(segment.b) && segment.a.x != segment.b.x && segment.a.y != segment.b.y;
            }
            const bool entry = n >= 1 && n <= 3;
            CHECK(filled ==
                  ((tool == sudoku::EntryTool::Erase && entry) || (tool == sudoku::EntryTool::Digit && n == 3)));
            CHECK(marked == (tool == sudoku::EntryTool::Check && entry));
            CHECK(bracketed == (tool == sudoku::EntryTool::Digit && showMatches && n == 0));
          }
          for (int n = 1; n <= 9; ++n) {
            const auto box = sudokuui::padKeyRect(c.device, n);
            bool filled = false;
            for (const auto& r : c.target.blackFills)
              filled |= r.x == box.x && r.y == box.y && r.width == box.width && r.height == box.height;
            CHECK(filled == (tool == sudoku::EntryTool::Digit && n == m.game.armed));
          }
          for (const auto action : {sudokuui::ActionErase, sudokuui::ActionHint}) {
            CHECK(c.has(action));
            for (size_t i = 0; i < c.interactions.count(); ++i) {
              const auto& hit = c.interactions.data()[i];
              if (hit.action != action) continue;
              bool filled = false;
              for (const auto& r : c.target.blackFills)
                filled |=
                    r.x == hit.rect.x && r.y == hit.rect.y && r.width == hit.rect.width && r.height == hit.rect.height;
              CHECK(filled ==
                    (tool == (action == sudokuui::ActionErase ? sudoku::EntryTool::Erase : sudoku::EntryTool::Check)));
            }
          }
          shot(c, "sudoku" + std::to_string(int(tool)) + (showMatches ? "" : "-no-matches"));
          auto game = m.game;
          for (int cell = -1; cell <= 81; ++cell) CHECK(!sudoku::applyEntryTool(game, cell, sudoku::EntryTool::Check));
          CHECK(std::memcmp(&before, &game, sizeof(before)) == 0);
          CHECK(!sudoku::applyEntryTool(game, 0, sudoku::EntryTool::Erase));
          CHECK(sudoku::applyEntryTool(game, 1, sudoku::EntryTool::Erase));
          CHECK(!game.entry[1]);
          CHECK(game.puzzle.given[0] == 1);
          CHECK(sudoku::undoOnce(game));
          CHECK(game.entry[1] == 2);
        }
    }
  }
  std::printf("Game guides: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

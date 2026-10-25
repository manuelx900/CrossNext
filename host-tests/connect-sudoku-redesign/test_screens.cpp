#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/connectfour/ConnectFourScreens.h"
#include "../../src/apps_local/sudoku/SudokuScreens.h"
#include "../../src/apps_local/ui/MatchSettings.h"
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
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      const int w = size.first, h = size.second;
      const auto save = [&](Canvas& c, const char* name) {
        c.checkHits();
        if (argc > 1 && lang == Language::ES)
          c.write(std::string(argv[1]) + "/" + name + "-" + std::to_string(w) + ".pbm");
      };
      {
        Canvas c(w, h);
        c4ui::MenuModel m;
        m.wins = 12;
        m.losses = 7;
        m.draws = 2;
        m.streak = 3;
        c4ui::buildMenu(c.screen, m);
        CHECK(c.has(c4ui::ActionMenuRow, int(c4ui::MenuRow::Play)));
        save(c, "four-menu");
      }
      for (int page = 0; page < 3; ++page) {
        Canvas c(w, h);
        c4ui::buildHowTo(c.screen, {page});
        save(c, ("four-help" + std::to_string(page + 1)).c_str());
        CHECK(page == 0 || c.has(c4ui::ActionHowToPrevious));
      }
      connectfour::Game game{};
      connectfour::start(game);
      for (int n : {2, 3, 2, 3, 2, 3, 5, 3}) connectfour::drop(game, n);
      {
        Canvas c(w, h);
        c4ui::BoardModel m;
        m.game = game;
        m.open = 0x7f;
        c4ui::buildBoard(c.screen, m);
        save(c, "four-board");
        for (int col = 0; col < 7; ++col) {
          auto slot = c4ui::slotRect(c.device, col);
          CHECK(c4ui::columnAt(c.device, slot.x + slot.width / 2, slot.y + slot.height / 2) == col);
          CHECK(slot.width >= 24);
          for (int row = 0; row < 6; ++row) {
            auto r = c4ui::cellRect(c.device, col, row);
            CHECK(r.x >= 0 && r.y >= 0 && r.right() <= w && r.bottom() <= h);
            CHECK(c4ui::columnAt(c.device, r.x + r.width / 2, r.y + r.height / 2) == col);
          }
        }
      }
      {
        Canvas c(w, h);
        c4ui::BoardModel m;
        m.game = game;
        m.review = true;
        c4ui::buildBoard(c.screen, m);
        save(c, "four-review");
        CHECK(c.has(c4ui::ActionReturnResult));
      }
      for (auto outcome :
           {connectfour::Outcome::LightWins, connectfour::Outcome::DarkWins, connectfour::Outcome::Draw}) {
        Canvas c(w, h);
        c4ui::ResultModel m;
        m.game = game;
        m.outcome = outcome;
        m.wins = 12;
        m.streak = 3;
        c4ui::buildResult(c.screen, m);
        save(c, outcome == connectfour::Outcome::LightWins  ? "four-win"
                : outcome == connectfour::Outcome::DarkWins ? "four-loss"
                                                            : "four-draw");
        CHECK(c.has(c4ui::ActionViewBoard) && c.has(c4ui::ActionAgain) && c.has(c4ui::ActionDone));
      }
      for (bool othello : {false, true}) {
        Canvas settings(w, h);
        matchsettings::build(settings.screen,
                             {othello ? matchsettings::Game::Othello : matchsettings::Game::ConnectFour, false, othello,
                              othello ? 1 : 2});
        save(settings, othello ? "othello-settings" : "four-settings");
        CHECK(settings.has(matchsettings::Done));
        CHECK(settings.has(matchsettings::Colour, 0) && settings.has(matchsettings::Colour, 1));
        CHECK(settings.has(matchsettings::Opponent, 0) == !othello);
        CHECK(settings.has(matchsettings::Difficulty, 0) && settings.has(matchsettings::Difficulty, 1) &&
              settings.has(matchsettings::Difficulty, 2));
      }
      {
        Canvas confirm(w, h);
        sudokuui::buildConfirmNew(confirm.screen);
        save(confirm, "sudoku-replace");
        CHECK(confirm.has(sudokuui::ActionAgain) && confirm.has(sudokuui::ActionMenu));
      }
      sudoku::Game puzzle{};
      static constexpr char clues[] =
          "530070000600195000098000060800060003400803001700020006060000280000419005000080079";
      for (int n = 0; n < 81; ++n) puzzle.puzzle.given[n] = clues[n] - '0';
      puzzle.armed = 4;
      puzzle.entry[2] = 4;
      puzzle.elapsedMs = 522000;
      for (bool saved : {false, true}) {
        Canvas c(w, h);
        sudokuui::MenuModel m;
        m.game = puzzle;
        m.hasGame = saved;
        sudokuui::buildMenu(c.screen, m);
        save(c, saved ? "sudoku-saved" : "sudoku-menu");
        CHECK(c.has(sudokuui::ActionPlay) == saved);
        CHECK(c.has(sudokuui::ActionNew));
      }
      for (int level = 0; level < sudoku::kLevelCount; ++level) {
        Canvas c(w, h);
        sudokuui::SettingsModel m;
        m.level = static_cast<sudoku::Level>(level);
        sudokuui::buildSettings(c.screen, m);
        save(c, "sudoku-level");
        CHECK(c.has(sudokuui::ActionPickLevel, level));
        CHECK(c.has(sudokuui::ActionApplySettings));
      }
      for (int page = 0; page < sudokuui::howToPages(); ++page) {
        Canvas c(w, h);
        sudokuui::buildHowTo(c.screen, {page});
        save(c, ("sudoku-help" + std::to_string(page + 1)).c_str());
      }
      for (int state = 0; state < 5; ++state) {
        Canvas c(w, h);
        sudokuui::BoardModel m;
        m.game = puzzle;
        m.selectedCell = 2;
        m.notesMode = state == 1;
        m.generating = state == 2 || state == 4;
        if (state == 4) m.game.solvedFlag = 1;
        if (state == 3) {
          m.game.entry[3] = 4;
          m.game.note[10] = sudoku::bitFor(2);
        }
        sudokuui::buildBoard(c.screen, m);
        save(c, state == 0   ? "sudoku-board"
                : state == 1 ? "sudoku-notes"
                : state == 2 ? "sudoku-generating"
                             : "sudoku-conflict");
        CHECK(!c.has(sudokuui::ActionNotes) && c.has(sudokuui::ActionErase));
        if (m.generating) {
          CHECK(c.has(sudokuui::ActionMenu));
          CHECK(!c.has(sudokuui::ActionSeeResult));
        }
        for (int n = 0; n < 81; ++n) {
          auto r = sudokuui::cellRect(c.device, n);
          int at = -1;
          CHECK(sudokuui::cellAt(c.device, r.x + r.width / 2, r.y + r.height / 2, at) && at == n);
          CHECK(r.x >= 0 && r.y >= 0 && r.right() <= w && r.bottom() <= h);
        }
        for (int n = 1; n <= 9; ++n) {
          auto r = sudokuui::padKeyRect(c.device, n);
          int value = 0;
          CHECK(sudokuui::padKeyAt(c.device, r.x + r.width / 2, r.y + r.height / 2, value) && value == n);
          CHECK(r.x >= 0 && r.y >= 0 && r.right() <= w && r.bottom() <= h);
        }
      }
      {
        Canvas c(w, h);
        sudokuui::ResultModel m;
        m.game = puzzle;
        m.elapsedMs = 522000;
        m.solvedTotal = 12;
        sudokuui::buildResult(c.screen, m);
        save(c, "sudoku-result");
        CHECK(c.has(sudokuui::ActionDone) && c.has(sudokuui::ActionMenu) && c.has(sudokuui::ActionAgain));
      }
      {
        auto m = puzzle;
        CHECK(!sudoku::eraseCell(m, 0));
        CHECK(sudoku::eraseCell(m, 2));
        CHECK(m.entry[2] == 0);
        CHECK(sudoku::undoOnce(m) && m.entry[2] == 4);
        m.note[10] = sudoku::bitFor(2);
        CHECK(sudoku::eraseCell(m, 10) && m.note[10] == 0);
        m.solvedFlag = 1;
        CHECK(!sudoku::eraseCell(m, 2));
        CHECK(!sudoku::eraseCell(m, -1) && !sudoku::eraseCell(m, 81));
      }
      CHECK(connectfour::back(connectfour::Screen::Review) == connectfour::Screen::Result);
      CHECK(sudoku::back(sudoku::Screen::Settings) == sudoku::Screen::Menu);
    }
  }
  std::printf("Connect Four/Sudoku: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

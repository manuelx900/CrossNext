#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/checkers/DraughtsScreens.h"
#include "../../src/apps_local/chess/ChessScreens.h"
#include "../../src/apps_local/chess/TraditionalPieces.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleScreens.h"
#include "../../src/apps_local/game2048/Game2048Screens.h"
#include "../../src/apps_local/minesweeper/MinesweeperScreens.h"
#include "../../src/apps_local/picross/PicrossScreens.h"
#include "../../src/apps_local/pocketgames/PocketGamesScreens.h"
#include "../../src/apps_local/pomodoro/PomodoroScreens.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/reading/ReadingStatsScreens.h"
#include "../../src/apps_local/ui/PageIndicator.h"
#include "../../src/apps_local/ui/QuickSettingsScreen.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/wordle/WordleScreens.h"

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
    fui::layoutText(backend, box, value, style, [&](const char* run, fui::Rect line) {
      if (std::strstr(run, "…") || std::strstr(run, "..."))
        std::printf("CUT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(!std::strstr(run, "…") && !std::strstr(run, "..."));
    });
    labels.push_back({value ? value : "", style.font, box});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, prose{ui_prose_10};
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
    target.setFont(4, prose.font);
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
      const bool dateCell = slot.action == readingui::SelectDay;
      const bool outsideSheet = slot.action == 1 && r.width == width && r.height < 40;
      CHECK(r.width >= (dateCell ? 24 : 44) && r.height >= (dateCell || outsideSheet ? 24 : 40));
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
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
    for (auto lang : {Language::EN, Language::ES}) {
      I18N.setLanguage(lang);
      const int w = size.first, h = size.second;
      auto shot = [&](Canvas& c, const std::string& name) {
        c.checkHits();
        if (argc > 1)
          c.write(std::string(argv[1]) + "/" + name + "-" + std::to_string(w) + "-" + std::to_string(int(lang)) +
                  ".pbm");
      };
      for (bool settings : {false, true}) {
        Canvas c(w, h);
        mineui::MenuModel m;
        m.settings = settings;
        mineui::buildMenu(c.screen, m);
        shot(c, settings ? "mines-settings" : "mines-menu");
        if (settings) CHECK(c.has(mineui::ActionSaveSize));
      }
      for (int page = 0; page < 5; ++page) {
        Canvas c(w, h);
        mineui::HowToModel m;
        m.page = page;
        mineui::buildHowTo(c.screen, m);
        shot(c, "mines-help-" + std::to_string(page));
        CHECK(c.has(mineui::ActionHowToPage, -1) == (page > 0));
        CHECK(c.has(mineui::ActionHowToPage, 1) == (page < 4));
      }
      for (int n : {10, 15, 20})
        for (bool over : {false, true}) {
          Canvas c(w, h);
          mineui::BoardModel m;
          minesweeper::start(m.game, 1, n, n);
          minesweeper::dig(m.game, n / 2, n / 2);
          if (over) m.game.status = minesweeper::Status::Lost;
          m.showMines = over;
          mineui::buildBoard(c.screen, m);
          shot(c, "mines-board-" + std::to_string(n) + "-" + std::to_string(over));
          for (int row = 0; row < n; ++row)
            for (int col = 0; col < n; ++col) {
              auto r = mineui::cellRect(c.device, col, row, n, n);
              int cc = -1, rr = -1;
              CHECK(mineui::cellAt(c.device, r.x + r.width / 2, r.y + r.height / 2, cc, rr, n, n));
              CHECK(cc == col && rr == row);
              CHECK(r.x >= 0 && r.y >= 0 && r.right() <= w && r.bottom() <= h);
            }
        }
      picross::Progress progress;
      picrossui::MenuModel m;
      m.progress = &progress;
      m.total = picross::kPuzzleCount;
      {
        Canvas c(w, h);
        picrossui::PickerLayout layout;
        picrossui::buildMenu(c.screen, m, layout);
        shot(c, "picross-menu");
      }
      {
        Canvas c(w, h);
        picrossui::PickerLayout layout;
        picrossui::buildPicker(c.screen, m, layout);
        shot(c, "picross-picker");
        CHECK(!c.has(picrossui::ActionButton, picrossui::ButtonPlay));
        CHECK(layout.count == 16);
        CHECK(layout.indexAt(layout.grid.x + layout.cell, layout.grid.y + 4) == -1);
      }
      for (int tab = 0; tab < picross::kSizeGroupCount; ++tab) {
        Canvas c(w, h);
        m.sizeTab = tab;
        picrossui::buildSizes(c.screen, m);
        shot(c, "picross-sizes-" + std::to_string(tab));
        CHECK(c.has(picrossui::ActionApplySize));
      }
      for (int page = 0; page < 4; ++page) {
        Canvas c(w, h);
        picrossui::buildHelp(c.screen, page);
        shot(c, "picross-help-" + std::to_string(page));
        CHECK(c.has(picrossui::ActionHelpPage, -1) == (page > 0));
      }
      for (int index : {0, picrossui::groupFirst(1), picrossui::groupFirst(2), picrossui::groupFirst(3)}) {
        Canvas c(w, h);
        picross::Board b;
        b.load(index);
        picrossui::BoardModel bm;
        bm.board = &b;
        picrossui::Layout l;
        picrossui::buildBoard(c.screen, bm, l);
        shot(c, "picross-board-" + std::to_string(b.size()));
        CHECK(l.board.right() <= w && l.board.bottom() <= h);
        int rr, cc;
        CHECK(l.cellAt(l.board.x + l.cell / 2, l.board.y + l.cell / 2, rr, cc));
        CHECK(rr == 0 && cc == 0);
      }
      {
        Canvas c(w, h);
        readertext::Model rm;
        readertext::Work rw;
        rm.contentsAvailable = true;
        rm.tab = 2;
        rm.listCount = 5;
        rm.listLabel = [](const void*, int n, char* out, size_t size) {
          std::snprintf(out, size, "%s",
                        n == 0   ? "Short"
                        : n == 1 ? "Alice's Adventures in Wonderland"
                                 : "An extremely long chapter heading that needs ellipsis at the same font size for "
                                   "every chapter in this book");
        };
        readertext::build(c.screen, rm, rw);
        shot(c, "reader-contents");
        int seen = 0;
        for (const auto& label : c.target.labels)
          if (label.value == "SHORT" || label.value.find("ALICE") == 0 || label.value.find("AN EXTREMELY") == 0) {
            CHECK(label.font == 3);
            ++seen;
            if (label.value.find("AN EXTREMELY") == 0) CHECK(label.value.find("...") == std::string::npos);
          }
        CHECK(seen == (w > h ? 2 : 3));
      }
      for (int group = 0; group < picross::kSizeGroupCount; ++group) {
        Canvas c(w, h);
        picrossui::MenuModel menu;
        picrossui::PickerLayout layout;
        menu.progress = &progress;
        menu.total = picross::kPuzzleCount;
        menu.selectedIndex = picrossui::groupFirst(group);
        menu.inProgressIndex = menu.selectedIndex;
        picrossui::buildPicker(c.screen, menu, layout);
        c.checkHits();
        char status[128], caps[128];
        std::snprintf(status, sizeof(status), tr(STR_PICROSS_PICK_STATUS), 1, tr(STR_PICROSS_IN_PROGRESS));
        pixelui::copyCaps(caps, sizeof(caps), status);
        bool found = false;
        for (const auto& label : c.target.labels)
          if (label.value == caps) found = true;
        CHECK(found);
      }
      for (int selected : {0, 1}) {
        Canvas c(w, h);
        game2048::State state;
        game2048::start(state, 1);
        game2048ui::Model model;
        model.state = &state;
        model.view = game2048ui::View::Confirm;
        model.selected = selected;
        game2048ui::Work work;
        game2048ui::build(c.screen, model, work);
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto& hit = c.interactions.data()[n];
          if (hit.action != game2048ui::ConfirmNew && hit.action != game2048ui::CancelNew) continue;
          bool filled = false;
          for (const auto r : c.target.blackFills)
            if (r.x == hit.rect.x && r.y == hit.rect.y && r.width == hit.rect.width && r.height == hit.rect.height)
              filled = true;
          CHECK(filled == (hit.action == (selected ? game2048ui::CancelNew : game2048ui::ConfirmNew)));
        }
      }
      {
        char caps[64] = u8"capítulo y ñandú";
        pixelui::copyCaps(caps, sizeof(caps), caps);
        CHECK(std::string(caps) == u8"CAPÍTULO Y ÑANDÚ");
      }
      wordleui::Work work;
      {
        Canvas c(w, h);
        wordleui::MenuModel menu;
        wordleui::buildMenu(c.screen, menu, work);
        shot(c, "wordle-menu");
      }
      {
        Canvas c(w, h);
        wordleui::buildHowTo(c.screen);
        shot(c, "wordle-help");
        CHECK(c.has(wordleui::ActionArchive));
      }
      for (int n : {4, 5, 6}) {
        {
          Canvas c(w, h);
          wordleui::buildLengths(c.screen, n, work);
          shot(c, "wordle-length-" + std::to_string(n));
          CHECK(c.has(wordleui::ActionApplyLength));
        }
        {
          Canvas c(w, h);
          wordle::Game g;
          g.start(1, n == 4 ? "GAME" : n == 5 ? "GAMES" : "GAMERS", n);
          wordleui::GameModel gm;
          gm.game = &g;
          gm.date = "5 LETTERS";
          auto k = wordleui::buildGame(c.screen, gm, work);
          shot(c, "wordle-board-" + std::to_string(n));
          CHECK(wordleui::keyAt(k, k.rowLeft[0] + k.keyW / 2, k.top + k.keyH / 2) == 'Q');
        }
      }
      game2048::State state;
      game2048::start(state, 1);
      for (auto view :
           {game2048ui::View::Menu, game2048ui::View::Board, game2048ui::View::Help, game2048ui::View::Confirm}) {
        Canvas c(w, h);
        game2048ui::Model gm;
        gm.state = &state;
        gm.view = view;
        game2048ui::Work gw;
        game2048ui::build(c.screen, gm, gw);
        shot(c, "2048-" + std::to_string(int(view)));
        if (view == game2048ui::View::Help) CHECK(c.has(game2048ui::CancelNew));
      }
    }
  std::printf("Game panels: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

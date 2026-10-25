#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/checkers/DraughtsScreens.h"
#include "../../src/apps_local/chess/ChessScreens.h"
#include "../../src/apps_local/chess/TraditionalPieces.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleScreens.h"
#include "../../src/apps_local/pocketgames/PocketGamesScreens.h"
#include "../../src/apps_local/pomodoro/PomodoroScreens.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/reading/ReadingStatsScreens.h"
#include "../../src/apps_local/ui/PageIndicator.h"
#include "../../src/apps_local/ui/QuickSettingsScreen.h"

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
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    const int w = size.first, h = size.second;
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      {
        Canvas c(w, h);
        quickui::Work work;
        quickui::Model m;
        quickui::build(c.screen, m, work);
        c.checkHits();
        CHECK(c.has(quickui::Tile, int(quickui::Shortcut::Light)));
        CHECK(!c.has(quickui::Tile, int(quickui::Shortcut::Wifi)));
        CHECK(c.has(quickui::Tile, -1));
        if (argc > 1)
          c.write(std::string(argv[1]) + "/quick-" + std::to_string(w) + "-" + std::to_string(int(language)) + ".pbm");
      }
      {
        Canvas c(w, h);
        readertext::Model m;
        readertext::Work work;
        m.contentsAvailable = true;
        m.tab = 2;
        m.listCount = 12;
        m.listLabel = [](const void*, int n, char* text, size_t size) {
          std::snprintf(text, size, "Chapter %d", n + 1);
        };
        readertext::build(c.screen, m, work);
        c.checkHits();
        CHECK(c.has(6, readertext::SheetNext));
        CHECK(!c.has(6, readertext::SheetPrevious));
        int chapters = 0;
        for (size_t n = 0; n < c.interactions.count(); ++n)
          if (c.interactions.data()[n].action == 6 && c.interactions.data()[n].value >= readertext::ChapterPick)
            ++chapters;
        CHECK(chapters == (w > h ? 2 : 5));
        if (argc > 1)
          c.write(std::string(argv[1]) + "/contents-" + std::to_string(w) + "-" + std::to_string(int(language)) +
                  ".pbm");
      }
      for (int tab : {0, 1, 3}) {
        Canvas c(w, h);
        readertext::Model m;
        readertext::Work work;
        m.contentsAvailable = true;
        m.tab = tab;
        m.listCount = 6;
        m.size = 14;
        m.spacingLabel = "Normal";
        m.family = "Noto Serif";
        m.marginCount = 8;
        m.sizeCount = 4;
        m.listLabel = [](const void*, int n, char* text, size_t size) {
          std::snprintf(text, size, "Reader action %d", n + 1);
        };
        readertext::build(c.screen, m, work);
        c.checkHits();
        if (tab == 1) {
          CHECK(c.has(6, readertext::AlignmentLeft));
          CHECK(c.has(6, readertext::AlignmentCenter));
          CHECK(c.has(6, readertext::AlignmentJustify));
        }
        if (argc > 1)
          c.write(std::string(argv[1]) + "/reader-" + std::to_string(tab) + "-" + std::to_string(w) + "-" +
                  std::to_string(int(language)) + ".pbm");
      }
      for (auto opponent : {chessui::Opponent::Computer, chessui::Opponent::PassAndPlay}) {
        Canvas c(w, h);
        chessui::SettingsModel m;
        m.fromMenu = true;
        m.opponent = opponent;
        chessui::buildSettings(c.screen, m);
        c.checkHits();
        CHECK(c.has(chessui::ActionSettingValue, int(chessui::SettingValue::Person)));
        CHECK(c.has(chessui::ActionSettingValue, int(chessui::SettingValue::Easy)));
        if (argc > 1)
          c.write(std::string(argv[1]) + "/chess-settings-" + std::to_string(int(opponent)) + "-" + std::to_string(w) +
                  "-" + std::to_string(int(language)) + ".pbm");
      }
      for (bool saved : {false, true})
        for (bool undo : {false, true}) {
          Canvas c(w, h);
          chessui::StartModel m;
          m.hasSavedGame = saved;
          m.canTakeBack = undo;
          const int rows = chessui::startRows(m);
          CHECK(rows == 3 + int(saved));
          bool takeBack = false;
          for (int n = 0; n < rows; ++n)
            takeBack |=
                std::strcmp(chessui::startRowLabel(chessui::startRowAt(m, n)), tr(STR_UI_TAKE_BACK_A026F7)) == 0;
          CHECK(!takeBack);
          chessui::buildStartMenu(c.screen, m);
          c.checkHits();
        }
      chesspuzzle::Progress progress;
      progress.lastIndex = 4;
      for (int n = 0; n < 4; ++n) chesspuzzle::markSolved(progress, n);
      chesspuzzle::Session session;
      chesspuzzle::load(session, 4);
      for (auto view :
           {chesspuzzleui::View::Difficulty, chesspuzzleui::View::Select, chesspuzzleui::View::Celebration}) {
        Canvas c(w, h);
        chesspuzzleui::Model m;
        chesspuzzleui::Work work;
        m.session = &session;
        m.progress = &progress;
        m.view = view;
        m.index = 4;
        chesspuzzleui::build(c.screen, m, work);
        c.checkHits();
        if (view == chesspuzzleui::View::Difficulty) CHECK(!c.has(chesspuzzleui::Resume));
        if (view == chesspuzzleui::View::Select) {
          CHECK(c.has(chesspuzzleui::Resume));
          CHECK(c.has(chesspuzzleui::Pick, 15));
          CHECK(c.has(chesspuzzleui::Page, 1));
          CHECK(!c.has(chesspuzzleui::Page, -1));
        }
        if (argc > 1)
          c.write(std::string(argv[1]) + "/problems-" + std::to_string(int(view)) + "-" + std::to_string(w) + "-" +
                  std::to_string(int(language)) + ".pbm");
      }
      for (int page = 0; page < (w > h ? 8 : 4); ++page) {
        Canvas c(w, h);
        draughtsui::Model m;
        m.view = draughtsui::View::Help;
        m.helpPage = page;
        draughtsui::build(c.screen, m);
        c.checkHits();
        CHECK(c.has(draughtsui::HelpDone));
        CHECK(c.has(draughtsui::HelpPage, -1) == (page > 0));
        if (argc > 1)
          c.write(std::string(argv[1]) + "/draughts-help-" + std::to_string(page) + "-" + std::to_string(w) + "-" +
                  std::to_string(int(language)) + ".pbm");
      }
    }
  }
  for (int pages : {1, 2, 1000})
    for (int page : {1, pages}) {
      Canvas c(480, 800);
      pageui::build(c.screen, fui::makeRect(10, 650, 460, 48), page, pages, 101, -1, 101, 1);
      c.checkHits();
      CHECK(c.has(101, -1) == (page > 1));
      CHECK(c.has(101, 1) == (page < pages));
    }
  for (int type = 1; type <= 6; ++type)
    for (int n = 0; n < 48 * 6; ++n) CHECK((traditional::dark(type)[n] & ~traditional::darkMask(type)[n]) == 0);
  std::printf("Interaction redesign: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

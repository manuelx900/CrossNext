#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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
  mutable bool sawNearby = false;
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
    bool fit = true;
    fui::layoutText(target, r, s, st, [&](const char* run, fui::Rect box) {
      if (box.bottom() > r.bottom() || box.right() > r.right() || std::strstr(run, "…")) fit = false;
    });
    if (!fit) {
      std::printf("TEXT CUT [%d,%d %dx%d] %s\n", r.x, r.y, r.width, r.height, s);
      CHECK(fit);
    }
    if (std::strstr(s, "NEARBY") || std::strstr(s, "CERCA")) sawNearby = true;
    target.text(r, s, st);
  }
};

}  // namespace

#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../src/apps_local/battleship/BattleshipScreens.h"
#include "../../src/apps_local/checkers/DraughtsScreens.h"
#include "../../src/apps_local/chess/ChessScreens.h"
#include "../../src/apps_local/chess/ChessStyle.h"
#include "../../src/apps_local/connectfour/ConnectFourScreens.h"
#include "../../src/apps_local/go/GoScreens.h"
#include "../../src/apps_local/hearts/HeartsScreens.h"
#include "../../src/apps_local/solitaire/SolitaireScreens.h"
#include "../../src/apps_local/sudoku/SudokuScreens.h"
#include "../../src/apps_local/ui/GameFrontDoor.h"

int main(int argc, char** argv) {
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (int saved = 0; saved < 2; ++saved)
        for (int game = 0; game < 9; ++game) {
          if (game == 2 || (game >= 7 && size.first != 800)) continue;
          I18N.setLanguage(lang);
          Canvas c(size.first, size.second);
          FontBridge compact(ubuntu_10_regular);
          c.target.setFont(4, compact.font);
          if (game < 6) c.target.setFont(0, c.tiny.font);
          if (game == 3) c.target.setFont(4, c.tiny.font);
          TextAudit audit(c.target, size.first, size.second);
          toybox::Frame frame(audit, c.device, c.input, c.interactions);
          auto tokens = toybox::themeTokens();
          if (game >= 7) tokens.headerHeight = 56;
          toybox::Screen s(frame, tokens);
          fui::Rect preview;
          if (game == 0) {
            sudokuui::MenuModel m;
            m.hasGame = saved;
            const char* demo = "530070000600195000098000060800060003400803001700020006060000280000419005000080079";
            for (int n = 0; n < 81; ++n) m.game.puzzle.given[n] = demo[n] - '0';
            sudokuui::buildMenu(s, m);
            CHECK(c.has(sudokuui::ActionPlay));
            CHECK(c.has(sudokuui::ActionMenuRow, int(sudokuui::MenuRow::HowTo)));
          }
          if (game == 1) {
            c4ui::MenuModel m;
            c4ui::buildMenu(s, m);
            CHECK(c.interactions.count() == 2);
          }
          if (game == 3) {
            bshipui::StartModel m;
            m.hasSavedGame = saved;
            preview = bshipui::buildStartMenu(s, m);
            CHECK(c.interactions.count() == bshipui::startRows(m));
          }
          if (game == 4) {
            goui::MenuModel m;
            m.inProgress = saved;
            goui::buildMenu(s, m);
            CHECK(c.has(goui::ActionMenuRow, int(goui::MenuRow::Settings)));
            CHECK(!saved || c.has(goui::ActionDiscard));
            CHECK(c.interactions.count() == (saved ? 4 : 3));
          }
          if (game == 5) {
            chessui::StartModel m;
            m.hasSavedGame = saved;
            m.continueDetail = "2 / WHITE";
            preview = chessui::buildStartMenu(s, m);
            CHECK(c.interactions.count() == chessui::startRows(m));
            int cell = std::min(int(preview.width), int(preview.height)) / 8;
            const int left = preview.x + (preview.width - cell * 8) / 2,
                      top = preview.y + (preview.height - cell * 8) / 2;
            for (int y = 0; y < 8; ++y)
              for (int x = 0; x < 8; ++x) {
                auto b = fui::makeRect(left + x * cell, top + y * cell, cell, cell);
                c.target.fill(
                    b, (x + y) % 2 ? fui::Paint::solid(fui::Color::Black) : fui::Paint::solid(fui::Color::White));
                int type = y == 1 || y == 6   ? 1
                           : y == 0 || y == 7 ? (x == 0 || x == 7   ? 4
                                                 : x == 1 || x == 6 ? 2
                                                 : x == 2 || x == 5 ? 3
                                                 : x == 3           ? 5
                                                                    : 6)
                                              : 0;
                if (type) chessstyle::draw(c.target, b.inset(fui::Insets{3, 3, 3, 3}), type, y < 2);
              }
          }
          if (game == 6) {
            draughts::Game g;
            draughts::start(g);
            draughtsui::Model m;
            m.game = &g;
            m.active = saved;
            draughtsui::build(s, m);
            CHECK(c.has(draughtsui::Play));
            CHECK(c.has(draughtsui::Settings));
          }
          if (game == 7) {
            heartsui::MenuModel m;
            m.hasSave = saved;
            m.savedHand = 3;
            m.gamesPlayed = saved ? 25 : 0;
            m.gamesWon = saved ? 17 : 0;
            m.bestPlace = saved ? 1 : 0;
            heartsui::buildMenu(s, m);
          }
          if (game == 8) {
            solitaireui::MenuModel m;
            m.hasSave = saved;
            m.savedMoves = 39;
            m.played = saved ? 25 : 0;
            m.wins = saved ? 17 : 0;
            m.streak = saved ? 4 : 0;
            for (int i = 0; i < 16; ++i) m.recent[i] = (i == 0 || i == 4 || i == 8 || i == 11) ? 1 : 2;
            solitaireui::buildMenu(s, m);
          }
          if (game >= 7) {
            for (size_t i = 0; i < c.interactions.count(); ++i) {
              const auto& hit = c.interactions.data()[i];
              if (hit.rect.y > 300) CHECK(hit.rect.height >= 44);
            }
          }
          if (game >= 1 && game <= 5) {
            CHECK(!audit.sawNearby);
          }
          c.checkHits();
          for (size_t a = 0; a < c.interactions.count(); ++a)
            for (size_t b = 0; b < a; ++b) {
              auto x = c.interactions.data()[a].rect, y = c.interactions.data()[b].rect;
              CHECK(x.right() <= y.x || y.right() <= x.x || x.bottom() <= y.y || y.bottom() <= x.y);
            }
          if (argc > 1)
            c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                    std::to_string(size.first) + "-" + std::to_string(game) + "-" + std::to_string(saved) + ".pbm");
        }
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (int page = 0; page < 1; ++page) {
        I18N.setLanguage(lang);
        Canvas c(size.first, size.second);
        FontBridge compact(ubuntu_10_regular);
        c.target.setFont(4, compact.font);
        TextAudit audit(c.target, size.first, size.second);
        toybox::Frame frame(audit, c.device, c.input, c.interactions);
        toybox::Screen s(frame);
        c.target.setFont(4, c.tiny.font);
        bshipui::buildHowTo(s, page);
        c.checkHits();
        CHECK(c.has(bshipui::ActionHelpNext));
      }
  for (auto lang : {Language::EN, Language::ES})
    for (int orientation = 0; orientation < 4; ++orientation)
      for (int rule = 0; rule < 4; ++rule)
        for (bool failed : {false, true}) {
          const bool wide = orientation % 2;
          I18N.setLanguage(lang);
          Canvas c(wide ? 800 : 480, wide ? 480 : 800);
          c.device.safeArea = orientation == 0   ? fui::Insets{10, 0, 0, 0}
                              : orientation == 1 ? fui::Insets{0, 10, 0, 0}
                              : orientation == 2 ? fui::Insets{0, 0, 10, 0}
                                                 : fui::Insets{0, 0, 0, 10};
          FontBridge compact(ubuntu_10_regular);
          c.target.setFont(4, compact.font);
          TextAudit audit(c.target, c.width, c.height);
          toybox::Frame frame(audit, c.device, c.input, c.interactions);
          toybox::Screen s(frame);
          draughts::Game game;
          draughts::start(game, draughts::Rules(rule));
          draughts::Generator legal;
          legal.begin(game);
          while (!legal.tick()) {
          }
          draughtsui::Model m;
          m.game = &game;
          m.legal = &legal;
          m.active = true;
          m.view = draughtsui::View::Board;
          m.saveFailed = failed;
          m.selected = legal.at(0).move.from;
          draughtsui::build(s, m);
          c.checkHits();
          CHECK(c.has(draughtsui::NewGame));
          CHECK(c.has(draughtsui::Menu));
          const auto r = draughtsui::boardRect(c.device, game.size);
          CHECK(r.x >= 0 && r.right() <= c.width &&
                r.bottom() <= (wide ? c.device.safeRect().bottom() : c.height - 80));
          for (int n = 0; n < game.size * game.size; ++n) {
            const int cell = r.width / game.size;
            CHECK(draughtsui::squareAt(c.device, game.size, r.x + n % game.size * cell + cell / 2,
                                       r.y + n / game.size * cell + cell / 2) == n);
          }
          CHECK(draughtsui::squareAt(c.device, game.size, r.right(), r.y) == -1);
          CHECK(draughtsui::squareAt(c.device, game.size, r.x - 1, r.y) == -1);
          const auto move = legal.at(0).move;
          CHECK(draughts::play(game, legal, move.from, move.to));
          CHECK(game.turn == 1);
          if (argc > 1 && lang == Language::ES && rule == 0 && !failed)
            c.write(std::string(argv[1]) + "/draughts-board-" + std::to_string(orientation) + ".pbm");
        }
  std::printf("Front doors: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

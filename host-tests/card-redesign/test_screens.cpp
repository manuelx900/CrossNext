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
#include "../../src/apps_local/hearts/HeartsScreens.h"
#include "../../src/apps_local/minesweeper/MinesweeperScreens.h"
#include "../../src/apps_local/picross/PicrossScreens.h"
#include "../../src/apps_local/pocketgames/PocketGamesScreens.h"
#include "../../src/apps_local/pomodoro/PomodoroScreens.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/reading/ReadingStatsScreens.h"
#include "../../src/apps_local/solitaire/SolitaireScreens.h"
#include "../../src/apps_local/ui/PageIndicator.h"
#include "../../src/apps_local/ui/QuickSettingsScreen.h"
#include "../../src/apps_local/ui/ReferenceArt.h"
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
      const bool dateCell = r.width < 44;
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
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    auto shot = [&](Canvas& c, const std::string& name) {
      c.checkHits();
      if (argc > 1) c.write(std::string(argv[1]) + "/" + name + "-" + std::to_string(int(lang)) + ".pbm");
    };
    solitaire::Game sol;
    sol.deal(42, false);
    {
      Canvas c(800, 480);
      solitaireui::MenuModel m;
      m.hasSave = true;
      m.savedMoves = 12;
      m.played = 3;
      solitaireui::buildMenu(c.screen, m);
      shot(c, "sol-menu");
    }
    {
      Canvas c(800, 480);
      solitaireui::Layout l;
      solitaireui::buildBoard(c.screen, {&sol, -1, 0, false, true}, l);
      shot(c, "sol-board");
    }
    for (bool three : {false, true}) {
      Canvas c(800, 480);
      solitaireui::buildSetup(c.screen, three, true);
      shot(c, three ? "sol-setup3" : "sol-setup1");
    }
    {
      Canvas c(800, 480);
      solitaireui::buildSettings(c.screen, {});
      shot(c, "sol-settings");
    }
    for (int page = 0; page < 2; ++page) {
      Canvas c(800, 480);
      solitaireui::buildHelp(c.screen, page);
      shot(c, "sol-help" + std::to_string(page));
    }
    hearts::Game game;
    uint32_t seed = 42;
    hearts::newGame(game, seed);
    {
      Canvas c(800, 480);
      heartsui::MenuModel m;
      m.hasSave = true;
      m.savedHand = 2;
      m.gamesPlayed = 3;
      m.gamesWon = 1;
      m.bestScore = 18;
      heartsui::buildMenu(c.screen, m);
      shot(c, "hearts-menu");
    }
    {
      Canvas c(800, 480);
      heartsui::buildSetup(c.screen, {}, true);
      shot(c, "hearts-setup");
    }
    {
      Canvas c(800, 480);
      heartsui::buildSettings(c.screen, {});
      shot(c, "hearts-settings");
    }
    {
      Canvas c(800, 480);
      heartsui::buildHowTo(c.screen, {});
      shot(c, "hearts-help");
    }
    for (bool passing : {true, false}) {
      Canvas c(800, 480);
      heartsui::BoardModel m;
      m.game = &game;
      m.pickedCount = 3;
      m.confirmEnabled = true;
      m.status = "FOLLOW CLUBS";
      for (int n = 0; n < 4; ++n) {
        static const char* names[] = {"YOU", "WEST", "NORTH", "EAST"};
        m.seats[n].name = names[n];
        m.seats[n].total = n * 7;
      }
      for (int n = 0; n < 13; ++n) m.legal[n] = n < 3;
      m.picked[2] = m.picked[8] = m.picked[12] = true;
      if (!passing) {
        game.phase = hearts::Phase::Playing;
        game.turn = hearts::Seat::South;
        for (int n = 1; n < 4; ++n) game.trick.played[n] = cards::makeCard(cards::Suit::Clubs, n + 3);
      }
      heartsui::Layout l;
      heartsui::buildBoard(c.screen, m, l);
      shot(c, passing ? "hearts-pass" : "hearts-play");
    }
    {
      Canvas c(800, 480);
      heartsui::ScoreModel m;
      m.game = &game;
      for (int n = 0; n < 4; ++n) {
        static const char* names[] = {"YOU", "WEST", "NORTH", "EAST"};
        m.seats[n].name = names[n];
        game.total[n] = 12 + n * 7;
        game.lastHand.scored[n] = n + 2;
      }
      heartsui::buildScore(c.screen, m);
      shot(c, "hearts-score");
    }
    for (auto dimensions : {std::pair<int, int>{480, 800}, {800, 480}}) {
      for (int error = 0; error < 8; ++error) {
        Canvas c(dimensions.first, dimensions.second);
        pocketgames::State state;
        pocketgames::start(state, pocketgames::Kind::Othello, 42);
        pocketui::Model m;
        m.state = &state;
        m.view = pocketui::View::Board;
        if (error == 3) state.othello.turn = 2;
        if (error == 4) state.othello.passed = true;
        if (error >= 5) {
          state.othello.over = true;
          for (int k = 0; k < 64; ++k) state.othello.board[k] = error == 5 ? (k < 32 ? 1 : 2) : error == 6 ? 1 : 2;
        }
        m.saveFailed = error == 1;
        m.invalid = error == 2;
        pocketui::build(c.screen, m);
        shot(c, "othello-" + std::to_string(dimensions.first) + "-" + std::to_string(error));
        if (error == 0 || error >= 3) {
          const char* expectedText = error == 0   ? tr(STR_OT_YOUR_TURN)
                                     : error == 3 ? tr(STR_OT_THINKING)
                                     : error == 4 ? tr(STR_OT_PASSED)
                                     : error == 5 ? tr(STR_UI_DRAW_7AAB1B)
                                     : error == 6 ? tr(STR_OT_VICTORY)
                                                  : tr(STR_OT_MACHINE_WINS);
          char expected[192];
          pixelui::copyCaps(expected, sizeof(expected), expectedText);
          bool found = false;
          for (const auto& text : c.target.labels) found |= text.value == expected || text.value == expectedText;
          CHECK(found);
        }

        if (error == 1 || error == 2) {
          char expected[192];
          pixelui::copyCaps(expected, sizeof(expected),
                            error == 1 ? tr(STR_UI_COULD_NOT_SAVE_GAME_615E7A) : tr(STR_OT_INVALID));
          const char* expectedText = error == 1 ? tr(STR_UI_COULD_NOT_SAVE_GAME_615E7A) : tr(STR_OT_INVALID);
          bool found = false;
          for (const auto& text : c.target.labels) found |= text.value == expected || text.value == expectedText;
          CHECK(found);
        }
      }
    }
    {
      Canvas c(800, 480);
      heartsui::BoardModel m;
      m.game = &game;
      game.phase = hearts::Phase::TrickTaken;
      game.turn = hearts::Seat::South;
      m.status = "WEST TAKES IT";
      heartsui::Layout l;
      heartsui::buildBoard(c.screen, m, l);
      char expected[192];
      pixelui::copyCaps(expected, sizeof(expected), tr(STR_HEARTS_TURN));
      for (const auto& text : c.target.labels) CHECK(text.value != expected);
      shot(c, "hearts-trick-taken");
    }
    for (int seed = 1; seed < 20; ++seed) {
      solitaire::Game game;
      game.deal(seed, false);
      for (int n = 0; n < 60; ++n) {
        const int foundation = game.foundationFor(solitaire::kWastePile);
        if (foundation >= 0) game.move(solitaire::kWastePile, foundation, 1);
        game.drawFromStock();
      }
      Canvas c(800, 480);
      solitaireui::Layout l;
      solitaireui::buildBoard(c.screen, {&game}, l);
      static constexpr int visual[] = {0, 1, 3, 2};
      for (int p = solitaire::kFirstFoundation; p < solitaire::kFirstTableau; ++p) {
        auto suit = solitaireui::foundationSuit(game, p);
        CHECK(l.x[p] == l.x[solitaire::kFirstTableau + 3 + visual[int(suit)]]);
      }
    }
    {
      Canvas c(480, 800);
      referenceart::draw(c.screen, fui::makeRect(170, 120, 140, 180), referenceart::KnightTrophy);
      shot(c, "knight");
    }
  }
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

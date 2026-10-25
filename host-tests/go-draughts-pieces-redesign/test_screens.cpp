#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/checkers/DraughtsScreens.h"
#include "../../src/apps_local/chess/ChessLosses.h"
#include "../../src/apps_local/chess/ChessScreens.h"
#include "../../src/apps_local/chess/ChessStyle.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleScreens.h"
#include "../../src/apps_local/go/GoScreens.h"
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
  d.safeArea = {9, 7, 3, 7};
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
    CHECK(r.x >= 0 && r.y >= 0 && r.right() <= clipRect().right() && r.bottom() <= clipRect().bottom());
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
      if (line.bottom() > box.bottom() || std::strstr(run, "…") || std::strstr(run, "..."))
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
      CHECK(!std::strstr(run, "…") && !std::strstr(run, "..."));
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
  CHECK(chessstyle::kDefault == chessstyle::Design::Classic);
  CHECK(chessstyle::parsePreference("1 1 1 1 0") == chessstyle::kDefault);
  CHECK(chessstyle::parsePreference("1 1 1 1") == chessstyle::kDefault);
  CHECK(chessstyle::parsePreference("1 1 1 1 0 0") == chessstyle::kDefault);
  CHECK(chessstyle::parsePreference("1 1 1 1 0 2") == chessstyle::kDefault);
  CHECK(chessstyle::parsePreference("1 1 1 1 0 5") == chessstyle::kDefault);
  CHECK(chessstyle::parsePreference("2 1 1 1 0 0") == chessstyle::kDefault);
  CHECK(chessstyle::parsePreference("") == chessstyle::kDefault);
  {
    chessui::Losses losses;
    chess::Position p;
    CHECK(chess::parseFen("7k/P7/8/8/8/8/8/7K w - - 0 1", p));
    chess::Move promotion{48, 56, chess::Queen, chess::FlagPromotion};
    chess::Undo undo;
    chess::makeMove(p, promotion, undo);
    losses.capture(undo.captured);
    CHECK(losses.counts[0][chess::Pawn] == 0);
    losses.capture(uint8_t(chess::Pawn | chess::BlackFlag));
    losses.capture(uint8_t(chess::Pawn | chess::BlackFlag));
    CHECK(losses.counts[1][chess::Pawn] == 2);
    char wire[64];
    losses.encode(wire, sizeof(wire));
    chessui::Losses decoded;
    CHECK(decoded.decode(wire));
    CHECK(decoded.counts[1][chess::Pawn] == 2);
    CHECK(!decoded.decode("0 0 -1 0 0 0 0 0 0 0"));
    CHECK(!decoded.decode("0 0 0 0 0 0 0 0 0"));
    CHECK(!decoded.decode("0 0 0 0 0 0 0 0 0 0 trailing"));
  }
  CHECK(draughtsui::outcomeFor(false, 0, false, 0) == draughtsui::Outcome::Playing);
  CHECK(draughtsui::outcomeFor(true, 0, false, 0) == draughtsui::Outcome::Black);
  CHECK(draughtsui::outcomeFor(true, 0, false, 1) == draughtsui::Outcome::White);
  CHECK(draughtsui::outcomeFor(true, 5, true, 1) == draughtsui::Outcome::Draw);
  CHECK(go::seatFor(go::Opponent::Computer, go::kWhite, 0) == go::kWhite);
  CHECK(go::seatFor(go::Opponent::Computer, go::kWhite, 2) == go::kBlack);
  CHECK(go::seatFor(go::Opponent::Computer, go::kBlack, 0) == go::kBlack);
  CHECK(go::seatFor(go::Opponent::Human, go::kWhite, 0) == go::kBlack);
  CHECK(go::back(go::Screen::Review) == go::Screen::Result);
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (auto dimensions : {std::pair<int, int>{480, 800}, {800, 480}}) {
      const int w = dimensions.first, h = dimensions.second;
      const auto save = [&](Canvas& c, const char* name, int n = 0) {
        c.checkHits();
        if (argc > 1) {
          char path[512];
          std::snprintf(path, sizeof(path), "%s/%s-%dx%d-%s-%d.pbm", argv[1], lang == Language::ES ? "es" : "en", w, h,
                        name, n);
          c.write(path);
        }
      };
      for (int size : {9, 13, 19}) {
        Canvas c(w, h);
        for (int point = 0; point < size * size; ++point) {
          int16_t x, y;
          goui::stoneCentre(c.device, size, point, x, y);
          int actual = -1;
          CHECK(goui::pointAt(c.device, size, x, y, actual));
          CHECK(actual == point);
        }
        const auto r = goui::boardRect(c.device);
        int p = -1;
        CHECK(!goui::pointAt(c.device, size, r.x - 1, r.y, p));
        CHECK(!goui::pointAt(c.device, size, r.right(), r.y, p));
        for (int review = 0; review < 2; ++review) {
          Canvas b(w, h);
          goui::BoardModel m;
          go::reset(m.game, size);
          m.review = review;
          go::play(m.game, go::pointAt(size, 4, 4));
          go::play(m.game, go::pointAt(size, 4, 5));
          goui::buildBoard(b.screen, m);
          CHECK(b.has(goui::ActionPass) == !review);
          save(b, "go-board", size * 10 + review);
        }
      }
      for (int paused = 0; paused < 2; ++paused) {
        Canvas c(w, h);
        goui::MenuModel m;
        m.inProgress = paused;
        goui::buildMenu(c.screen, m);
        CHECK(c.has(goui::ActionHelp));
        save(c, "go-menu", paused);
      }
      for (int opponent = 0; opponent < 2; ++opponent) {
        Canvas c(w, h);
        goui::SettingsModel m;
        m.opponent = go::Opponent(opponent);
        goui::buildSettings(c.screen, m);
        CHECK(c.has(goui::ActionSettingsRow, int(goui::SettingsRow::Level)) == !opponent);
        save(c, "go-settings", opponent);
      }
      for (int page = 0; page < 3; ++page) {
        Canvas c(w, h);
        goui::buildHelp(c.screen, page);
        CHECK(c.has(goui::ActionHelpPage, 1) == (page < 2));
        save(c, "go-help", page);
      }
      for (int outcome = 0; outcome < 3; ++outcome) {
        Canvas c(w, h);
        goui::ResultModel m;
        go::reset(m.game, 9);
        m.blackHalves = outcome == 0 ? 96 : 76;
        m.whiteHalves = outcome == 2 ? 76 : 81;
        goui::buildResult(c.screen, m);
        CHECK(c.has(goui::ActionReview));
        save(c, "go-result", outcome);
      }
      {
        Canvas c(w, h);
        goui::CountModel m;
        go::reset(m.game, 9);
        goui::buildCount(c.screen, m);
        CHECK(c.has(goui::ActionAccept));
        save(c, "go-count");
      }
      for (int rules = 0; rules < 4; ++rules) {
        draughts::Game game;
        draughts::start(game, draughts::Rules(rules));
        draughts::Generator legal;
        legal.begin(game);
        while (!legal.tick()) {
        }
        for (auto view :
             {draughtsui::View::Menu, draughtsui::View::Settings, draughtsui::View::Board, draughtsui::View::Review}) {
          Canvas c(w, h);
          draughtsui::Model m;
          m.game = &game;
          m.legal = &legal;
          m.rules = game.rules;
          m.view = view;
          m.active = true;
          draughtsui::build(c.screen, m);
          if (view == draughtsui::View::Menu) {
            CHECK(c.has(draughtsui::NewGame));
            int playY = -1, newY = -1, settingsY = -1;
            for (size_t n = 0; n < c.interactions.count(); ++n) {
              const auto& hit = c.interactions.data()[n];
              if (hit.action == draughtsui::Play) playY = hit.rect.y;
              if (hit.action == draughtsui::NewGame) newY = hit.rect.y;
              if (hit.action == draughtsui::Settings) settingsY = hit.rect.y;
            }
            CHECK(playY >= 0 && playY < newY && newY < settingsY);
          }
          save(c, "draughts", rules * 10 + int(view));
        }
        for (int result = 0; result < 3; ++result) {
          Canvas c(w, h);
          draughtsui::Model m;
          m.game = &game;
          m.legal = &legal;
          m.result = draughtsui::Outcome(result + 1);
          m.view = draughtsui::View::Result;
          draughtsui::build(c.screen, m);
          CHECK(c.has(draughtsui::Review));
          save(c, "draughts-result", rules * 10 + result);
        }
      }
      for (auto view : {draughtsui::View::Menu, draughtsui::View::Settings, draughtsui::View::Board,
                        draughtsui::View::Result, draughtsui::View::Review}) {
        CHECK(draughtsui::canPlay(view, true, true, true, 0, false, 1) == (view == draughtsui::View::Board));
        CHECK(!draughtsui::canPlay(view, false, true, true, 0, false, 1));
        CHECK(!draughtsui::canPlay(view, true, true, true, 0, true, 1));
        CHECK(!draughtsui::canPlay(view, true, true, true, 0, false, 0));
      }
      for (auto view :
           {draughtsui::View::Menu, draughtsui::View::Board, draughtsui::View::Review, draughtsui::View::Result}) {
        Canvas c(w, h);
        draughts::Game game;
        draughts::start(game);
        draughts::Generator legal;
        legal.begin(game);
        while (!legal.tick()) {
        }
        draughtsui::Model m;
        m.game = &game;
        m.legal = &legal;
        m.view = view;
        m.result = draughtsui::Outcome::White;
        m.saveFailed = true;
        draughtsui::build(c.screen, m);
        CHECK(c.label(tr(STR_WORDLE_SAVE_FAILED)));
        if (view == draughtsui::View::Review || view == draughtsui::View::Board) {
          const auto board = draughtsui::boardRect(c.device, game.size);
          for (const auto& label : c.target.labels)
            if (label.value.find("10") != std::string::npos)
              CHECK(w > h ? label.box.x >= board.right() : label.box.bottom() <= board.y);
        }
        save(c, "draughts-save-failed", int(view));
      }
      for (int colour = 0; colour < 2; ++colour) {
        CHECK(draughtsui::canPlay(draughtsui::View::Board, true, true, false, colour, false, 1, bool(colour)));
        CHECK(!draughtsui::canPlay(draughtsui::View::Board, true, true, false, 1 - colour, false, 1, bool(colour)));
        Canvas c(w, h);
        const auto r = draughtsui::boardRect(c.device, 10);
        CHECK(draughtsui::squareAt(c.device, 10, r.x + 1, r.y + 1, bool(colour)) == (colour ? 99 : 0));
        draughts::Game game;
        draughts::start(game);
        for (int winner = 0; winner < 2; ++winner) {
          Canvas result(w, h);
          draughtsui::Model m;
          m.game = &game;
          m.view = draughtsui::View::Result;
          m.result = winner ? draughtsui::Outcome::Black : draughtsui::Outcome::White;
          m.humanBlack = colour;
          draughtsui::build(result.screen, m);
          CHECK(result.label(winner == colour ? tr(STR_MATCH_WON) : tr(STR_MATCH_LOST)));
          save(result, "draughts-colour-result", colour * 2 + winner);
          Canvas chessResult(w, h);
          chessui::buildResult(chessResult.screen, {false, bool(winner), bool(colour), "CHECKMATE"});
          CHECK(chessResult.label(winner == colour ? tr(STR_MATCH_WON) : tr(STR_MATCH_LOST)));
          CHECK(chessResult.has(chessui::ActionResultReview));
          save(chessResult, "chess-result", colour * 2 + winner);
        }
        Canvas board(w, h);
        chessui::BoardModel m;
        m.whiteAtBottom = colour == 0;
        m.status = "YOUR TURN";
        m.level = "NORMAL";
        m.lastMove = "1. Nc3";
        chessui::buildBoardChrome(board.screen, m);
        const auto g = chessui::boardLayout(board.device);
        CHECK(g.board.width >= (w > h ? 300 : 400));
        CHECK(g.lastMove.y >= g.board.bottom() + 24 || w > h);
        CHECK(g.losses.bottom() <= board.device.safeRect().bottom());
        int zeroes = 0;
        for (const auto& text : board.target.labels) zeroes += text.value == "×0";
        CHECK(zeroes == 10);
        save(board, "chess-board-chrome", colour);
      }
      for (int design = 0; design < 3; ++design) {
        Canvas c(w, h);
        chessui::SettingsModel m;
        m.pieceDesign = chessstyle::Design(design);
        chessui::buildSettings(c.screen, m);
        CHECK(c.has(chessui::ActionSettingValue, int(chessui::SettingValue::Hints)));
        CHECK(c.has(chessui::ActionSettingValue, int(chessui::SettingValue::Highlights)));
        CHECK(!c.has(chessui::ActionSettingValue, 40));
        CHECK(!c.has(chessui::ActionSettingValue, 41));
        CHECK(!c.has(chessui::ActionSettingValue, 42));
        save(c, "chess-settings", design);
        Canvas sprites(w, h);
        for (int colour = 0; colour < 2; ++colour)
          for (int type = 1; type <= 6; ++type)
            chessstyle::draw(sprites.target, fui::makeRect(20 + (type - 1) * 70, 120 + colour * 80, 60, 60), type,
                             colour, chessstyle::Design(design));
        save(sprites, "chess-pieces", design);
      }
    }
  }
  std::printf("Go/Draughts/Pieces: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

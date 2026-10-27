#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../lib/EpdFont/builtinFonts/notosans_8_regular.h"
#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../src/apps_local/ui/fonts/reading_serif_14.h"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#pragma GCC diagnostic pop

#include "../../src/apps_local/battleship/BattleshipScreens.h"
#include "../../src/apps_local/carnivoro/CarnivoroScreens.h"
#include "../../src/apps_local/chess/ChessScreens.h"
#include "../../src/apps_local/connectfour/ConnectFourScreens.h"
#include "../../src/apps_local/flashcards/FlashScreens.h"
#include "../../src/apps_local/go/GoScreens.h"
#include "../../src/apps_local/habits/HabitsScreens.h"
#include "../../src/apps_local/home/HomeScreen.h"
#include "../../src/apps_local/library/LibraryMenu.h"
#include "../../src/apps_local/picross/PicrossScreens.h"
#include "../../src/apps_local/pocketgames/PocketGamesScreens.h"
#include "../../src/apps_local/pomodoro/PomodoroScreens.h"
#include "../../src/apps_local/readertext/EndOfBookScreen.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/reading/ReadingStatsScreens.h"
#include "../../src/apps_local/tools/ToolsScreens.h"
#include "../../src/apps_local/ui/CardPanels.h"
#include "../../src/apps_local/ui/KeyboardViewport.h"
#include "../../src/apps_local/ui/NetworkLandscape.h"
#include "../../src/apps_local/ui/PageIndicator.h"
#include "../../src/apps_local/ui/ToolsDigits.h"
#include "../../src/apps_local/wallpapers/WallpapersScreens.h"
#include "../../src/apps_local/wordle/WordleScreens.h"
#include "../../src/apps_local/words/WordsScreens.h"

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
    outlines.push_back({r, width});
    backend.stroke(r, p, width, radius, corners);
  }
  std::vector<std::pair<fui::Rect, uint8_t>> outlines;
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
    });
    labels.push_back({value ? value : "", style.font, box});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge regular{ui_prose_10}, tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30},
      library{ubuntu_10_regular}, caption{notosans_8_regular}, prose{ui_prose_14}, reading{reading_serif_14};
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
      if (slot.action == fui::NO_ACTION) continue;
      CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
      CHECK(r.width >= 24 && r.height >= 24);
      for (size_t j = 0; j < n; ++j) {
        if (interactions.data()[j].action == fui::NO_ACTION) continue;
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
  for (int rows : {4, 5, 6}) {
    const int keyHeight = keyboardview::keyHeight(56, true);
    CHECK(keyHeight >= 44);
    CHECK(keyboardview::keyHeight(56, false) == 56);
    const int keyboardTop = 480 - rows * keyHeight - (rows - 1) * 6 - 8 - 19;
    const int fieldTop = 100, lineHeight = 28, gap = 20;
    const int visible = keyboardview::visibleLines(keyboardTop, fieldTop, lineHeight, gap, true);
    CHECK(visible >= 1);
    CHECK(fieldTop + visible * lineHeight + gap <= keyboardTop);
  }
  const std::string input = "áéíóú café lectura 日本語 abcdefghijklmnopqrstuvwxyz áéíóú café lectura 日本語";
  std::vector<int> boundaries{0};
  for (int n = 1; n <= int(input.size()); ++n)
    if (n == int(input.size()) || (static_cast<unsigned char>(input[n]) & 0xc0) != 0x80) boundaries.push_back(n);
  const auto next = [&](int start) {
    const auto it = std::find(boundaries.begin(), boundaries.end(), start);
    return boundaries[std::min(int(boundaries.size()) - 1, int(it - boundaries.begin()) + 8)];
  };
  for (int visible : {1, 2, 3})
    for (int cursor : boundaries) {
      const int first = keyboardview::firstByte(int(input.size()), cursor, visible, next);
      CHECK(std::find(boundaries.begin(), boundaries.end(), first) != boundaries.end());
      CHECK(first <= cursor);
      int last = first;
      for (int n = 0; n < visible && last < int(input.size()); ++n) last = next(last);
      CHECK(cursor <= last);
      CHECK(last == int(input.size()) || cursor < last);
    }

  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (int orientation = 0; orientation < 4; ++orientation) {
      const bool wide = orientation % 2;
      const int width = wide ? 800 : 480, height = wide ? 480 : 800;
      const auto suffix = std::string(language == Language::ES ? "-es-" : "-en-") + std::to_string(orientation);
      Canvas probe(width, height);
      const auto safe = probe.device.safeRect();
      for (int n = 0, lastSize = 0; n < picross::kPuzzleCount; ++n) {
        if (picross::kPuzzles[n].size == lastSize) continue;
        lastSize = picross::kPuzzles[n].size;
        Canvas puzzle(width, height);
        picross::Board board;
        board.load(n);
        picrossui::Layout geometry;
        picrossui::buildBoard(puzzle.screen, {&board}, geometry);
        puzzle.checkHits();
        CHECK(geometry.cell >= (lastSize == 5 ? 32 : 18));
        int row = -1, column = -1;
        CHECK(geometry.cellAt(geometry.board.right() - 1, geometry.board.bottom() - 1, row, column));
        CHECK(row == lastSize - 1 && column == lastSize - 1);
        if (argc > 1) puzzle.write(std::string(argv[1]) + "/picross-" + std::to_string(lastSize) + suffix + ".pbm");
      }
      for (int size : {9, 13, 19}) {
        Canvas goCanvas(width, height);
        goui::BoardModel model;
        go::reset(model.game, size);
        goui::buildBoard(goCanvas.screen, model);
        goCanvas.checkHits();
        const auto board = goui::boardRect(goCanvas.device);
        CHECK(board.width >= (wide ? 360 : 300));
        for (int point = 0; point < size * size; ++point) {
          int16_t x, y;
          goui::stoneCentre(goCanvas.device, size, point, x, y);
          int resolved = -1;
          CHECK(goui::pointAt(goCanvas.device, size, x, y, resolved) && resolved == point);
        }
        if (argc > 1 && size == 9) goCanvas.write(std::string(argv[1]) + "/go-board" + suffix + ".pbm");
      }
      if (wide)
        for (int state = 0; state < 5; ++state) {
          Canvas naval(width, height);
          bship::Game game{};
          bshipui::Layout geometry;
          if (state == 0)
            bshipui::buildStartMenu(naval.screen, {});
          else if (state == 1) {
            bshipui::PlaceModel model;
            bshipui::buildPlaceChrome(naval.screen, model, &geometry);
            CHECK(geometry.place.width >= 330);
          } else {
            bshipui::BoardModel model;
            model.game = &game;
            model.canFire = state == 3;
            model.gameOver = state == 4;
            model.aim = model.canFire ? 53 : -1;
            model.status = model.canFire ? tr(STR_NAVAL_TARGET) : tr(STR_UI_TAP_A_TARGET_0B935C);
            bshipui::buildBoardChrome(naval.screen, model, &geometry);
            CHECK(geometry.target.width >= 330 && geometry.own.width >= 100);
          }
          naval.checkHits();
          if (argc > 1) naval.write(std::string(argv[1]) + "/naval-" + std::to_string(state) + suffix + ".pbm");
        }
      {
        Canvas suspects(width, height);
        carnivoro::State state;
        state.caseId = 0;
        state.status[0] = carnivoro::Status::Active;
        carnivoroui::Model model;
        model.state = &state;
        model.view = carnivoroui::View::Statements;
        carnivoroui::Geometry geometry;
        pagedtext::Work work;
        carnivoroui::build(suspects.screen, model, geometry, work);
        suspects.checkHits();
        if (wide) CHECK(geometry.suspectCapacity >= 2);
        if (argc > 1) suspects.write(std::string(argv[1]) + "/suspects" + suffix + ".pbm");
      }
      {
        habits::Store store;
        const int day = habits::dayIndex(2026, 10, 27);
        for (int n = 0; n < 12; ++n) {
          char name[32];
          std::snprintf(name, sizeof(name), "Habit %d", n + 1);
          habits::add(store, name, day);
        }
        Canvas today(width, height);
        const int capacity = habitsui::todayCapacity(today.device);
        CHECK(capacity == 6);
        habitsui::buildToday(today.screen, {&store, day});
        today.checkHits();
        CHECK(today.hits(habitsui::ActionOpen) == capacity);
        CHECK(!today.label("HABITS FOR THIS DAY"));
        if (argc > 1) today.write(std::string(argv[1]) + "/habits" + suffix + ".pbm");
      }
      if (wide)
        for (int offset : {-1, 1}) {
          Canvas history(width, height);
          habits::Store store;
          const int day = habits::dayIndex(2026, 10, 27);
          habits::add(store, "Read", day - 4);
          habitsui::TodayModel model;
          model.store = &store;
          model.today = day;
          model.displayedDay = day + offset;
          habitsui::buildToday(history.screen, model);
          history.checkHits();
          const auto body = pixelui::geometry(history.device, 12).body.inset({8, 4, 0, 4});
          const auto labelArea = fui::makeRect(body.right() - 208 + 10, body.y, 104, 48);
          std::string caption;
          for (const auto& label : history.target.labels) {
            if (label.font != 3 || label.box.x < labelArea.x || label.box.right() > labelArea.right() ||
                label.box.y < labelArea.y || label.box.bottom() > labelArea.bottom())
              continue;
            if (!caption.empty()) caption += " ";
            caption += label.value;
          }
          char expected[96];
          pixelui::copyCaps(expected, sizeof(expected), tr(STR_HABITS_DAILY_PROGRESS));
          CHECK(caption == expected);
          if (argc > 1)
            history.write(std::string(argv[1]) + "/habits-history-" + std::to_string(offset) + suffix + ".pbm");
        }
      {
        Canvas decks(width, height);
        flashui::DeckRow rows[flashui::kMaxLibraryRows];
        for (int n = 0; n < flashui::kMaxLibraryRows; ++n)
          rows[n] = {n % 2 ? "Languages" : "General science", 120, 10, 4, n % 2 == 0, n % 2 != 0};
        flashui::LibraryModel model;
        model.count = flashui::libraryCapacity(decks.device);
        CHECK(model.count >= (wide ? 5 : 6));
        model.rows = rows;
        model.pages = 4;
        model.totalDecks = 17;
        decks.target.setFont(4, decks.caption.font);
        flashui::library(decks.screen, model);
        decks.checkHits();
        CHECK(decks.hits(flashui::OpenDeck) == model.count);
        int firstRow = height;
        for (size_t n = 0; n < decks.interactions.count(); ++n)
          if (decks.interactions.data()[n].action == flashui::OpenDeck)
            firstRow = std::min(firstRow, int(decks.interactions.data()[n].rect.y));
        char heading[64];
        pixelui::copyCaps(heading, sizeof(heading), tr(STR_FLASH_MY_DECKS));
        bool headingFound = false;
        for (const auto& label : decks.target.labels)
          if (label.value == heading) {
            const auto* cut = toybox::cutForLineHeight(decks.target.lineHeight(label.font));
            CHECK(cut != nullptr);
            if (cut) CHECK(label.box.y + cut->ascender + 4 <= firstRow);
            headingFound = true;
          }
        CHECK(headingFound);

        CHECK(!decks.label("Open to prepare this deck") && !decks.label("Files in /flashcards"));
        if (argc > 1) decks.write(std::string(argv[1]) + "/decks" + suffix + ".pbm");
      }
      {
        Canvas keys(width, height);
        wordle::Game game;
        game.start(1, "CLOUD");
        wordleui::GameModel model;
        model.game = &game;
        const auto keyboard = wordleui::buildGame(keys.screen, model);
        keys.checkHits();
        if (wide) {
          const auto body = pixelui::geometry(keys.device).body;
          CHECK(std::abs((keyboard.top - 44 + keyboard.top + 3 * keyboard.keyH + 2 * keyboard.gap) -
                         (body.y + body.bottom())) <= 2);
        }
        if (argc > 1) keys.write(std::string(argv[1]) + "/wordle" + suffix + ".pbm");
      }
      int counterHeight = -1;
      for (const char* value : {"0", "9", "10", "27", "99", "100", "9999"}) {
        Canvas digits(width, height);
        const auto box = fui::makeRect(30, 100, wide ? 340 : 420, 180);
        toolsdigits::counter(digits.screen, box, value, wide ? 112 : 120);
        int top = height, bottom = 0;
        for (const auto& fill : digits.target.blackFills) {
          CHECK(fill.x >= box.x && fill.right() <= box.right());
          CHECK(fill.y >= box.y && fill.bottom() <= box.bottom());
          top = std::min(top, int(fill.y));
          bottom = std::max(bottom, int(fill.bottom()));
        }
        CHECK(bottom - top <= (wide ? 112 : 120));
        if (counterHeight < 0) counterHeight = bottom - top;
        CHECK(std::abs(bottom - top - counterHeight) <= 1);
      }
      {
        Canvas counter(width, height);
        toolsui::Model model;
        model.kind = toolsui::Kind::Counter;
        model.count = 27;
        toolsui::Work work;
        toolsui::build(counter.screen, model, work);
        counter.checkHits();
        if (argc > 1) counter.write(std::string(argv[1]) + "/counter" + suffix + ".pbm");
      }
      for (bool review : {false, true}) {
        Canvas four(width, height);
        c4ui::BoardModel model;
        model.review = review;
        model.open = 127;
        c4ui::buildBoard(four.screen, model);
        four.checkHits();
        if (argc > 1) four.write(std::string(argv[1]) + "/four-board-" + std::to_string(review) + suffix + ".pbm");
        Canvas othello(width, height);
        pocketgames::State state;
        pocketui::Model ot;
        ot.state = &state;
        ot.view = review ? pocketui::View::Review : pocketui::View::Board;
        pocketui::build(othello.screen, ot);
        othello.checkHits();
        if (argc > 1)
          othello.write(std::string(argv[1]) + "/othello-board-" + std::to_string(review) + suffix + ".pbm");
      }
      for (int outcome = 0; outcome < 3; ++outcome) {
        Canvas four(width, height);
        c4ui::ResultModel result;
        result.outcome = outcome == 0   ? connectfour::Outcome::LightWins
                         : outcome == 1 ? connectfour::Outcome::DarkWins
                                        : connectfour::Outcome::Draw;
        c4ui::buildResult(four.screen, result);
        four.checkHits();
        CHECK(four.has(c4ui::ActionAgain) && four.has(c4ui::ActionViewBoard) && four.has(c4ui::ActionDone));
        for (const auto& label : four.target.labels) {
          if (label.value != tr(STR_C4_WIN_DETAIL) && label.value != tr(STR_C4_LOSS_DETAIL) &&
              label.value != tr(STR_C4_DRAW_DETAIL))
            continue;
          int firstFrame = height;
          for (const auto& outline : four.target.outlines)
            if (outline.second == 4 && outline.first.width > safe.width / 2 && outline.first.y > label.box.y)
              firstFrame = std::min(firstFrame, int(outline.first.y));
          CHECK(firstFrame < height && label.box.bottom() + 12 <= firstFrame);
        }
        if (argc > 1) four.write(std::string(argv[1]) + "/four-result-" + std::to_string(outcome) + suffix + ".pbm");
        for (bool saveFailed : {false, true}) {
          Canvas othello(width, height);
          pocketgames::State state;
          for (int n = 0; n < 64; ++n)
            state.othello.board[n] = n < (outcome == 0 ? 46 : outcome == 1 ? 18 : 32) ? 1 : 2;
          pocketui::Model model;
          model.state = &state;
          model.view = pocketui::View::Result;
          model.saveFailed = saveFailed;
          pocketui::build(othello.screen, model);
          othello.checkHits();
          CHECK(othello.has(pocketui::New) && othello.has(pocketui::ViewBoard) && othello.has(pocketui::Menu));
          for (const auto& label : othello.target.labels) {
            char victory[96];
            pixelui::copyCaps(victory, sizeof(victory), tr(STR_OT_VICTORY));
            if (label.value != victory && label.value != tr(STR_OT_TRY_AGAIN) && label.value != tr(STR_OT_NO_WINNER) &&
                label.value != tr(STR_UI_COULD_NOT_SAVE_GAME_615E7A))
              continue;
            for (size_t n = 0; n < othello.interactions.count(); ++n)
              if (othello.interactions.data()[n].action == pocketui::New)
                CHECK(label.box.bottom() + 8 <= othello.interactions.data()[n].rect.y);
          }
          if (argc > 1)
            othello.write(std::string(argv[1]) + "/othello-result-" + std::to_string(outcome) +
                          (saveFailed ? "-error" : "") + suffix + ".pbm");
        }
      }
      {
        Canvas end(width, height);
        readerend::build(end.screen);
        const auto g = readerend::layout(end.device);
        CHECK(end.interactions.count() == 2);
        CHECK(end.has(readerend::kHome));
        CHECK(end.has(readerend::kLastPage));
        CHECK(end.label(tr(STR_END_OF_BOOK)));
        CHECK(end.label(tr(STR_EOB_HOME)));
        CHECK(g.title.bottom() + 16 <= g.home.y);
        CHECK(g.home.y >= safe.y && g.home.bottom() <= safe.bottom());
        CHECK(g.home.width >= 44 && g.home.height >= 44);
        end.checkHits();
        if (argc > 1) end.write(std::string(argv[1]) + "/reader-end" + suffix + ".pbm");
      }
      for (auto style : {homeui::Style::Carousel, homeui::Style::Framed}) {
        for (int count = 1; count <= 8; ++count) {
          const auto g = homeui::layout(probe.device, count, style);
          CHECK(g.cover.height >= (wide ? 180 : 120));
          if (wide && style == homeui::Style::Carousel) {
            CHECK(g.badge.empty() && g.previous.empty() && g.next.empty());
            CHECK(g.coverRegion.y <= g.card.y + 16);
          }
          if (wide || style == homeui::Style::Carousel) CHECK(g.title.y >= g.cover.bottom());
          CHECK(g.title.bottom() <= g.author.y);
          if (style == homeui::Style::Carousel) CHECK(g.author.bottom() + 8 <= g.progress.y);
          for (int n = 0; n < count; ++n) {
            const auto row = g.rows[n];
            CHECK(row.x >= safe.x && row.right() <= safe.right());
            CHECK(row.y >= safe.y && row.bottom() <= safe.bottom());
            CHECK(row.width >= 44 && row.height >= 44);
          }
        }
      }
      const auto grid = libraryui::gridLayout(probe.device, pixelui::geometry(probe.device, 44).body);
      CHECK(grid.capacity() == libraryui::gridCapacity(width, height));
      CHECK(grid.capacity() == (wide ? 4 : 9));
      for (int total : {0, 1, 4, 9, 10, 29, 30}) {
        std::array<bool, 30> visited{};
        const int pages = std::max(1, (total + grid.capacity() - 1) / grid.capacity());
        for (int page = 0; page < pages; ++page)
          for (int slot = 0; slot < grid.capacity() && page * grid.capacity() + slot < total; ++slot) {
            const int ordinal = page * grid.capacity() + slot;
            CHECK(!visited[ordinal]);
            visited[ordinal] = true;
            const auto card = grid.card(slot);
            CHECK(card.width >= 64 && card.height >= (wide ? 200 : 100));
            CHECK(card.x >= safe.x && card.right() <= safe.right());
            CHECK(card.y >= grid.area.y && card.bottom() <= grid.area.bottom());
            CHECK(card.bottom() + 12 <= grid.pager.y);
          }
        for (int n = 0; n < total; ++n) CHECK(visited[n]);
      }
      for (int page : {0, 1, 7}) {
        Canvas c(width, height);
        readertext::Model m;
        m.tab = 2;
        m.contentsAvailable = true;
        m.listCount = 32;
        m.listPage = page;
        m.currentChapter = 4;
        m.listLabel = [](const void*, int n, char* out, size_t size) {
          std::snprintf(out, size, "Chapter %d: The Modern Prometheus", n + 1);
        };
        readertext::Work work;
        readertext::build(c.screen, m, work);
        c.checkHits();
        int chapters = 0;
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto& hit = c.interactions.data()[n];
          if (hit.action != 6 || hit.value < readertext::ChapterPick || hit.value >= readertext::MorePick) continue;
          CHECK(hit.value == readertext::ChapterPick + page * 4 + chapters);
          CHECK(hit.rect.height >= 44);
          ++chapters;
        }
        CHECK(chapters == 4);
        CHECK(c.has(1));
        if (argc > 1 && !page) c.write(std::string(argv[1]) + "/contents" + suffix + ".pbm");
      }

      if (wide) {
        Canvas statsCanvas(width, height);
        readingstats::Summary summary;
        summary.seconds = 250000;
        summary.pages = 800;
        summary.sessions = 30;
        summary.longest = 12;
        readingui::Model model;
        model.summary = &summary;
        model.tab = 1;
        model.today = readingstats::dayIndex({2026, 10, 27});
        readingui::Work work;
        readingui::build(statsCanvas.screen, model, work);
        for (auto key : {StrId::STR_READING_CURRENT_STREAK, StrId::STR_READING_BEST_STREAK, StrId::STR_READING_DAYS}) {
          char caption[80];
          pixelui::copyCaps(caption, sizeof(caption), I18N.get(key));
          bool found = false;
          for (const auto& label : statsCanvas.target.labels) {
            if (label.value != caption) continue;
            int top = height, bottom = 0;
            for (const auto& fill : statsCanvas.target.blackFills) {
              if (fill.x < label.box.x + label.box.width / 3 || fill.right() > label.box.right() ||
                  fill.y < label.box.y - 76 || fill.bottom() > label.box.y)
                continue;
              top = std::min(top, int(fill.y));
              bottom = std::max(bottom, int(fill.bottom()));
            }
            CHECK(bottom - top >= 24);
            found = true;
          }
          CHECK(found);
        }
      }
      if (wide) {
        int firstTabY = -1;
        for (int tab = 0; tab < 4; ++tab) {
          Canvas sheet(width, height);
          readertext::Model model;
          model.tab = tab;
          model.contentsAvailable = true;
          model.family = "Noto Serif";
          readertext::Work work;
          readertext::build(sheet.screen, model, work);
          bool found = false;
          for (const auto& outline : sheet.target.outlines) {
            if (outline.second != pixelui::kBorder || outline.first.height != 44 || outline.first.width < width - 50)
              continue;
            if (firstTabY < 0) firstTabY = outline.first.y;
            CHECK(firstTabY == outline.first.y);
            found = true;
            break;
          }
          CHECK(found);
          if (argc > 1) sheet.write(std::string(argv[1]) + "/reader-tab-" + std::to_string(tab) + suffix + ".pbm");
        }
      }
      const auto chess = chessui::boardLayout(probe.device);
      int timerHeight = -1;
      for (const char* time : {"00:00", "00:11", "01:51", "11:11", "25:00", "99:59", "180:00"}) {
        Canvas digits(width, height);
        const auto box = fui::makeRect(20, 100, width - 40, 100);
        pomodoroui::drawCountdown(digits.target, box, time, wide ? 96 : 82);
        int top = height, bottom = 0;
        for (const auto& fill : digits.target.blackFills) {
          CHECK(fill.x >= box.x && fill.right() <= box.right());
          CHECK(fill.y >= box.y && fill.bottom() <= box.bottom());
          top = std::min(top, int(fill.y));
          bottom = std::max(bottom, int(fill.bottom()));
        }
        CHECK(bottom - top >= (wide ? 94 : 80));
        if (timerHeight < 0) timerHeight = bottom - top;
        CHECK(bottom - top == timerHeight);
      }
      for (uint32_t seconds : {0u, 11u, 111u, 671u, 1500u, 5999u}) {
        for (bool saveFailed : {false, true}) {
          Canvas clock(width, height);
          pomodoro::State state;
          state.remainingMs = seconds * 1000;
          pomodoroui::Work work;
          pomodoroui::build(clock.screen, {&state, pomodoroui::View::Timer, saveFailed}, work);
          char notice[96];
          pixelui::copyCaps(notice, sizeof(notice), tr(STR_UI_READY_WHEN_YOU_ARE_31EF54));
          for (const auto& label : clock.target.labels) {
            if (label.value != notice) continue;
            for (const auto& fill : clock.target.blackFills)
              CHECK(fill.right() <= label.box.x || fill.x >= label.box.right() || fill.bottom() <= label.box.y ||
                    fill.y >= label.box.bottom());
          }
          clock.checkHits();
          if (argc > 1 && seconds == 1500)
            clock.write(std::string(argv[1]) + "/pomodoro" + (saveFailed ? "-error" : "") + suffix + ".pbm");
        }
      }
      {
        Canvas timer(width, height);
        pomodoro::State state;
        state.config.rounds = 4;
        pomodoroui::Model model;
        model.state = &state;
        model.stagePage = 2;
        pomodoroui::Work work;
        pomodoroui::build(timer.screen, model, work);
        timer.checkHits();
        CHECK(timer.hits(pomodoroui::SelectStage) == 2);
        if (argc > 1) timer.write(std::string(argv[1]) + "/pomodoro-last-page" + suffix + ".pbm");
      }
      if (wide) {
        Canvas cards(width, height);
        pixelui::begin(cards.screen);
        const auto body = cards.screen.body();
        const auto lesson = fui::makeRect(body.x, body.y + 54, body.width, body.height - 118);
        cardpanel::deckLesson(cards.screen, lesson, true);
        bool found = false;
        for (const auto& label : cards.target.labels)
          if (label.value == tr(STR_CARDS_GUIDE_ACE_HIGH)) {
            CHECK(label.box.y >= lesson.y && label.box.bottom() <= lesson.bottom());
            found = true;
          }
        CHECK(found);
        if (argc > 1) cards.write(std::string(argv[1]) + "/card-lesson" + suffix + ".pbm");
      }
      CHECK(chess.losses.bottom() <= safe.bottom());
      if (wide) {
        CHECK(std::abs((chess.status.y + chess.losses.bottom()) - (chess.board.y + chess.board.bottom())) <= 2);
      }
      for (int count : {0, 1, 4, 9, 30}) {
        for (int page : {0, 1, 3, 7}) {
          const int resized = wallpapersui::pageAfterResize(page, wide ? 9 : 4, wide ? 4 : 9, count);
          const int first = count ? std::min(page * (wide ? 9 : 4), count - 1) : 0;
          CHECK(resized == first / (wide ? 4 : 9));
        }
      }
      const auto wallpaper = wallpapersui::gridGeom(probe.device);
      CHECK(wallpaper.perPage == (wide ? 4 : 9));
      for (const auto size : {std::pair{480, 800}, std::pair{800, 480}, std::pair{200, 200}, std::pair{1600, 900},
                              std::pair{1, 1000}, std::pair{1000, 1}, std::pair{1, 1}}) {
        const auto fitted = wallpapersui::previewRect(width, height, size.first, size.second);
        CHECK(fitted.width > 0 && fitted.height > 0);
        CHECK(fitted.x >= 0 && fitted.y >= 0 && fitted.right() <= width && fitted.bottom() <= height);
        CHECK(std::abs(fitted.x - (width - fitted.right())) <= 1);
        CHECK(std::abs(fitted.y - (height - fitted.bottom())) <= 1);
        CHECK(fitted.width <= size.first && fitted.height <= size.second);
        CHECK(std::abs(fitted.width * size.second - fitted.height * size.first) <= size.first + size.second);
      }
      CHECK(wallpapersui::previewRect(width, height, 0, 800).empty());
      for (int state = 0; state < 4; ++state) {
        Canvas wall(width, height);
        if (state == 0) {
          wallpapersui::NoticeModel model;
          model.headline = tr(STR_UI_CANNOT_PREVIEW_215D84);
          model.body = tr(STR_UI_THIS_FILE_COULD_NOT_BE_OPENED_495DB9);
          model.actionLabel = tr(STR_DONE);
          model.action = wallpapersui::ActionDismiss;
          wallpapersui::buildNotice(wall.screen, model);
        } else if (state == 1) {
          wallpapersui::SheetModel model;
          model.name = "Botanical wallpaper";
          model.detail = "480 x 800";
          wallpapersui::buildSheet(wall.screen, model);
        } else if (state == 2) {
          wallpapersui::ConfirmModel model;
          model.name = "Botanical wallpaper";
          model.consequence = tr(STR_WORDS_DELETE_HELP);
          wallpapersui::buildConfirm(wall.screen, model);
        } else {
          wallpapersui::EmptyModel model;
          model.title = tr(STR_WALLPAPERS_TITLE);
          wallpapersui::buildEmpty(wall.screen, model);
        }
        wall.checkHits();
        if (argc > 1) wall.write(std::string(argv[1]) + "/wallpaper-" + std::to_string(state) + suffix + ".pbm");
      }
      for (int n = 0; n < wallpaper.perPage; ++n) {
        const auto card = wallpapersui::thumbRect(wallpaper, n);
        CHECK(card.width >= 48 && card.height >= (wide ? 160 : 100));
        CHECK(card.x >= safe.x && card.right() <= safe.right());
        CHECK(card.bottom() + 12 <= wallpaper.pageDotsY);
        CHECK(wallpapersui::cellAt(wallpaper, card.x + card.width / 2, card.y + card.height / 2) == n);
      }
      if (wide) {
        for (int subHeader : {40, 56, 64}) {
          const auto body = networkui::landscapeBody(probe.device, subHeader);
          for (bool hotspot : {false, true}) {
            Canvas network(width, height);
            const auto transfer = networkui::transferLayout(network.device, subHeader, hotspot);
            for (const auto box : {transfer.firstHeading, transfer.firstQr, transfer.firstAddress,
                                   transfer.secondHeading, transfer.secondQr, transfer.url, transfer.alternate}) {
              if (box.empty()) continue;
              CHECK(box.x >= body.x && box.right() <= body.right());
              CHECK(box.y >= body.y && box.bottom() <= body.bottom());
            }
            CHECK(transfer.firstQr.width >= 160 && transfer.firstQr.height == transfer.firstQr.width);
            if (hotspot) CHECK(transfer.secondQr.x >= transfer.firstQr.right() + 24);
            networkui::transferText(network.target, transfer, hotspot, "CrossNext", "http://crossnext-abcdef.local/",
                                    "http://192.168.100.100/");
            if (argc > 1 && subHeader == 56)
              network.write(std::string(argv[1]) + (hotspot ? "/hotspot" : "/file-transfer") + suffix + ".pbm");
          }
          Canvas calibre(width, height);
          const auto layout = networkui::calibreLayout(calibre.device, subHeader);
          for (const auto box : {layout.title, layout.status, layout.receiving, layout.progress, layout.complete}) {
            CHECK(box.x >= body.x && box.right() <= body.right());
            CHECK(box.y >= body.y && box.bottom() <= body.bottom());
          }
          networkui::calibreText(calibre.target, layout, true, "The Adventures of Sherlock Holmes.epub",
                                 "Frankenstein.epub");
          if (argc > 1 && subHeader == 56) calibre.write(std::string(argv[1]) + "/calibre" + suffix + ".pbm");
        }
        Canvas c(width, height);
        wordsui::Model m;
        m.view = wordsui::View::Delete;
        m.term = "curious";
        wordsui::Geometry g;
        pagedtext::Work work;
        wordsui::build(c.screen, m, g, work);
        c.checkHits();
        bool found = false;
        for (const auto& label : c.target.labels)
          if (label.value == tr(STR_WORDS_DELETE_HELP)) {
            CHECK(label.box.bottom() + 12 <= g.buttons[wordsui::ConfirmDelete].y);
            found = true;
          }
        CHECK(found);
        if (argc > 1) c.write(std::string(argv[1]) + "/word-delete" + suffix + ".pbm");
      }
    }
  }
  std::printf("Landscape layouts: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

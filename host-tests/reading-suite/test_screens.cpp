#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/chess/puzzles/ChessPuzzleScreens.h"
#include "../../src/apps_local/game2048/Game2048Screens.h"
#include "../../src/apps_local/pocketgames/PocketGamesScreens.h"
#include "../../src/apps_local/pomodoro/PomodoroScreens.h"
#include "../../src/apps_local/readertext/ReaderTextSheet.h"
#include "../../src/apps_local/reading/ReadingStatsScreens.h"
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
      CHECK(r.width >= (dateCell ? 24 : 44) && r.height >= (dateCell ? 24 : 40));
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
  readingstats::Summary summary;
  const int today = readingstats::dayIndex({2026, 10, 15});
  summary.anchorDay = today;
  readingstats::Book books[6]{};
  for (int n = 0; n < 6; ++n) {
    auto& b = books[n];
    readingstats::copyText(b.title, sizeof(b.title), n == 0 ? "ALICE'S ADVENTURES IN WONDERLAND" : "THE HOBBIT");
    readingstats::copyText(b.author, sizeof(b.author), "Lewis Carroll");
    b.seconds = 27720;
    b.sessions = 12;
    b.pages = 200;
    b.progressBp = 10000;
    b.completed = true;
    b.firstEpoch = 1789260000;
    b.finishedEpoch = 1790030000;
    b.readingDays = 8;
    b.buckets[3] = 22000;
  }
  for (int d = today - 120; d <= today; ++d) {
    if (d % 5 == 0) continue;
    readingstats::Record record;
    record.seconds = (d % 4 + 1) * 900;
    record.pages = 20;
    record.localDay = d;
    record.secondOfDay = 19 * 3600;
    record.firstCompletion = d % 19 == 0;
    summary.add(record);
  }
  game2048::State state;
  game2048::start(state, 42);
  for (int n = 0; n < 16; ++n) state.cells[n] = n % 12;
  state.score = 6720;
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    const int w = size.first, h = size.second;
    readingui::Model m;
    m.summary = &summary;
    m.books = books;
    m.today = today;
    m.month = 2026 * 12 + 9;
    m.bookTotal = 6;
    m.battery = 100;
    for (int tab = 0; tab < 4; ++tab) {
      Canvas c(w, h);
      m.tab = tab;
      m.pageSize = readingui::capacity(c.device);
      CHECK(m.pageSize == (w > h ? 3 : 5));
      m.count = m.pageSize;
      readingui::Work work;
      readingui::build(c.screen, m, work);
      c.checkHits();
      CHECK(c.has(readingui::Tab, tab));
      if (tab == 3) CHECK(c.has(readingui::OpenBook, m.count - 1));
      if (argc > 1) c.write(std::string(argv[1]) + "/stats-" + std::to_string(tab) + "-" + std::to_string(w) + ".pbm");
    }
    for (auto filter : {readingstats::BookFilter::Recent, readingstats::BookFilter::Finished}) {
      Canvas c(w, h);
      m.tab = 3;
      m.page = 1;
      m.count = 1;
      m.books = books + 5;
      m.filter = filter;
      readingui::Work work;
      readingui::build(c.screen, m, work);
      c.checkHits();
      CHECK(c.has(readingui::OpenBook, 0));
      CHECK(!c.has(readingui::OpenBook, 1));
    }
    m.page = 0;
    m.books = books;
    m.count = m.pageSize;
    m.filter = readingstats::BookFilter::Finished;
    for (auto period : {readingstats::Period::Day, readingstats::Period::Week, readingstats::Period::Month}) {
      readingstats::PeriodStats stats;
      readingstats::setPeriod(stats, period, today, false);
      stats.seconds = 2340;
      stats.pages = 16;
      stats.sessions = 2;
      stats.hours[2] = 1320;
      stats.hours[5] = 1020;
      m.period = period;
      m.periodStats = &stats;
      m.tab = 2;
      Canvas c(w, h);
      readingui::Work work;
      readingui::build(c.screen, m, work);
      c.checkHits();
      CHECK(c.has(readingui::BackToCalendar));
      CHECK(c.has(readingui::PeriodPage, -1) && c.has(readingui::PeriodPage, 1));
      if (argc > 1)
        c.write(std::string(argv[1]) + "/period-" + std::to_string(int(period)) + "-" + std::to_string(w) + ".pbm");
    }
    m.period = readingstats::Period::Calendar;
    m.periodStats = nullptr;
    {
      m.tab = 2;
      Canvas c(w, h);
      readingui::Work work;
      readingui::build(c.screen, m, work);
      c.checkHits();
      // Exact drawn-cell geometry, both week starts and all legal dates including blanks.
      for (bool sunday : {false, true}) {
        m.weekStartsSunday = sunday;
        readingui::build(c.screen, m, work);
        const auto area = work.calendarGrid;
        const int first = readingstats::dayIndex({2026, 10, 1});
        const int offset = (readingstats::weekday(first) + (sunday ? 1 : 0)) % 7;
        for (int d = 1; d <= 31; ++d) {
          const int n = 7 + offset + d - 1;
          const int x = area.x + ((n % 7) * 2 + 1) * area.width / 14;
          const int y = area.y + ((n / 7) * 2 + 1) * area.height / 14;
          CHECK(readingui::calendarDayAt(work, m.month, sunday, x, y) == first + d - 1);
        }
        CHECK(readingui::calendarDayAt(work, m.month, sunday, -1, -1) == 0);
      }
      m.weekStartsSunday = false;
    }
    {
      Canvas c(w, h);
      m.detail = books;
      readingui::Work work;
      readingui::build(c.screen, m, work);
      c.checkHits();
      m.damaged = true;
      readingui::build(c.screen, m, work);
      c.checkHits();
      m.damaged = false;
      if (argc > 1) c.write(std::string(argv[1]) + "/book-" + std::to_string(w) + ".pbm");
      m.detail = nullptr;
    }
    for (int state = 0; state < 4; ++state) {
      Canvas c(w, h);
      readingui::Work work;
      m.detail = nullptr;
      m.tab = 0;
      m.loading = state == 1;
      m.failed = state == 2;
      m.damaged = state == 3;
      readingui::build(c.screen, m, work);
      c.checkHits();
    }
    m.loading = false;
    m.failed = false;
    m.damaged = false;
    for (auto view :
         {game2048ui::View::Menu, game2048ui::View::Board, game2048ui::View::Help, game2048ui::View::Confirm}) {
      Canvas c(w, h);
      game2048ui::Work work;
      game2048ui::build(c.screen, {&state, 18000, view, false, 0}, work);
      c.checkHits();
      if (view == game2048ui::View::Board)
        for (int n = 0; n < 4; ++n) CHECK(c.has(game2048ui::Move, n));
      if (argc > 1)
        c.write(std::string(argv[1]) + "/2048-" + std::to_string(int(view)) + "-" + std::to_string(w) + ".pbm");
    }
  }
  for (auto dimensions : {std::pair<int, int>{480, 800}, {800, 480}}) {
    chesspuzzle::Session session;
    chesspuzzle::Progress progress;
    chesspuzzle::load(session, 0);
    for (auto view : {chesspuzzleui::View::Difficulty, chesspuzzleui::View::Board, chesspuzzleui::View::Promotion,
                      chesspuzzleui::View::ConfirmReset, chesspuzzleui::View::Celebration}) {
      Canvas c(dimensions.first, dimensions.second);
      chesspuzzleui::Work work;
      chesspuzzleui::build(c.screen, {&session, &progress, view, 0, 0, false}, work);
      c.checkHits();
      if (view == chesspuzzleui::View::Difficulty) {
        for (int n = 0; n < 3; ++n) CHECK(c.has(chesspuzzleui::Difficulty, n));
      } else if (view == chesspuzzleui::View::Board) {
        CHECK(c.has(chesspuzzleui::Restart, 0));
        CHECK(c.has(chesspuzzleui::Next, 0));
        CHECK(c.has(chesspuzzleui::Previous, 0));
        const auto r = chesspuzzleui::boardRect(c.device);
        CHECK(chesspuzzleui::squareAt(c.device, true, r.x, r.y) == 56);
        CHECK(chesspuzzleui::squareAt(c.device, false, r.x, r.y) == 7);
        CHECK(chesspuzzleui::squareAt(c.device, true, r.right(), r.y) == -1);
        for (int square = 0; square < 64; ++square) {
          const int x = r.x + (square % 8) * r.width / 8 + r.width / 16;
          const int y = r.y + (7 - square / 8) * r.width / 8 + r.width / 16;
          CHECK(chesspuzzleui::squareAt(c.device, true, x, y) == square);
          CHECK(chesspuzzleui::squareAt(c.device, false, x, y) == 63 - square);
        }
      }
      if (argc > 1)
        c.write(std::string(argv[1]) + "/puzzles-" + std::to_string(int(view)) + "-" +
                std::to_string(dimensions.first) + ".pbm");
    }
    for (auto view : {chesspuzzleui::View::Difficulty, chesspuzzleui::View::Board, chesspuzzleui::View::Celebration}) {
      Canvas normal(dimensions.first, dimensions.second), invalid(dimensions.first, dimensions.second);
      chesspuzzleui::Work work;
      chesspuzzleui::build(normal.screen, {&session, &progress, view, 0, 0, false, false}, work);
      chesspuzzleui::build(invalid.screen, {&session, &progress, view, 0, 0, false, true}, work);
      CHECK(normal.pixels != invalid.pixels);
      invalid.checkHits();
    }
    for (int index = 0; index < chesspuzzle::kCount; ++index) chesspuzzle::markSolved(progress, index);
    Canvas complete(dimensions.first, dimensions.second);
    chesspuzzleui::Work work;
    chesspuzzleui::build(complete.screen, {&session, &progress, chesspuzzleui::View::Celebration, 2, 999, false}, work);
    complete.checkHits();
    CHECK(complete.has(chesspuzzleui::Choose));
    if (argc > 1) complete.write(std::string(argv[1]) + "/all-puzzles-" + std::to_string(dimensions.first) + ".pbm");
  }
  for (auto dimensions : {std::pair<int, int>{480, 800}, {800, 480}}) {
    const int w = dimensions.first, h = dimensions.second;
    {
      pocketgames::State state;
      pocketgames::start(state, pocketgames::Kind::Othello, 8213, 1);
      pocketgames::play(state.othello, 19);
      pocketgames::bot(state.othello);
      for (auto view : {pocketui::View::Board, pocketui::View::Menu, pocketui::View::Help, pocketui::View::Level,
                        pocketui::View::ConfirmNew}) {
        if (view == pocketui::View::Level) continue;
        Canvas c(w, h);
        pocketui::build(c.screen, {&state, view, false, false, -1});
        c.checkHits();
        if (view == pocketui::View::Board) {
          const auto board = pocketui::boardRect(c.device);
          CHECK(board.x >= 0 && board.y >= 0 && board.right() <= w && board.bottom() <= h);
          for (int n = 0; n < 64; ++n)
            CHECK(pocketui::squareAt(c.device, 8, board.x + (n % 8) * board.width / 8 + board.width / 16,
                                     board.y + (n / 8) * board.width / 8 + board.width / 16) == n);
        }
        if (argc > 1)
          c.write(std::string(argv[1]) + "/pocket-" + std::string("othello") + "-" + std::to_string(int(view)) + "-" +
                  std::to_string(w) + ".pbm");
      }
    }
    for (int lights = 0; lights < 3; ++lights) {
      Canvas c(w, h);
      quickui::Work work;
      quickui::Model model;
      model.light = lights > 0;
      model.temperature = lights == 2;
      quickui::build(c.screen, model, work);
      c.checkHits();
      if (argc > 1)
        c.write(std::string(argv[1]) + "/quick-" + std::to_string(lights) + "-" + std::to_string(w) + ".pbm");
    }
  }
  char time[32];
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    for (auto view : {pomodoroui::View::Timer, pomodoroui::View::Settings, pomodoroui::View::ConfirmReset}) {
      for (bool error : {false, true}) {
        Canvas c(size.first, size.second);
        pomodoro::State state;
        pomodoroui::Work work;
        HeaderBackTapTarget::clear();
        pomodoroui::build(c.screen, {&state, view, error}, work);
        const auto back = pixelui::geometry(c.device).back;
        CHECK(HeaderBackTapTarget::contains(back.x + back.width / 2, back.y + back.height / 2));
        c.checkHits();
        if (view == pomodoroui::View::Timer) {
          CHECK(c.has(pomodoroui::Toggle));
          CHECK(c.has(pomodoroui::Settings));
        }
        if (argc > 1)
          c.write(std::string(argv[1]) + "/pomodoro-" + std::to_string(int(view)) + "-" + std::to_string(size.first) +
                  (error ? "-error.pbm" : ".pbm"));
      }
    }
    for (bool fixed : {false, true})
      for (int tab = fixed ? 1 : 0; tab < 4; ++tab) {
        Canvas c(size.first, size.second);
        fui::TextStyle text;
        text.font = pixelui::kSmall;
        text.maxLines = 1;
        const char* lines[] = {"ALICE'S ADVENTURES IN WONDERLAND",       "Alice was beginning to get very tired",
                               "of sitting by her sister on the bank.",  "She had peeped into the book her sister",
                               "was reading. What is the use of a book", "without pictures or conversation?"};
        for (int line = 0; line < 6; ++line)
          c.target.text(fui::makeRect(20, 20 + line * 36, size.first - 40, 32), lines[line], text);
        const auto before = c.pixels;
        readertext::Model model;
        readertext::Work work;
        model.fixed = fixed;
        model.contentsAvailable = true;
        model.listCount = 4;
        model.listLabel = [](const void*, int, char* out, size_t size) {
          std::snprintf(out, size, "CHAPTER / MORE OPTION");
        };
        model.tab = tab;
        model.family = "NOTO SERIF";
        model.alignment = "JUSTIFY";
        model.spacingLabel = "NORMAL";
        model.placement = "BOTTOM";
        model.refresh = "NEVER";
        readertext::build(c.screen, model, work);
        c.checkHits();
        const auto rect = readertext::sheetRect(c.device, fixed);
        CHECK(rect.y > size.second / 5 && rect.bottom() <= size.second);
        for (int row = 0; row < rect.y; ++row)
          for (int byte = 0; byte < (size.first + 7) / 8; ++byte)
            CHECK(c.pixels[row * ((size.first + 7) / 8) + byte] == before[row * ((size.first + 7) / 8) + byte]);
        CHECK(c.has(1));
        CHECK(c.has(2, tab));
        if (!fixed && tab == 0) {
          CHECK(c.has(6, readertext::SizeUp));
          CHECK(c.has(6, readertext::WeightUp));
          CHECK(!c.has(6, readertext::WeightDown));
          Canvas bold(size.first, size.second);
          model.bold = true;
          readertext::build(bold.screen, model, work);
          bold.checkHits();
          CHECK(bold.has(6, readertext::WeightDown));
          CHECK(!bold.has(6, readertext::WeightUp));
        }
        if (argc > 1)
          c.write(std::string(argv[1]) + "/reader-sheet-" + (fixed ? "xtc-" : "epub-") + std::to_string(tab) + "-" +
                  std::to_string(size.first) + ".pbm");
      }
  }
  for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    Canvas c(size.first, size.second);
    readertext::Model model;
    readertext::Work work;
    model.fixed = true;
    model.statusAvailable = false;
    model.tab = 3;
    model.listCount = 2;
    model.refresh = "NEVER";
    readertext::build(c.screen, model, work);
    c.checkHits();
    CHECK(c.has(2, 3));
    CHECK(!c.has(2, 1));
    CHECK(!c.has(6, readertext::Placement));
    CHECK(!c.has(6, readertext::Percentage));
    CHECK(c.has(6, readertext::MorePick));
    CHECK(c.has(6, readertext::MorePick + 1));
  }
  for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    for (int page : {0, 1, 31}) {
      Canvas c(size.first, size.second);
      readertext::Model model;
      readertext::Work work;
      model.view = readertext::SheetView::Families;
      model.fontPage = page;
      model.fontCount = 130;
      model.fontLabel = [](const void*, int index) -> const char* { return index == 2 ? "BITTER" : "LEXEND DECA"; };
      readertext::build(c.screen, model, work);
      c.checkHits();
      CHECK(c.has(6, readertext::FontBack));
      const int first = page * readertext::fontRowsPerPage(c.device);
      if (first < 130) CHECK(c.has(6, readertext::FontPick + first));
      CHECK(!c.has(2));
      if (argc > 1 && !page) c.write(std::string(argv[1]) + "/font-families-" + std::to_string(size.first) + ".pbm");
    }
  }
  for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    readertext::Model model;
    readertext::Work work;
    model.tab = 3;
    model.listCount = 2;
    model.listLabel = [](const void*, int n, char* out, size_t size) {
      std::snprintf(out, size, "%s", n ? "REFRESH" : "NIGHT MODE");
    };
    bool enabled = false;
    model.listContext = &enabled;
    model.listCheck = [](const void* context, int n) { return n == 0 ? int(*static_cast<const bool*>(context)) : -1; };
    model.listValue = [](const void* context, int n, char* out, size_t size) {
      if (n == 1) std::snprintf(out, size, "%s", *static_cast<const bool*>(context) ? "NEVER" : "15 PAGES");
    };
    Canvas off(size.first, size.second);
    readertext::build(off.screen, model, work);
    off.checkHits();
    enabled = true;
    Canvas on(size.first, size.second);
    readertext::build(on.screen, model, work);
    on.checkHits();
    CHECK(on.has(6, readertext::MorePick) && on.has(6, readertext::MorePick + 1));
    CHECK(off.has(6, readertext::MorePick) && off.has(6, readertext::MorePick + 1));
    CHECK(on.pixels != off.pixels);
    CHECK(std::string(work.value) == "NEVER");
    if (argc > 1) {
      off.write(std::string(argv[1]) + "/reader-more-off-" + std::to_string(size.first) + ".pbm");
      on.write(std::string(argv[1]) + "/reader-more-on-" + std::to_string(size.first) + ".pbm");
    }
  }
  readingui::duration(3660, time, sizeof(time));
  CHECK(std::string(time) == "1h 01m");
  I18N.setLanguage(Language::ES);
  readingui::duration(120, time, sizeof(time));
  CHECK(std::string(time) == "2m");
  I18N.setLanguage(Language::EN);
  std::printf("reading suite UI: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

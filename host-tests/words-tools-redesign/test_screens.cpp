#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/reading/ReadingStatsCore.h"
#include "../../src/apps_local/study/StudyScheduler.h"
#include "../../src/apps_local/tools/CounterInput.h"
#include "../../src/apps_local/tools/ToolState.h"
#include "../../src/apps_local/tools/ToolsScreens.h"
#include "../../src/apps_local/wallpapers/WallpapersScreens.h"
#include "../../src/apps_local/words/WordsScreens.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/reading_serif_14.h"
#include "../../src/apps_local/ui/fonts/reading_serif_bold_16.h"
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

fui::DeviceContext makeDevice(fui::DisplayTarget& target, int topInset = 9) {
  auto d = target.deviceContext();
  d.hasTouch = true;
  d.safeArea = {static_cast<int16_t>(topInset), 7, 3, 7};
  return d;
}
struct RecordingTarget : fui::DrawTarget {
  fui::DisplayTarget backend;
  RecordingTarget(uint8_t* data, int w, int h, int stride, fui::Orientation orientation)
      : backend(data, w, h, stride, orientation) {}
  void setFont(fui::FontId id, const fui::BitmapFont& font) { backend.setFont(id, font); }
  mutable size_t measurements = 0;
  fui::Size measureText(fui::FontId id, const char* text, fui::TextStyle style) const override {
    ++measurements;
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
    bool bold, italic;
  };
  std::vector<Label> labels;
  std::vector<fui::Rect> blackFills;
  void text(fui::Rect box, const char* value, fui::TextStyle style) override {
    fui::layoutText(backend, box, value, style, [&](const char* run, fui::Rect line) {
      if (line.bottom() > box.bottom() || std::strstr(run, "…") || std::strstr(run, "..."))
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
      CHECK(!std::strstr(run, "…") && !std::strstr(run, "..."));
    });
    labels.push_back({value ? value : "", style.font, box, style.bold, style.italic});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, library{ui_prose_10},
      prose{ui_prose_14}, reading{reading_serif_14}, readingBold{reading_serif_bold_16};
  RecordingTarget target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;

  Canvas(int w, int h, int topInset = 9)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(makeDevice(target.backend, topInset)),
        frame(target, device, input, interactions, nullptr, {100, false, true}),
        screen(frame) {
    target.setFont(3, tiny.font);
    target.setFont(4, library.font);
    target.setFont(0, small.font);
    target.setFont(1, body.font);
    target.setFont(2, title.font);
    target.setFont(toybox::kSmallFont, small.font);
    target.setFont(toybox::kUiFont, body.font);
    target.setFont(toybox::kDisplayFont, title.font);
  }

  void checkHits() {
    CHECK(!interactions.overflowed());
    paintclock::notePainted();
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& slot = interactions.data()[n];
      if (slot.action == fui::NO_ACTION && fui::acceptsInput(slot.inputMask, fui::InputCoordinates)) continue;
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
  tools::CounterInput pending;
  for (int n = 0; n < 10000; ++n) CHECK(pending.push(tools::CounterInput::Kind::Add));
  tools::CounterInput::Event queued;
  int delivered = 0;
  while (pending.pop(queued)) {
    CHECK(queued.kind == tools::CounterInput::Kind::Add);
    ++delivered;
  }
  CHECK(delivered == 10000);
  for (int initial = 0; initial < 4; ++initial) {
    const int rotated = tools::toggleOrientation(initial, initial);
    CHECK((rotated % 2) != (initial % 2));
    CHECK(tools::toggleOrientation(rotated, initial) == initial);
  }

  CHECK(wordsui::reviewRating(0) == int(study::Rating::Again));
  CHECK(wordsui::reviewRating(1) == int(study::Rating::Hard));
  {
    Canvas c(480, 800);
    wordsui::Model m;
    wordsui::Geometry g;
    pagedtext::Work work;
    m.view = wordsui::View::Review;
    m.answer = true;
    m.meaning = "sense one\nsense two\nsense three\nsense four";
    m.text = m.meaning;
    wordsui::build(c.screen, m, g, work);
    CHECK(c.label("sense four"));
  }
  {
    Canvas c(480, 800);
    wordsui::Model m;
    wordsui::Geometry g;
    pagedtext::Work work;
    m.view = wordsui::View::Review;
    m.readerContent = true;
    m.text = "reader question";
    m.term = "reader question";
    wordsui::build(c.screen, m, g, work);
    CHECK(c.label("reader question"));
  }
  {
    std::string meaning;
    for (int n = 0; n < 90; ++n) meaning += "A long definition keeps all its words. ";
    wordsui::Model m;
    m.view = wordsui::View::Detail;
    m.term = "A long selected expression with several words";
    m.meaning = meaning.c_str();
    m.sentence = "This original sentence remains available after every definition page.";
    m.book = "final-book-token";
    int pages = 1;
    bool last = false;
    for (int page = 0; page < pages; ++page) {
      Canvas c(480, 800);
      c.target.setFont(4, c.reading.font);
      c.target.setFont(2, c.readingBold.font);
      c.target.setFont(3, c.library.font);
      wordsui::Geometry g;
      pagedtext::Work work;
      m.textPage = page;
      wordsui::build(c.screen, m, g, work);
      pages = m.textPages;
      CHECK(pages > 1);
      CHECK(page == 0 || !g.before.empty());
      last |= c.label("final-book-token");
      c.checkHits();
    }
    CHECK(last);
  }
  {
    wordsui::Model cursor;
    cursor.view = wordsui::View::WordList;
    cursor.capacity = 10;
    cursor.page = 1;
    cursor.focus = 9;
    wordsui::setListCapacity(cursor, 4);
    CHECK(cursor.page == 4 && cursor.focus == 3 && cursor.capacity == 4);
    wordsui::setListCapacity(cursor, 10);
    CHECK(cursor.page == 1 && cursor.focus == 9 && cursor.capacity == 10);
    wordsui::setListCapacity(cursor, 10);
    CHECK(cursor.page == 1 && cursor.focus == 9);
  }

  {
    Canvas c(480, 800);
    std::string definition;
    for (int n = 0; n < 300; ++n) definition += "Complete definition with all its words. ";
    fui::TextStyle style;
    style.font = 4;
    const wordsui::Paragraph parts[] = {
        {"Term", style, "Word:"}, {definition.c_str(), style, "Definition:"}, {"FINAL EXAMPLE", style, "Example:"}};
    wordsui::ParagraphPages cache;
    pagedtext::Work work;
    const auto area = fui::makeRect(20, 100, 440, 90);
    const int pages = wordsui::paragraphs(c.target, area, parts, 3, -1, work, 12, nullptr, &cache);
    CHECK(pages > 64);
    for (int page : {0, pages / 2, pages - 1}) {
      c.target.labels.clear();
      c.target.measurements = 0;
      CHECK(wordsui::paragraphs(c.target, area, parts, 3, page, work, 12, nullptr, &cache) == pages);
      const auto cached = c.target.labels;
      const auto reads = c.target.measurements;
      c.target.labels.clear();
      c.target.measurements = 0;
      CHECK(wordsui::paragraphs(c.target, area, parts, 3, page, work) == pages);
      CHECK(cached.size() == c.target.labels.size());
      for (size_t n = 0; n < cached.size(); ++n)
        CHECK(cached[n].value == c.target.labels[n].value && cached[n].box.y == c.target.labels[n].box.y);
      CHECK(reads < c.target.measurements);
      std::printf("Word page %d: cached %zu measurements, uncached %zu\n", page, reads, c.target.measurements);
    }
    cache.invalidate();
    CHECK(!cache.pages);
  }

  {
    Canvas c(480, 800);
    pagedtext::Work cached;
    std::string text(9000, 'W');
    text += "END";
    const auto area = fui::makeRect(20, 100, 100, 24);
    const int pages = pagedtext::draw(c.target, area, text.c_str(), 4, -1, cached);
    CHECK(pages > 512);
    for (int page : {0, 511, pages - 1}) {
      c.target.labels.clear();
      c.target.measurements = 0;
      CHECK(pagedtext::draw(c.target, area, text.c_str(), 4, page, cached) == pages);
      const auto lines = c.target.labels;
      c.target.labels.clear();
      pagedtext::Work fresh;
      CHECK(pagedtext::draw(c.target, area, text.c_str(), 4, page, fresh) == pages);
      CHECK(lines.size() == c.target.labels.size());
      for (size_t n = 0; n < lines.size(); ++n) CHECK(lines[n].value == c.target.labels[n].value);
    }
    text[0] = 'A';
    c.target.labels.clear();
    pagedtext::draw(c.target, area, text.c_str(), 4, 0, cached);
    CHECK(!c.target.labels.empty() && c.target.labels[0].value[0] == 'A');
  }
  tools::State old;
  old.count = 123;
  old.orientation = 2;
  uint8_t wire[tools::kStateBytes];
  tools::encode(old, wire);
  tools::State loaded;
  CHECK(tools::decode(wire, loaded) && loaded.count == 123 && !loaded.twelveHour);
  old.twelveHour = true;
  tools::encode(old, wire);
  CHECK(tools::decode(wire, loaded) && loaded.twelveHour);
  wire[18] ^= 1;
  CHECK(!tools::decode(wire, loaded));
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (auto dimensions : {std::pair<int, int>{480, 800}, {800, 480}}) {
      const int w = dimensions.first, h = dimensions.second;
      auto save = [&](Canvas& c, const char* name, int n = 0) {
        c.checkHits();
        if (argc > 1) {
          char path[512];
          std::snprintf(path, sizeof(path), "%s/%s-%dx%d-%s-%d.pbm", argv[1], lang == Language::ES ? "es" : "en", w, h,
                        name, n);
          c.write(path);
        }
      };
      for (auto kind : {toolsui::Kind::Clock, toolsui::Kind::Calendar, toolsui::Kind::Counter}) {
        for (int variant = 0; variant < 3; ++variant) {
          Canvas c(w, h);
          toolsui::Model m;
          toolsui::Work work;
          m.kind = kind;
          m.year = 2026;
          m.month = 10;
          m.day = 18;
          m.hour = 14;
          m.minute = 9;
          m.validTime = true;
          m.today = m.selected = readingstats::dayIndex({2026, 10, 18});
          m.analog = variant == 1;
          m.twelveHour = variant == 2;
          m.count = variant == 2 ? 999999999 : variant;
          m.sunday = variant == 2;
          toolsui::build(c.screen, m, work);
          if (kind == toolsui::Kind::Calendar) {
            CHECK(c.hits(toolsui::SelectDay) == 1);
            CHECK(c.has(toolsui::Rotate));
          }
          if (kind == toolsui::Kind::Clock) {
            CHECK(c.has(toolsui::Appearance));
            CHECK(c.has(toolsui::Rotate));
          }
          save(c,
               kind == toolsui::Kind::Clock      ? "clock"
               : kind == toolsui::Kind::Calendar ? "calendar"
                                                 : "counter",
               variant);
        }
      }
      for (int month = 1; month <= 12; ++month) {
        Canvas c(w, h);
        toolsui::Model m;
        toolsui::Work work;
        m.month = month;
        m.selected = m.today = readingstats::dayIndex({2026, month, 18});
        toolsui::build(c.screen, m, work);
        CHECK(c.hits(toolsui::SelectDay) == 1);
        c.checkHits();
        for (const auto& label : c.target.labels) {
          char* end = nullptr;
          const int day = int(std::strtol(label.value.c_str(), &end, 10));
          if (!end || *end || day < 1 || day > 31) continue;
          const auto box = label.box;
          CHECK(toolsui::calendarDayAt(c.device, m, box.x + box.width / 2, box.y + box.height / 2) == day);
        }
      }
      for (auto view : {wordsui::View::Home, wordsui::View::List, wordsui::View::WordList, wordsui::View::Review,
                        wordsui::View::Guide, wordsui::View::Detail, wordsui::View::Waiting, wordsui::View::Delete}) {
        for (int variant = 0; variant < 3; ++variant) {
          Canvas c(w, h);
          wordsui::Model m;
          wordsui::Geometry g;
          pagedtext::Work work;
          m.view = view;
          m.total = 12;
          m.count = m.capacity = wordsui::listCapacity(view, w, h);
          m.saved = 18;
          m.pending = m.due = 5;
          m.term = "curious";
          m.meaning = "curioso";
          m.language = "EN > ES";
          m.book = "Alice in Wonderland";
          m.sentence = "Alice was curious about the tiny door.";
          m.text = m.sentence;
          m.filter = tr(STR_WORDS_TRANSLATIONS);
          m.guidePage = variant;
          m.answer = variant == 1;
          m.review = variant == 2 ? wordsui::ReviewKind::Difficult : wordsui::ReviewKind::DueWords;
          m.reviewTotal = 5;
          std::snprintf(m.delays[0], sizeof(m.delays[0]), "1 min");
          std::snprintf(m.delays[1], sizeof(m.delays[1]), "6 min");
          std::snprintf(m.delays[2], sizeof(m.delays[2]), "10 min");
          std::snprintf(m.delays[3], sizeof(m.delays[3]), "16 d");
          if (view == wordsui::View::List && variant == 1) m.total = m.count = 0;
          if (view == wordsui::View::List && variant == 2) m.message = tr(STR_WORDS_SESSION_DONE);
          for (int n = 0; n < m.capacity; ++n) {
            std::snprintf(m.rows[n].label, sizeof(m.rows[n].label), "curious %d", n);
            std::snprintf(m.rows[n].source, sizeof(m.rows[n].source), "curioso");
          }
          pixelui::Header header;
          header.title = tr(STR_WORDS_APP);
          header.back = true;
          pixelui::header(c.screen, header);
          if (view == wordsui::View::Detail || view == wordsui::View::Review) {
            c.target.setFont(4, c.reading.font);
            c.target.setFont(2, c.readingBold.font);
            c.target.setFont(3, c.library.font);
          }
          wordsui::build(c.screen, m, g, work);
          if (view == wordsui::View::Detail || view == wordsui::View::Review) {
            bool term = false, prefix = false;
            for (const auto& line : c.target.labels) {
              if (line.value == m.term) {
                term = true;
                CHECK(!line.bold);
              }
              if (line.value == tr(STR_WORDS_TERM_LABEL)) {
                prefix = true;
                CHECK(line.bold);
              }
            }
            CHECK(term);
            CHECK(prefix == (view != wordsui::View::Review || m.answer));
          }
          if (view == wordsui::View::Detail) {
            CHECK(c.has(wordsui::Passage) && c.has(wordsui::Delete) && c.has(wordsui::ReviewWord));
            bool italic = false, bold = false;
            for (const auto& line : c.target.labels) {
              italic |= line.italic;
              bold |= line.bold;
            }
            CHECK(!italic && bold);
          }
          if (view == wordsui::View::Review && m.answer) CHECK(c.hits(wordsui::Rate) == 4);
          if (view == wordsui::View::Home) {
            CHECK(c.has(wordsui::Settings) && c.has(wordsui::Help));
            CHECK(c.hits(wordsui::Settings) == 1);
            CHECK(c.label(tr(STR_CARDS_SETTINGS)));
            const auto settings = g.buttons[wordsui::Settings];
            CHECK(settings.width > 100 && settings.height >= 44);
          }
          if (view == wordsui::View::List && variant == 0) CHECK(c.hits(wordsui::Open) == wordsui::kListCapacity);
          if (view == wordsui::View::List && variant == 0) {
            CHECK(c.has(wordsui::Review) && c.has(wordsui::ReviewHard) && c.has(wordsui::ReviewAll) &&
                  c.has(wordsui::FullList));
            CHECK(g.buttons[wordsui::Review].y == g.buttons[wordsui::ReviewHard].y);
            CHECK(g.buttons[wordsui::ReviewAll].y > g.buttons[wordsui::Review].bottom());
          }
          if (view == wordsui::View::WordList) {
            CHECK(c.hits(wordsui::Open) == m.capacity);
            CHECK(!c.label("curioso"));
          }
          save(c,
               view == wordsui::View::WordList  ? "words-full-list"
               : view == wordsui::View::Home    ? "words-home"
               : view == wordsui::View::List    ? "words-list"
               : view == wordsui::View::Review  ? "words-review"
               : view == wordsui::View::Guide   ? "words-guide"
               : view == wordsui::View::Waiting ? "words-waiting"
               : view == wordsui::View::Delete  ? "words-delete"
                                                : "words-detail",
               variant);
        }
      }
      for (int topInset : {0, 10}) {
        for (int page : {1, 2, 3}) {
          Canvas c(w, h, topInset);
          wordsui::Model m;
          wordsui::Geometry g;
          pagedtext::Work work;
          m.view = wordsui::View::Guide;
          m.guidePage = page;
          wordsui::build(c.screen, m, g, work);
          const char* copy = page == 1   ? tr(STR_WORDS_EXAMPLE_SENTENCE)
                             : page == 2 ? tr(STR_WORDS_SPACING_HELP)
                                         : tr(STR_WORDS_LIST_SECTION_HELP);
          bool found = false;
          for (const auto& label : c.target.labels) {
            if (label.value != copy) continue;
            found = true;
            for (const auto& fill : c.target.blackFills) {
              const bool overlaps = label.box.x < fill.right() && fill.x < label.box.right() &&
                                    label.box.y < fill.bottom() && fill.y < label.box.bottom();
              CHECK(!overlaps);
            }
          }
          CHECK(found);
          save(c, "words-guide-extra", page + topInset);
        }
      }
      for (bool answer : {false, true}) {
        Canvas c(w, h);
        wordsui::Model m;
        wordsui::Geometry g;
        pagedtext::Work work;
        m.view = wordsui::View::Review;
        m.review = wordsui::ReviewKind::AllSaved;
        m.answer = answer;
        m.term = "curious";
        m.meaning = "curioso";
        m.reviewIndex = 122;
        wordsui::build(c.screen, m, g, work);
        CHECK(c.label("123"));
        save(c, "words-all-saved", answer);
      }
      Canvas wall(w, h);
      const auto grid = wallpapersui::gridGeom(wall.device);
      CHECK(grid.cols == 3 && grid.rows == 3 && grid.perPage == 9);
      for (int n = 0; n < 9; ++n) {
        const auto r = wallpapersui::thumbRect(grid, n);
        CHECK(r.width > 0 && r.height > 0);
        CHECK(wallpapersui::cellAt(grid, r.x + r.width / 2, r.y + r.height / 2) == n);
        CHECK(r.bottom() + 16 <= grid.pageDotsY);
        CHECK(grid.pageDotsY + 44 + 16 <= wall.device.safeRect().bottom());
      }
    }
  }
  std::printf("words/tools screens: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

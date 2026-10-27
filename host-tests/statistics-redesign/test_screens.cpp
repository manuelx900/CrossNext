#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../lib/EpdFont/builtinFonts/ubuntu_12_bold.h"
#include "../../src/apps_local/reading/ReadingStatsScreens.h"
#include "../../src/apps_local/ui/QuickSettingsScreen.h"
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
      size_t index = n;
      for (uint32_t i = 0; i < source.intervalCount; ++i) {
        const auto& range = source.intervals[i];
        if (n + 32 >= range.first && n + 32 <= range.last) {
          index = range.offset + n + 32 - range.first;
          break;
        }
      }
      const EpdGlyph& g = source.glyph[index];
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
  fui::Rect lastBitmap;
  void bitmap(fui::Rect r, fui::BitmapRef ref, fui::BitmapMode mode,
              fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rotation = fui::Rotation::None) override {
    CHECK(r.width > 0 && r.height > 0);
    CHECK(r.x >= 0 && r.y >= 0 && r.right() <= clipRect().right() && r.bottom() <= clipRect().bottom());
    lastBitmap = r;
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
    fui::layoutText(backend, box, value, style, [&](const char*, fui::Rect line) {
      if (line.bottom() > box.bottom())
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
    });
    labels.push_back({value ? value : "", style.font, box});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge caption{ubuntu_10_regular}, tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30},
      library{ui_prose_10}, prose{ui_prose_14}, reading{reading_serif_14};
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
        frame(target, device, input, interactions, nullptr, {100, false, true}),
        screen(frame) {
    target.setFont(3, tiny.font);
    target.setFont(4, caption.font);
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
  const int today = readingstats::dayIndex({2026, 10, 18});
  readingstats::Summary summary;
  summary.anchorDay = today;
  readingstats::Book books[5]{};
  for (int n = 0; n < 5; ++n) {
    auto& b = books[n];
    std::snprintf(b.title, sizeof(b.title), "MAPAS DEL CIELO %d", n + 1);
    b.seconds = 17700;
    b.sessions = 10;
    b.pages = 206;
    b.progressBp = 10000;
    b.completed = true;
    b.firstEpoch = 1788600000;
    b.finishedEpoch = 1791630000;
  }
  for (int day = today - 120; day < today; ++day) {
    if (day % 5 == 0) continue;
    readingstats::Record record;
    record.localDay = day;
    record.secondOfDay = 8 * 3600;
    record.seconds = day % 3 ? 1800 : 600;
    record.pages = 20;
    record.firstCompletion = day % 19 == 0;
    summary.add(record);
  }
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}, {600, 800}, {800, 600}}) {
      readingui::Model m;
      m.summary = &summary;
      m.books = books;
      m.today = today;
      m.displayedDay = today;
      m.month = 2026 * 12 + 9;
      m.bookTotal = 5;
      m.battery = 100;
      for (int tab = 0; tab < 4; ++tab) {
        for (bool alternate : {false, true}) {
          Canvas c(size.first, size.second);
          readingui::Work work;
          m.tab = tab;
          m.pageSize = readingui::capacity(c.device);
          CHECK(m.pageSize == (size.first > size.second ? 3 : 5));
          m.count = m.pageSize;
          m.filter = alternate ? readingstats::BookFilter::Recent : readingstats::BookFilter::Finished;
          m.weekStartsSunday = alternate;
          readingui::build(c.screen, m, work);
          c.checkHits();
          CHECK(c.has(readingui::Tab, tab));
          if (tab == 3) CHECK(c.hits(readingui::OpenBook) == m.pageSize);
          if (tab == 2) {
            const int first = readingstats::weekStart(today, alternate);
            CHECK(c.hits(readingui::SelectChartDay) == today - first + 1);
            CHECK(c.has(readingui::SelectWeek, first));
          }
          if (argc > 1)
            c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                    (std::to_string(size.first) + "x" + std::to_string(size.second)) + "-tab" + std::to_string(tab) +
                    (alternate ? "-alt" : "") + ".pbm");
        }
      }
      for (auto period : {readingstats::Period::Day, readingstats::Period::Week, readingstats::Period::Month,
                          readingstats::Period::Year}) {
        readingstats::PeriodStats stats;
        readingstats::setPeriod(stats, period, today - 10, false);
        stats.seconds = 22680;
        stats.sessions = 9;
        stats.pages = 191;
        for (int n = 0; n < 12; ++n) {
          stats.annualSeconds[n] = 360000 + n * 3600;
          stats.annualCompleted[n] = n % 5;
        }
        stats.hours[2] = 3480;
        stats.hours[5] = 900;
        for (int n = 0; n < 7; ++n) stats.dailySeconds[n] = 1800 + n * 60;
        Canvas c(size.first, size.second);
        readingui::Work work;
        m.periodFromPicker = period == readingstats::Period::Month || period == readingstats::Period::Year;
        m.tab = m.periodFromPicker ? 1 : 2;
        m.period = period;
        m.periodStats = &stats;
        readingui::build(c.screen, m, work);
        c.checkHits();
        CHECK(c.has(readingui::BackToCalendar));
        CHECK(work.items[m.tab].selected);
        if (m.periodFromPicker) CHECK(!work.items[2].selected);
        if (period == readingstats::Period::Year) CHECK(c.label(tr(STR_STATS_MONTH_FINISHED)));
        if (period == readingstats::Period::Month) {
          CHECK(c.label(tr(STR_STATS_DAYS_TO_DATE)));
          const auto& asOf = c.target.labels.back().box;
          for (const auto& label : c.target.labels) {
            char expected[96];
            pixelui::copyCaps(expected, sizeof(expected), tr(STR_STATS_DAYS_TO_DATE));
            if (label.value != expected) continue;
            int dots = 0;
            for (const auto& fill : c.target.blackFills) {
              if (fill.height != 13 || fill.width > 13 || fill.y <= label.box.y) continue;
              CHECK(label.box.bottom() <= fill.y);
              CHECK(c.target.lastBitmap.bottom() <= fill.y);
              CHECK(fill.bottom() <= asOf.y);
              ++dots;
            }
            CHECK(dots > 0);
          }
        }
        if (argc > 1)
          c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                  (std::to_string(size.first) + "x" + std::to_string(size.second)) + "-period" +
                  std::to_string(int(period)) + ".pbm");
      }
      for (int year : {2024, 2026}) {
        Canvas c(size.first, size.second);
        readingui::Work work;
        m.tab = 1;
        m.pickerVisible = true;
        m.pickerYear = year;
        readingui::build(c.screen, m, work);
        c.checkHits();
        CHECK(c.has(readingui::PickerYear, -1) && c.has(readingui::SelectYear));
        CHECK(c.has(readingui::BackToData));
        CHECK(work.items[1].selected && !work.items[2].selected);
        CHECK(c.hits(readingui::PickerMonth) == (year == 2026 ? 10 : 12));
        if (argc > 1)
          c.write(std::string(argv[1]) + "/picker-" + (std::to_string(size.first) + "x" + std::to_string(size.second)) +
                  "-" + std::to_string(year) + ".pbm");
      }
      m.pickerVisible = false;
      m.period = readingstats::Period::Calendar;
      m.periodStats = nullptr;
      for (bool finished : {true, false}) {
        Canvas c(size.first, size.second);
        readingui::Work work;
        m.detail = books;
        books[0].completed = finished;
        books[0].progressBp = finished ? 10000 : 5700;
        readingui::build(c.screen, m, work);
        c.checkHits();
        CHECK(c.has(readingui::BackToBooks));
        if (argc > 1)
          c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                  (std::to_string(size.first) + "x" + std::to_string(size.second)) + "-book" +
                  std::to_string(finished) + ".pbm");
      }
      m.detail = nullptr;
      for (int state = 0; state < 4; ++state) {
        Canvas c(size.first, size.second);
        readingui::Work work;
        m.tab = state ? 0 : 3;
        m.count = 0;
        m.failed = state == 1;
        m.loading = state == 2;
        m.damaged = state == 3;
        readingui::build(c.screen, m, work);
        c.checkHits();
      }
      Canvas c(size.first, size.second);
      quickui::Work work;
      quickui::Model quick;
      quick.shortcuts[0] = 5;
      quick.shortcuts[1] = 6;
      quick.shortcuts[2] = 7;
      quick.shortcuts[3] = 8;
      quickui::build(c.screen, quick, work);
      c.checkHits();
      for (int n = 0; n < int(quickui::Shortcut::Count); ++n) {
        const auto action = static_cast<quickui::Shortcut>(n);
        CHECK(*quickui::label(action));
        CHECK(quickui::icon(action).w > 0);
      }
    }
  }
  std::printf("Statistics reference geometry: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

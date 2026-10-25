#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../src/apps_local/carnivoro/CarnivoroScreens.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleScreens.h"
#include "../../src/apps_local/habits/HabitsScreens.h"
#include "../../src/apps_local/home/HomeScreen.h"
#include "../../src/apps_local/library/LibraryMenu.h"
#include "../../src/apps_local/murdle/MurdleScreens.h"
#include "../../src/apps_local/notes/NotesScreens.h"
#include "../../src/apps_local/study/StudyScreens.h"
#include "../../src/apps_local/ui/FitText.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/SettingsHome.h"
#include "../../src/apps_local/ui/SettingsMenuStyle.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#include "../../src/apps_local/wallpapers/WallpapersScreens.h"
#include "../../src/apps_local/words/WordsScreens.h"
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
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, prose{ui_prose_10},
      largeProse{ui_prose_14}, flashProse{ubuntu_10_regular};
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
  fui::Rect textRects[64]{};
  int textCount = 0, errorText = -1, proseBottom = 0;
  bool sawNotice = false, sawTail = false;
  std::vector<std::string> values;
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
    if (s) values.emplace_back(s);
    if (s && std::strstr(s, "TAIL")) sawTail = true;
    if (st.font == 4) proseBottom = std::max(proseBottom, int(r.bottom()));
    if (s && (std::strcmp(s, tr(STR_WORDS_CLOCK)) == 0 || std::strcmp(s, tr(STR_WORDS_EXPORTED)) == 0 ||
              std::strcmp(s, tr(STR_WORDS_EXPORT_CANCELLED)) == 0))
      sawNotice = true;
    if (s && s[0] && textCount < 64) {
      if (std::strcmp(s, tr(STR_CHESS_PUZZLE_SAVE_ERROR)) == 0 || std::strcmp(s, tr(STR_CHESS_PUZZLE_BAD_NUMBER)) == 0)
        errorText = textCount;
      textRects[textCount++] = r;
    }
    bool fit = true;
    fui::layoutText(target, r, s, st, [&](const char* run, fui::Rect box) {
      if (box.bottom() > r.bottom() || box.right() > r.right() || std::strstr(run, "…") || std::strstr(run, "..."))
        fit = false;
    });
    if (!fit) {
      std::printf("TEXT CUT [%d,%d %dx%d] %s\n", r.x, r.y, r.width, r.height, s);
      CHECK(fit);
    }
    target.text(r, s, st);
  }
};

}  // namespace

void regressions() {
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    for (int count : {0, 1, 2, 7, 17, 31}) {
      Canvas measure(size.first, size.second);
      measure.target.setFont(4, measure.prose.font);
      std::vector<notesui::Task> tasks;
      tasks.reserve(count);
      for (int n = 0; n < count; ++n)
        tasks.push_back({n % 3 ? "Read the next chapter and take a short note" : "Water plants", n % 2 == 0});
      notesui::NoteModel m;
      m.todo = true;
      m.freeTasks = true;
      m.title = "Tareas sueltas";
      m.tasks = tasks.data();
      m.count = count;
      m.total = count;
      m.done = (count + 1) / 2;
      m.anyDone = m.done > 0;
      const auto starts = notesui::notePageStarts(measure.target, measure.device, m);
      std::vector<int> seen(count, 0);
      for (size_t page = 0; page < starts.size(); ++page) {
        Canvas c(size.first, size.second);
        c.target.setFont(4, c.prose.font);
        TextAudit audit(c.target, c.width, c.height);
        toybox::Frame f(audit, c.device, c.input, c.interactions);
        toybox::Screen screen(f);
        char label[24];
        std::snprintf(label, sizeof(label), "%d / %d", int(page + 1), int(starts.size()));
        m.pageLabel = "Page label deliberately is not numeric";
        m.firstVisible = starts[page];
        notesui::buildNote(screen, m);
        c.checkHits();
        CHECK(!c.has(notesui::ActionMenu));
        CHECK(c.has(notesui::ActionPage, -1) == (page > 0));
        CHECK(c.has(notesui::ActionPage, 1) == (page + 1 < starts.size()));
        for (size_t h = 0; h < c.interactions.count(); ++h) {
          const auto& hit = c.interactions.data()[h];
          if (hit.action == notesui::ActionToggleTask) {
            CHECK(hit.value >= 0 && hit.value < count);
            if (hit.value >= 0 && hit.value < count) ++seen[hit.value];
          }
        }
      }
      for (int value : seen) CHECK(value == 1);
    }
    for (const char* message : {tr(STR_WORDS_CLOCK), tr(STR_WORDS_EXPORTED), tr(STR_WORDS_EXPORT_CANCELLED)}) {
      Canvas c(size.first, size.second);
      c.target.setFont(4, c.prose.font);
      TextAudit audit(c.target, c.width, c.height);
      toybox::Frame f(audit, c.device, c.input, c.interactions);
      toybox::Screen screen(f);
      wordsui::Model m;
      m.view = wordsui::View::List;
      m.filter = tr(STR_WORDS_ALL);
      m.count = 3;
      m.total = 3;
      m.message = message;
      wordsui::Geometry g;
      pagedtext::Work work;
      wordsui::build(screen, m, g, work);
      c.checkHits();
      CHECK(audit.sawNotice);
      CHECK(c.has(wordsui::Dismiss));
      CHECK(!c.has(wordsui::Open));
    }
    {
      Canvas focused(size.first, size.second);
      wordsui::Model m;
      m.view = wordsui::View::List;
      m.count = m.total = 1;
      m.focus = -3;
      wordsui::Geometry g;
      pagedtext::Work w;
      wordsui::build(focused.screen, m, g, w);
      CHECK(focused.has(wordsui::ReviewAll));
    }
    for (int year : {2020, 2024, 2026, 2099})
      for (int month = 1; month <= 12; ++month) {
        const int first = habits::dayIndex(year, month, 1), next = habits::shiftMonth(first, 1),
                  days = next > first ? next - first : 31;
        const int today = first + days - 1;
        auto store = std::make_unique<habits::Store>();
        habits::add(*store, "Read", first);
        habits::advance(*store, today);
        Canvas c(size.first, size.second);
        c.target.setFont(4, c.prose.font);
        TextAudit audit(c.target, c.width, c.height);
        toybox::Frame f(audit, c.device, c.input, c.interactions);
        toybox::Screen screen(f);
        habitsui::DetailModel model{store.get(), 0, today, first, today};
        habitsui::buildDetail(screen, model);
        c.checkHits();
        fui::Rect grid;
        for (size_t n = 0; n < c.interactions.count(); ++n)
          if (c.interactions.data()[n].action == habitsui::ActionCalendar) grid = c.interactions.data()[n].rect;
        CHECK(!grid.empty());
        const int rows = (habits::weekday(first) + days + 6) / 7;
        for (int n = 0; n < rows * 7; ++n) {
          const int day = first - habits::weekday(first) + n;
          const int x = grid.x + (2 * (n % 7) + 1) * grid.width / 14,
                    y = grid.y + (2 * (n / 7) + 1) * grid.height / (2 * rows);
          CHECK(habitsui::calendarDayAt(c.device, model, x, y) == (day >= first && day <= today ? day : -1));
          auto future = model;
          future.today = first;
          CHECK(habitsui::calendarDayAt(c.device, future, x, y) == (day == first ? first : -1));
        }
      }
    for (bool answer : {false, true})
      for (bool undo : {false, true}) {
        Canvas c(size.first, size.second);
        const auto g = studyui::cardLayout(c.device, answer, undo);
        CHECK(g.face.x <= g.content.x && g.face.y <= g.content.y && g.content.right() <= g.face.right() &&
              g.content.bottom() <= g.face.bottom());
        const auto within = [&](fui::Rect x) {
          CHECK(x.x >= 0 && x.y >= 0 && x.right() <= c.width && x.bottom() <= c.height);
        };
        if (answer)
          for (auto box : g.ratings) {
            within(box);
            CHECK(box.width >= 44 && box.height >= 44);
            CHECK(box.right() <= g.face.x || box.x >= g.face.right() || box.y >= g.face.bottom());
          }
        else {
          within(g.reveal);
          within(g.undo);
          CHECK(g.reveal.bottom() <= g.undo.y);
        }
      }
    Canvas home(size.first, size.second);
    const auto focus = homeui::layout(home.device, 6, homeui::Style::Carousel);
    CHECK(focus.cover.y >= focus.badge.bottom() + 8);
    CHECK(focus.author.bottom() + 8 <= focus.progress.y);
    CHECK(focus.title.bottom() <= focus.author.y);
    CHECK(focus.author.right() <= focus.menu.x || focus.author.bottom() <= focus.menu.y);
  }
  CHECK(habits::shiftMonth(habits::kMinDay, -1) == habits::kMinDay);
  CHECK(habits::shiftMonth(habits::kMaxDay, 1) == habits::dayIndex(2099, 12, 1));
}

void freeCaseCatalogs() {
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      I18N.setLanguage(lang);
      murdlecampaign::Progress progress;
      for (int tier = 0; tier < murdle::kTierCount; ++tier) {
        const auto filter = static_cast<murdle::Tier>(tier);
        const int first = murdlecampaign::first(filter);
        for (int page = 0; page < murdlecampaign::kPagesPerTier; ++page) {
          Canvas c(size.first, size.second);
          TextAudit target(c.target, c.width, c.height);
          toybox::Frame frame(target, c.device, c.input, c.interactions);
          toybox::Screen screen(frame);
          murdleui::buildCampaign(screen, progress, page, first + page * murdlecampaign::kCasesPerPage, -1, filter);
          for (int n = 0; n < murdlecampaign::kCasesPerPage; ++n) {
            const int index = first + page * murdlecampaign::kCasesPerPage + n;
            CHECK(c.has(murdleui::ActionOpenCampaign, index) == (index < first + murdlecampaign::kCasesPerTier));
          }
          for (int n = 0; n < murdle::kTierCount; ++n) CHECK(c.has(murdleui::ActionTier, n));
          CHECK(!progress.count());
          c.checkHits();
        }
      }
    }
}
void fixedHomeRows() {
  for (auto dimensions : {std::pair<int, int>{480, 800}, {800, 480}}) {
    Canvas c(dimensions.first, dimensions.second);
    for (auto style : {homeui::Style::Framed, homeui::Style::Carousel}) {
      const auto full = homeui::layout(c.device, 6, style);
      for (int count = 1; count < 6; ++count) {
        const auto reduced = homeui::layout(c.device, count, style);
        CHECK(full.card.x == reduced.card.x && full.card.y == reduced.card.y &&
              full.card.height == reduced.card.height);
        for (int n = 0; n < count; ++n)
          CHECK(full.rows[n].x == reduced.rows[n].x && full.rows[n].y == reduced.rows[n].y &&
                full.rows[n].width == reduced.rows[n].width && full.rows[n].height == reduced.rows[n].height);
      }
    }
  }
}
void informationalCopy() {
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      I18N.setLanguage(lang);
      Canvas c(size.first, size.second);
      c.target.setFont(4, c.prose.font);
      TextAudit target(c.target, c.width, c.height);
      const auto body = pixelui::geometry(c.device).body;
      auto style = fui::TextStyle{};
      style.font = 4;
      style.maxLines = std::max(1, (body.height - 84) / target.lineHeight(4));
      const std::string guide =
          std::string(tr(STR_FLASH_COPY_HELP)) + "\n" + tr(STR_FLASH_CHOOSE_HELP) + "\n" + tr(STR_FLASH_START_HELP);
      fittext::draw(target, fui::makeRect(body.x, body.y, body.width, body.height - 84), guide.c_str(), 4,
                    style.maxLines);
      CHECK(target.values.size() > 1);
      for (const auto& run : target.values) {
        CHECK(run.find('\n') == std::string::npos);
        CHECK(target.measureText(4, run.c_str(), style).width <= body.width);
      }
      CHECK(target.proseBottom <= body.bottom() - 84);
    }
}
void wordGuides() {
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (int page = 0; page < 3; ++page) {
        I18N.setLanguage(lang);
        Canvas c(size.first, size.second);
        c.target.setFont(4, c.prose.font);
        TextAudit target(c.target, c.width, c.height);
        toybox::Frame frame(target, c.device, c.input, c.interactions);
        toybox::Screen screen(frame);
        wordsui::Model model;
        model.view = wordsui::View::Guide;
        model.guidePage = page;
        wordsui::Geometry geometry;
        pagedtext::Work work;
        wordsui::build(screen, model, geometry, work);
        c.checkHits();
      }
}
void wordPreviews() {
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      I18N.setLanguage(lang);
      Canvas c(size.first, size.second);
      c.target.setFont(4, c.prose.font);
      TextAudit target(c.target, c.width, c.height);
      toybox::Frame frame(target, c.device, c.input, c.interactions);
      toybox::Screen screen(frame);
      wordsui::Model model;
      model.view = wordsui::View::List;
      model.count = 4;
      model.total = 6;
      for (int n = 0; n < 4; ++n) {
        std::snprintf(model.rows[n].label, sizeof(model.rows[n].label), "Word %d", n);
        std::snprintf(model.rows[n].source, sizeof(model.rows[n].source),
                      "A complete definition whose original wording remains available when this word is opened, even "
                      "when the list row has insufficient space to display every sentence.");
      }
      wordsui::Geometry geometry;
      pagedtext::Work work;
      wordsui::build(screen, model, geometry, work);
      CHECK(std::find(target.values.begin(), target.values.end(), tr(STR_WORDS_OPEN_WORD)) != target.values.end());
      for (int n = 0; n < 4; ++n) CHECK(c.has(wordsui::Open, n));
      CHECK(c.has(wordsui::Next));
    }
}
void fittedRows() {
  Canvas c(480, 800);
  fui::ListProps props;
  pixelui::styleList(props, true);
  props.labelText.maxLines = 1;
  props.count = 60;
  props.topIndex = 40;
  props.rowProvider = [](void*, uint16_t n, fui::ListItem& item) {
    item = {};
    item.label = n < 40 ? "SHORT" : "LA HISTORIA INTERMINABLE";
  };
  pixelui::RowSource rows;
  rows.bind(props, c.target, fui::makeRect(0, 0, 280, 500));
  fui::ListItem item;
  props.rowProvider(props.rowProviderCtx, 40, item);
  CHECK(std::strcmp(item.label, "LA HISTORIA INTERMINABLE") == 0);
  CHECK(c.target.measureText(props.labelText.font, item.label, props.labelText).width <=
        280 - props.sidePadding * 2 - 5);
  char title[] = "LA HISTORIA INTERMINABLE";
  auto style = fui::TextStyle{};
  style.font = 1;
  libraryui::format(c.target, title, 220, style);
  CHECK(style.font != 1);
  CHECK(c.target.measureText(style.font, title, style).width <= 220);
  CHECK(std::strstr(title, "...") == nullptr);
}
void longTasks() {
  std::string copy;
  copy.reserve(4000);
  for (int n = 0; n < 400; ++n) copy += "complete ";
  copy += "TAIL";
  const notesui::Task task{copy.c_str(), false};
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
    for (bool todo : {false, true}) {
      Canvas c(size.first, size.second);
      c.target.setFont(4, c.prose.font);
      notesui::NoteModel model;
      model.todo = todo;
      model.title = "Tasks";
      model.tasks = &task;
      model.count = 1;
      const auto starts = notesui::notePageStarts(c.target, c.device, model);
      CHECK(starts.size() > 1);
      bool sawTail = false;
      for (int cursor : starts) {
        model.firstVisible = cursor;
        c.interactions.clear();
        TextAudit target(c.target, c.width, c.height);
        toybox::Frame frame(target, c.device, c.input, c.interactions);
        toybox::Screen screen(frame, toybox::themeTokens());
        notesui::buildNote(screen, model);
        sawTail |= target.sawTail;
        CHECK(c.has(notesui::ActionToggleTask, 0));
      }
      CHECK(sawTail);
    }
}
void murdleGuides() {
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (int page = 0; page < murdleui::howToPages(size.first > size.second); ++page) {
        I18N.setLanguage(lang);
        Canvas c(size.first, size.second);
        c.target.setFont(4, c.prose.font);
        TextAudit target(c.target, c.width, c.height);
        toybox::Frame frame(target, c.device, c.input, c.interactions);
        toybox::Screen screen(frame, toybox::themeTokens());
        murdleui::buildHowTo(screen, {page});
        CHECK(target.proseBottom <= screen.body().bottom() - 64);
        c.checkHits();
        CHECK(HeaderBackTapTarget::w >= 44 && HeaderBackTapTarget::h >= 44);
        CHECK(c.has(page == 0 ? murdleui::ActionHowTo : murdleui::ActionPlay));
        CHECK(murdleui::howToPages(size.first > size.second) == 2);
      }
}
int main(int argc, char** argv) {
  regressions();
  freeCaseCatalogs();
  murdleGuides();
  longTasks();
  fittedRows();
  wordPreviews();
  wordGuides();
  informationalCopy();
  fixedHomeRows();
  for (auto lang : {Language::EN, Language::ES})
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      I18N.setLanguage(lang);
      const std::string stem = std::string(argc > 1 ? argv[1] : "") + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                               std::to_string(size.first);
      for (int view = 0; view < 16; ++view) {
        Canvas c(size.first, size.second);
        c.target.setFont(4, view >= 3 && view <= 7 ? c.flashProse.font : c.prose.font);
        TextAudit target(c.target, c.width, c.height);
        toybox::Frame frame(target, c.device, c.input, c.interactions);
        toybox::Screen screen(frame, toybox::themeTokens());
        if (view < 3) {
          wordsui::Model m;
          m.view = view ? wordsui::View::List : wordsui::View::Home;
          m.filter = tr(STR_WORDS_ALL);
          m.capacity = 3;
          if (view == 2) {
            m.count = 3;
            m.total = 24;
            for (int n = 0; n < 3; ++n) {
              std::snprintf(m.rows[n].label, sizeof(m.rows[n].label), "glanced");
              std::snprintf(m.rows[n].source, sizeof(m.rows[n].source), "Language practice · English-Spanish");
            }
          }
          pixelui::Header h;
          h.back = true;
          h.title = tr(STR_WORDS_APP);
          pixelui::header(target, c.device, h);
          wordsui::Geometry g;
          pagedtext::Work w;
          wordsui::build(screen, m, g, w);
        } else if (view == 3 || view == 4) {
          studyui::DeckModel m;
          m.localOnly = true;
          m.name = "Legacy demo.apkg";
          m.fresh = 3;
          m.total = 3;
          int history[14] = {3, 1, 2, 0, 5, 2, 1, 1, 0, 1, 2, 4, 2, 1};
          if (view == 4) {
            m.history = history;
            m.lifetimeReviews = 30;
          }
          studyui::buildDeck(screen, m);
        } else if (view >= 5 && view <= 7) {
          studyui::CardModel m;
          m.deck = "Legacy demo.apkg";
          m.remaining = 3;
          m.answer = view == 7;
          m.canUndo = view == 6;
          for (int n = 0; n < 4; ++n) std::snprintf(m.delays[n], 16, "%d min", n + 1);
          studyui::buildCard(screen, m);
          const auto g = studyui::cardLayout(c.device, m.answer, m.canUndo);
          CHECK(g.content.width > 0 && g.content.height > 0);
          CHECK(g.face.bottom() <= c.height);
        } else if (view == 8) {
          notesui::DeckItem items[4] = {{"Casa", nullptr, nullptr, 0, 2},
                                        {"Estudio", nullptr, nullptr, 0, 2},
                                        {"Lecturas", nullptr, nullptr, 1, 2},
                                        {"Work", nullptr, nullptr, 2, 3}};
          notesui::buildDeck(screen, {true, items, 4, 0, "1 / 2"});
        } else if (view == 9 || view == 10) {
          const notesui::Task tasks[] = {{"Regar las plantas", false}, {"Ordenar los libros", view == 10}};
          notesui::NoteModel m;
          m.todo = true;
          m.freeTasks = view == 10;
          m.title = view == 10 ? tr(STR_TODO_FREE_TASKS) : "Casa";
          m.tasks = tasks;
          m.count = 2;
          m.done = view == 10;
          m.total = 2;
          m.anyDone = view == 10;
          notesui::buildNote(screen, m);
        } else if (view >= 14) {
          c.target.setFont(4, c.largeProse.font);
          murdleui::buildStorageError(screen, view == 15);
          CHECK(c.has(murdleui::ActionRetry));
          CHECK(c.has(murdleui::ActionBack));
        } else {
          auto store = std::make_unique<habits::Store>();
          const int today = habits::dayIndex(2026, 10, 17);
          habits::advance(*store, today - 30);
          habits::add(*store, tr(STR_HABITS_READ), today - 30);
          habits::add(*store, tr(STR_HABITS_EXERCISE), today - 30);
          habits::add(*store, tr(STR_HABITS_WATER), today - 30);
          habits::advance(*store, today);
          for (int d = today - 12; d <= today; ++d)
            if (d != today - 4) habits::toggle(*store, 0, d, today);
          habits::toggle(*store, 2, today, today);
          if (view == 11)
            habitsui::buildToday(screen, {store.get(), today, 0, false, 0});
          else if (view == 12)
            habitsui::buildPresets(screen, 0);
          else
            habitsui::buildDetail(screen, {store.get(), 0, today, habits::monthStart(today), today});
        }
        c.checkHits();
        for (size_t i = 0; i < c.interactions.count(); ++i)
          for (size_t j = 0; j < i; ++j) {
            auto a = c.interactions.data()[i].rect, b = c.interactions.data()[j].rect;
            CHECK(a.right() <= b.x || b.right() <= a.x || a.bottom() <= b.y || b.bottom() <= a.y);
          }
        if (argc > 1) c.write(stem + "-view-" + std::to_string(view) + ".pbm");
      }
    }
  std::printf("Coherence UI: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

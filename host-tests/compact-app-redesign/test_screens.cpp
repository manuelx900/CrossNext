#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/checkers/DraughtsScreens.h"
#include "../../src/apps_local/chess/ChessScreens.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleScreens.h"
#include "../../src/apps_local/habits/HabitsScreens.h"
#include "../../src/apps_local/notes/NotesScreens.h"
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
      if (line.bottom() > box.bottom() || std::strstr(run, "…"))
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
      CHECK(!std::strstr(run, "…"));
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
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      const int w = size.first, h = size.second;
      auto save = [&](Canvas& c, const char* name, int n = 0) {
        c.checkHits();
        if (argc > 1) {
          char path[512];
          std::snprintf(path, sizeof(path), "%s/%s-%dx%d-%s-%d.pbm", argv[1], lang == Language::ES ? "es" : "en", w, h,
                        name, n);
          c.write(path);
        }
      };
      habits::Store store;
      const int today = habits::dayIndex(2026, 10, 18);
      for (const auto& preset : habitsui::kPresets)
        CHECK(habits::add(store, preset.get(), today) == habits::NameResult::Ok);
      habits::add(store, "Stretch", today);
      for (int page = 0; page < 2; ++page) {
        Canvas c(w, h);
        habitsui::TodayModel m;
        m.store = &store;
        m.today = today;
        m.page = page;
        habitsui::buildToday(c.screen, m);
        CHECK(habitsui::todayCapacity(c.device) == 4);
        CHECK(c.hits(habitsui::ActionToggleToday) == (page ? 2 : 4));
        CHECK(!c.label(tr(STR_TASK_PENDING)));
        CHECK(!c.label(tr(STR_HABITS_DONE)));
        CHECK(c.has(habitsui::ActionOpen, page ? 4 : 0));
        save(c, "habits", page);
      }
      {
        Canvas c(w, h);
        habitsui::buildPresets(c.screen, 0);
        CHECK(c.hits(habitsui::ActionPreset) == 5);
        CHECK(c.has(habitsui::ActionCustom, 5));
        save(c, "presets");
      }
      notesui::DeckItem lists[8];
      for (auto& l : lists) {
        l.title = "Casa";
        l.total = 8;
      }
      {
        Canvas c(w, h);
        notesui::DeckModel m;
        m.todo = true;
        m.items = lists;
        m.count = 8;
        notesui::buildDeck(c.screen, m);
        CHECK(notesui::deckCapacity(c.device, true) == (w < h ? 6 : 3));
        CHECK(c.hits(notesui::ActionOpenNote) == (w < h ? 6 : 3));
        CHECK(c.hits(notesui::ActionFreeTasks) == 1);
        CHECK(!c.label("Sin lista asignada"));
        save(c, "lists");
      }
      notesui::Task tasks[18];
      for (auto& t : tasks) t.text = "Regar las plantas";
      for (bool free : {false, true})
        for (bool done : {false, true}) {
          tasks[2].checked = done;
          Canvas c(w, h);
          notesui::NoteModel m;
          m.todo = true;
          m.freeTasks = free;
          m.title = free ? tr(STR_TODO_FREE_TASKS) : "Casa";
          m.tasks = tasks;
          m.count = 18;
          m.total = 18;
          m.done = done;
          m.anyDone = done;
          const auto starts = notesui::notePageStarts(c.target, c.device, m);
          CHECK(starts.size() > 1);
          if (w < h) CHECK(starts[1] == 8);
          notesui::buildNote(c.screen, m);
          CHECK(c.has(notesui::ActionTodoLists));
          CHECK(!c.has(notesui::ActionFreeTasks));
          CHECK(!c.label(tr(STR_TASK_PENDING_PLURAL)));
          CHECK(!c.label(tr(STR_TASK_COMPLETED)));
          CHECK(!c.label("Sin lista asignada"));
          CHECK(c.has(notesui::ActionClearDone) == done);
          CHECK(c.hits(notesui::ActionToggleTask) == starts[1]);
          save(c, free ? "loose" : "tasks", done);
          Canvas next(w, h);
          m.firstVisible = starts[1];
          notesui::buildNote(next.screen, m);
          CHECK(next.hits(notesui::ActionToggleTask) > 0);
          save(next, free ? "loose-page2" : "tasks-page2", done);
        }
      {
        Canvas c(w, h);
        notesui::MenuModel m;
        m.todo = true;
        m.title = "Lecturas";
        m.isList = true;
        notesui::buildMenu(c.screen, m);
        CHECK(!c.has(notesui::ActionUsePhone));
        CHECK(c.has(notesui::ActionRename));
        CHECK(c.has(notesui::ActionSwitchKind));
        CHECK(c.has(notesui::ActionShowAsleep));
        CHECK(c.has(notesui::ActionDelete));
        CHECK(c.has(notesui::ActionDismiss));
        save(c, "list-options");
      }
      {
        Canvas c(w, h);
        notesui::MenuModel m;
        m.title = "Notes";
        notesui::buildMenu(c.screen, m);
        CHECK(c.has(notesui::ActionUsePhone));
      }
      {
        Canvas c(w, h);
        notesui::MenuModel m;
        m.todo = true;
        m.title = "Converted note";
        m.isList = false;
        notesui::buildMenu(c.screen, m);
        CHECK(c.has(notesui::ActionUsePhone));
        CHECK(c.label(tr(STR_UI_MAKE_IT_A_LIST_D0621D)));
        CHECK(!c.label(tr(STR_UI_MAKE_IT_A_NOTE_A34558)));
      }
      {
        const notesui::Task reference[] = {{"Regar las plantas", false},   {"Ordenar libros", false},
                                           {"Sacar la basura", false},     {"Limpiar la cocina", false},
                                           {"Lavar la ropa", false},       {"Cambiar las sabanas", false},
                                           {"Revisar la despensa", false}, {"Guardar las herramientas", false}};
        Canvas c(w, h);
        notesui::NoteModel m;
        m.todo = true;
        m.title = "Casa";
        m.tasks = reference;
        m.count = 8;
        m.total = 8;
        const auto pages = notesui::notePageStarts(c.target, c.device, m);
        if (w < h) CHECK(pages.size() == 1);
        notesui::buildNote(c.screen, m);
        if (w < h) CHECK(c.hits(notesui::ActionToggleTask) == 8);
        save(c, "reference-eight");
      }
      for (int page = 0; page < (w < h ? 4 : 8); ++page) {
        Canvas c(w, h);
        draughtsui::Model m;
        m.view = draughtsui::View::Help;
        m.helpPage = page;
        draughtsui::build(c.screen, m);
        CHECK(c.has(draughtsui::HelpDone));
        save(c, "draughts", page);
      }
      for (auto opponent : {chessui::Opponent::Computer, chessui::Opponent::PassAndPlay}) {
        Canvas c(w, h);
        chessui::SettingsModel m;
        m.opponent = opponent;
        m.level = 2;
        m.humanPlaysWhite = false;
        chessui::buildSettings(c.screen, m);
        CHECK(c.has(chessui::ActionSettingValue, int(chessui::SettingValue::Hints)));
        CHECK(c.has(chessui::ActionSettingValue, int(chessui::SettingValue::Hard)) ==
              (opponent == chessui::Opponent::Computer));
        save(c, "chess-settings", int(opponent));
      }
      for (int error = 0; error < 2; ++error) {
        Canvas c(w, h);
        chesspuzzle::Progress progress;
        chesspuzzleui::Work work;
        chesspuzzleui::Model m;
        m.progress = &progress;
        m.saveFailed = error;
        chesspuzzleui::build(c.screen, m, work);
        CHECK(c.hits(chesspuzzleui::Difficulty) == 3);
        CHECK(c.has(chesspuzzleui::Reset));
        save(c, "puzzles", error);
      }
    }
  }
  std::printf("Compact redesign: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

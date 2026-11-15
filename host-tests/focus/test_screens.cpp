#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/focus/FocusScreens.h"
#include "../../src/apps_local/picross/PicrossScreens.h"
#include "../../src/apps_local/pomodoro/PomodoroScreens.h"
#include "../../src/apps_local/pomodoro/TimerSettingsUi.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/apps_local/ui/TextAlignment.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#pragma GCC diagnostic pop

namespace fui = freeink::ui;
namespace {
unsigned checks = 0;
#define CHECK(value)                                  \
  do {                                                \
    ++checks;                                         \
    if (!(value)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #value); \
      std::exit(1);                                   \
    }                                                 \
  } while (0)
struct Font {
  std::array<fui::FontGlyph, 224> glyphs{};
  fui::BitmapFont font{};
  explicit Font(const EpdFontData& source) {
    for (uint32_t cp = 32; cp <= 255; ++cp) {
      for (uint32_t i = 0; i < source.intervalCount; ++i) {
        const auto& interval = source.intervals[i];
        if (cp < interval.first || cp > interval.last) continue;
        const auto& g = source.glyph[interval.offset + cp - interval.first];
        glyphs[cp - 32] = {uint16_t(g.dataOffset), g.width,       g.height, uint8_t(fp4::toPixel(g.advanceX)),
                           int8_t(g.left),         int8_t(-g.top)};
        break;
      }
    }
    font = {source.bitmap, glyphs.data(), 32, 255, source.advanceY, uint8_t(source.ascender), 64, 64, 1};
  }
};
struct Target : fui::DrawTarget {
  fui::DisplayTarget backend;
  std::vector<fui::Rect> outlines;
  Target(uint8_t* bits, int width, int height, int stride,
         fui::Orientation orientation = fui::Orientation::LandscapeCounterClockwise)
      : backend(bits, width, height, stride, orientation) {}
  void setFont(int slot, const fui::BitmapFont& font) { backend.setFont(slot, font); }
  auto deviceContext() const { return backend.deviceContext(); }
  fui::Size measureText(fui::FontId font, const char* value, fui::TextStyle style) const override {
    return backend.measureText(font, value, style);
  }
  int16_t lineHeight(fui::FontId font) const override { return backend.lineHeight(font); }
  fui::Rect inkBounds(fui::FontId font, const char* value, fui::TextStyle style) const override {
    return backend.inkBounds(font, value, style);
  }
  void fill(fui::Rect rect, fui::Paint paint, uint8_t radius, uint8_t corners) override {
    backend.fill(rect, paint, radius, corners);
  }
  void stroke(fui::Rect rect, fui::Paint paint, uint8_t width, uint8_t radius, uint8_t corners) override {
    outlines.push_back(rect);
    backend.stroke(rect, paint, width, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t width, fui::Paint paint) override { backend.line(a, b, width, paint); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint paint) override {
    backend.triangle(a, b, c, paint);
  }
  void bitmap(fui::Rect rect, fui::BitmapRef bitmap, fui::BitmapMode mode, fui::Paint paint,
              fui::Rotation rotation) override {
    backend.bitmap(rect, bitmap, mode, paint, rotation);
  }
  struct Run {
    fui::Rect rect;
    std::string text;
    fui::TextStyle style;
  };
  std::vector<Run> runs;
  void text(fui::Rect rect, const char* value, fui::TextStyle style) override {
    runs.push_back({rect, value, style});
    backend.text(rect, value, style);
  }
};
void screens() {
  focus::State state;
  for (int p = 0; p < focus::kProjects; ++p) {
    focus::addProject(state, "Biología y técnicas de estudio");
    for (int n = 0; n < (p == focus::kProjects - 1 ? 6 : 8); ++n) {
      const int t = focus::addTask(state, p, "Estudiar célula y resumir los conceptos principales");
      state.tasks[t].planned = 4;
      state.tasks[t].completed = n % 5;
      state.tasks[t].done = n % 3 == 0;
    }
  }
  CHECK(focus::addTask(state, -1, "Only in Today", focus::TaskList::Today) >= 0);
  CHECK(focus::addTask(state, -1, "Only in Tasks", focus::TaskList::Tasks) >= 0);
  focusui::Model independent{&state};
  independent.view = focusui::View::Today;
  CHECK(focusui::count(independent) == 1);
  independent.view = focusui::View::AllTasks;
  CHECK(focusui::count(independent) == 1);
  Font normal(ui_prose_14), small(ui_prose_10), pixel(toybox_14), tile(toybox_10);
  for (const auto language : {Language::EN, Language::ES})
    for (bool wide : {false, true})
      for (bool light : {false, true})
        for (bool jersey : {false, true})
          for (int empty = 0; empty < 2; ++empty)
            for (int v = 0; v <= int(focusui::View::ConfirmTimerChange); ++v) {
              focus::State blank;
              const int width = wide ? 800 : 480, height = wide ? 480 : 800;
              std::vector<uint8_t> bits(width * height / 8, 255);
              Target target(bits.data(), width, height, width / 8);
              for (int n = 0; n < 8; ++n) target.setFont(n, jersey ? pixel.font : normal.font);
              target.setFont(3, jersey ? tile.font : small.font);
              auto device = target.deviceContext();
              device.safeArea = {9, 7, 3, 7};
              toybox::Interactions interactions;
              const fui::InputSnapshot input{};
              toybox::Frame frame(target, device, input, interactions);
              toybox::Screen screen(frame);
              focusui::Work work;
              focusui::Model model{empty && v <= int(focusui::View::Projects) ? &blank : &state};
              model.view = focusui::View(v);
              model.project = model.task = 0;
              model.day = 20759;
              model.filter = empty ? focusui::Filter::Completed : focusui::Filter::All;
              model.saveFailed = model.view == focusui::View::Settings && empty;
              I18N.setLanguage(language);
              chrome::lightHeader = light;
              pixelui::uppercaseLabels = jersey;
              focusui::build(screen, model, work);
              if (model.view == focusui::View::ConfirmTimerChange) {
                toybox::Frame modalFrame(target, target.deviceContext(), input, interactions);
                toybox::Screen modal(modalFrame);
                timerui::confirmation(modal, tr(STR_FOCUS_SKIP_CONFIRM), tr(STR_UI_SKIP_431DE8),
                                      focusui::ApplyTimerChange, focusui::Back);
                CHECK(interactions.count() == 2);
              }
              if (model.view == focusui::View::Settings) {
                unsigned clear = 0, changes = 0, done = 0;
                for (size_t n = 0; n < interactions.count(); ++n) {
                  clear += interactions.data()[n].action == focusui::ClearCompleted;
                  changes += interactions.data()[n].action == focusui::Change;
                  done += interactions.data()[n].action == focusui::Done;
                }
                CHECK(clear == 1 && changes == 8 && done == 1);
                for (const auto& run : target.runs)
                  if (run.text == tr(STR_FOCUS_SESSION_UNIT))
                    CHECK(target.measureText(run.style.font, run.text.c_str(), run.style).width <= run.rect.width);
              }
              if (model.view == focusui::View::Timer) {
                unsigned sessionControls = 0, timerControls = 0, editControls = 0, completeControls = 0,
                         renameControls = 0;
                int statsX = width, settingsX = 0;
                for (size_t n = 0; n < interactions.count(); ++n) {
                  const auto& hit = interactions.data()[n];
                  sessionControls += hit.action == focusui::Planned;
                  timerControls += hit.action == focusui::Toggle;
                  editControls += hit.action == focusui::EditTask;
                  completeControls += hit.action == focusui::Complete;
                  renameControls += hit.action == focusui::Rename;
                  if (hit.action == focusui::OpenTaskStats) statsX = hit.rect.x;
                  if (hit.action == focusui::EditTask) settingsX = hit.rect.x;
                  CHECK(hit.action != focusui::Delete && hit.action != focusui::Prepare);
                }
                CHECK(sessionControls == 2 && timerControls == 1 && editControls == 1 && completeControls == 1 &&
                      renameControls == 1);
                CHECK(statsX < settingsX);
                bool session = false;
                for (const auto& run : target.runs) {
                  session |= run.text == tr(STR_FOCUS_SESSION);
                }
                CHECK(session);
              }
              if (model.view == focusui::View::CleanupScope) {
                unsigned choices = 0;
                for (size_t n = 0; n < interactions.count(); ++n)
                  choices += interactions.data()[n].action == focusui::ChooseCleanup;
                CHECK(choices == 4);
              }
              if (model.view == focusui::View::ConfirmTimerChange) {
                unsigned confirm = 0;
                for (size_t n = 0; n < interactions.count(); ++n)
                  confirm += interactions.data()[n].action == focusui::ApplyTimerChange;
                CHECK(confirm == 1);
              }
              if (empty && (model.view == focusui::View::Today || model.view == focusui::View::AllTasks)) {
                bool completedEmpty = false;
                for (const auto& run : target.runs) completedEmpty |= run.text == tr(STR_FOCUS_NO_COMPLETED);
                CHECK(completedEmpty);
              }
              CHECK(!interactions.overflowed());
              paintclock::notePainted();
              for (size_t n = 0; n < interactions.count(); ++n) {
                const auto& hit = interactions.data()[n];
                if (hit.rect.x < 0 || hit.rect.y < 0 || hit.rect.right() > width || hit.rect.bottom() > height)
                  std::printf("view=%d wide=%d empty=%d action=%u bounds=%d,%d %dx%d\n", v, wide, empty, hit.action,
                              hit.rect.x, hit.rect.y, hit.rect.width, hit.rect.height);
                CHECK(hit.rect.x >= 0 && hit.rect.y >= 0 && hit.rect.right() <= width && hit.rect.bottom() <= height);
                CHECK(hit.rect.width >= 24 && hit.rect.height >= 24);
                fui::InputSnapshot tap;
                tap.touchReleased = true;
                tap.touchX = hit.rect.x + hit.rect.width / 2;
                tap.touchY = hit.rect.y + hit.rect.height / 2;
                const auto event = interactions.route(tap);
                CHECK(event.action == hit.action && event.value == hit.value);
              }
            }
}
void timerLimits() {
  focus::State state;
  const int project = focus::addProject(state, "Biología");
  const int task = focus::addTask(state, project, "Summarize the notes for tomorrow");
  state.tasks[task].planned = 99;
  state.tasks[task].completed = 999;
  Font pixelSmall(toybox_14), pixelBody(toybox_20), prose(ui_prose_14);
  for (bool wide : {false, true})
    for (bool light : {false, true})
      for (bool jersey : {false, true})
        for (int stage : {0, 1, 7}) {
          pomodoro::selectStage(state.timer, stage);
          const int width = wide ? 800 : 480, height = wide ? 480 : 800;
          std::vector<uint8_t> bits(width * height / 8, 255);
          Target target(bits.data(), width, height, width / 8);
          for (int n = 0; n < 8; ++n) target.setFont(n, jersey ? pixelSmall.font : prose.font);
          target.setFont(1, jersey ? pixelBody.font : prose.font);
          auto device = target.deviceContext();
          toybox::Interactions interactions;
          const fui::InputSnapshot input{};
          toybox::Frame frame(target, device, input, interactions);
          toybox::Screen screen(frame);
          focusui::Work work;
          focusui::Model model{&state};
          model.view = focusui::View::Timer;
          model.task = task;
          model.project = project;
          chrome::lightHeader = light;
          pixelui::uppercaseLabels = jersey;
          focusui::build(screen, model, work);
          if (model.view == focusui::View::ConfirmTimerChange) {
            toybox::Frame modalFrame(target, target.deviceContext(), input, interactions);
            toybox::Screen modal(modalFrame);
            timerui::confirmation(modal, tr(STR_FOCUS_SKIP_CONFIRM), tr(STR_UI_SKIP_431DE8), focusui::ApplyTimerChange,
                                  focusui::Back);
            CHECK(interactions.count() == 2);
          }
          int sessionTop = height;
          for (size_t n = 0; n < interactions.count(); ++n)
            if (interactions.data()[n].action == focusui::Planned) sessionTop = interactions.data()[n].rect.y - 8;
          bool foundTitle = false, foundCounter = false, selectedStage = false, selectedDuration = false;
          for (const auto& run : target.runs) {
            if (run.text == state.tasks[task].name) {
              foundTitle = true;
              CHECK(run.rect.height >= 2 * target.lineHeight(run.style.font));
              CHECK(run.rect.bottom() < sessionTop);
            }
            if (run.text == "1000 / 99") {
              foundCounter = true;
              CHECK(target.measureText(run.style.font, run.text.c_str(), run.style).width <= run.rect.width);
            }
            CHECK(run.text.find("Pomodoro ·") == std::string::npos);
            if (run.style.color == fui::Color::White && run.rect.y > sessionTop + 60) {
              const auto phase = pomodoro::phaseAt(state.timer.config, stage);
              const char* name = phase == pomodoro::Phase::Focus       ? tr(STR_POMODORO_STAGE_FOCUS)
                                 : phase == pomodoro::Phase::LongBreak ? tr(STR_POMODORO_STAGE_LONG)
                                                                       : tr(STR_POMODORO_STAGE_BREAK);
              selectedStage |= run.text.find(name) != std::string::npos;
              selectedDuration |= run.text.find(" MIN") != std::string::npos;
              CHECK(run.text.find('\n') == std::string::npos);
              CHECK(run.rect.height >= target.lineHeight(run.style.font));
              CHECK(run.style.maxLines == 1);
            }
          }
          if (!(foundTitle && foundCounter && selectedStage && selectedDuration))
            std::printf("timer wide=%d light=%d jersey=%d stage=%d title=%d counter=%d selected=%d duration=%d\n", wide,
                        light, jersey, stage, foundTitle, foundCounter, selectedStage, selectedDuration);
          CHECK(foundTitle && foundCounter && selectedStage && selectedDuration);
        }
}
void alignment() {
  std::array<uint8_t, 480 * 100 / 8> bits{};
  Target target(bits.data(), 480, 100, 60);
  Font font(ui_prose_14);
  target.setFont(0, font.font);
  fui::TextStyle style;
  style.font = 0;
  const auto box = fui::makeRect(10, 0, 400, 40);
  for (uint8_t n = 0; n < 3; ++n) {
    target.runs.clear();
    textalignment::line(target, box, "One word", style, n, false);
    CHECK(target.runs.size() == 1 && target.runs[0].style.align == textalignment::align(n));
  }
  for (const auto* text : {"One two three", "  One   two  three  "}) {
    target.runs.clear();
    textalignment::line(target, box, text, style, 3, false);
    CHECK(target.runs.size() == 3);
    CHECK(target.runs.front().rect.x == box.x);
    CHECK(target.runs.back().rect.right() == box.right() + 1);
  }
  target.runs.clear();
  textalignment::line(target, box, "Last paragraph line", style, 3, true);
  CHECK(target.runs.size() == 1 && target.runs[0].style.align == fui::TextAlign::Left);
}
void picrossWindows() {
  Font font(ui_prose_14);
  I18N.setLanguage(Language::EN);
  pixelui::uppercaseLabels = false;
  for (bool wide : {false, true}) {
    for (int size : {5, 10}) {
      int puzzle = 0;
      while (puzzle < picross::kPuzzleCount && picross::kPuzzles[puzzle].size != size) ++puzzle;
      CHECK(puzzle < picross::kPuzzleCount);
      picross::Board board;
      board.load(puzzle);
      const int width = wide ? 800 : 480, height = wide ? 480 : 800;
      for (int mistakes = 0; mistakes <= 2; ++mistakes) {
        std::vector<uint8_t> bits(width * height / 8, 255);
        Target target(bits.data(), width, height, width / 8);
        for (int n = 0; n < 8; ++n) target.setFont(n, font.font);
        const auto device = target.deviceContext();
        toybox::Interactions interactions;
        const fui::InputSnapshot input{};
        toybox::Frame frame(target, device, input, interactions);
        toybox::Screen screen(frame);
        picrossui::BoardModel model;
        model.board = &board;
        picrossui::Layout layout;
        picrossui::buildBoard(screen, model, layout);
        char expected[64];
        if (mistakes)
          std::snprintf(expected, sizeof(expected), tr(STR_UI_MISTAKES_D_9071EC), mistakes);
        else
          std::snprintf(expected, sizeof(expected), "%s", tr(STR_UI_NO_MISTAKES_D67257));
        pixelui::copyNatural(expected, sizeof(expected), expected);
        bool found = false;
        for (const auto& run : target.runs)
          if (run.text == expected) {
            found = true;
            const int baseline = run.rect.y + font.font.ascent;
            for (const unsigned char cp : run.text) {
              const auto& glyph = font.glyphs[cp - 32];
              if (glyph.height)
                CHECK(baseline + glyph.yOffset >= layout.repaint.y &&
                      baseline + glyph.yOffset + glyph.height <= layout.repaint.bottom());
            }
          }
        CHECK(found && board.mistakes() == mistakes);
        CHECK(layout.repaint.x >= 0 && layout.repaint.right() <= width && layout.repaint.bottom() <= height);
        if (mistakes < 2) {
          bool wrong = false;
          for (int r = 0; r < size && !wrong; ++r)
            for (int c = 0; c < size && !wrong; ++c)
              if (!board.solid(r, c) && board.cell(r, c) == picross::Cell::Blank) wrong = board.fill(r, c);
          CHECK(wrong);
        }
      }
    }
  }
}
void previews(const char* directory, bool completed = false) {
  focus::State state;
  const int project = focus::addProject(state, "Estudio CMF");
  const int second = focus::addProject(state, "Clindek");
  const int secondTask = focus::addTask(state, second, "Revisar bibliografía");
  state.tasks[secondTask].planned = 6;
  state.tasks[secondTask].completed = 2;
  state.archived[second] = {3, 0, 0};
  const char* titles[] = {"Revisar paper BRAF", "Resumen ameloblastoma", "Preparar presentación"};
  for (int n = 0; n < 3; ++n) {
    const int task = focus::addTask(state, project, titles[n]);
    state.tasks[task].planned = n == 0 ? 2 : 1;
    state.tasks[task].completed = n == 0 ? 2 : n == 1 ? 1 : 0;
    state.tasks[task].done = n == 1;
  }
  state.archived[project] = {7, 4, 3};
  for (int n = 0; n < 4; ++n) {
    const int task = focus::addTask(state, -1, titles[n % 3], focus::TaskList::Today);
    state.tasks[task].done = n == 3;
    state.tasks[task].completed = n == 3;
  }
  for (int n = 0; n < 14; ++n) {
    const int task = focus::addTask(state, -1, titles[n % 3], focus::TaskList::Tasks);
    if (n >= 12) {
      state.tasks[task].done = true;
      state.tasks[task].completed = 1;
    }
  }
  constexpr uint32_t today = 20759;
  constexpr unsigned minutes[] = {20, 45, 30, 45, 110, 5, 20};
  for (unsigned n = 0; n < 7; ++n) {
    const uint32_t day = today - 6 + n;
    state.days[day % 7] = {day, minutes[n] * 60000, uint16_t(n == 4 ? 2 : 1), 0, 0};
  }
  Font prose(ui_prose_14), small(ui_prose_10), pixelSmall(toybox_14), pixelBody(toybox_20), tile(toybox_10);
  I18N.setLanguage(Language::ES);
  for (bool wide : {false, true})
    for (bool rounded : {false, true})
      for (bool jersey : {false, true})
        for (int empty = 0; empty < 2; ++empty)
          for (auto view : {focusui::View::Menu, focusui::View::Projects, focusui::View::Tasks, focusui::View::AllTasks,
                            focusui::View::Stats, focusui::View::Settings, focusui::View::ConfirmDelete,
                            focusui::View::Today, focusui::View::Timer, focusui::View::TaskStats,
                            focusui::View::CleanupScope, focusui::View::ConfirmTimerChange}) {
            if (completed && (empty || (view != focusui::View::Today && view != focusui::View::AllTasks &&
                                        view != focusui::View::Tasks && view != focusui::View::Projects)))
              continue;
            const int width = wide ? 800 : 480, height = wide ? 480 : 800;
            std::vector<uint8_t> bits(width * height / 8, 255);
            Target target(bits.data(), width, height, width / 8);
            for (int n = 0; n < 8; ++n) target.setFont(n, jersey ? pixelSmall.font : prose.font);
            target.setFont(1, jersey ? pixelBody.font : prose.font);
            target.setFont(3, jersey ? tile.font : small.font);
            toybox::Interactions interactions;
            const fui::InputSnapshot input{};
            toybox::Frame frame(target, target.deviceContext(), input, interactions);
            toybox::Screen screen(frame);
            focusui::Work work;
            focus::State blank;
            focus::State prepared = empty ? blank : state;
            focusui::Model model{empty && view != focusui::View::Tasks ? &blank : &state};
            model.view = view;
            model.filter = completed ? focusui::Filter::Completed : focusui::Filter::All;
            model.project = 0;
            model.task = 0;
            model.day = today;
            model.deleteTarget = focusui::DeleteTarget::Completed;
            if (view == focusui::View::TaskStats) {
              model.state = &prepared;
              model.task = focus::prepareTodayFocus(prepared, "Revisar bibliografía");
              auto& task = prepared.tasks[model.task];
              task.study.day = today;
              task.study.hours[8] = 900;
              task.study.hours[9] = 1500;
              task.study.hours[14] = 1800;
              task.study.hours[19] = 600;
              task.study.seconds = empty ? 0 : 4800;
              if (empty) task.study = {};
              task.completed = empty ? 0 : 3;
              task.study.skipped = empty ? 0 : 1;
            }
            if (view == focusui::View::Timer) {
              model.state = &prepared;
              model.task = focus::prepareTodayFocus(prepared, tr(STR_FOCUS_UNTITLED));
              model.project = -1;
            }
            chrome::lightHeader = rounded;
            pixelui::uppercaseLabels = jersey;
            focusui::build(screen, model, work);
            if (model.view == focusui::View::ConfirmTimerChange) {
              toybox::Frame modalFrame(target, target.deviceContext(), input, interactions);
              toybox::Screen modal(modalFrame);
              timerui::confirmation(modal, tr(STR_FOCUS_SKIP_CONFIRM), tr(STR_UI_SKIP_431DE8),
                                    focusui::ApplyTimerChange, focusui::Back);
              CHECK(interactions.count() == 2);
            }
            char path[512];
            std::snprintf(path, sizeof(path), "%s/%s-%d-%s-%s-%s.pbm", directory,
                          view == focusui::View::Menu                 ? "home"
                          : view == focusui::View::Projects           ? "projects"
                          : view == focusui::View::Tasks              ? "project"
                          : view == focusui::View::AllTasks           ? "tasks"
                          : view == focusui::View::Today              ? "today"
                          : view == focusui::View::Timer              ? "timer"
                          : view == focusui::View::Settings           ? "settings"
                          : view == focusui::View::ConfirmDelete      ? "delete"
                          : view == focusui::View::TaskStats          ? "focus-stats"
                          : view == focusui::View::CleanupScope       ? "cleanup"
                          : view == focusui::View::ConfirmTimerChange ? "confirm-stage"
                                                                      : "stats",
                          width, rounded ? "rounded" : "square", jersey ? "jersey" : "prose",
                          completed ? "completed"
                          : empty   ? "empty"
                                    : "seeded");
            auto* file = std::fopen(path, "wb");
            CHECK(file);
            std::fprintf(file, "P4\n%d %d\n", width, height);
            for (const auto byte : bits) std::fputc(byte ^ 255, file);
            std::fclose(file);
          }
}
void weeklyStatistics() {
  focus::State state;
  constexpr uint32_t today = 20759;
  state.days[(today - 6) % 7] = {today - 6, 30 * 60000, 1, 0, 0};
  state.days[(today - 3) % 7] = {today - 3, 110 * 60000, 4, 0, 0};
  state.days[today % 7] = {today, 10 * 60000, 1, 0, 0};
  state.days[(today - 8) % 7] = {today - 8, 200 * 60000, 7, 0, 0};
  Font prose(ui_prose_14), small(ui_prose_10);
  for (const auto language : {Language::EN, Language::ES})
    for (bool wide : {false, true}) {
      const int width = wide ? 800 : 480, height = wide ? 480 : 800;
      std::vector<uint8_t> bits(width * height / 8, 255);
      Target target(bits.data(), width, height, width / 8);
      for (int n = 0; n < 8; ++n) target.setFont(n, prose.font);
      target.setFont(3, small.font);
      toybox::Interactions interactions;
      const fui::InputSnapshot input{};
      toybox::Frame frame(target, target.deviceContext(), input, interactions);
      toybox::Screen screen(frame);
      focusui::Work work;
      focusui::Model model{&state};
      model.view = focusui::View::Stats;
      model.day = today;
      I18N.setLanguage(language);
      focusui::build(screen, model, work);
      if (model.view == focusui::View::ConfirmTimerChange) {
        toybox::Frame modalFrame(target, target.deviceContext(), input, interactions);
        toybox::Screen modal(modalFrame);
        timerui::confirmation(modal, tr(STR_FOCUS_SKIP_CONFIRM), tr(STR_UI_SKIP_431DE8), focusui::ApplyTimerChange,
                              focusui::Back);
        CHECK(interactions.count() == 2);
      }
      bool sessions = false, time = false, consistency = false;
      for (const auto& run : target.runs) {
        sessions |= run.text == "6";
        time |= run.text == "2 h 30 min";
        consistency |= run.text == "42 %";
      }
      CHECK(sessions && time && consistency);
      for (const auto& r : target.outlines) CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
    }
}
void filters() {
  focus::State state;
  const int pending = focus::addProject(state, "Pending");
  const int complete = focus::addProject(state, "Completed");
  const int empty = focus::addProject(state, "Empty");
  const int archived = focus::addProject(state, "Retained completed totals");
  focus::addTask(state, pending, "Pending task");
  const int task = focus::addTask(state, complete, "Completed task");
  state.tasks[task].done = true;
  state.archived[archived] = {2, 4, 3};
  focusui::Model model{&state};
  model.view = focusui::View::Projects;
  CHECK(focusui::count(model) == 4);
  model.filter = focusui::Filter::Completed;
  CHECK(focusui::count(model) == 2);
  CHECK(!focusui::projectMatches(state, empty, model.filter));
  CHECK(!focusui::projectMatches(state, -1, model.filter));
  CHECK(!focusui::projectMatches(state, focus::kProjects, model.filter));
  state.tasks[task].done = false;
  CHECK(focusui::count(model) == 1);
  CHECK(focus::purgeCompleted(state) == 0);
  state.tasks[task].done = true;
  CHECK(focus::purgeCompleted(state) == 1);
  CHECK(focusui::count(model) == 2);
}
void taskFilters() {
  focus::State state;
  const int project = focus::addProject(state, "Project");
  for (const auto list : {focus::TaskList::Today, focus::TaskList::Tasks, focus::TaskList::Project}) {
    for (int n = 0; n < 3; ++n) {
      const int task = focus::addTask(state, project, "Task", list);
      CHECK(task >= 0);
      state.tasks[task].done = n == 1;
    }
  }
  for (const auto view : {focusui::View::Today, focusui::View::AllTasks, focusui::View::Tasks}) {
    focusui::Model model{&state};
    model.view = view;
    model.project = project;
    CHECK(focusui::count(model) == 3);
    model.filter = focusui::Filter::Completed;
    CHECK(focusui::count(model) == 1);
  }
}
void isolatedStatistics() {
  focus::State state;
  const int selected = focus::prepareTodayFocus(state, "Selected focus");
  const int other = focus::addTask(state, -1, "Other focus", focus::TaskList::Tasks);
  state.tasks[selected].study = {};
  state.tasks[selected].study.seconds = 90 * 60;
  state.tasks[selected].study.day = 20759;
  state.tasks[selected].study.hours[10] = 1800;
  state.tasks[selected].completed = 3;
  state.tasks[selected].study.skipped = 2;
  state.tasks[other].study.seconds = 36000;
  state.tasks[other].completed = 20;
  Font prose(ui_prose_14), small(ui_prose_10);
  for (const auto language : {Language::EN, Language::ES}) {
    std::vector<uint8_t> bits(480 * 800 / 8, 255);
    Target target(bits.data(), 480, 800, 60);
    for (int n = 0; n < 8; ++n) target.setFont(n, prose.font);
    target.setFont(3, small.font);
    toybox::Interactions interactions;
    const fui::InputSnapshot input{};
    toybox::Frame frame(target, target.deviceContext(), input, interactions);
    toybox::Screen screen(frame);
    focusui::Work work;
    focusui::Model model{&state};
    model.view = focusui::View::TaskStats;
    model.task = selected;
    model.day = 20759;
    I18N.setLanguage(language);
    focusui::build(screen, model, work);
    if (model.view == focusui::View::ConfirmTimerChange) {
      toybox::Frame modalFrame(target, target.deviceContext(), input, interactions);
      toybox::Screen modal(modalFrame);
      timerui::confirmation(modal, tr(STR_FOCUS_SKIP_CONFIRM), tr(STR_UI_SKIP_431DE8), focusui::ApplyTimerChange,
                            focusui::Back);
      CHECK(interactions.count() == 2);
    }
    bool time = false, sessions = false, skipped = false;
    for (const auto& run : target.runs) {
      time |= run.text == "1 h 30 min";
      sessions |= run.text == "3";
      skipped |= run.text == "2";
      CHECK(run.text != "20" && run.text != "10 h 0 min");
    }
    CHECK(time && sessions && skipped);
  }
}
void pomodoroPreviews(const char* directory) {
  Font prose(ui_prose_14), small(ui_prose_10), pixelSmall(toybox_14), pixelBody(toybox_20), tile(toybox_10);
  pomodoro::State state;
  state.config = {35, 7, 20, 5, true, false};
  for (const auto language : {Language::EN, Language::ES})
    for (bool wide : {false, true})
      for (bool rounded : {false, true})
        for (bool jersey : {false, true})
          for (bool failed : {false, true}) {
            const int width = wide ? 800 : 480, height = wide ? 480 : 800;
            std::vector<uint8_t> bits(width * height / 8, 255);
            Target target(bits.data(), width, height, width / 8);
            for (int n = 0; n < 8; ++n) target.setFont(n, jersey ? pixelSmall.font : prose.font);
            target.setFont(1, jersey ? pixelBody.font : prose.font);
            target.setFont(3, jersey ? tile.font : small.font);
            toybox::Interactions interactions;
            const fui::InputSnapshot input{};
            toybox::Frame frame(target, target.deviceContext(), input, interactions);
            toybox::Screen screen(frame);
            pomodoroui::Work work;
            I18N.setLanguage(language);
            chrome::lightHeader = rounded;
            pixelui::uppercaseLabels = jersey;
            pomodoroui::build(screen, {&state, pomodoroui::View::Settings, failed}, work);
            char path[512];
            std::snprintf(path, sizeof(path), "%s/pomodoro-settings-%d-%s-%s-%s-%s.pbm", directory, width,
                          rounded ? "rounded" : "square", jersey ? "jersey" : "prose",
                          language == Language::ES ? "es" : "en", failed ? "error" : "seeded");
            auto* file = std::fopen(path, "wb");
            CHECK(file);
            std::fprintf(file, "P4\n%d %d\n", width, height);
            for (const auto byte : bits) std::fputc(byte ^ 255, file);
            std::fclose(file);
          }
}
}  // namespace
int main(int argc, char** argv) {
  screens();
  timerLimits();
  alignment();
  picrossWindows();
  weeklyStatistics();
  filters();
  taskFilters();
  isolatedStatistics();
  if (argc == 3 && !std::strcmp(argv[1], "--render")) {
    previews(argv[2]);
    previews(argv[2], true);
    pomodoroPreviews(argv[2]);
  }
  std::printf("Focus screens, touch routing and text alignment: %u checks\n", checks);
}

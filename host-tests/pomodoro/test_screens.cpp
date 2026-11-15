#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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

void save(const char* directory, const char* name, int width, int height, const std::vector<uint8_t>& bits) {
  if (!directory) return;
  char path[512];
  std::snprintf(path, sizeof(path), "%s/%s-%d.pbm", directory, name, width);
  FILE* file = std::fopen(path, "wb");
  CHECK(file);
  std::fprintf(file, "P4\n%d %d\n", width, height);
  for (uint8_t b : bits) std::fputc(b ^ 255, file);
  std::fclose(file);
}
void screens(const char* directory) {
  Font prose(ui_prose_14), small(ui_prose_10), pixel(toybox_14), tiny(toybox_10), title(toybox_20);
  pomodoro::State state;
  state.completed = 123;
  pomodoro::Stats stats, accumulation;
  pomodoro::DisplayOptions options;
  const int today = readingstats::dayIndex({2026, 11, 5});
  for (bool rounded : {false, true})
    for (bool spanish : {false, true})
      for (bool large : {false, true})
        for (int width : {480, 800})
          for (auto view : {pomodoroui::View::Timer, pomodoroui::View::Settings, pomodoroui::View::ConfirmReset,
                            pomodoroui::View::ConfirmSkip, pomodoroui::View::Stats}) {
            const int height = width == 480 ? 800 : 480, stride = (width + 7) / 8;
            chrome::lightHeader = rounded;
            I18N.setLanguage(spanish ? Language::ES : Language::EN);
            for (int period = 0; period < (view == pomodoroui::View::Stats ? 3 : 1); ++period) {
              stats.begin(static_cast<pomodoro::Period>(period), today);
              accumulation.begin(options.accumulation, today);
              for (int n = 0; n < 65; ++n) {
                const pomodoro::Delta delta{today - n, uint64_t(n ? (n * 17) % 120 : 75) * 60000,
                                            uint32_t(n ? n % 5 : 3)};
                stats.add(delta);
                accumulation.add(delta);
              }
              std::vector<uint8_t> bits(stride * height, 255);
              Target target(bits.data(), width, height, stride);
              target.setFont(0, large ? pixel.font : prose.font);
              target.setFont(1, large ? title.font : prose.font);
              target.setFont(2, large ? pixel.font : prose.font);
              target.setFont(3, large ? tiny.font : small.font);
              target.setFont(4, large ? prose.font : small.font);
              toybox::Interactions interactions;
              fui::InputSnapshot input;
              toybox::Frame frame(target, target.deviceContext(), input, interactions);
              toybox::Screen screen(frame);
              pomodoroui::Work work;
              pomodoroui::build(screen, {&state, view, false, 0, today, &accumulation, &stats, false, false, &options},
                                work);
              if (view == pomodoroui::View::ConfirmReset || view == pomodoroui::View::ConfirmSkip) {
                toybox::Frame modalFrame(target, target.deviceContext(), input, interactions);
                toybox::Screen modal(modalFrame);
                timerui::confirmation(
                    modal,
                    view == pomodoroui::View::ConfirmSkip ? tr(STR_POMODORO_SKIP_CONFIRM)
                                                          : tr(STR_FOCUS_RESTART_CONFIRM),
                    view == pomodoroui::View::ConfirmSkip ? tr(STR_UI_SKIP_431DE8) : tr(STR_UI_RESTART_69ED00),
                    pomodoroui::Confirm, pomodoroui::Cancel);
                CHECK(interactions.count() == 2);
              }
              CHECK(!interactions.overflowed());
              for (const auto& r : target.outlines) {
                CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
              }
              if (view == pomodoroui::View::Stats && stats.period == pomodoro::Period::Month) {
                const int offset = readingstats::weekday(stats.first), rows = (offset + stats.count + 6) / 7;
                for (int day = 0; day < stats.count; ++day) {
                  const int cell = offset + day;
                  const int x = work.calendarGrid.x + work.calendarGrid.width * (2 * (cell % 7) + 1) / 14;
                  const int y = work.calendarGrid.y + work.calendarGrid.height * (2 * (cell / 7) + 1) / (2 * rows);
                  CHECK(pomodoroui::calendarDay(stats, work.calendarGrid, x, y) == stats.first + day);
                }
                CHECK(pomodoroui::calendarDay(stats, work.calendarGrid, work.calendarGrid.right(),
                                              work.calendarGrid.y) == 0);
              }
              for (size_t n = 0; n < interactions.count(); ++n) {
                const auto& hit = interactions.data()[n];
                CHECK(hit.rect.x >= 0 && hit.rect.y >= 0 && hit.rect.right() <= width && hit.rect.bottom() <= height);
              }
              if (view == pomodoroui::View::Timer) {
                CHECK(interactions.count() >= 8);
                bool oldSettings = false;
                for (const auto& r : target.runs)
                  if (r.text == tr(STR_UI_SETTINGS_C4B924)) oldSettings = true;
                CHECK(!oldSettings);
                for (const auto& run : target.runs)
                  if (run.text == tr(STR_POMODORO_THIS_WEEK)) CHECK(run.style.align == fui::TextAlign::Center);
                pomodoro::Pending live;
                live.add(today, 120000, 0);
                accumulation.add(pomodoro::Delta{today, 120000, 0});
                pomodoroui::Model held{&state, view,  false, 0,        today, &accumulation,
                                       &stats, false, false, &options, false, &live};
                target.runs.clear();
                toybox::Frame heldFrame(target, target.deviceContext(), input, interactions);
                toybox::Screen heldScreen(heldFrame);
                pomodoroui::build(heldScreen, held, work);
                char settled[64];
                std::snprintf(settled, sizeof(settled), tr(STR_FOCUS_DURATION_HOURS), 2u, 57u);
                bool foundSettled = false;
                for (const auto& run : target.runs)
                  if (run.text == settled) {
                    foundSettled = true;
                    CHECK(run.style.align == fui::TextAlign::Center);
                  }
                CHECK(foundSettled);
                accumulation.milliseconds -= 120000;
                accumulation.buckets[today - accumulation.plotFirst] -= 120000;

                for (const auto& outline : target.outlines) CHECK(outline.height > 20);
                pomodoroui::Model picker{&state, view,  false, 0,        today, &accumulation,
                                         &stats, false, false, &options, true};
                toybox::Frame pickFrame(target, target.deviceContext(), input, interactions);
                toybox::Screen pickScreen(pickFrame);
                pomodoroui::build(pickScreen, picker, work);
                int choices = 0;
                for (size_t n = 0; n < interactions.count(); ++n)
                  if (interactions.data()[n].action == pomodoroui::SelectAccumulation) ++choices;
                CHECK(choices == 4 && !interactions.overflowed());
                options.showAccumulation = false;
                picker.choosingAccumulation = false;
                toybox::Frame hiddenFrame(target, target.deviceContext(), input, interactions);
                toybox::Screen hiddenScreen(hiddenFrame);
                pomodoroui::build(hiddenScreen, picker, work);
                for (size_t n = 0; n < interactions.count(); ++n)
                  CHECK(interactions.data()[n].action != pomodoroui::ChooseAccumulation);
                options.showAccumulation = true;
              }
              if (view == pomodoroui::View::Timer) {
                std::fill(bits.begin(), bits.end(), 255);
                toybox::Frame finalFrame(target, target.deviceContext(), input, interactions);
                toybox::Screen finalScreen(finalFrame);
                pomodoroui::build(finalScreen,
                                  {&state, view, false, 0, today, &accumulation, &stats, false, false, &options}, work);
              }
              if (view == pomodoroui::View::Settings) {
                auto spacing = [&]() {
                  int automaticBottom = 0;
                  fui::Rect visibility{}, done{};
                  for (size_t n = 0; n < interactions.count(); ++n) {
                    const auto& hit = interactions.data()[n];
                    if (hit.action == pomodoroui::Change && hit.value >= 8)
                      automaticBottom = std::max(automaticBottom, int(hit.rect.bottom()));
                    if (hit.action == pomodoroui::ToggleAccumulation) visibility = hit.rect;
                    if (hit.action == pomodoroui::Done) done = hit.rect;
                  }
                  CHECK(visibility.y >= automaticBottom + 12);
                  if (width > height)
                    CHECK(visibility.right() + 12 <= done.x);
                  else
                    CHECK(done.y >= visibility.bottom() + 12);
                };
                spacing();
                toybox::Frame errorFrame(target, target.deviceContext(), input, interactions);
                toybox::Screen errorScreen(errorFrame);
                pomodoroui::build(errorScreen,
                                  {&state, view, true, 0, today, &accumulation, &stats, false, false, &options}, work);
                CHECK(!interactions.overflowed());
                spacing();
              }
              std::fill(bits.begin(), bits.end(), 255);
              toybox::Frame cleanFrame(target, target.deviceContext(), input, interactions);
              toybox::Screen cleanScreen(cleanFrame);
              pomodoroui::build(cleanScreen,
                                {&state, view, false, 0, today, &accumulation, &stats, false, false, &options}, work);
              if (directory && rounded && spanish && !large) {
                char name[96];
                std::snprintf(name, sizeof(name), "%s-%d",
                              view == pomodoroui::View::Timer      ? "timer"
                              : view == pomodoroui::View::Stats    ? "history"
                              : view == pomodoroui::View::Settings ? "settings"
                                                                   : "confirm",
                              period);
                save(directory, name, width, height, bits);
              }
            }
          }
}
}  // namespace
int main(int argc, char** argv) {
  screens(argc == 3 && !std::strcmp(argv[1], "--render") ? argv[2] : nullptr);
  std::printf("Pomodoro UI: %u checks\n", checks);
}

#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/habits/HabitsScreens.h"

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
        device(target.deviceContext()),
        frame(target, device, input, interactions),
        screen(frame) {
    target.setFont(toybox::kSmallFont, small.font);
    target.setFont(toybox::kUiFont, body.font);
    target.setFont(toybox::kDisplayFont, title.font);
    target.setFont(3, tiny.font);
  }

  void checkHits() {
    CHECK(!interactions.overflowed());
    paintclock::notePainted();
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& slot = interactions.data()[n];
      const auto& r = slot.rect;
      CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
      CHECK(r.width >= 44 && r.height >= 44);
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

void seed(habits::Store& store, const int today) {
  static constexpr const char* names[] = {"Exercise",      "Eat mindfully", "Read",          "Drink water",
                                          "Sleep on time", "Take a walk",   "Stretch",       "Practice piano",
                                          "Journal",       "Meditate",      "Learn English", "Less screen time"};
  for (int n = 0; n < habits::kMaxHabits; ++n) habits::add(store, names[n], today - 45);
  for (int day = today - 45; day <= today; ++day) {
    for (int n = 0; n < store.count; ++n) {
      if ((day + n) % 5 != 0 && !(day == today && n % 2)) habits::toggle(store, n, day, today);
    }
  }
  for (int day = today - 6; day <= today; ++day)
    if (!habits::checked(store, 0, day)) habits::toggle(store, 0, day, today);
}
}  // namespace

int main(int argc, char** argv) {
  habits::Store store;
  const int today = habits::dayIndex(2026, 10, 15);
  seed(store, today);
  for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    const int w = size.first, h = size.second;
    Canvas first(w, h);
    habitsui::buildToday(first.screen, {&store, today, 0, false});
    first.checkHits();
    CHECK(first.has(habitsui::ActionAdd));
    CHECK(first.has(habitsui::ActionToggleToday, 0));
    CHECK(first.has(habitsui::ActionOpen, 0));
    const int capacity = habitsui::todayCapacity(first.device);
    const int last = (store.count - 1) / capacity;
    Canvas lastPage(w, h);
    habitsui::buildToday(lastPage.screen, {&store, today, last, false});
    lastPage.checkHits();
    CHECK(lastPage.has(habitsui::ActionOpen, store.count - 1));
    CHECK(!lastPage.has(habitsui::ActionPage, 1));
    Canvas details(w, h);
    habitsui::buildDetail(details.screen, {&store, 0, today, today - habits::weekday(today) - 7, today});
    details.checkHits();
    CHECK(details.has(habitsui::ActionToggleDay));
    CHECK(details.has(habitsui::ActionCalendar));
    CHECK(!details.has(habitsui::ActionDay, today + 1 - habits::kMinDay));
    Canvas choices(w, h);
    habitsui::buildPresets(choices.screen, 0);
    choices.checkHits();
    Canvas custom(w, h);
    const int presetLast = habitsui::kPresetCount / habitsui::presetCapacity(custom.device);
    habitsui::buildPresets(custom.screen, presetLast);
    custom.checkHits();
    CHECK(custom.has(habitsui::ActionCustom, habitsui::kPresetCount));
    Canvas menu(w, h), confirm(w, h), notice(w, h);
    habitsui::buildMenu(menu.screen, store.items[0].name);
    menu.checkHits();
    habitsui::buildDelete(confirm.screen, store.items[0].name);
    confirm.checkHits();
    habitsui::buildMessage(notice.screen, "HABITS", "Check your SD card, then try again.", habitsui::ActionRetry,
                           "TRY AGAIN");
    notice.checkHits();
    habits::Store empty;
    Canvas onboarding(w, h);
    habitsui::buildToday(onboarding.screen, {&empty, today, 0, false});
    onboarding.checkHits();
    CHECK(onboarding.has(habitsui::ActionAdd));
    if (argc > 1) {
      const std::string prefix = std::string(argv[1]) + "/" + std::to_string(w);
      first.write(prefix + "-today.pbm");
      details.write(prefix + "-progress.pbm");
      choices.write(prefix + "-new.pbm");
      onboarding.write(prefix + "-empty.pbm");
      menu.write(prefix + "-menu.pbm");
      confirm.write(prefix + "-delete.pbm");
    }
  }
  std::printf("Habits UI: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}

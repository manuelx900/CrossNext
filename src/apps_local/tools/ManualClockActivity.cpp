#include "ManualClockActivity.h"

#include <HalClock.h>
#include <I18n.h>

#include "../reading/ReadingStatsCore.h"
#include "../ui/GameFrontDoor.h"
#include "../ui/PixelMenuTheme.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/SideButtonNavigation.h"

namespace fui = freeink::ui;
namespace {
constexpr fui::ActionId Step = 1, Save = 2;
}
void ManualClockActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  if (!halClock.localTime(value)) {
    value.tm_year = 126;
    value.tm_mon = 0;
    value.tm_mday = 1;
  }
  value.tm_year = std::clamp(value.tm_year, 120, 199);
  value.tm_mday = std::min(value.tm_mday, readingstats::daysInMonth(value.tm_year + 1900, value.tm_mon + 1));
  requestUpdate();
}
void ManualClockActivity::step(int field, int delta) {
  switch (field) {
    case 0:
      value.tm_year = std::clamp(value.tm_year + delta, 120, 199);
      break;
    case 1:
      value.tm_mon = std::clamp(value.tm_mon + delta, 0, 11);
      break;
    case 2:
      value.tm_mday += delta;
      break;
    case 3:
      value.tm_hour = std::clamp(value.tm_hour + delta, 0, 23);
      break;
    case 4:
      value.tm_min = std::clamp(value.tm_min + delta, 0, 59);
      break;
    default:
      return;
  }
  value.tm_mday = std::clamp(value.tm_mday, 1, readingstats::daysInMonth(value.tm_year + 1900, value.tm_mon + 1));
  selected = field;
  failed = false;
  ready = false;
  requestUpdate();
}
void ManualClockActivity::commit() {
  bool ok;
  {
    RenderLock lock(*this);
    ok = halClock.setLocalTime(value);
  }
  if (ok) {
    SETTINGS.clockHasBeenSynced = 1;
    SETTINGS.saveToFile();
    finish();
  } else {
    failed = true;
    requestUpdate();
  }
}
void ManualClockActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (!ready) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selected = (selected + 4) % 5;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selected = (selected + 1) % 5;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) step(selected, -1);
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) step(selected, 1);
  if ((!interactions.hasButtonFocus() && mappedInput.wasReleased(MappedInputManager::Button::Confirm))) {
    commit();
    return;
  }
  int x, y;
  if (!sidebuttons::poll(interactions, mappedInput, x, y)) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = x;
  input.touchY = y;
  const auto e = sidebuttons::route(interactions, input, mappedInput, *this);
  if (e.action == Step)
    step(e.value / 2, e.value % 2 ? 1 : -1);
  else if (e.action == Save)
    commit();
}
void ManualClockActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  const fui::InputSnapshot input{};
  toybox::Frame frame(target, target.deviceContext(), input, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  frontdoor::begin(screen, tr(STR_CLOCK_MANUAL));
  const auto b = screen.body();
  const bool wide = b.width > b.height;
  static constexpr StrId names[] = {StrId::STR_CLOCK_YEAR, StrId::STR_CLOCK_MONTH, StrId::STR_CLOCK_DAY,
                                    StrId::STR_CLOCK_HOUR, StrId::STR_CLOCK_MINUTE};
  const int values[] = {value.tm_year + 1900, value.tm_mon + 1, value.tm_mday, value.tm_hour, value.tm_min};
  const int rowH = wide ? 70 : 84, gap = 10, cols = wide ? 2 : 1, width = (b.width - (cols - 1) * gap) / cols;
  for (int n = 0; n < 5; ++n) {
    const auto r = fui::makeRect(b.x + n % cols * (width + gap), b.y + n / cols * (rowH + gap), width, rowH);
    dashboard::frame(screen, r, n == selected ? 3 : 2);
    pixelui::text(target, fui::makeRect(r.x + 60, r.y + 2, r.width - 120, 28), I18N.get(names[n]), 3, fui::Color::Black,
                  fui::TextAlign::Center);
    char label[16];
    std::snprintf(label, sizeof(label), n == 0 ? "%04d" : "%02d", values[n]);
    pixelui::text(target, fui::makeRect(r.x + 60, r.y + 30, r.width - 120, rowH - 32), label, 1, fui::Color::Black,
                  fui::TextAlign::Center);
    dashboard::button(screen, fui::makeRect(r.x + 6, r.y + 10, 48, rowH - 20), "-", Step, n * 2);
    dashboard::button(screen, fui::makeRect(r.right() - 54, r.y + 10, 48, rowH - 20), "+", Step, n * 2 + 1);
  }
  if (failed) pixelui::text(target, fui::makeRect(b.x, b.bottom() - 104, b.width, 38), tr(STR_CLOCK_MANUAL_FAILED), 3);
  dashboard::button(screen, fui::makeRect(b.x, b.bottom() - 58, b.width, 58), tr(STR_CONFIRM), Save, 0, true);
  ready = true;
  toybox::reportOverflow(interactions, "Manual clock", &renderer);
  renderer.displayBuffer();
}

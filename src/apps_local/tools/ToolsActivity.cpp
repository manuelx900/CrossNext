#include "ToolsActivity.h"

#include <CrossPointSettings.h>
#include <HalClock.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <ctime>

#include "../Shelf.h"
#include "../reading/ReadingStatsStore.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/Rotation.h"
#include "components/SideButtonNavigation.h"
namespace fui = freeink::ui;
namespace {
std::unique_ptr<Activity> create(GfxRenderer& r, MappedInputManager& i, toolsui::Kind k) {
  auto app = makeUniqueNoThrow<ToolsActivity>(r, i, k);
  if (!app) LOG_ERR("TOOLS", "OOM: %u bytes", unsigned(sizeof(ToolsActivity)));
  return app;
}
}  // namespace
std::unique_ptr<Activity> ToolsActivity::calendar(GfxRenderer& r, MappedInputManager& i) {
  return create(r, i, toolsui::Kind::Calendar);
}
std::unique_ptr<Activity> ToolsActivity::clock(GfxRenderer& r, MappedInputManager& i) {
  return create(r, i, toolsui::Kind::Clock);
}
std::unique_ptr<Activity> ToolsActivity::counter(GfxRenderer& r, MappedInputManager& i) {
  return create(r, i, toolsui::Kind::Counter);
}
void ToolsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  model.kind = kind;
  model.sunday = SETTINGS.weekStartsSunday;
  model.failed = !tools::load(state, writable);
  model.analog = state.analog;
  model.twelveHour = state.twelveHour;
  model.count = state.count;
  originalOrientation = int(renderer.getOrientation());
  if (kind == toolsui::Kind::Clock) {
    if (state.orientation != (originalOrientation + rotation::step(SETTINGS.rotationDirection != 0)) % 4)
      state.orientation = originalOrientation;
    renderer.setOrientation(static_cast<GfxRenderer::Orientation>(state.orientation));
  }
  updateTime(true);
  model.selected = model.today;
  requestUpdate();
}
void ToolsActivity::onExit() {
  persist();
  renderer.setOrientation(static_cast<GfxRenderer::Orientation>(originalOrientation));
  Activity::onExit();
}
void ToolsActivity::persist() {
  if (!dirty) return;
  model.failed = !tools::save(state, writable);
  touched = millis();
  if (!model.failed) dirty = false;
}
void ToolsActivity::updateTime(bool resetMonth) {
  tm local{};
  const auto now = std::time(nullptr);
  const bool valid = (now > 1577836800 && localtime_r(&now, &local)) || halClock.localTime(local);
  model.validTime = valid;
  model.today = valid ? readingstats::dayIndex({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday}) : 0;
  if (resetMonth || kind == toolsui::Kind::Clock) {
    model.year = valid ? local.tm_year + 1900 : 2026;
    model.month = valid ? local.tm_mon + 1 : 1;
  }
  model.day = local.tm_mday;
  model.hour = local.tm_hour;
  model.minute = local.tm_min;
}
void ToolsActivity::action(fui::ActionId a, int value) {
  using namespace toolsui;
  switch (a) {
    case Previous:
    case Next: {
      int n = model.year * 12 + model.month - 1 + (a == Next ? 1 : -1);
      n = std::max(2020 * 12, std::min(2199 * 12 + 11, n));
      model.year = n / 12;
      model.month = n % 12 + 1;
      break;
    }
    case Today:
      updateTime(true);
      model.selected = model.today;
      break;
    case SelectDay:
      if (value < 1 || value > readingstats::daysInMonth(model.year, model.month)) return;
      model.selected = readingstats::dayIndex({model.year, model.month, value});
      break;
    case HourFormat:
      state.twelveHour = !state.twelveHour;
      model.twelveHour = state.twelveHour;
      dirty = true;
      persist();
      break;
    case WeekStart:
      SETTINGS.weekStartsSunday = !SETTINGS.weekStartsSunday;
      model.sunday = SETTINGS.weekStartsSunday;
      SETTINGS.saveToFile();
      break;
    case Appearance:
      state.analog = !state.analog;
      model.analog = state.analog;
      model.twelveHour = state.twelveHour;
      dirty = true;
      persist();
      break;
    case Rotate:
      state.orientation =
          rotation::toggle(int(renderer.getOrientation()), originalOrientation, SETTINGS.rotationDirection != 0);
      renderer.setOrientation(static_cast<GfxRenderer::Orientation>(state.orientation));
      dirty = true;
      persist();
      break;
    case Add:
    case Subtract:
      if ((a == Add && state.count >= 999999999) || (a == Subtract && state.count == 0)) return;
      undoCount = state.count;
      canUndo = true;
      state.count += a == Add ? 1 : -1;
      dirty = true;
      break;
    case Undo:
      if (!canUndo) return;
      state.count = undoCount;
      canUndo = false;
      dirty = true;
      break;
    case Reset:
      model.confirm = true;
      break;
    case ConfirmReset:
      undoCount = state.count;
      canUndo = true;
      state.count = 0;
      model.confirm = false;
      dirty = true;
      persist();
      break;
    case CancelReset:
      model.confirm = false;
      break;
    default:
      return;
  }
  touched = millis();
  model.count = state.count;
  if (a != Add && a != Subtract && a != Undo) ready = false;
  requestUpdate();
}
void ToolsActivity::counterLoop() {
  using Kind = tools::CounterInput::Kind;
  bool queued = true;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back))
    queued = counterInput.push(Kind::Back);
  else if (ready.load()) {
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y))
      queued = counterInput.push(Kind::Touch, x, y);
    else if (const int step = sidebuttons::shortStep(mappedInput))
      queued = counterInput.push(!model.confirm ? Kind::Add : step > 0 ? Kind::FocusNext : Kind::FocusPrevious);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm))
      queued = counterInput.push(model.confirm ? Kind::Confirm : Kind::Add);
  }
  if (!queued) LOG_ERR("TOOLS", "Counter input queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  tools::CounterInput::Event event;
  while (counterInput.pop(event)) {
    if (event.kind == Kind::Back) {
      counterInput.clear();
      if (model.confirm)
        action(toolsui::CancelReset);
      else {
        persist();
        shelf::leave(renderer, mappedInput);
      }
      return;
    }
    if (!ready.load()) continue;
    if (event.kind == Kind::Add)
      action(toolsui::Add);
    else {
      freeink::ui::InputSnapshot snapshot{};
      snapshot.touchReleased = event.kind == Kind::Touch;
      snapshot.focusNext = event.kind == Kind::FocusNext;
      snapshot.focusPrev = event.kind == Kind::FocusPrevious;
      snapshot.confirm = event.kind == Kind::Confirm;
      snapshot.touchX = event.x;
      snapshot.touchY = event.y;
      if (snapshot.touchReleased) interactions.setFocusedIndex(-1);
      const auto before = interactions.focusPosition();
      const auto hit = interactions.route(snapshot);
      if (before != interactions.focusPosition()) requestUpdate();
      action(hit.action, hit.value);
    }
    if (!ready.load()) {
      counterInput.clear();
      return;
    }
  }
  if (dirty && uint32_t(millis() - touched) > (model.failed ? 10000u : 800u)) {
    const bool failed = model.failed;
    persist();
    if (model.failed != failed) {
      ready = false;
      requestUpdate();
    }
  }
}
void ToolsActivity::loop() {
  if (kind == toolsui::Kind::Counter) {
    counterLoop();
    return;
  }
  RenderLock lock(*this);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (model.confirm) {
      action(toolsui::CancelReset);
      return;
    }
    persist();
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (dirty && uint32_t(millis() - touched) > (model.failed ? 10000u : 800u)) {
    persist();
    requestUpdate();
  }
  if (uint32_t(millis() - polled) > 1000) {
    polled = millis();
    const int minute = model.minute, day = model.today;
    updateTime(false);
    if (minute != model.minute || day != model.today) requestUpdate();
  }
  if ((!interactions.hasButtonFocus() && mappedInput.wasReleased(MappedInputManager::Button::Confirm)))
    action(kind == toolsui::Kind::Counter ? toolsui::Add
           : kind == toolsui::Kind::Clock ? toolsui::Appearance
                                          : toolsui::Today);
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) action(toolsui::Previous);
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) action(toolsui::Next);
  if (ready && !model.confirm) {
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      if (kind == toolsui::Kind::Calendar) {
        action(direction > 0 ? toolsui::Next : toolsui::Previous);
        return;
      }
      if (kind == toolsui::Kind::Counter) {
        action(toolsui::Add);
        return;
      }
      if (kind == toolsui::Kind::Clock) {
        action(toolsui::Appearance);
        return;
      }
    }
  }
  int x, y;
  if (!ready || !sidebuttons::poll(interactions, mappedInput, x, y, kind != toolsui::Kind::Clock)) return;
  fui::InputSnapshot in;
  in.touchReleased = true;
  in.touchX = x;
  in.touchY = y;
  const auto event = sidebuttons::route(interactions, in, mappedInput, *this, kind != toolsui::Kind::Clock);
  const auto device = toybox::makeTarget(renderer).deviceContext();
  action(event.action, event.action == toolsui::SelectDay ? toolsui::calendarDayAt(device, model, x, y) : event.value);
}
void ToolsActivity::render(RenderLock&&) {
  model.rotateRight = SETTINGS.rotationDirection != 0;
  if (kind == toolsui::Kind::Clock) interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const fui::InputSnapshot in{};
  toybox::Frame frame(target, target.deviceContext(), in, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  toolsui::build(screen, model, work);
  toybox::reportOverflow(interactions, "Tools", &renderer);
  renderer.displayBuffer();
  ready = true;
}

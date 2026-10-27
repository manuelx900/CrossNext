#include "PomodoroActivity.h"

#include <I18n.h>
#include <Memory.h>

#include <algorithm>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "util/LoopDeadline.h"
std::unique_ptr<Activity> PomodoroActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  // The activity stack owns the fixed UI/save workspace for this screen's lifetime.
  auto activity = makeUniqueNoThrow<PomodoroActivity>(renderer, input);
  if (!activity) LOG_ERR("POMODORO", "OOM: activity");
  return activity;
}
void PomodoroActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  saveFailed = !store.load(state);
  stagePage = state.stage / 3;
  lastTick = lastSave = millis();
  requestUpdate();
}
void PomodoroActivity::save() {
  if (!dirty) return;
  saveFailed = !store.save(state);
  if (!saveFailed) dirty = false;
  lastSave = millis();
  saveSoon = false;
}
void PomodoroActivity::pause() {
  if (state.running) {
    pomodoro::advance(state, millis() - lastTick);
    state.running = false;
    dirty = true;
  }
  lastTick = millis();
  save();
}
void PomodoroActivity::onExit() {
  pause();
  Activity::onExit();
}
void PomodoroActivity::prepareForSleep() { pause(); }
void PomodoroActivity::action(freeink::ui::ActionId action, int value) {
  const auto previousView = view;
  const int previousPage = stagePage;
  switch (action) {
    case pomodoroui::SelectStage:
      if (value < 0 || value >= 2 * state.config.rounds || value == state.stage) return;
      pause();
      pomodoro::selectStage(state, value);
      stagePage = state.stage / 3;
      dirty = true;
      save();
      break;
    case pomodoroui::StagePage: {
      const int next = std::clamp(stagePage + value, 0, (2 * state.config.rounds - 1) / 3);
      if (next == stagePage) return;
      stagePage = next;
    } break;
    case pomodoroui::Toggle:
      state.running = !state.running;
      state.notice = false;
      lastTick = millis();
      dirty = true;
      break;
    case pomodoroui::Settings:
      pause();
      view = pomodoroui::View::Settings;
      break;
    case pomodoroui::Change:
      pomodoro::configure(state, value / 2, value % 2 ? 1 : -1);
      dirty = true;
      break;
    case pomodoroui::Done:
      stagePage = state.stage / 3;
      view = pomodoroui::View::Timer;
      save();
      break;
    case pomodoroui::Skip:
      pomodoro::skip(state);
      stagePage = state.stage / 3;
      dirty = true;
      save();
      break;
    case pomodoroui::Reset:
      pause();
      view = pomodoroui::View::ConfirmReset;
      break;
    case pomodoroui::Confirm:
      pomodoro::reset(state);
      dirty = true;
      save();
      view = pomodoroui::View::Timer;
      break;
    case pomodoroui::Cancel:
      view = pomodoroui::View::Timer;
      break;
    default:
      return;
  }
  if (dirty) {
    saveSoon = true;
    saveRequestedAt = millis();
  }
  if (previousView != view || previousPage != stagePage) {
    ++viewRevision;
    pendingTap.clear();
  }
  ready = false;
  requestUpdate();
}
void PomodoroActivity::loop() {
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("APP-NAV", "Navigation queue full");
  int tapX = 0, tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY))
    pendingTap.capture(tapX, tapY, paintedRevision.load(std::memory_order_acquire));
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  sectionnav::Input::Event navCommand{};
  const bool hasNavCommand = (ready || navigation.backPending()) && navigation.pop(navCommand);
  const bool navBack = hasNavCommand && navCommand.kind == sectionnav::Input::Kind::Back;
  const int navStep = hasNavCommand && (navCommand.kind == sectionnav::Input::Kind::Step ||
                                        navCommand.kind == sectionnav::Input::Kind::Section)
                          ? navCommand.amount
                          : 0;
  const uint32_t now = millis();
  if (state.running) {
    const bool complete = pomodoro::advance(state, now - lastTick);
    dirty = true;
    if (complete) {
      ++viewRevision;
      pendingTap.clear();
      stagePage = state.stage / 3;
      save();
      ready = false;
      requestUpdate();
    }
  }
  lastTick = now;
  if (navBack) {
    pendingTap.clear();
    ++viewRevision;
    pause();
    if (view == pomodoroui::View::Timer) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    view = pomodoroui::View::Timer;
    ready = false;
    requestUpdate();
    return;
  }
  if (ready && view == pomodoroui::View::Timer) {
    if (hasNavCommand && navCommand.kind == sectionnav::Input::Kind::Step) {
      if ((std::abs(navStep) & 1) == 0) return;
      action(pomodoroui::Toggle, 0);
      return;
    }
  }
  int x = 0, y = 0;
  if (ready && pendingTap.take(viewRevision, x, y)) {
    interactions.setFocusedIndex(-1);
    freeink::ui::InputSnapshot tap;
    tap.touchReleased = true;
    tap.touchX = x;
    tap.touchY = y;
    const auto event = interactions.route(tap);
    if (event.action) {
      action(event.action, event.value);
      return;
    }
  }
  if (ready && view == pomodoroui::View::Timer &&
      (!interactions.hasButtonFocus() && hasNavCommand && navCommand.kind == sectionnav::Input::Kind::Confirm)) {
    action(pomodoroui::Toggle, 0);
    return;
  }
  if (dirty && ((saveSoon && now - saveRequestedAt >= 350u) || now - lastSave >= 30000u)) save();
  const uint32_t seconds = (state.remainingMs + 999) / 1000;
  if (state.running && seconds != shownSeconds) requestUpdate();
}
std::optional<uint32_t> PomodoroActivity::nextLoopWakeDelayMs(const uint32_t now) const {
  if (!ready || navigation.hasPending() || pendingTap.hasPending()) return loopdeadline::LEGACY_POLL_MS;
  loopdeadline::Deadline next(now);
  if (state.running) {
    const uint32_t elapsed = now - lastTick;
    next.limit(elapsed >= state.remainingMs ? 0 : (state.remainingMs - elapsed - 1) % 1000 + 1);
  }
  if (dirty) {
    next.after(lastSave, 30000);
    if (saveSoon) next.after(saveRequestedAt, 350);
  }
  return next.delayMs();
}

void PomodoroActivity::render(RenderLock&&) {
  interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const freeink::ui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions);
  toybox::Screen screen(frame);
  pomodoroui::build(screen, {&state, view, saveFailed, stagePage}, work);
  toybox::reportOverflow(interactions, "Pomodoro", &renderer);
  shownSeconds = (state.remainingMs + 999) / 1000;
  ready = true;
  renderer.displayBuffer(++paints % 60 == 0 ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  paintedRevision.store(viewRevision, std::memory_order_release);
}

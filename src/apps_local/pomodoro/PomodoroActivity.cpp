#include "PomodoroActivity.h"

#include <HalClock.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <ctime>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "TimerSettingsUi.h"
#include "util/LoopDeadline.h"
namespace {
pomodoro::Clock studyClock() {
  tm local{};
  const auto now = std::time(nullptr);
  if (!(now > 0 && localtime_r(&now, &local) && local.tm_year >= 120) && !halClock.localTime(local)) return {};
  const int day = readingstats::dayIndex({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
  if (day < 18262) return {};
  return {day, uint32_t(local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec)};
}
}  // namespace
std::unique_ptr<Activity> PomodoroActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  // Bounded history projection and checkpoint buffers belong to this activity,
  // not the render stack or a singleton retained by the reader.
  static_assert(sizeof(PomodoroActivity) < 12 * 1024);
  auto activity = makeUniqueNoThrow<PomodoroActivity>(renderer, input);
  if (!activity) LOG_ERR("POMODORO", "OOM: activity");
  return activity;
}
void PomodoroActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  saveFailed = !store.load(state, &options);
  stagePage = state.stage / 3;
  lastClock = studyClock();
  today = lastClock.day;
  beginStats(pomodoro::Period::Day, today);
  lastTick = lastSave = millis();
  requestUpdate();
}
void PomodoroActivity::save() {
  if (!dirty) return;
  const bool previousFailure = saveFailed;
  saveFailed = !store.save(state, pending, options);
  if (!saveFailed) dirty = false;
  if (saveFailed != previousFailure) {
    ready = false;
    requestUpdate();
  }
  lastSave = millis();
  saveSoon = false;
}
void PomodoroActivity::credit(const pomodoro::Pending& delta) {
  for (const auto& d : delta.dates) {
    const int day = pending.add(d.day, d.milliseconds, d.sessions);
    activeCredit.add(day, d.milliseconds, 0);
    const pomodoro::Delta recorded{day, d.milliseconds, d.sessions};
    stats.add(recorded);
    accumulation.add(recorded);
  }
  pending.add(0, delta.undatedMs, delta.undatedSessions);
}
bool PomodoroActivity::tick(uint32_t now) {
  const auto clock = studyClock();
  const bool complete = pomodoro::advanceTracked(state, now - lastTick, lastClock, clock, elapsedCredit);
  if (state.running || !elapsedCredit.empty() || complete) dirty = true;
  credit(elapsedCredit);
  if (complete) activeCredit = {};
  lastClock = clock;
  lastTick = now;
  if (today != clock.day) {
    today = clock.day;
    beginStats(stats.period, view == pomodoroui::View::Stats && stats.day ? std::min(stats.day, today) : today);
    ++viewRevision;
    pendingTap.clear();
    ready = false;
    requestUpdate();
  }
  return complete;
}
void PomodoroActivity::beginStats(pomodoro::Period period, int day) {
  stats.begin(period, day);
  stats.add(pending);
  accumulation.begin(options.accumulation, today);
  accumulation.add(pending);
  scanning = store.beginScan();
  historyFailed = !scanning;
  if (!scanning) saveFailed = true;
}
void PomodoroActivity::scanHistory() {
  if (!scanning) return;
  for (int n = 0; n < 16; ++n) {
    const auto result = store.next(scanCredit);
    if (result == pomodoro::Scan::Record) {
      stats.add(scanCredit);
      accumulation.add(scanCredit);
      continue;
    }
    scanning = false;
    historyFailed = result == pomodoro::Scan::Error || store.damaged();
    if (historyFailed) saveFailed = true;
    ready = false;
    requestUpdate();
    break;
  }
}
void PomodoroActivity::pause() {
  if (tick(millis())) {
    stagePage = state.stage / 3;
    ++viewRevision;
    pendingTap.clear();
    ready = false;
    requestUpdate();
  }
  if (state.running) {
    state.running = false;
    dirty = true;
  }
  activeCredit = {};
  save();
}
void PomodoroActivity::onExit() {
  pause();
  store.close();
  Activity::onExit();
}
void PomodoroActivity::prepareForSleep() {
  pause();
  store.close();
  restartScan = scanning;
  scanning = false;
  pendingTap.clear();
  ++viewRevision;
  ready = false;
  requestUpdate();
}
void PomodoroActivity::action(freeink::ui::ActionId action, int value) {
  const auto previousView = view;
  const int previousPage = stagePage;
  const int previousDay = stats.day, previousStage = state.stage;
  const auto previousPeriod = stats.period;
  const auto previousAccumulation = options.accumulation;
  const bool wasChoosing = choosingAccumulation, wasVisible = options.showAccumulation;
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
      if (state.running) activeCredit = {};
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
    case pomodoroui::ChooseAccumulation:
      choosingAccumulation = !choosingAccumulation;
      break;
    case pomodoroui::SelectAccumulation:
      if (value < 0 || value > int(pomodoro::Period::Week)) return;
      options.accumulation = static_cast<pomodoro::Period>(value);
      choosingAccumulation = false;
      beginStats(stats.period, stats.day);
      dirty = true;
      break;
    case pomodoroui::ToggleAccumulation:
      options.showAccumulation = !options.showAccumulation;
      choosingAccumulation = false;
      dirty = true;
      break;
    case pomodoroui::OpenStats:
      choosingAccumulation = false;
      view = pomodoroui::View::Stats;
      beginStats(pomodoro::Period::Day, today);
      break;
    case pomodoroui::StatsPeriod:
      if (value < 0 || value > int(pomodoro::Period::Year)) return;
      beginStats(static_cast<pomodoro::Period>(value), stats.day);
      break;
    case pomodoroui::StatsMove:
      beginStats(stats.period, pomodoro::shiftPeriod(stats.period, stats.day, value, today));
      break;
    case pomodoroui::StatsToday:
      beginStats(pomodoro::Period::Day, today);
      break;
    case pomodoroui::StatsDay:
      if (value < 0 || value >= stats.count) return;
      value = stats.plotFirst + value;
      [[fallthrough]];
    case pomodoroui::StatsCalendar:
      if (value < 18262 || value > 84005) return;
      beginStats(pomodoro::Period::Day, value);
      break;
    case pomodoroui::StatsMonth: {
      if (value < 1 || value > 12) return;
      const auto date = readingstats::dateFromDay(stats.day);
      const int day = readingstats::dayIndex({date.year, value, 1});
      beginStats(pomodoro::Period::Month, day);
      break;
    }
    case pomodoroui::Skip:
      pause();
      view = pomodoroui::View::ConfirmSkip;
      break;
    case pomodoroui::Reset:
      pause();
      view = pomodoroui::View::ConfirmReset;
      break;
    case pomodoroui::Confirm:
      if (view == pomodoroui::View::ConfirmSkip)
        pomodoro::skip(state);
      else
        pomodoro::reset(state);
      stagePage = state.stage / 3;
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
  if (view != pomodoroui::View::Timer) choosingAccumulation = false;
  if (dirty) {
    saveSoon = true;
    saveRequestedAt = millis();
  }
  if (previousView != view || previousPage != stagePage || previousDay != stats.day || previousPeriod != stats.period ||
      previousStage != state.stage || previousAccumulation != options.accumulation ||
      wasChoosing != choosingAccumulation || wasVisible != options.showAccumulation) {
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
  if (restartScan) {
    restartScan = false;
    beginStats(stats.period, stats.day);
  }
  if (state.running) {
    const bool complete = tick(now);
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
  if (!state.running && lastTick != now) tick(now);
  scanHistory();
  if (navBack) {
    if (choosingAccumulation) {
      choosingAccumulation = false;
      pendingTap.clear();
      ++viewRevision;
      ready = false;
      requestUpdate();
      return;
    }
    pendingTap.clear();
    ++viewRevision;
    if (view != pomodoroui::View::Stats) pause();
    if (view == pomodoroui::View::Timer) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    view = pomodoroui::View::Timer;
    ready = false;
    requestUpdate();
    return;
  }
  if (ready && view == pomodoroui::View::Stats && navStep) {
    action(pomodoroui::StatsMove, navStep);
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
      action(event.action, event.action == pomodoroui::StatsCalendar
                               ? pomodoroui::calendarDay(stats, work.calendarGrid, x, y)
                               : event.value);
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
  if (state.running && seconds != shownSeconds && view == pomodoroui::View::Timer) requestUpdate();
}
std::optional<uint32_t> PomodoroActivity::nextLoopWakeDelayMs(const uint32_t now) const {
  if (!ready || navigation.hasPending() || pendingTap.hasPending()) return loopdeadline::LEGACY_POLL_MS;
  loopdeadline::Deadline next(now);
  next.limit(scanning ? 0 : 30000);
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
  {
    toybox::Frame frame(target, target.deviceContext(), noInput, interactions);
    toybox::Screen screen(frame);
    pomodoroui::build(screen,
                      {&state, view, saveFailed, stagePage, today, &accumulation, &stats, scanning, historyFailed,
                       &options, choosingAccumulation, &activeCredit},
                      work);
  }
  if (view == pomodoroui::View::ConfirmSkip || view == pomodoroui::View::ConfirmReset) {
    toybox::Frame modalFrame(target, target.deviceContext(), noInput, interactions);
    toybox::Screen modal(modalFrame);
    timerui::confirmation(
        modal, view == pomodoroui::View::ConfirmSkip ? tr(STR_POMODORO_SKIP_CONFIRM) : tr(STR_FOCUS_RESTART_CONFIRM),
        view == pomodoroui::View::ConfirmSkip ? tr(STR_UI_SKIP_431DE8) : tr(STR_UI_RESTART_69ED00), pomodoroui::Confirm,
        pomodoroui::Cancel);
  }
  toybox::reportOverflow(interactions, "Pomodoro", &renderer);
  shownSeconds = (state.remainingMs + 999) / 1000;
  ready = true;
  renderer.displayBuffer(++paints % 60 == 0 ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  paintedRevision.store(viewRevision, std::memory_order_release);
}

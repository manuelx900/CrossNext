#include "FocusActivity.h"

#include <HalClock.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <ctime>

#include "../../activities/util/KeyboardEntryActivity.h"
#include "../Shelf.h"
#include "../pomodoro/TimerSettingsUi.h"
#include "../reading/ReadingStatsStore.h"
#include "../ui/PixelMenuTheme.h"
#include "util/LoopDeadline.h"

namespace {
struct StudyClock {
  uint32_t day = 0, second = focus::kUnknownSecond;
};
StudyClock studyClock() {
  tm local{};
  const auto now = std::time(nullptr);
  if (!(now > 0 && localtime_r(&now, &local) && local.tm_year >= 120) && !halClock.localTime(local)) return {};
  return {uint32_t(readingstats::dayIndex({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday})),
          uint32_t(local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec)};
}
}  // namespace
std::unique_ptr<Activity> FocusActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  // Fixed tasks, hourly histories, rollback copy and save buffer live only with this activity (under 48 KiB).
  // They cannot live on the task stack or remain resident while the reader is open.
  static_assert(sizeof(FocusActivity) < 48 * 1024);
  auto activity = makeUniqueNoThrow<FocusActivity>(renderer, input);
  if (!activity) LOG_ERR("FOCUS", "OOM: activity (%u bytes)", unsigned(sizeof(FocusActivity)));
  return activity;
}
focusui::Model FocusActivity::model() const {
  return {
      &state, view,         project,    task, page, saveFailed, deleteTarget, message, uint32_t(readingstats::today()),
      filter, cleanupScope, timerChange};
}
void FocusActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  saveFailed = !store.load(state, draft);
  lastTick = lastSave = millis();
  requestUpdate();
}
bool FocusActivity::save() {
  if (!dirty) return !saveFailed;
  saveFailed = !store.save(state);
  if (!saveFailed) dirty = false;
  lastSave = millis();
  return !saveFailed;
}
void FocusActivity::pause() {
  if (state.timer.running) {
    const auto clock = studyClock();
    focus::advance(state, millis() - lastTick, clock.day, clock.second);
    state.timer.running = false;
    dirty = true;
  }
  lastTick = millis();
  save();
}
void FocusActivity::onExit() {
  pause();
  Activity::onExit();
}
void FocusActivity::prepareForSleep() { pause(); }
void FocusActivity::show(focusui::View next) {
  view = next;
  if (view == focusui::View::Today || view == focusui::View::AllTasks || view == focusui::View::Projects ||
      view == focusui::View::Tasks) {
    const auto target = toybox::makeTarget(renderer);
    const auto snapshot = model();
    const int size = focusui::pageSize(target.deviceContext(), snapshot);
    page = std::clamp(page, 0, std::max(0, (focusui::count(snapshot) - 1) / size));
  }
  ++revision;
  pendingTap.clear();
  ready = false;
  requestUpdate();
}
void FocusActivity::back() {
  message = nullptr;
  switch (view) {
    case focusui::View::Menu:
      pause();
      shelf::leave(renderer, mappedInput);
      return;
    case focusui::View::Today:
    case focusui::View::AllTasks:
    case focusui::View::Projects:
    case focusui::View::Stats:
      page = 0;
      show(focusui::View::Menu);
      return;
    case focusui::View::Tasks:
      page = 0;
      show(focusui::View::Projects);
      return;
    case focusui::View::Timer:
      pause();
      show(taskReturnView);
      return;
    case focusui::View::TaskStats:
      show(focusui::View::Timer);
      return;
    case focusui::View::CleanupScope:
      show(focusui::View::Settings);
      return;
    case focusui::View::ConfirmTimerChange:
      show(focusui::View::Timer);
      return;
    case focusui::View::Settings:
      save();
      show(returnView);
      return;
    case focusui::View::ConfirmDelete:
      show(deleteTarget == focusui::DeleteTarget::Completed ? focusui::View::CleanupScope
           : deleteTarget == focusui::DeleteTarget::Project ? focusui::View::ProjectActions
                                                            : focusui::View::Detail);
      return;
    case focusui::View::Detail:
      show(state.tasks[task].done || state.activeTask != task ? taskReturnView : focusui::View::Timer);
      return;
    case focusui::View::ProjectActions:
      show(focusui::View::Tasks);
      return;
  }
}
void FocusActivity::name(bool rename) {
  pause();
  const auto previous = view;
  const bool isProject = view == focusui::View::Projects || view == focusui::View::ProjectActions;
  const char* initial = rename ? (isProject ? state.projects[project].name : state.tasks[task].name) : "";
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(
      renderer, mappedInput, isProject ? tr(STR_FOCUS_PROJECT_NAME) : tr(STR_FOCUS_TASK_NAME), initial,
      isProject ? sizeof(focus::Project::name) - 1 : sizeof(focus::Task::name) - 1);
  if (!keyboard) {
    LOG_ERR("FOCUS", "OOM: keyboard");
    message = tr(STR_UI_NOT_ENOUGH_MEMORY_TO_OPEN_THE_7CC7D5);
    show(previous);
    return;
  }
  ready = false;
  startActivityForResult(std::move(keyboard), [this, rename, isProject, previous](const ActivityResult& result) {
    RenderLock lock(*this);
    if (!result.isCancelled) {
      draft = state;
      const auto& value = std::get<KeyboardResult>(result.data).text;
      bool accepted = false;
      if (rename)
        accepted = isProject ? focus::setName(state.projects[project].name, sizeof(focus::Project::name), value.c_str())
                             : focus::setName(state.tasks[task].name, sizeof(focus::Task::name), value.c_str());
      else {
        const int index = isProject ? focus::addProject(state, value.c_str())
                                    : focus::addTask(state, project, value.c_str(), focusui::taskList(previous));
        accepted = index >= 0;
        if (accepted && !isProject) {
          task = index;
          taskReturnView = previous;
        }
      }
      if (accepted) {
        dirty = true;
        if (!save()) state = draft;
        if (!rename && !saveFailed) filter = focusui::Filter::All;
      } else
        message = tr(STR_FOCUS_NAME_OR_LIMIT);
    }
    if (!rename && !isProject && !saveFailed && !result.isCancelled && !message) {
      focus::selectTask(state, task);
      dirty = true;
      save();
      show(focusui::View::Timer);
    } else {
      show(previous);
    }
  });
}
void FocusActivity::action(freeink::ui::ActionId id, int value) {
  message = nullptr;
  switch (id) {
    case focusui::Overview:
      if (value < 0 || value > 3) return;
      page = 0;
      filter = focusui::Filter::All;
      show(value == 0   ? focusui::View::Today
           : value == 1 ? focusui::View::Projects
           : value == 2 ? focusui::View::Stats
                        : focusui::View::AllTasks);
      return;
    case focusui::QuickStart: {
      pause();
      if (saveFailed) {
        show(focusui::View::Menu);
        return;
      }
      draft = state;
      const int created = focus::prepareTodayFocus(state, tr(STR_FOCUS_UNTITLED));
      if (created < 0) {
        LOG_ERR("FOCUS", "Cannot prepare Today focus: no available task slot");
        message = tr(STR_FOCUS_TASK_LIMIT);
        show(focusui::View::Menu);
        return;
      }
      dirty = true;
      if (!save()) {
        state = draft;
        show(focusui::View::Menu);
        return;
      }
      task = created;
      project = -1;
      page = 0;
      filter = focusui::Filter::All;
      taskReturnView = focusui::View::Today;
      lastTick = millis();
      show(focusui::View::Timer);
      return;
    }
    case focusui::Back:
      back();
      return;
    case focusui::Add:
      name(false);
      return;
    case focusui::Rename:
      name(true);
      return;
    case focusui::Open:
      if (view == focusui::View::Projects) {
        if (value < 0 || value >= focus::kProjects || !state.projects[value].used) return;
        project = value;
        page = 0;
        show(focusui::View::Tasks);
      } else {
        if (value < 0 || value >= focus::kTasks || !state.tasks[value].used ||
            (view == focusui::View::Tasks && state.tasks[value].project != project))
          return;
        taskReturnView = view;
        task = value;
        project = state.tasks[task].project;
        if (state.tasks[task].done) {
          show(focusui::View::Detail);
        } else {
          action(focusui::Prepare, 0);
        }
      }
      return;
    case focusui::Page: {
      auto target = toybox::makeTarget(renderer);
      const int pages = std::max(1, (focusui::count(model()) + focusui::pageSize(target.deviceContext(), model()) - 1) /
                                        focusui::pageSize(target.deviceContext(), model()));
      const int next = std::clamp(page + value, 0, pages - 1);
      if (next == page) return;
      page = next;
      show(view);
      return;
    }
    case focusui::EditProject:
      show(focusui::View::ProjectActions);
      return;
    case focusui::EditTask:
      pause();
      show(focusui::View::Detail);
      return;
    case focusui::Settings:
      pause();
      returnView = view;
      show(focusui::View::Settings);
      return;
    case focusui::Done:
      back();
      return;
    case focusui::Delete:
      pause();
      deleteTarget =
          view == focusui::View::ProjectActions ? focusui::DeleteTarget::Project : focusui::DeleteTarget::Task;
      show(focusui::View::ConfirmDelete);
      return;
    case focusui::OpenTaskStats:
      if (task < 0 || task >= focus::kTasks || !state.tasks[task].used) return;
      show(focusui::View::TaskStats);
      return;
    case focusui::ClearCompleted:
      pause();
      show(focusui::View::CleanupScope);
      return;
    case focusui::ChooseCleanup:
      if (view != focusui::View::CleanupScope || value < 0 || value > int(focus::CompletedScope::All)) return;
      cleanupScope = static_cast<focus::CompletedScope>(value);
      deleteTarget = focusui::DeleteTarget::Completed;
      show(focusui::View::ConfirmDelete);
      return;
    case focusui::ConfirmDelete:
      draft = state;
      switch (deleteTarget) {
        case focusui::DeleteTarget::Project:
          focus::removeProject(state, project);
          break;
        case focusui::DeleteTarget::Task:
          focus::removeTask(state, task);
          break;
        case focusui::DeleteTarget::Completed:
          focus::purgeCompleted(state, cleanupScope);
          break;
      }
      dirty = true;
      if (!save()) {
        state = draft;
        show(focusui::View::ConfirmDelete);
        return;
      }
      if (deleteTarget == focusui::DeleteTarget::Completed && task >= 0 && !state.tasks[task].used) {
        task = -1;
        if (returnView == focusui::View::Detail || returnView == focusui::View::Timer ||
            returnView == focusui::View::TaskStats)
          returnView = taskReturnView;
      }
      page = 0;
      show(deleteTarget == focusui::DeleteTarget::Completed ? focusui::View::Settings
           : deleteTarget == focusui::DeleteTarget::Project ? focusui::View::Projects
                                                            : taskReturnView);
      return;
    case focusui::Prepare:
      if (task < 0 || state.tasks[task].done) return;
      pause();
      focus::selectTask(state, task);
      state.timer.running = false;
      state.timer.notice = false;
      lastTick = millis();
      dirty = true;
      save();
      show(focusui::View::Timer);
      return;
    case focusui::Toggle:
      if (!focus::selectTask(state, task)) return;
      state.timer.running = !state.timer.running;
      state.timer.notice = false;
      lastTick = millis();
      dirty = true;
      break;
    case focusui::Skip:
    case focusui::Reset:
      pause();
      timerChange = id == focusui::Skip ? focusui::TimerChange::Skip : focusui::TimerChange::Restart;
      show(focusui::View::ConfirmTimerChange);
      return;
    case focusui::ApplyTimerChange:
      if (view != focusui::View::ConfirmTimerChange) return;
      draft = state;
      switch (timerChange) {
        case focusui::TimerChange::Restart:
          pomodoro::reset(state.timer);
          break;
        case focusui::TimerChange::Skip:
          focus::skipStage(state);
          break;
      }
      dirty = true;
      if (!save()) state = draft;
      show(focusui::View::Timer);
      return;
    case focusui::Planned:
      if (task < 0) return;
      draft = state;
      state.tasks[task].planned = uint8_t(std::clamp(int(state.tasks[task].planned) + value, 1, 99));
      dirty = true;
      if (!save()) state = draft;
      break;
    case focusui::Complete:
      if (task < 0) return;
      pause();
      draft = state;
      state.tasks[task].done = !state.tasks[task].done;
      if (state.tasks[task].done && state.activeTask == task) state.activeTask = focus::kNone;
      dirty = true;
      if (!save()) {
        state = draft;
      } else if (view == focusui::View::Timer && state.tasks[task].done) {
        show(taskReturnView);
        return;
      }
      show(view);
      return;
    case focusui::CompleteAt:
      if (value < 0 || value >= focus::kTasks || !state.tasks[value].used) return;
      task = value;
      action(focusui::Complete, 0);
      return;
    case focusui::SetFilter:
      if (view != focusui::View::Today && view != focusui::View::AllTasks && view != focusui::View::Projects &&
          view != focusui::View::Tasks)
        return;
      if (value < int(focusui::Filter::All) || value > int(focusui::Filter::Completed)) return;
      filter = static_cast<focusui::Filter>(value);
      page = 0;
      show(view);
      return;
    case focusui::Change:
      draft = state;
      pomodoro::configure(state.timer, value / 2, value % 2 ? 1 : -1);
      dirty = true;
      if (!save()) state = draft;
      break;
    default:
      return;
  }
  ready = false;
  requestUpdate();
}
void FocusActivity::loop() {
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("FOCUS", "Navigation queue full");
  int x = 0, y = 0;
  if (mappedInput.wasScreenTapped(x, y)) pendingTap.capture(x, y, paintedRevision.load(std::memory_order_acquire));
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  const uint32_t now = millis();
  if (state.timer.running) {
    const auto clock = studyClock();
    const bool completed = focus::advance(state, now - lastTick, clock.day, clock.second);
    dirty = true;
    if (completed) {
      save();
      requestUpdate();
    }
  }
  lastTick = now;
  sectionnav::Input::Event command{};
  if ((ready || navigation.backPending()) && navigation.pop(command)) {
    if (command.kind == sectionnav::Input::Kind::Back) {
      back();
      return;
    }
    if (ready && view == focusui::View::Timer) {
      if (command.kind == sectionnav::Input::Kind::Step) {
        if (std::abs(int(command.amount)) & 1) action(focusui::Toggle, 0);
        return;
      }
      if (command.kind == sectionnav::Input::Kind::Confirm && !interactions.hasButtonFocus()) {
        action(focusui::Toggle, 0);
        return;
      }
    }
    if (ready &&
        (view == focusui::View::Today || view == focusui::View::AllTasks || view == focusui::View::Projects ||
         view == focusui::View::Tasks) &&
        (command.kind == sectionnav::Input::Kind::Step || command.kind == sectionnav::Input::Kind::Section)) {
      action(focusui::Page, command.amount);
      return;
    }
  }
  if (ready && pendingTap.take(revision, x, y)) {
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
  if (ready && (view == focusui::View::Today || view == focusui::View::AllTasks || view == focusui::View::Projects ||
                view == focusui::View::Tasks)) {
    const auto swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right) {
      action(focusui::Page, swipe == MappedInputManager::SwipeDir::Left ? 1 : -1);
      return;
    }
  }
  if (dirty && now - lastSave >= 30000) save();
  if (state.timer.running) {
    const uint32_t seconds = (state.timer.remainingMs + 999) / 1000;
    if ((view == focusui::View::Timer && seconds != shownSeconds) ||
        (view == focusui::View::TaskStats && seconds / 60 != shownSeconds / 60))
      requestUpdate();
  }
}
std::optional<uint32_t> FocusActivity::nextLoopWakeDelayMs(uint32_t now) const {
  if (!ready || navigation.hasPending() || pendingTap.hasPending()) return loopdeadline::LEGACY_POLL_MS;
  loopdeadline::Deadline next(now);
  if (state.timer.running) {
    const uint32_t elapsed = now - lastTick;
    next.limit(elapsed >= state.timer.remainingMs ? 0 : (state.timer.remainingMs - elapsed - 1) % 1000 + 1);
  }
  if (dirty) next.after(lastSave, 30000);
  return next.delayMs();
}
void FocusActivity::render(RenderLock&&) {
  interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const freeink::ui::InputSnapshot noInput{};
  {
    toybox::Frame frame(target, target.deviceContext(), noInput, interactions);
    toybox::Screen screen(frame);
    focusui::build(screen, model(), work);
  }
  if (view == focusui::View::ConfirmTimerChange) {
    toybox::Frame modalFrame(target, target.deviceContext(), noInput, interactions);
    toybox::Screen modal(modalFrame);
    timerui::confirmation(
        modal, timerChange == focusui::TimerChange::Skip ? tr(STR_FOCUS_SKIP_CONFIRM) : tr(STR_FOCUS_RESTART_CONFIRM),
        timerChange == focusui::TimerChange::Skip ? tr(STR_UI_SKIP_431DE8) : tr(STR_UI_RESTART_69ED00),
        focusui::ApplyTimerChange, focusui::Back);
  }
  toybox::reportOverflow(interactions, "Focus", &renderer);
  shownSeconds = (state.timer.remainingMs + 999) / 1000;
  ready = true;
  renderer.displayBuffer(++paints % 60 == 0 ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  paintedRevision.store(revision, std::memory_order_release);
}

#pragma once
#include <atomic>

#include "../../activities/Activity.h"
#include "../pomodoro/PomodoroInput.h"
#include "../ui/SectionNavigation.h"
#include "FocusScreens.h"
#include "FocusStore.h"

class FocusActivity final : public Activity {
 public:
  FocusActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("Focus", renderer, input) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void prepareForSleep() override;
  bool preventAutoSleep() override { return state.timer.running; }
  void loop() override;
  void render(RenderLock&&) override;
  std::optional<uint32_t> nextLoopWakeDelayMs(uint32_t now) const override;

 private:
  focus::State state, draft;
  focus::Repository store;
  focusui::Work work;
  focusui::View view = focusui::View::Menu, returnView = focusui::View::Menu, taskReturnView = focusui::View::Today;
  sectionnav::Input navigation;
  pomodoro::PendingTap pendingTap;
  toybox::Interactions interactions;
  uint32_t revision = 1, lastTick = 0, lastSave = 0, shownSeconds = 0;
  std::atomic<uint32_t> paintedRevision{0};
  int project = -1, task = -1, page = 0;
  uint8_t paints = 0;
  bool ready = false, dirty = false, saveFailed = false;
  focusui::DeleteTarget deleteTarget = focusui::DeleteTarget::Task;
  focusui::Filter filter = focusui::Filter::All;
  focus::CompletedScope cleanupScope = focus::CompletedScope::All;
  focusui::TimerChange timerChange = focusui::TimerChange::Skip;
  const char* message = nullptr;
  focusui::Model model() const;
  void show(focusui::View next);
  void pause();
  bool save();
  void back();
  void name(bool rename);
  void action(freeink::ui::ActionId action, int value);
};

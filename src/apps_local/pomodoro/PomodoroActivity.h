#pragma once
#include "../../activities/Activity.h"
#include "../ui/SectionNavigation.h"
#include "PomodoroScreens.h"
#include "PomodoroStore.h"
class PomodoroActivity final : public Activity {
 public:
  PomodoroActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("Pomodoro", renderer, input) {}
  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& input);
  void onEnter() override;
  void onExit() override;
  void prepareForSleep() override;
  bool preventAutoSleep() override { return state.running; }
  void loop() override;
  std::optional<uint32_t> nextLoopWakeDelayMs(uint32_t now) const override;
  void render(RenderLock&&) override;

 private:
  sectionnav::Input navigation;
  int stagePage = 0;
  void save();
  void pause();
  void action(freeink::ui::ActionId action, int value);
  pomodoro::State state;
  pomodoro::Store store;
  pomodoroui::Work work;
  pomodoroui::View view = pomodoroui::View::Timer;
  toybox::Interactions interactions;
  uint32_t lastTick = 0, lastSave = 0, shownSeconds = 0, saveRequestedAt = 0;
  uint8_t paints = 0;
  bool ready = false, dirty = false, saveFailed = false, saveSoon = false;
};

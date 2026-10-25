#pragma once
#include <atomic>

#include "../../activities/Activity.h"
#include "CounterInput.h"
#include "ToolState.h"
#include "ToolsScreens.h"
class ToolsActivity final : public Activity {
 public:
  ToolsActivity(GfxRenderer& r, MappedInputManager& i, toolsui::Kind k) : Activity("Tools", r, i), kind(k) {}
  static std::unique_ptr<Activity> calendar(GfxRenderer&, MappedInputManager&);
  static std::unique_ptr<Activity> clock(GfxRenderer&, MappedInputManager&);
  static std::unique_ptr<Activity> counter(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  void prepareForSleep() override { persist(); }
  bool preventAutoSleep() override { return kind == toolsui::Kind::Clock; }

 private:
  toolsui::Kind kind;
  toolsui::Model model;
  toolsui::Work work;
  tools::State state;
  toybox::Interactions interactions;
  std::atomic<bool> ready{false};
  bool writable = true, dirty = false;
  tools::CounterInput counterInput;
  void counterLoop();
  uint32_t touched = 0, polled = 0;
  int64_t undoCount = 0;
  bool canUndo = false;
  int originalOrientation = 0;
  void action(freeink::ui::ActionId, int value = 0);
  void updateTime(bool resetMonth);
  void persist();
};

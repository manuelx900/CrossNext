#pragma once
#include "../ui/ToyboxScreen.h"
#include "PomodoroCore.h"
namespace pomodoroui {
enum class View : uint8_t { Timer, Settings, ConfirmReset };
enum : freeink::ui::ActionId {
  Toggle = 1,
  Settings,
  Change,
  Done,
  Skip,
  Reset,
  Confirm,
  Cancel,
  SelectStage,
  StagePage
};
struct Model {
  const pomodoro::State* state;
  View view = View::Timer;
  bool saveFailed = false;
  int stagePage = 0;
};
struct Work {
  freeink::ui::ButtonProps button;
  char label[128]{};
};
void build(toybox::Screen& screen, const Model& model, Work& work);
}  // namespace pomodoroui

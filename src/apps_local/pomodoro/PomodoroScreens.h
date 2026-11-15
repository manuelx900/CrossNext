#pragma once
#include "../ui/ToyboxScreen.h"
#include "PomodoroCore.h"
#include "PomodoroHistory.h"
namespace pomodoroui {
void drawCountdown(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* value, int maxInkHeight = 82);
enum class View : uint8_t { Timer, Settings, ConfirmReset, Stats, ConfirmSkip };
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
  StagePage,
  OpenStats,
  StatsPeriod,
  StatsMove,
  StatsToday,
  StatsDay,
  StatsMonth,
  StatsCalendar,
  ChooseAccumulation,
  SelectAccumulation,
  ToggleAccumulation
};
struct Model {
  const pomodoro::State* state;
  View view = View::Timer;
  bool saveFailed = false;
  int stagePage = 0;
  int today = 0;
  const pomodoro::Stats* accumulation = nullptr;
  const pomodoro::Stats* stats = nullptr;
  bool loading = false;
  bool historyFailed = false;
  const pomodoro::DisplayOptions* options = nullptr;
  bool choosingAccumulation = false;
  const pomodoro::Pending* activeCredit = nullptr;
};
struct Work {
  freeink::ui::ButtonProps button;
  char label[128]{};
  freeink::ui::Rect calendarGrid{};
};
void build(toybox::Screen& screen, const Model& model, Work& work);
void statistics(toybox::Screen& screen, freeink::ui::Rect body, const Model& model, Work& work);
int calendarDay(const pomodoro::Stats& stats, freeink::ui::Rect grid, int x, int y);
}  // namespace pomodoroui

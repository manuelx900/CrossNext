#pragma once
#include "../ui/ToyboxScreen.h"
#include "FocusCore.h"

namespace focusui {
enum class View : uint8_t {
  Menu,
  AllTasks,
  Today,
  Projects,
  Tasks,
  ProjectActions,
  Detail,
  Timer,
  Settings,
  ConfirmDelete,
  Stats,
  TaskStats,
  CleanupScope,
  ConfirmTimerChange
};
enum class DeleteTarget : uint8_t { Task, Project, Completed };
enum class Filter : uint8_t { All, Completed };
enum class TimerChange : uint8_t { Restart, Skip };

constexpr focus::TaskList taskList(View view) {
  return view == View::Today      ? focus::TaskList::Today
         : view == View::AllTasks ? focus::TaskList::Tasks
                                  : focus::TaskList::Project;
}

enum : freeink::ui::ActionId {
  Open = 1,
  Add,
  EditProject,
  Rename,
  Delete,
  ConfirmDelete,
  Back,
  Page,
  Planned,
  Prepare,
  Toggle,
  Skip,
  Reset,
  Complete,
  Settings,
  Change,
  Done,
  Overview,
  QuickStart,
  CompleteAt,
  EditTask,
  ClearCompleted,
  SetFilter,
  OpenTaskStats,
  ChooseCleanup,
  ApplyTimerChange
};
struct Model {
  const focus::State* state;
  View view = View::Menu;
  int project = -1, task = -1, page = 0;
  bool saveFailed = false;
  DeleteTarget deleteTarget = DeleteTarget::Task;
  const char* message = nullptr;
  uint32_t day = 0;
  Filter filter = Filter::All;
  focus::CompletedScope cleanupScope = focus::CompletedScope::All;
  TimerChange timerChange = TimerChange::Skip;
};
struct Work {
  char label[160]{};
};
int pageSize(const freeink::ui::DeviceContext& device, const Model& model);
int count(const Model& model);
bool projectMatches(const focus::State& state, int index, Filter filter);
void build(toybox::Screen& screen, const Model& model, Work& work);
}  // namespace focusui

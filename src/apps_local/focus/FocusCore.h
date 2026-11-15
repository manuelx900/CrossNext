#pragma once
#include <cstddef>
#include <cstdint>

#include "../pomodoro/PomodoroCore.h"

namespace focus {
inline constexpr int kProjects = 12, kTasks = 96;
inline constexpr uint8_t kNone = 255;
struct Project {
  char name[48]{};
  bool used = false;
};
enum class TaskList : uint8_t { Project, Today, Tasks };
enum class CompletedScope : uint8_t { Today, Projects, Tasks, All };
struct Study {
  uint32_t seconds = 0;
  uint16_t remainderMs = 0, skipped = 0;
  uint32_t day = 0;
  uint16_t hours[24]{};
  bool incomplete = false;
};
struct Task {
  char name[64]{};
  uint16_t completed = 0;
  uint8_t project = 0, planned = 1;
  bool used = false, done = false;
  TaskList list = TaskList::Project;
  Study study;
};
struct Daily {
  uint32_t day = 0, milliseconds = 0;
  uint16_t sessions = 0, todaySessions = 0;
  uint32_t todayMilliseconds = 0;
};
struct Archive {
  uint32_t tasks = 0, planned = 0, sessions = 0;
  uint32_t studySeconds = 0;
};
struct Totals {
  uint32_t tasks = 0, done = 0, planned = 0, sessions = 0;
  uint32_t studySeconds = 0;
};
inline constexpr int kArchives = kProjects + 2;
struct State {
  Project projects[kProjects];
  Task tasks[kTasks];
  pomodoro::State timer;
  uint8_t activeTask = kNone;
  Daily days[7]{};
  Archive archived[kArchives]{};
};
inline constexpr size_t kV1FileBytes = 96 + 52 * kProjects + 72 * kTasks + 4;
inline constexpr size_t kV2FileBytes = kV1FileBytes + 12 * 7;
inline constexpr size_t kV3FileBytes = kV2FileBytes + 8 * 7 + 12 * kArchives;
inline constexpr size_t kStudyBytes = 61;
inline constexpr size_t kFileBytes = kV3FileBytes + kStudyBytes * kTasks + 4 * kArchives;
inline constexpr uint32_t kUnknownSecond = 86400;
bool valid(const State& state);
bool setName(char* destination, size_t capacity, const char* value);
int addProject(State& state, const char* name);
int addTask(State& state, int project, const char* name, TaskList list = TaskList::Project);
bool belongsTo(const Task& task, TaskList list, int project = -1);
Totals totals(const State& state, TaskList list, int project = -1);
Totals totals(const State& state);
int completedCount(const State& state, CompletedScope scope);
int purgeCompleted(State& state, CompletedScope scope = CompletedScope::All);
void removeTask(State& state, int index);
void removeProject(State& state, int index);
bool selectTask(State& state, int index);
int prepareTodayFocus(State& state, const char* title);
bool advance(State& state, uint32_t elapsedMs, uint32_t day = 0, uint32_t secondOfDay = kUnknownSecond);
bool skipStage(State& state);
const Daily* onDay(const State& state, uint32_t day);
int taskCount(const State& state, int project, bool doneOnly = false);
uint32_t checksum(const uint8_t* bytes, size_t size);
void encode(const State& state, uint32_t generation, uint8_t* bytes);
bool decode(const uint8_t* bytes, State& state, uint32_t& generation, size_t size = kFileBytes);
}  // namespace focus

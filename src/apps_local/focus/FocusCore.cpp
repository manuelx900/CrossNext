#include "FocusCore.h"

#include <algorithm>
#include <cstring>

namespace focus {
namespace {
uint32_t addCount(uint32_t a, uint32_t b) { return uint32_t(std::min(uint64_t(a) + b, uint64_t(UINT32_MAX))); }
int archiveIndex(TaskList list, int project) {
  switch (list) {
    case TaskList::Project:
      return project;
    case TaskList::Today:
      return kProjects;
    case TaskList::Tasks:
      return kProjects + 1;
  }
  return -1;
}
void put(uint8_t* p, uint32_t value) {
  for (int n = 0; n < 4; ++n) p[n] = uint8_t(value >> (n * 8));
}
uint32_t get(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
bool named(const char* name, size_t capacity) { return name[0] && std::memchr(name, 0, capacity); }
bool inScope(const Task& task, CompletedScope scope) {
  switch (scope) {
    case CompletedScope::Today:
      return task.list == TaskList::Today;
    case CompletedScope::Projects:
      return task.list == TaskList::Project;
    case CompletedScope::Tasks:
      return task.list == TaskList::Tasks;
    case CompletedScope::All:
      return true;
  }
  return false;
}
void creditDay(State& state, uint32_t day, uint32_t milliseconds, bool today, bool session) {
  if (!day) return;
  auto& entry = state.days[day % 7];
  if (entry.day != day) entry = {day, 0, 0};
  entry.milliseconds = std::min<uint32_t>(86400000u, entry.milliseconds + milliseconds);
  if (session && entry.sessions < UINT16_MAX) ++entry.sessions;
  if (today) {
    entry.todayMilliseconds = std::min(entry.milliseconds, entry.todayMilliseconds + milliseconds);
    if (session && entry.todaySessions < UINT16_MAX) ++entry.todaySessions;
  }
}
void creditHours(Study& study, uint64_t begin, uint64_t end) {
  while (begin < end) {
    const uint32_t day = begin / 86400;
    const unsigned hour = begin / 3600 % 24;
    const uint64_t next = std::min(end, (begin / 3600 + 1) * 3600);
    if (study.day != day) {
      study.day = day;
      std::memset(study.hours, 0, sizeof(study.hours));
    }
    study.hours[hour] = std::min<uint32_t>(UINT16_MAX, study.hours[hour] + uint32_t(next - begin));
    begin = next;
  }
}
}  // namespace
bool setName(char* destination, size_t capacity, const char* value) {
  if (!value || capacity < 2) return false;
  while (*value == ' ' || *value == '\t') ++value;
  size_t size = std::strlen(value);
  while (size && (value[size - 1] == ' ' || value[size - 1] == '\t')) --size;
  if (!size || size >= capacity) return false;
  for (size_t n = 0; n < size; ++n)
    if (static_cast<unsigned char>(value[n]) < 32) return false;
  std::memset(destination, 0, capacity);
  std::memcpy(destination, value, size);
  return true;
}
bool valid(const State& state) {
  if (!pomodoro::valid(state.timer)) return false;
  for (int n = 0; n < 7; ++n) {
    const auto& day = state.days[n];
    if (day.milliseconds > 86400000u || (day.day && day.day % 7 != unsigned(n)) ||
        day.todayMilliseconds > day.milliseconds || day.todaySessions > day.sessions ||
        (!day.day && (day.milliseconds || day.sessions || day.todayMilliseconds || day.todaySessions)))
      return false;
  }
  for (const auto& project : state.projects)
    if (project.used && !named(project.name, sizeof(project.name))) return false;
  for (const auto& task : state.tasks) {
    if (!task.used) continue;
    if (!named(task.name, sizeof(task.name)) || task.planned < 1 || task.planned > 99 || task.completed > 999)
      return false;
    if (task.study.remainderMs >= 1000) return false;
    uint32_t recorded = 0;
    for (const auto seconds : task.study.hours) recorded += seconds;
    if ((!task.study.day && recorded) || recorded > task.study.seconds) return false;
    switch (task.list) {
      case TaskList::Project:
        if (task.project >= kProjects || !state.projects[task.project].used) return false;
        break;
      case TaskList::Today:
      case TaskList::Tasks:
        if (task.project != kNone) return false;
        break;
      default:
        return false;
    }
  }
  return state.activeTask == kNone ||
         (state.activeTask < kTasks && state.tasks[state.activeTask].used && !state.tasks[state.activeTask].done);
}
int addProject(State& state, const char* name) {
  for (int n = 0; n < kProjects; ++n)
    if (!state.projects[n].used) {
      if (!setName(state.projects[n].name, sizeof(state.projects[n].name), name)) return -1;
      state.projects[n].used = true;
      return n;
    }
  return -1;
}
int addTask(State& state, int project, const char* name, TaskList list) {
  if (list == TaskList::Project && (project < 0 || project >= kProjects || !state.projects[project].used)) return -1;
  if (list != TaskList::Project && list != TaskList::Today && list != TaskList::Tasks) return -1;
  for (int n = 0; n < kTasks; ++n)
    if (!state.tasks[n].used) {
      auto& task = state.tasks[n];
      if (!setName(task.name, sizeof(task.name), name)) return -1;
      task.completed = 0;
      task.planned = 1;
      task.project = list == TaskList::Project ? uint8_t(project) : kNone;
      task.list = list;
      task.used = true;
      task.done = false;
      task.study = {};
      return n;
    }
  return -1;
}
bool belongsTo(const Task& task, TaskList list, int project) {
  return task.used && task.list == list && (list != TaskList::Project || project < 0 || task.project == project);
}
Totals totals(const State& state, TaskList list, int project) {
  Totals result;
  for (int n = 0; n < kArchives; ++n) {
    const bool matches =
        list == TaskList::Project ? n < kProjects && (project < 0 || n == project) : n == archiveIndex(list, -1);
    if (!matches) continue;
    const auto& saved = state.archived[n];
    result.tasks = addCount(result.tasks, saved.tasks);
    result.done = addCount(result.done, saved.tasks);
    result.planned = addCount(result.planned, saved.planned);
    result.sessions = addCount(result.sessions, saved.sessions);
    result.studySeconds = addCount(result.studySeconds, saved.studySeconds);
  }
  for (const auto& task : state.tasks) {
    if (!belongsTo(task, list, project)) continue;
    result.tasks = addCount(result.tasks, 1);
    result.done = addCount(result.done, task.done);
    result.planned = addCount(result.planned, task.planned);
    result.sessions = addCount(result.sessions, task.completed);
    result.studySeconds = addCount(result.studySeconds, task.study.seconds);
  }
  return result;
}
Totals totals(const State& state) {
  Totals result;
  for (auto list : {TaskList::Project, TaskList::Today, TaskList::Tasks}) {
    const auto part = totals(state, list);
    result.tasks = addCount(result.tasks, part.tasks);
    result.done = addCount(result.done, part.done);
    result.planned = addCount(result.planned, part.planned);
    result.sessions = addCount(result.sessions, part.sessions);
    result.studySeconds = addCount(result.studySeconds, part.studySeconds);
  }
  return result;
}
int completedCount(const State& state, CompletedScope scope) {
  int count = 0;
  for (const auto& task : state.tasks) count += task.used && task.done && inScope(task, scope);
  return count;
}
int purgeCompleted(State& state, CompletedScope scope) {
  int removed = 0;
  for (int n = 0; n < kTasks; ++n) {
    const auto& task = state.tasks[n];
    if (!task.used || !task.done || !inScope(task, scope)) continue;
    const int bucket = archiveIndex(task.list, task.project);
    if (bucket < 0 || bucket >= kArchives) continue;
    auto& saved = state.archived[bucket];
    saved.tasks = addCount(saved.tasks, 1);
    saved.planned = addCount(saved.planned, task.planned);
    saved.sessions = addCount(saved.sessions, task.completed);
    saved.studySeconds = addCount(saved.studySeconds, task.study.seconds);
    removeTask(state, n);
    ++removed;
  }
  return removed;
}
void removeTask(State& state, int index) {
  if (index < 0 || index >= kTasks) return;
  if (state.activeTask == index) {
    state.activeTask = kNone;
    pomodoro::selectStage(state.timer, 0);
  }
  state.tasks[index] = {};
}
void removeProject(State& state, int index) {
  if (index < 0 || index >= kProjects) return;
  for (int n = 0; n < kTasks; ++n)
    if (state.tasks[n].used && state.tasks[n].project == index) removeTask(state, n);
  state.projects[index] = {};
  state.archived[index] = {};
}
bool selectTask(State& state, int index) {
  if (index < 0 || index >= kTasks || !state.tasks[index].used || state.tasks[index].done) return false;
  if (state.activeTask != index) {
    pomodoro::selectStage(state.timer, 0);
    state.activeTask = uint8_t(index);
  }
  return true;
}
int prepareTodayFocus(State& state, const char* title) {
  const int index = addTask(state, -1, title, TaskList::Today);
  if (index < 0) return -1;
  selectTask(state, index);
  state.timer.running = false;
  state.timer.notice = false;
  return index;
}
const Daily* onDay(const State& state, uint32_t day) {
  return day && state.days[day % 7].day == day ? &state.days[day % 7] : nullptr;
}
bool advance(State& state, uint32_t elapsedMs, uint32_t day, uint32_t secondOfDay) {
  if (state.activeTask >= kTasks || !state.tasks[state.activeTask].used || state.tasks[state.activeTask].done) {
    state.timer.running = false;
    return false;
  }
  auto& task = state.tasks[state.activeTask];
  const uint32_t before = state.timer.completed;
  const uint32_t credited = state.timer.running && state.timer.phase == pomodoro::Phase::Focus
                                ? std::min(elapsedMs, state.timer.remainingMs)
                                : 0;
  const bool transitioned = pomodoro::advance(state.timer, elapsedMs);
  const bool completed = state.timer.completed != before;
  if (credited) {
    const uint32_t accumulated = task.study.remainderMs + credited;
    const uint32_t seconds = accumulated / 1000;
    task.study.remainderMs = accumulated % 1000;
    task.study.seconds = addCount(task.study.seconds, seconds);
    if (day && secondOfDay < kUnknownSecond) {
      const uint64_t now = (uint64_t(day) * 86400 + secondOfDay) * 1000;
      const uint64_t end = now - std::min<uint64_t>(now, elapsedMs - credited);
      const uint64_t begin = end - std::min<uint64_t>(end, credited);
      creditHours(task.study, end / 1000 - std::min<uint64_t>(end / 1000, seconds), end / 1000);
      uint64_t at = begin;
      while (at < end) {
        const uint64_t next = std::min(end, (at / 86400000 + 1) * 86400000);
        creditDay(state, at / 86400000, next - at, task.list == TaskList::Today, completed && next == end);
        at = next;
      }
    } else {
      creditDay(state, day, credited, task.list == TaskList::Today, completed);
    }
  }
  if (completed) task.completed = std::min(999, int(task.completed) + 1);
  return transitioned;
}
bool skipStage(State& state) {
  if (state.activeTask >= kTasks || !state.tasks[state.activeTask].used || state.tasks[state.activeTask].done)
    return false;
  auto& task = state.tasks[state.activeTask];
  if (state.timer.phase == pomodoro::Phase::Focus && task.study.skipped < UINT16_MAX) ++task.study.skipped;
  pomodoro::skip(state.timer);
  return true;
}
int taskCount(const State& state, int project, bool doneOnly) {
  int total = 0;
  for (const auto& task : state.tasks)
    if (task.used && task.project == project && (!doneOnly || task.done)) ++total;
  return total;
}
uint32_t checksum(const uint8_t* bytes, size_t size) {
  uint32_t sum = 2166136261u;
  for (size_t n = 0; n < size; ++n) sum = (sum ^ bytes[n]) * 16777619u;
  return sum;
}
void encode(const State& state, uint32_t generation, uint8_t* bytes) {
  std::memset(bytes, 0, kFileBytes);
  std::memcpy(bytes, "FCS4", 4);
  put(bytes + 4, generation);
  bytes[8] = state.activeTask;
  pomodoro::encode(state.timer, 0, bytes + 16);
  size_t at = 96;
  for (const auto& project : state.projects) {
    bytes[at] = project.used;
    std::memcpy(bytes + at + 1, project.name, sizeof(project.name));
    at += 52;
  }
  for (const auto& task : state.tasks) {
    bytes[at] = task.used;
    bytes[at + 1] = task.project;
    bytes[at + 2] = task.planned;
    bytes[at + 3] = task.done;
    bytes[at + 4] = uint8_t(task.completed);
    bytes[at + 5] = uint8_t(task.completed >> 8);
    std::memcpy(bytes + at + 6, task.name, sizeof(task.name));
    bytes[at + 70] = uint8_t(task.list);
    at += 72;
  }
  for (const auto& day : state.days) {
    put(bytes + at, day.day);
    put(bytes + at + 4, day.milliseconds);
    put(bytes + at + 8, day.sessions);
    put(bytes + at + 12, day.todayMilliseconds);
    put(bytes + at + 16, day.todaySessions);
    at += 20;
  }
  for (const auto& saved : state.archived) {
    put(bytes + at, saved.tasks);
    put(bytes + at + 4, saved.planned);
    put(bytes + at + 8, saved.sessions);
    at += 12;
  }
  for (const auto& task : state.tasks) {
    const auto& study = task.study;
    put(bytes + at, study.seconds);
    bytes[at + 4] = uint8_t(study.remainderMs);
    bytes[at + 5] = uint8_t(study.remainderMs >> 8);
    bytes[at + 6] = uint8_t(study.skipped);
    bytes[at + 7] = uint8_t(study.skipped >> 8);
    put(bytes + at + 8, study.day);
    for (int hour = 0; hour < 24; ++hour) {
      bytes[at + 12 + hour * 2] = uint8_t(study.hours[hour]);
      bytes[at + 13 + hour * 2] = uint8_t(study.hours[hour] >> 8);
    }
    bytes[at + 60] = study.incomplete;
    at += kStudyBytes;
  }
  for (const auto& saved : state.archived) {
    put(bytes + at, saved.studySeconds);
    at += 4;
  }
  put(bytes + kFileBytes - 4, checksum(bytes, kFileBytes - 4));
}
bool decode(const uint8_t* bytes, State& state, uint32_t& generation, size_t size) {
  const bool old = size == kV1FileBytes && !std::memcmp(bytes, "FCS1", 4);
  const bool v2 = size == kV2FileBytes && !std::memcmp(bytes, "FCS2", 4);
  const bool v3 = size == kV3FileBytes && !std::memcmp(bytes, "FCS3", 4);
  const bool v4 = size == kFileBytes && !std::memcmp(bytes, "FCS4", 4);
  const bool current = v3 || v4;
  if ((!old && !v2 && !current) || get(bytes + size - 4) != checksum(bytes, size - 4)) return false;
  uint32_t unused = 0;
  if (!pomodoro::decode(bytes + 16, state.timer, unused)) return false;
  state.activeTask = bytes[8];
  size_t at = 96;
  for (auto& project : state.projects) {
    if (bytes[at] > 1) return false;
    project.used = bytes[at];
    std::memcpy(project.name, bytes + at + 1, sizeof(project.name));
    at += 52;
  }
  for (auto& task : state.tasks) {
    task = {};
    if (bytes[at] > 1 || bytes[at + 3] > 1) return false;
    task.used = bytes[at];
    task.list = current ? TaskList(bytes[at + 70]) : TaskList::Project;
    task.project = bytes[at + 1];
    task.planned = bytes[at + 2];
    task.done = bytes[at + 3];
    task.completed = uint16_t(bytes[at + 4]) | uint16_t(bytes[at + 5]) << 8;
    std::memcpy(task.name, bytes + at + 6, sizeof(task.name));
    at += 72;
  }
  for (auto& day : state.days) {
    day = {};
    if (!old) {
      day.day = get(bytes + at);
      day.milliseconds = get(bytes + at + 4);
      const auto sessions = get(bytes + at + 8);
      if (sessions > UINT16_MAX) return false;
      day.sessions = uint16_t(sessions);
      if (current) {
        day.todayMilliseconds = get(bytes + at + 12);
        const auto todaySessions = get(bytes + at + 16);
        if (todaySessions > UINT16_MAX) return false;
        day.todaySessions = uint16_t(todaySessions);
      }
      at += current ? 20 : 12;
    }
  }
  for (auto& saved : state.archived) {
    saved = {};
    if (current) {
      saved.tasks = get(bytes + at);
      saved.planned = get(bytes + at + 4);
      saved.sessions = get(bytes + at + 8);
      at += 12;
    }
  }
  for (auto& task : state.tasks) {
    if (!v4) {
      task.study.incomplete = task.used;
      continue;
    }
    auto& study = task.study;
    study.seconds = get(bytes + at);
    study.remainderMs = uint16_t(bytes[at + 4]) | uint16_t(bytes[at + 5]) << 8;
    study.skipped = uint16_t(bytes[at + 6]) | uint16_t(bytes[at + 7]) << 8;
    study.day = get(bytes + at + 8);
    for (int hour = 0; hour < 24; ++hour)
      study.hours[hour] = uint16_t(bytes[at + 12 + hour * 2]) | uint16_t(bytes[at + 13 + hour * 2]) << 8;
    if (bytes[at + 60] > 1) return false;
    study.incomplete = bytes[at + 60];
    at += kStudyBytes;
  }
  if (v4)
    for (auto& saved : state.archived) {
      saved.studySeconds = get(bytes + at);
      at += 4;
    }
  if (!valid(state)) return false;
  generation = get(bytes + 4);
  return true;
}
}  // namespace focus

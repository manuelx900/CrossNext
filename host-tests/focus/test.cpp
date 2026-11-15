#include <HalStorage.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../src/apps_local/focus/FocusStore.h"
#include "../../src/apps_local/words/DefinitionText.h"
#include "../../src/util/EnglishInflections.h"
#include "../../src/util/WordEdges.h"

namespace {
int checks = 0;
void check(bool pass, int line) {
  ++checks;
  if (!pass) {
    std::printf("FAIL line %d\n", line);
    std::exit(1);
  }
}
#define CHECK(value) check(bool(value), __LINE__)
focus::State state, scratch, copy;
uint8_t bytes[focus::kFileBytes];
void core() {
  CHECK(focus::valid(state));
  CHECK(focus::addTask(state, 0, "Missing project") == -1);
  const int project = focus::addProject(state, "  Biología  ");
  CHECK(project == 0);
  CHECK(!std::strcmp(state.projects[0].name, "Biología"));
  const int task = focus::addTask(state, project, "Estudiar células");
  CHECK(task == 0 && state.tasks[task].planned == 1);
  CHECK(focus::selectTask(state, task));
  state.timer.running = true;
  CHECK(!focus::advance(state, 25 * 60000 - 1));
  CHECK(state.tasks[task].completed == 0);
  CHECK(focus::advance(state, 1));
  CHECK(state.tasks[task].completed == 1 && state.timer.phase == pomodoro::Phase::ShortBreak);
  state.timer.running = true;
  CHECK(focus::advance(state, 5 * 60000));
  CHECK(state.tasks[task].completed == 1);
  pomodoro::skip(state.timer);
  CHECK(state.tasks[task].completed == 1);
  CHECK(focus::taskCount(state, project) == 1);
  CHECK(focus::taskCount(state, project, true) == 0);
  for (int n = 1; n < focus::kTasks; ++n) CHECK(focus::addTask(state, project, "Task") == n);
  CHECK(focus::addTask(state, project, "Overflow") == -1);
  for (int n = 1; n < focus::kProjects; ++n) CHECK(focus::addProject(state, "Project") == n);
  CHECK(focus::addProject(state, "Overflow") == -1);
  focus::encode(state, 0xFFFFFFFFu, bytes);
  uint32_t generation = 0;
  CHECK(focus::decode(bytes, copy, generation));
  CHECK(generation == 0xFFFFFFFFu && focus::valid(copy));
  CHECK(!copy.timer.running && copy.tasks[task].completed == 1);
  for (size_t n = 0; n < focus::kFileBytes; ++n) {
    bytes[n] ^= 1;
    CHECK(!focus::decode(bytes, scratch, generation));
    bytes[n] ^= 1;
  }
  focus::removeProject(copy, project);
  CHECK(focus::valid(copy) && copy.activeTask == focus::kNone && focus::taskCount(copy, project) == 0);
  CHECK(copy.projects[1].used);
  CHECK(focus::addProject(copy, "New") == project);
  CHECK(focus::addTask(copy, project, "New task") == 0 && copy.tasks[0].completed == 0);
  CHECK(focus::selectTask(copy, 0));
  copy.tasks[0].done = true;
  CHECK(!focus::selectTask(copy, 0));
  CHECK(!focus::advance(copy, 9999) && !copy.timer.running);
  copy.activeTask = focus::kNone;
  CHECK(focus::valid(copy));
}
void persistence() {
  testStorage = {};
  state = {};
  focus::Repository repository;
  CHECK(repository.load(state, scratch));
  CHECK(focus::addProject(state, "Biology") == 0);
  CHECK(focus::addTask(state, 0, "Read chapter 1") == 0);
  CHECK(repository.save(state));
  const int writes = testStorage.writes;
  CHECK(repository.save(state) && testStorage.writes == writes);
  state.tasks[0].planned = 6;
  testStorage.writeLimit = 19;
  CHECK(!repository.save(state));
  testStorage.writeLimit = -1;
  focus::Repository recovered;
  CHECK(recovered.load(copy, scratch));
  CHECK(copy.tasks[0].planned == 1);
  CHECK(repository.save(state));
  focus::Repository loaded;
  CHECK(loaded.load(copy, scratch) && copy.tasks[0].planned == 6);
  auto& latest = testStorage.files["/.crosspoint/focus-b.bin"];
  latest[latest.size() / 2] ^= 1;
  focus::Repository damaged;
  CHECK(damaged.load(copy, scratch) && copy.tasks[0].planned == 1);
  testStorage.files["/.crosspoint/focus-a.bin"][0] ^= 1;
  focus::Repository unavailable;
  CHECK(!unavailable.load(copy, scratch));
  const int before = testStorage.writes;
  CHECK(!unavailable.save(state) && before == testStorage.writes);
  testStorage = {};
  testStorage.files["/.crosspoint/focus-a.bin"] = {'F', 'C', 'S', '5'};
  focus::Repository future;
  CHECK(!future.load(copy, scratch) && !future.save(state));
}
void historyAndMigration() {
  state = {};
  CHECK(focus::addProject(state, "Study") == 0);
  CHECK(focus::addTask(state, 0, "Chapter") == 0);
  CHECK(focus::selectTask(state, 0));
  state.timer.running = true;
  CHECK(!focus::advance(state, 60000, 21000));
  CHECK(focus::onDay(state, 21000)->milliseconds == 60000);
  CHECK(!focus::onDay(state, 20999));
  CHECK(focus::advance(state, 24 * 60000, 21000));
  CHECK(focus::onDay(state, 21000)->sessions == 1);
  CHECK(!state.tasks[0].done);
  state.timer.running = true;
  focus::advance(state, 60000, 21000);
  CHECK(focus::onDay(state, 21000)->milliseconds == 25 * 60000);
  pomodoro::selectStage(state.timer, 0);
  state.timer.running = true;
  focus::advance(state, 12345, 21007);
  CHECK(!focus::onDay(state, 21000));
  CHECK(focus::onDay(state, 21007)->milliseconds == 12345);
  focus::encode(state, 77, bytes);
  uint32_t generation = 0;
  CHECK(focus::decode(bytes, copy, generation) && generation == 77);
  CHECK(focus::onDay(copy, 21007)->milliseconds == 12345);
  bytes[3] = '1';
  const auto sum = focus::checksum(bytes, focus::kV1FileBytes - 4);
  for (int n = 0; n < 4; ++n) bytes[focus::kV1FileBytes - 4 + n] = uint8_t(sum >> (8 * n));
  CHECK(focus::decode(bytes, copy, generation, focus::kV1FileBytes));
  CHECK(copy.tasks[0].completed == 1 && !copy.tasks[0].done);
  CHECK(!focus::onDay(copy, 21007));
  testStorage = {};
  auto& saved = testStorage.files["/.crosspoint/focus-a.bin"];
  saved.assign(bytes, bytes + focus::kV1FileBytes);
  focus::Repository old;
  CHECK(old.load(copy, scratch));
  copy.tasks[0].planned = 5;
  CHECK(old.save(copy));
  CHECK(saved.size() == focus::kV1FileBytes);
  CHECK(testStorage.files["/.crosspoint/focus-b.bin"].size() == focus::kFileBytes);
  focus::Repository migrated;
  CHECK(migrated.load(scratch, state) && scratch.tasks[0].planned == 5);
}
void independentListsAndCleanup() {
  state = {};
  const int project = focus::addProject(state, "Biology");
  const int linked = focus::addTask(state, project, "Project chapter");
  const int today = focus::addTask(state, -1, "Today only", focus::TaskList::Today);
  const int loose = focus::addTask(state, -1, "Standalone task", focus::TaskList::Tasks);
  CHECK(linked >= 0 && today >= 0 && loose >= 0 && focus::valid(state));
  CHECK(state.tasks[today].project == focus::kNone && state.tasks[loose].project == focus::kNone);
  CHECK(focus::belongsTo(state.tasks[linked], focus::TaskList::Project, project));
  CHECK(!focus::belongsTo(state.tasks[linked], focus::TaskList::Today));
  CHECK(!focus::belongsTo(state.tasks[today], focus::TaskList::Tasks));
  CHECK(focus::totals(state, focus::TaskList::Today).tasks == 1);
  CHECK(focus::totals(state, focus::TaskList::Tasks).tasks == 1);
  CHECK(focus::selectTask(state, linked));
  state.timer.running = true;
  CHECK(focus::advance(state, 25 * 60000, 21007));
  CHECK(focus::onDay(state, 21007)->sessions == 1 && focus::onDay(state, 21007)->todaySessions == 0);
  CHECK(focus::selectTask(state, today));
  state.timer.running = true;
  CHECK(focus::advance(state, 25 * 60000, 21007));
  CHECK(focus::onDay(state, 21007)->sessions == 2 && focus::onDay(state, 21007)->todaySessions == 1);
  state.tasks[linked].done = state.tasks[today].done = true;
  state.activeTask = focus::kNone;
  const auto before = focus::totals(state);
  const auto day = *focus::onDay(state, 21007);
  CHECK(focus::purgeCompleted(state) == 2 && focus::purgeCompleted(state) == 0);
  CHECK(!state.tasks[linked].used && !state.tasks[today].used && state.tasks[loose].used);
  const auto after = focus::totals(state);
  CHECK(after.tasks == before.tasks && after.done == before.done && after.sessions == before.sessions &&
        after.planned == before.planned);
  CHECK(focus::totals(state, focus::TaskList::Project, project).tasks == 1);
  CHECK(focus::totals(state, focus::TaskList::Today).done == 1);
  CHECK(focus::onDay(state, 21007)->milliseconds == day.milliseconds &&
        focus::onDay(state, 21007)->sessions == day.sessions);
  CHECK(focus::valid(state));
  focus::encode(state, 12, bytes);
  uint32_t generation = 0;
  CHECK(focus::decode(bytes, copy, generation) && generation == 12);
  CHECK(focus::totals(copy).done == after.done && focus::totals(copy).sessions == after.sessions);
  CHECK(focus::onDay(copy, 21007)->todayMilliseconds == 25 * 60000u);
  const int replacement = focus::addTask(copy, -1, "New Today", focus::TaskList::Today);
  CHECK(replacement >= 0 && copy.tasks[replacement].completed == 0 && !copy.tasks[replacement].done);
  focus::removeProject(copy, project);
  CHECK(copy.tasks[replacement].used && copy.tasks[loose].used && focus::valid(copy));
  // V2 days used 12 bytes; its project tasks and history survive the V3 migration.
  state = {};
  CHECK(focus::addProject(state, "Legacy") == 0);
  CHECK(focus::addTask(state, 0, "Legacy chapter") == 0);
  state.tasks[0].completed = 3;
  state.days[21007 % 7] = {21007, 300000, 2};
  focus::encode(state, 77, bytes);
  constexpr size_t dayOffset = focus::kV1FileBytes - 4;
  for (size_t n = 0; n < 7; ++n) std::memmove(bytes + dayOffset + n * 12, bytes + dayOffset + n * 20, 12);
  bytes[3] = '2';
  const auto sum = focus::checksum(bytes, focus::kV2FileBytes - 4);
  for (int n = 0; n < 4; ++n) bytes[focus::kV2FileBytes - 4 + n] = uint8_t(sum >> (8 * n));
  CHECK(focus::decode(bytes, copy, generation, focus::kV2FileBytes));
  CHECK(generation == 77 && focus::belongsTo(copy.tasks[0], focus::TaskList::Project, 0));
  CHECK(copy.tasks[0].completed == 3 && focus::onDay(copy, 21007)->milliseconds == 300000);
  CHECK(focus::totals(copy, focus::TaskList::Today).tasks == 0 &&
        focus::totals(copy, focus::TaskList::Tasks).tasks == 0);
  testStorage = {};
  testStorage.files["/.crosspoint/focus-a.bin"].assign(bytes, bytes + focus::kV2FileBytes);
  focus::Repository migration;
  CHECK(migration.load(state, scratch));
  CHECK(focus::addTask(state, -1, "Independent", focus::TaskList::Tasks) >= 0);
  CHECK(migration.save(state));
  focus::Repository reloaded;
  CHECK(reloaded.load(copy, scratch) && focus::totals(copy, focus::TaskList::Tasks).tasks == 1);
}
void words() {
  static constexpr const char* words[] = {"“Cataclysm”).”", "¿imaginación?", "«l'amour»",
                                          "(well-being).",  "猫。",          "risen"};
  static constexpr const char* expected[] = {"Cataclysm", "imaginación", "l'amour", "well-being", "猫", "risen"};
  for (size_t n = 0; n < sizeof(words) / sizeof(words[0]); ++n) {
    const auto span = wordedges::trim(words[n]);
    CHECK(span.end - span.first == std::strlen(expected[n]));
    CHECK(!std::memcmp(words[n] + span.first, expected[n], span.end - span.first));
  }
  char definition[] = "  First definition. \n\n\n  Synonyms:  idea,  thought.\r\n\r\nExample.   ";
  wordsui::compactDefinition(definition);
  CHECK(!std::strcmp(definition, "First definition.\nSynonyms: idea, thought.\nExample."));
  CHECK(wordedges::trim(".,‘’").end == wordedges::trim(".,‘’").first);
}
void quickFocus() {
  state = {};
  CHECK(focus::addProject(state, "Existing project") == 0);
  const int previous = focus::addTask(state, 0, "Existing task");
  CHECK(focus::selectTask(state, previous));
  state.tasks[previous].completed = 3;
  state.timer.config.focus = 37;
  state.timer.config.rounds = 6;
  pomodoro::selectStage(state.timer, 1);
  state.timer.running = true;
  state.days[1] = {20756, 90000, 0, 0, 0};
  const int created = focus::prepareTodayFocus(state, "Sin título");
  CHECK(created >= 0 && created != previous && state.activeTask == created);
  CHECK(focus::belongsTo(state.tasks[created], focus::TaskList::Today));
  CHECK(state.tasks[created].project == focus::kNone && state.tasks[created].planned == 1);
  CHECK(!state.tasks[created].done && state.tasks[created].completed == 0);
  CHECK(!std::strcmp(state.tasks[created].name, "Sin título"));
  CHECK(!state.timer.running && !state.timer.notice && state.timer.stage == 0);
  CHECK(state.timer.phase == pomodoro::Phase::Focus && state.timer.remainingMs == 37 * 60000);
  CHECK(state.timer.config.rounds == 6 && state.tasks[previous].completed == 3);
  CHECK(!focus::advance(state, 30000, 20756) && state.timer.remainingMs == 37 * 60000);
  CHECK(state.days[1].milliseconds == 90000);
  CHECK(focus::setName(state.tasks[created].name, sizeof(state.tasks[created].name), "My new focus"));
  state.tasks[created].planned = 5;
  focus::encode(state, 15, bytes);
  uint32_t generation = 0;
  CHECK(focus::decode(bytes, copy, generation) && generation == 15);
  CHECK(copy.activeTask == created && !copy.timer.running && copy.tasks[created].planned == 5);
  CHECK(!std::strcmp(copy.tasks[created].name, "My new focus"));
  for (int n = 2; n < focus::kTasks; ++n) CHECK(focus::addTask(state, -1, "Saved task", focus::TaskList::Today) >= 0);
  focus::encode(state, 0, bytes);
  const auto before = focus::checksum(bytes, sizeof(bytes));
  CHECK(focus::prepareTodayFocus(state, "Another focus") == -1);
  focus::encode(state, 0, bytes);
  CHECK(before == focus::checksum(bytes, sizeof(bytes)) && state.activeTask == created);
  state = {};
  CHECK(focus::prepareTodayFocus(state, "") == -1 && state.activeTask == focus::kNone);
}
void studyHistory() {
  state = {};
  constexpr uint32_t day = 20759;
  const int task = focus::prepareTodayFocus(state, "Current focus");
  const int other = focus::addTask(state, -1, "Other focus", focus::TaskList::Today);
  state.timer.running = true;
  CHECK(!focus::advance(state, 60000, day, 9 * 3600 + 30));
  CHECK(state.tasks[task].study.seconds == 60 && state.tasks[other].study.seconds == 0);
  CHECK(state.tasks[task].study.hours[8] == 30 && state.tasks[task].study.hours[9] == 30);
  CHECK(focus::onDay(state, day)->todayMilliseconds == 60000);
  state.timer.running = false;
  CHECK(!focus::advance(state, 60000, day, 10 * 3600));
  CHECK(state.tasks[task].study.seconds == 60);
  pomodoro::reset(state.timer);
  CHECK(state.tasks[task].study.seconds == 60 && !state.timer.running);
  CHECK(focus::skipStage(state));
  CHECK(state.tasks[task].study.skipped == 1 && state.tasks[task].completed == 0 && !state.timer.running);
  CHECK(state.timer.phase == pomodoro::Phase::ShortBreak);
  state.timer.running = true;
  focus::advance(state, 20000, day, 10 * 3600 + 20);
  CHECK(state.tasks[task].study.seconds == 60);
  CHECK(focus::skipStage(state));
  CHECK(state.tasks[task].study.skipped == 1 && !state.timer.running);
  state.timer.running = true;
  focus::advance(state, 500, day, 10 * 3600 + 21);
  focus::advance(state, 500, day, 10 * 3600 + 22);
  CHECK(state.tasks[task].study.seconds == 61 && state.tasks[task].study.remainderMs == 0);
  state.timer.running = true;
  focus::advance(state, 60000, day + 1, 30);
  CHECK(state.tasks[task].study.day == day + 1 && state.tasks[task].study.hours[0] == 30);
  CHECK(state.tasks[task].study.hours[8] == 0 && state.tasks[task].study.seconds == 121);
  CHECK(focus::onDay(state, day + 1)->milliseconds == 30000);
  CHECK(focus::onDay(state, day)->milliseconds == 91000);
  // A late loop credits only the remaining focus duration, before its actual boundary.
  state.timer.remainingMs = 10000;
  CHECK(focus::advance(state, 30000, day + 1, 3600 + 10));
  CHECK(state.tasks[task].study.hours[0] == 40 && state.tasks[task].study.hours[1] == 0);
  CHECK(state.tasks[task].completed == 1 && state.tasks[task].study.seconds == 131);
  CHECK(focus::valid(state));
  focus::encode(state, 8, bytes);
  uint32_t generation = 0;
  CHECK(focus::decode(bytes, copy, generation) && generation == 8);
  CHECK(copy.tasks[task].study.hours[0] == 40 && copy.tasks[task].study.seconds == 131);
  CHECK(copy.tasks[task].study.skipped == 1);
  // V3 preserves counts and marks unavailable per-task history explicitly.
  bytes[3] = '3';
  const auto sum = focus::checksum(bytes, focus::kV3FileBytes - 4);
  for (int n = 0; n < 4; ++n) bytes[focus::kV3FileBytes - 4 + n] = uint8_t(sum >> (8 * n));
  CHECK(focus::decode(bytes, copy, generation, focus::kV3FileBytes));
  CHECK(copy.tasks[task].study.incomplete && copy.tasks[task].study.seconds == 0);
  CHECK(copy.tasks[task].completed == 1 && focus::onDay(copy, day + 1)->milliseconds == 40000);
}
void scopedCleanup() {
  for (const auto scope : {focus::CompletedScope::Today, focus::CompletedScope::Projects, focus::CompletedScope::Tasks,
                           focus::CompletedScope::All}) {
    state = {};
    const int project = focus::addProject(state, "Project");
    for (const auto list : {focus::TaskList::Today, focus::TaskList::Project, focus::TaskList::Tasks}) {
      const int task = focus::addTask(state, project, "Completed task", list);
      state.tasks[task].done = true;
      state.tasks[task].completed = 2;
      state.tasks[task].study.seconds = 345;
      CHECK(focus::addTask(state, project, "Pending task", list) >= 0);
    }
    const auto before = focus::totals(state);
    CHECK(focus::completedCount(state, scope) == (scope == focus::CompletedScope::All ? 3 : 1));
    CHECK(focus::purgeCompleted(state, scope) == (scope == focus::CompletedScope::All ? 3 : 1));
    CHECK(focus::completedCount(state, scope) == 0);
    const auto after = focus::totals(state);
    CHECK(after.tasks == before.tasks && after.sessions == before.sessions &&
          after.studySeconds == before.studySeconds);
    for (const auto& task : state.tasks)
      if (task.used) CHECK(task.done || task.study.seconds == 0);
    CHECK(focus::valid(state));
    focus::encode(state, 1, bytes);
    uint32_t generation = 0;
    CHECK(focus::decode(bytes, copy, generation) && focus::totals(copy).studySeconds == before.studySeconds);
  }
}
}  // namespace
int main() {
  core();
  persistence();
  words();
  historyAndMigration();
  independentListsAndCleanup();
  quickFocus();
  studyHistory();
  scopedCleanup();
  std::printf("Focus, persistence and word boundaries: %d checks, 0 failures; state=%u save=%u bytes\n", checks,
              unsigned(sizeof(focus::State)), unsigned(focus::kFileBytes));
}

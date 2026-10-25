#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CarnivoroScreens.h"

namespace shelf {
void leave(int, int) {}
}  // namespace shelf
unsigned long millis() { return 0; }
#define LOG_ERR(...) ((void)0)
int checks = 0;
#define CHECK(value)                                  \
  do {                                                \
    ++checks;                                         \
    if (!(value)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #value); \
      std::exit(1);                                   \
    }                                                 \
  } while (0)

class CarnivoroActivity {
 public:
  carnivoro::State state, pending;
  carnivoroui::Model model;
  carnivoroui::Geometry geometry;
  enum class PendingOperation { None, Accusation, Replay, Reset };
  PendingOperation pendingOperation = PendingOperation::None;
  bool dirty = false, writable = true;
  unsigned long changedAt = 0;
  int renderer = 0, mappedInput = 0, saveCalls = 0;
  void resetUi() {}
  void requestUpdate() {}
  void load() {}
  bool flush() {
    ++saveCalls;
    return false;
  }
  void changed();
  void openCase(int);
  void back();
  void dispatch(int, int = 0);
};
#include "activity_flow.inc"

int main() {
  using namespace carnivoroui;
  using carnivoro::GuiltNote;
  CarnivoroActivity app;
  app.model.state = &app.state;
  app.openCase(999);
  app.dispatch(Start);
  CHECK(app.model.view == View::Statements);
  app.dispatch(Mark, 2);
  CHECK(carnivoro::guiltNote(app.state, 0) == GuiltNote::Innocent);
  app.dispatch(CycleGuilt, 0);
  CHECK(carnivoro::guiltNote(app.state, 0) == GuiltNote::Unknown);
  app.dispatch(CycleGuilt, 0);
  CHECK(carnivoro::guiltNote(app.state, 0) == GuiltNote::Suspect);
  app.dispatch(CycleGuilt, 0);
  CHECK(carnivoro::guiltNote(app.state, 0) == GuiltNote::Innocent);
  CHECK(!app.state.picks);
  app.dispatch(Mark, 0);
  CHECK(!app.state.innocents);
  app.dispatch(CycleNote, 6);
  CHECK(app.state.notes[6] == carnivoro::Note::Truth);
  app.dispatch(ReadStatement, 6);
  CHECK(app.model.view == View::Statements && app.model.openSuspect == 6);
  // Rendering resolves the named suspect to the current orientation's card page.
  app.model.openSuspect = -1;
  app.geometry.suspectCapacity = 3;
  app.model.page = 2;
  app.dispatch(Next);
  CHECK(app.model.page == 2);
  app.dispatch(Previous);
  CHECK(app.model.page == 1);
  app.dispatch(ShowOverview);
  CHECK(app.model.view == View::Overview && app.model.page == 1);
  app.dispatch(Next);
  CHECK(app.model.page == 1);
  app.back();
  CHECK(app.model.view == View::Statements && app.model.page == 1);
  app.back();
  CHECK(app.model.view == View::Intro);
  app.dispatch(Mark, 6 * 3 + 2);
  app.dispatch(Accuse);
  app.dispatch(Pick, 6);
  CHECK(!(app.state.innocents & (1 << 6)));
  app.dispatch(Pick, 5);
  CHECK(carnivoro::population(app.state.picks) == 2);
  const auto before = app.state;
  app.dispatch(Confirm);
  CHECK(app.pendingOperation == CarnivoroActivity::PendingOperation::Accusation);
  CHECK(app.saveCalls == 1);
  CHECK(!std::memcmp(&before, &app.state, sizeof(before)));
  CHECK(app.pending.notes[6] == carnivoro::Note::Truth);
  app.pendingOperation = CarnivoroActivity::PendingOperation::None;
  app.dispatch(ChooseCase, 0);
  CHECK(!app.state.innocents && !app.state.picks);
  CHECK(app.state.notes[6] == carnivoro::Note::Unknown);
  app.state.status[0] = carnivoro::Status::Solved;
  app.model.view = View::Verdict;
  const auto closed = app.state;
  app.dispatch(CycleGuilt, 0);
  app.dispatch(CycleNote, 0);
  app.dispatch(ReadStatement, 0);
  CHECK(!std::memcmp(&closed, &app.state, sizeof(closed)));
  CHECK(app.model.view == View::Solution && app.model.openSuspect == 0);
  app.model.openSuspect = -1;
  app.model.page = 1;
  app.dispatch(ShowOverview);
  CHECK(app.model.view == View::Overview);
  app.back();
  CHECK(app.model.view == View::Solution && app.model.page == 1);
  app.dispatch(Campaign);
  const auto saved = app.state;
  CHECK(app.model.view == View::Cases);
  CHECK(app.model.casePage == carnivoro::caseIndex(app.state.caseId) / app.geometry.caseCapacity);
  for (int tier = 0; tier < 4; ++tier) {
    app.dispatch(SelectDifficulty, tier);
    CHECK(static_cast<int>(app.state.difficulty) == tier);
    CHECK(app.state.caseId == saved.caseId && app.state.picks == saved.picks && app.state.innocents == saved.innocents);
    CHECK(!std::memcmp(app.state.status, saved.status, sizeof(saved.status)));
    CHECK(!std::memcmp(app.state.misses, saved.misses, sizeof(saved.misses)));
    CHECK(!std::memcmp(app.state.notes, saved.notes, sizeof(saved.notes)));
    app.model.casePage = 999;
    app.dispatch(Next);
    CHECK(app.model.casePage == (carnivoro::caseCount(app.state.difficulty) - 1) / app.geometry.caseCapacity);
    app.model.casePage = 0;
    app.dispatch(Previous);
    CHECK(app.model.casePage == 0);
  }
  app.dispatch(SelectDifficulty, -1);
  CHECK(app.state.difficulty == carnivoro::Difficulty::Expert);
  app.dispatch(SelectDifficulty, 4);
  CHECK(app.state.difficulty == carnivoro::Difficulty::Expert);
  app.dispatch(ChooseCase, 100);
  CHECK(app.state.caseId == 100 && app.state.difficulty == carnivoro::Difficulty::Normal);
  app.dispatch(SelectDifficulty, 3);
  CHECK(app.state.difficulty == carnivoro::Difficulty::Normal);
  app.back();
  CHECK(app.model.view == View::Cases && app.model.casePage == carnivoro::caseIndex(100) / app.geometry.caseCapacity);
  std::printf("%d production flow checks, 0 failures\n", checks);
}

#pragma once
#include "../ui/PagedText.h"
#include "../ui/PixelMenu.h"
#include "CarnivoroCore.h"
namespace carnivoroui {
namespace fui = freeink::ui;
enum class View : uint8_t { Menu, Cases, Intro, Overview, Statements, Accuse, Verdict, Solution, ResetConfirm, Help };
enum Action : int {
  Open = 1,
  Campaign,
  Help,
  ResetAll,
  ConfirmReset,
  Replay,
  ChooseCase,
  Start,
  Previous,
  Next,
  Accuse,
  Note,
  Mark,
  CycleNote,
  CycleGuilt,
  ShowOverview,
  ReadStatement,
  Pick,
  Confirm,
  Continue,
  RetrySave,
  ViewSolution,
  Menu,
  Back,
  SelectDifficulty,
  ActionCount
};
struct Hit {
  fui::Rect box{};
  int action = 0, value = 0;
};
struct Model {
  const carnivoro::State* state = nullptr;
  carnivoro::Solution solution;
  char formatted[224]{};
  View view = View::Menu;
  int casePage = 0, page = 0, focus = 0, textPages = 1, openSuspect = -1;
  bool campaign = false, saveError = false, keyboardFocus = false;
  int focusAction = 0, focusValue = 0;
};
struct Geometry {
  Hit hits[24]{};
  int count = 0, caseCapacity = 16, suspectCapacity = 1;
};
const char* animalName(int);
const char* difficultyName(carnivoro::Difficulty);
void statement(const carnivoro::Case&, int speaker, char* out, size_t);
void draw(fui::DrawTarget&, const fui::DeviceContext&, Model&, Geometry&, pagedtext::Work&);
template <typename Screen>
void build(Screen& screen, Model& m, Geometry& g, pagedtext::Work& work) {
  m.keyboardFocus = false;
  for (int i = 0; m.view != View::Cases && i < g.count; ++i) {
    if (!screen.frame().buttonSelected(g.hits[i].action, g.hits[i].value)) continue;
    m.keyboardFocus = true;
    m.focus = i;
    m.focusAction = g.hits[i].action;
    m.focusValue = g.hits[i].value;
  }
  draw(screen.target(), screen.device(), m, g, work);
  for (int i = 0; i < g.count; ++i)
    screen.frame().hit(g.hits[i].box, g.hits[i].action, g.hits[i].value, fui::InputTouch);
}
}  // namespace carnivoroui

#pragma once
#include "../ui/ToyboxScreen.h"
#include "DraughtsCore.h"
namespace draughtsui {
enum class Outcome : uint8_t { Playing, White, Black, Draw };
enum class View : uint8_t { Menu, Settings, Board, Help, ConfirmNew, Result, Review };
enum : freeink::ui::ActionId {
  Play = 1,
  Settings,
  Help,
  Rule,
  Difficulty,
  Local,
  NewGame,
  Cancel,
  Square,
  HelpPage,
  HelpDone,
  Colour,
  Menu,
  Review,
  Result
};
struct Model {
  const draughts::Game* game = nullptr;
  const draughts::Generator* legal = nullptr;
  View view = View::Menu;
  draughts::Rules rules = draughts::Rules::International;
  int difficulty = 1, selected = -1;
  bool local = false, active = false, saveFailed = false;
  int helpPage = 0;
  Outcome result = Outcome::Playing;
  bool humanBlack = false;
};
constexpr Outcome outcomeFor(bool ready, int count, bool draw, int turn) {
  return !ready  ? Outcome::Playing
         : draw  ? Outcome::Draw
         : count ? Outcome::Playing
         : turn  ? Outcome::White
                 : Outcome::Black;
}
constexpr bool canPlay(View view, bool revealed, bool ready, bool local, int turn, bool draw, int count,
                       bool humanBlack = false) {
  return view == View::Board && revealed && ready && (local || turn == int(humanBlack)) && !draw && count > 0;
}
freeink::ui::Rect boardRect(const freeink::ui::DeviceContext&, int size);
int squareAt(const freeink::ui::DeviceContext&, int size, int x, int y, bool humanBlack = false);
void build(toybox::Screen&, const Model&);
}  // namespace draughtsui
